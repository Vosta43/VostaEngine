#pragma once

#include "Core/Core.h"
#include "Network/BitReader.h"
#include "Network/NetClock.h"
#include "Network/NetHandle.h"
#include "Network/RepConnection.h"
#include "Network/RepLayout.h"
#include "Network/SnapshotView.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct _ENetHost;
struct _ENetPeer;

namespace ve {

	class Scene;

	namespace rpc { enum class Direction : uint8_t; }

	enum class NetPacketType : uint8_t {
		Snapshot = 1,
		Rpc      = 2
	};

	struct NetStats {
		uint64_t packetsSent = 0;
		uint64_t packetsReceived = 0;
		uint64_t objectsSent = 0;
		uint64_t objectsApplied = 0;
		uint64_t bytesSent = 0;
		uint64_t bytesReceived = 0;
		uint64_t parseErrors = 0;
		uint64_t rpcsSent = 0;
		uint64_t rpcsReceived = 0;
		uint64_t rpcErrors = 0;

		// Client-side interpolation telemetry.
		uint64_t snapshotsSent = 0;
		uint64_t snapshotsReceived = 0;
		uint64_t starvedFrames = 0;   // render time behind the oldest sample
		uint64_t lateFrames = 0;      // render time past the newest sample
		float    interpDelayMs = 0.0f;
		double   clockOffsetMs = 0.0;
	};

	// Server-authoritative replication over one reliable ENet channel. The server
	// sends a snapshot at a fixed 20 Hz; a client only applies what arrives.
	// Every peer keeps its own baseline, so a peer that joins late -- or one whose
	// packets were dropped before the transport acked them -- starts from a full
	// resend without any extra handshake.
	class VE_API NetSession {
	public:
		enum class Role { None, Server, Client };

		NetSession();
		~NetSession();

		NetSession(const NetSession&) = delete;
		NetSession& operator=(const NetSession&) = delete;

		bool startServer(Scene& scene, uint16_t port, uint32_t maxPeers = 32);
		bool startClient(Scene& scene, const std::string& address, uint16_t port);
		void stop();

		// Pumps ENet and, on the server, sends at most one snapshot per 1/20 s.
		void update(float deltaTime);

		Role role() const { return m_role; }
		bool isConnected() const { return m_connected; }
		size_t peerCount() const { return m_peers.size(); }
		const NetStats& stats() const { return m_stats; }

		// One packet per RPC. ClientToServer goes to the cached server peer;
		// ServerToAll is broadcast to every connected peer. Sending never retries:
		// the RPC is treated as droppable and the caller only observes false.
		bool sendRpcPacket(const uint8_t* data, uint32_t size, rpc::Direction direction);

	private:
		struct ReplicatedObject {
			NetHandle handle;
			const RepLayout* layout = nullptr;
			void* component = nullptr;
		};

		struct PeerState {
			RepConnection rep;
		};

		void beginSession();
		double localNowMs() const;

		void pumpEvents();
		void gatherObjects(std::vector<ReplicatedObject>& out);
		void tickServer(float deltaTime);
		void sendSnapshot(_ENetPeer* peer, PeerState& state, const std::vector<ReplicatedObject>& objects,
			uint32_t tickId, uint32_t timeMs);
		void handlePacket(const uint8_t* data, size_t size);
		void handleSnapshot(BitReader& reader);
		void closeReceivedTick();

		_ENetHost* m_host = nullptr;
		Scene* m_scene = nullptr;
		Role m_role = Role::None;
		bool m_connected = false;
		float m_accumulator = 0.0f;
		_ENetPeer* m_serverPeer = nullptr;
		std::unordered_map<_ENetPeer*, PeerState> m_peers;
		RepConnection m_receiver;
		NetStats m_stats;

		// Appended after NetStats so modules built against the previous header
		// still read every field at its old offset.
		uint32_t m_tickId = 0;
		uint32_t m_tickTimeMs = 0;
		uint32_t m_recvTick = 0;
		uint32_t m_recvTimeMs = 0;
		bool m_recvOpen = false;
		bool m_recvStarted = false;
		NetClock m_clock;
		SnapshotView m_view;
		std::chrono::steady_clock::time_point m_origin;
	};

}
