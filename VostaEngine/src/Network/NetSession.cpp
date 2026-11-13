#include "vepch.h"
#include "Network/NetSession.h"

#include "Network/BitReader.h"
#include "Network/BitWriter.h"
#include "Network/Rpc.h"
#include "Scene/EntityRegistry.h"
#include "Scene/Scene.h"

#include <enet/enet.h>

namespace ve {

	namespace {

		constexpr float kTickRate = 20.0f;
		// Bytes per snapshot before we open another packet. ENet's default MTU is
		// 1400 and oversized datagrams are fragmented through the reliable path,
		// which costs a full packet for every lost fragment.
		constexpr size_t kPacketBudget = 1000;
		// objectCount is an 8-bit field in the snapshot header.
		constexpr uint32_t kMaxObjectsPerPacket = 255;

		int g_enetUsers = 0;

		bool acquireEnet() {
			if (g_enetUsers++ == 0 && enet_initialize() != 0) {
				--g_enetUsers;
				return false;
			}
			return true;
		}

		void releaseEnet() {
			if (g_enetUsers > 0 && --g_enetUsers == 0)
				enet_deinitialize();
		}

		bool ensureLayouts() {
			if (RepLayoutRegistry::get().isFrozen()) {
				// RPC argument ops resolve against the same type table.
				rpc::RpcRegistry::get().finalize();
				return true;
			}

			std::vector<std::string> errors;
			if (RepLayoutRegistry::get().buildFromReflection(errors)) {
				rpc::RpcRegistry::get().finalize();
				return true;
			}

			for (const std::string& error : errors)
				VE_CORE_ERROR_PRINT("NetSession: %s", error.c_str());
			return false;
		}

	}

	NetSession::NetSession() = default;

	NetSession::~NetSession() {
		stop();
	}

	void NetSession::beginSession() {
		m_origin = std::chrono::steady_clock::now();
		m_accumulator = 0.0f;
		m_tickId = 0;
		m_tickTimeMs = 0;
		m_recvTick = 0;
		m_recvTimeMs = 0;
		m_recvOpen = false;
		m_recvStarted = false;
		m_clock.reset();
		m_view.reset();
	}

	double NetSession::localNowMs() const {
		return std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - m_origin).count();
	}

	bool NetSession::startServer(Scene& scene, uint16_t port, uint32_t maxPeers) {
		stop();

		if (!ensureLayouts())
			return false;
		if (!acquireEnet()) {
			VE_CORE_ERROR_PRINT("%s", "NetSession: enet_initialize failed");
			return false;
		}

		ENetAddress address{};
		address.host = ENET_HOST_ANY;
		address.port = port;

		m_host = enet_host_create(&address, maxPeers, 1, 0, 0);
		if (!m_host) {
			VE_CORE_ERROR_PRINT("NetSession: cannot listen on port %u", static_cast<unsigned>(port));
			releaseEnet();
			return false;
		}

		m_scene = &scene;
		m_role = Role::Server;
		beginSession();
		rpc::session() = this;
		VE_CORE_SUCCESS_PRINT("NetSession: listening on port %u", static_cast<unsigned>(port));
		return true;
	}

	bool NetSession::startClient(Scene& scene, const std::string& address, uint16_t port) {
		stop();

		if (!ensureLayouts())
			return false;
		if (!acquireEnet()) {
			VE_CORE_ERROR_PRINT("%s", "NetSession: enet_initialize failed");
			return false;
		}

		m_host = enet_host_create(nullptr, 1, 1, 0, 0);
		if (!m_host) {
			VE_CORE_ERROR_PRINT("%s", "NetSession: cannot create client host");
			releaseEnet();
			return false;
		}

		ENetAddress remote{};
		remote.port = port;
		if (enet_address_set_host(&remote, address.c_str()) != 0) {
			VE_CORE_ERROR_PRINT("NetSession: cannot resolve '%s'", address.c_str());
			enet_host_destroy(m_host);
			m_host = nullptr;
			releaseEnet();
			return false;
		}

		if (!enet_host_connect(m_host, &remote, 1, 0)) {
			VE_CORE_ERROR_PRINT("%s", "NetSession: enet_host_connect failed");
			enet_host_destroy(m_host);
			m_host = nullptr;
			releaseEnet();
			return false;
		}

		m_scene = &scene;
		m_role = Role::Client;
		beginSession();
		rpc::session() = this;
		VE_CORE_SUCCESS_PRINT("NetSession: connecting to %s:%u", address.c_str(),
			static_cast<unsigned>(port));
		return true;
	}

	void NetSession::stop() {
		if (rpc::session() == this)
			rpc::session() = nullptr;
		m_peers.clear();
		m_receiver.reset();
		m_serverPeer = nullptr;
		m_scene = nullptr;
		m_role = Role::None;
		m_connected = false;
		m_accumulator = 0.0f;
		m_stats = NetStats{};
		m_tickId = 0;
		m_tickTimeMs = 0;
		m_recvOpen = false;
		m_recvStarted = false;
		m_clock.reset();
		m_view.reset();

		if (!m_host)
			return;

		enet_host_destroy(m_host);
		m_host = nullptr;
		releaseEnet();
	}

	void NetSession::update(float deltaTime) {
		if (!m_host)
			return;

		pumpEvents();
		if (m_role == Role::Server) {
			tickServer(deltaTime);
		}
		else if (m_role == Role::Client && m_scene && m_clock.started()) {
			// Runs after the scene tick and before rendering: interpolated fields
			// are what the frame draws.
			const SnapshotView::InterpStats interp =
				m_view.interpolate(*m_scene, m_clock.tick(localNowMs()));
			if (interp.starved)
				++m_stats.starvedFrames;
			if (interp.late)
				++m_stats.lateFrames;
			m_stats.interpDelayMs = m_clock.delayMs();
			m_stats.clockOffsetMs = m_clock.offsetMs();
		}

		// RPCs sent from inside a handler land in ENet's queue; one flush per
		// frame pushes them without waiting for the next service call.
		enet_host_flush(m_host);
	}

	void NetSession::pumpEvents() {
		ENetEvent event;
		while (enet_host_service(m_host, &event, 0) > 0) {
			switch (event.type) {
			case ENET_EVENT_TYPE_CONNECT:
				if (m_role == Role::Client) {
					m_connected = true;
					m_serverPeer = event.peer;
					// A reconnect may land on a restarted server whose tick ids begin
					// again at 1; stale receive state would drop every snapshot.
					m_recvOpen = false;
					m_recvStarted = false;
					m_clock.reset();
					m_view.reset();
					VE_CORE_SUCCESS_PRINT("%s", "NetSession: connected to server");
				}
				else {
					m_peers.emplace(event.peer, PeerState{});
					VE_CORE_INFO_PRINT("NetSession: peer %u connected",
						static_cast<unsigned>(event.peer->connectID));
				}
				break;

			case ENET_EVENT_TYPE_DISCONNECT:
				if (m_role == Role::Client) {
					m_connected = false;
					m_serverPeer = nullptr;
					m_recvOpen = false;
					m_recvStarted = false;
					m_clock.reset();
					m_view.reset();
					VE_CORE_WARN_PRINT("%s", "NetSession: disconnected from server");
				}
				else {
					// Dropping the peer state drops its baseline with it, so a
					// reconnect starts from a full resend.
					m_peers.erase(event.peer);
					VE_CORE_WARN_PRINT("%s", "NetSession: peer disconnected");
				}
				break;

			case ENET_EVENT_TYPE_RECEIVE:
				handlePacket(event.packet->data, event.packet->dataLength);
				enet_packet_destroy(event.packet);
				break;

			default:
				break;
			}
		}
	}

	void NetSession::gatherObjects(std::vector<ReplicatedObject>& out) {
		out.clear();
		if (!m_scene)
			return;

		EntityRegistry& registry = m_scene->getRegistry();
		for (uint32_t entityId : registry.each()) {
			for (auto& [name, component] : registry.getComponentsForEntity(entityId)) {
				if (!component)
					continue;
				const RepLayout* layout = RepLayoutRegistry::get().find(name);
				if (!layout)
					continue;
				out.push_back({ NetHandle{ entityId, layout->classId }, layout, component });
			}
		}
	}

	void NetSession::tickServer(float deltaTime) {
		const float step = 1.0f / kTickRate;
		m_accumulator += deltaTime;
		if (m_accumulator < step)
			return;
		// Keep the overshoot or the rate drifts below kTickRate whenever the frame
		// time does not divide the tick period. A stall drops the backlog instead:
		// snapshots carry absolute state, so there is nothing to catch up on.
		m_accumulator -= step;
		if (m_accumulator > step)
			m_accumulator = 0.0f;

		if (m_peers.empty())
			return;

		// One id and one clock stamp for every packet of this tick. They are the
		// client's only way to tell "another logical snapshot", so they must not
		// vary between the packets a split snapshot produces.
		++m_tickId;
		++m_stats.snapshotsSent;
		m_tickTimeMs = (uint32_t)localNowMs();

		std::vector<ReplicatedObject> objects;
		gatherObjects(objects);

		for (auto& [peer, state] : m_peers)
			sendSnapshot(peer, state, objects, m_tickId, m_tickTimeMs);
	}

	void NetSession::sendSnapshot(_ENetPeer* peer, PeerState& state,
		const std::vector<ReplicatedObject>& objects, uint32_t tickId, uint32_t timeMs) {
		struct PendingCommit {
			const RepLayout* layout = nullptr;
			NetHandle handle;
			std::vector<RepChange> changes;
		};

		struct Payload {
			BitWriter writer;
			std::vector<PendingCommit> commits;
			uint32_t count = 0;
		};

		std::vector<Payload> payloads;
		payloads.emplace_back();

		for (const ReplicatedObject& object : objects) {
			std::vector<RepChange> changes;
			if (!state.rep.collect(object.handle, *object.layout, object.component, changes))
				continue;

			if (payloads.back().count >= kMaxObjectsPerPacket
				|| payloads.back().writer.sizeBytes() >= kPacketBudget) {
				payloads.emplace_back();
			}

			Payload& payload = payloads.back();
			state.rep.write(object.handle, *object.layout, changes, payload.writer);
			payload.commits.push_back({ object.layout, object.handle, std::move(changes) });
			++payload.count;
		}

		// [ type:8 ][ tickId:32 ][ serverTimeMs:32 ][ endOfTick:1 ][ objectCount:8 ]
		// The last packet of a tick carries the flag the client closes on. A tick
		// with no changed objects still sends its header: without it the client's
		// timeline stalls and the next change would smear across the gap.
		for (size_t i = 0; i < payloads.size(); ++i) {
			Payload& payload = payloads[i];
			const bool last = (i + 1 == payloads.size());
			if (payload.count == 0 && !last)
				continue;

			BitWriter packet;
			packet.writeBit(static_cast<uint32_t>(NetPacketType::Snapshot), 8);
			packet.writeBit(tickId, 32);
			packet.writeBit(timeMs, 32);
			packet.writeBit(last);
			packet.writeBit(payload.count, 8);
			payload.writer.flush();
			if (!payload.writer.bytes().empty()) {
				packet.writeBytes(payload.writer.bytes().data(),
					static_cast<uint32_t>(payload.writer.bytes().size()));
			}
			packet.flush();

			ENetPacket* data = enet_packet_create(packet.bytes().data(), packet.bytes().size(),
				ENET_PACKET_FLAG_RELIABLE);
			if (!data || enet_peer_send(peer, 0, data) != 0) {
				if (data)
					enet_packet_destroy(data);
				// Nothing is committed: the same objects stay dirty for the next
				// tick, so a failed send costs a resend rather than a lost update.
				return;
			}

			for (PendingCommit& entry : payload.commits)
				state.rep.commit(entry.handle, *entry.layout, entry.changes);

			++m_stats.packetsSent;
			m_stats.objectsSent += payload.count;
			m_stats.bytesSent += packet.bytes().size();
		}
	}

	void NetSession::handlePacket(const uint8_t* data, size_t size) {
		++m_stats.packetsReceived;
		m_stats.bytesReceived += size;

		BitReader reader(data, size);
		const uint32_t type = reader.readBit(8);
		if (reader.isOverflow()) {
			++m_stats.parseErrors;
			return;
		}

		if (type == static_cast<uint32_t>(NetPacketType::Snapshot)) {
			if (m_role == Role::Client)
				handleSnapshot(reader);
			return;
		}
		if (type != static_cast<uint32_t>(NetPacketType::Rpc)) {
			++m_stats.parseErrors;
			return;
		}

		const uint32_t id = reader.readBit(32);
		const rpc::RpcEntry* entry = rpc::RpcRegistry::get().find(id);
		if (!entry || !entry->ready || !m_scene) {
			++m_stats.rpcErrors;
			return;
		}
		// Direction is a property of the local registry; nothing the packet says
		// can make a ServerToAll call legal on a server.
		if (!rpc::canReceive(entry->direction, m_role)) {
			++m_stats.rpcErrors;
			return;
		}

		++m_stats.rpcsReceived;
		if (!entry->invoke(entry->binding, reader, *m_scene))
			++m_stats.rpcErrors;
	}

	bool NetSession::sendRpcPacket(const uint8_t* data, uint32_t size, rpc::Direction direction) {
		if (!m_host || !data || size == 0)
			return false;

		if (direction == rpc::Direction::ClientToServer) {
			if (m_role != Role::Client || !m_serverPeer)
				return false;

			ENetPacket* packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
			if (!packet)
				return false;
			if (enet_peer_send(m_serverPeer, 0, packet) != 0) {
				enet_packet_destroy(packet);
				return false;
			}

			++m_stats.rpcsSent;
			++m_stats.packetsSent;
			m_stats.bytesSent += size;
			return true;
		}

		if (m_role != Role::Server)
			return false;

		// The host takes ownership of the packet, including when no peer is
		// connected.
		ENetPacket* packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
		if (!packet)
			return false;
		enet_host_broadcast(m_host, 0, packet);

		++m_stats.rpcsSent;
		++m_stats.packetsSent;
		m_stats.bytesSent += size;
		return true;
	}

	void NetSession::handleSnapshot(BitReader& reader) {
		const uint32_t tickId = reader.readBit(32);
		const uint32_t timeMs = reader.readBit(32);
		const bool endOfTick = reader.readBit();
		const uint32_t count = reader.readBit(8);
		if (reader.isOverflow() || !m_scene) {
			++m_stats.parseErrors;
			return;
		}

		if (!m_recvStarted) {
			m_recvStarted = true;
			m_recvTick = tickId;
			m_recvTimeMs = timeMs;
			m_recvOpen = true;
		}
		else if (tickId != m_recvTick) {
			// The reliable channel keeps ticks ordered; anything older is stale.
			if ((int32_t)(tickId - m_recvTick) <= 0)
				return;

			// The previous tick never closed because one of its packets failed to
			// send. Close it anyway so its objects still get a timeline sample.
			if (m_recvOpen)
				closeReceivedTick();

			m_recvTick = tickId;
			m_recvTimeMs = timeMs;
			m_recvOpen = true;
		}

		for (uint32_t i = 0; i < count; ++i) {
			NetHandle handle;
			std::vector<RepChange> changes;
			if (!m_receiver.peek(reader, handle, changes)) {
				++m_stats.parseErrors;
				return;
			}
			if (!m_view.applyBlock(handle, changes)) {
				++m_stats.parseErrors;
				return;
			}
			++m_stats.objectsApplied;
		}

		if (endOfTick)
			closeReceivedTick();
	}

	void NetSession::closeReceivedTick() {
		if (!m_recvOpen || !m_scene)
			return;

		m_view.closeTick(*m_scene, m_recvTick, m_recvTimeMs);

		// One clock sample per logical tick, at close: the spread between the
		// packets of a single tick must not leak into the offset estimate.
		const uint32_t rtt = m_serverPeer ? m_serverPeer->lowestRoundTripTime : 0;
		m_clock.onSnapshot(m_recvTimeMs, rtt, localNowMs());

		m_recvOpen = false;
		++m_stats.snapshotsReceived;
	}

}
