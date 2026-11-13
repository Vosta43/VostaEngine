// Two-process replication smoke test. Run "NetSmoke server" in one console and
// "NetSmoke client" in another; both build the same scene (same entity ids in
// the same order), the server drifts every Player.health and the client prints
// what it applied.

#include <VostaEngine.h>

#include "src/Network/NetSession.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace {

	constexpr int kEntityCount = 4;

	// Round-trip probe. The client asks, the server answers, and the last answer
	// must match the last request because both travel on the reliable channel.
	struct SmokeRpc {
		VE_RPC(SmokeRpc, Ping, ve::rpc::Direction::ClientToServer)
		void Ping(uint32_t seq) {
			++Pings;
			SmokeRpc::Send_Pong(EntityId, seq);
		}

		VE_RPC(SmokeRpc, Pong, ve::rpc::Direction::ServerToAll)
		void Pong(uint32_t seq) {
			++Pongs;
			LastPong = seq;
		}

		static inline uint32_t EntityId = 0;
		static inline uint32_t Pings = 0;
		static inline uint32_t Pongs = 0;
		static inline uint32_t LastPing = 0;
		static inline uint32_t LastPong = 0;
	};

	void buildScene(ve::Scene& scene, std::vector<ve::Entity>& entities) {
		for (int i = 0; i < kEntityCount; ++i) {
			ve::Entity entity = scene.createEntity();
			ve::Player& player = scene.assignComponent<ve::Player>(entity);
			player.health = 100.0f;
			entities.push_back(entity);
		}

		// Same creation order on both ends, so this id matches.
		scene.assignComponent<SmokeRpc>(entities[0]);
		SmokeRpc::EntityId = entities[0].getId();
	}

}

int main(int argc, char** argv) {
	ve::Logger::Get().printAllToConsole();

	const bool server = argc > 1 && std::string(argv[1]) == "server";
	const uint16_t port = argc > 2 ? static_cast<uint16_t>(std::atoi(argv[2])) : 7777u;
	const float seconds = argc > 3 ? static_cast<float>(std::atof(argv[3])) : 8.0f;

	ve::Scene scene;
	std::vector<ve::Entity> entities;
	buildScene(scene, entities);

	ve::NetSession session;
	const bool started = server
		? session.startServer(scene, port)
		: session.startClient(scene, "127.0.0.1", port);
	if (!started) {
		std::printf("NetSmoke: session failed to start\n");
		return 1;
	}

	float elapsed = 0.0f;
	float printTimer = 0.0f;
	int tick = 0;
	auto lastFrame = std::chrono::steady_clock::now();

	auto stepOnce = [&](bool allowPing) {
		if (server) {
			for (size_t i = 0; i < entities.size(); ++i)
				scene.getComponent<ve::Player>(entities[i]).health =
					50.0f + 10.0f * static_cast<float>(i) + elapsed * 5.0f;
		}

		const auto now = std::chrono::steady_clock::now();
		const float step = std::chrono::duration<float>(now - lastFrame).count();
		lastFrame = now;

		if (!server && allowPing && session.isConnected() && (tick % 5) == 0) {
			++SmokeRpc::LastPing;
			SmokeRpc::Send_Ping(SmokeRpc::EntityId, SmokeRpc::LastPing);
		}
		++tick;

		session.update(step);
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		elapsed += step;

		printTimer += step;
		if (printTimer >= 0.5f) {
			printTimer = 0.0f;
			const ve::NetStats& stats = session.stats();
			std::printf("[%s] t=%.1f %s=%u packets=%llu/%llu objects=%llu/%llu bytes=%llu/%llu rpc=%llu/%llu errors=%llu/%llu snap=%llu/%llu starved=%llu late=%llu delay=%.0f offset=%.1f\n",
				server ? "server" : "client", elapsed,
				server ? "peers" : "connected",
				server ? static_cast<unsigned>(session.peerCount()) : (session.isConnected() ? 1u : 0u),
				static_cast<unsigned long long>(stats.packetsSent),
				static_cast<unsigned long long>(stats.packetsReceived),
				static_cast<unsigned long long>(stats.objectsSent),
				static_cast<unsigned long long>(stats.objectsApplied),
				static_cast<unsigned long long>(stats.bytesSent),
				static_cast<unsigned long long>(stats.bytesReceived),
				static_cast<unsigned long long>(stats.rpcsSent),
				static_cast<unsigned long long>(stats.rpcsReceived),
				static_cast<unsigned long long>(stats.parseErrors),
				static_cast<unsigned long long>(stats.rpcErrors),
				static_cast<unsigned long long>(stats.snapshotsSent),
				static_cast<unsigned long long>(stats.snapshotsReceived),
				static_cast<unsigned long long>(stats.starvedFrames),
				static_cast<unsigned long long>(stats.lateFrames),
				static_cast<double>(stats.interpDelayMs),
				stats.clockOffsetMs);
			std::fflush(stdout);
		}
	};

	// The server outlives the client's drain: it is started first, so an equal
	// duration would let it exit before the client's last pings are answered.
	const float runFor = server ? seconds + 3.0f : seconds;
	while (elapsed < runFor)
		stepOnce(true);

	// Stop asking and let the last request/response drain.
	const float drainUntil = elapsed + 1.0f;
	while (elapsed < drainUntil)
		stepOnce(false);

	session.stop();

	std::printf("[%s] final health:", server ? "server" : "client");
	for (const ve::Entity& entity : entities)
		std::printf(" %u=%.1f", entity.getId(),
			static_cast<double>(scene.getComponent<ve::Player>(entity).health));
	std::printf("\n");

	if (!server) {
		const bool ok = session.stats().parseErrors == 0
			&& session.stats().rpcErrors == 0
			&& SmokeRpc::Pongs > 0
			&& SmokeRpc::LastPong == SmokeRpc::LastPing;
		std::printf("[client] rpc ping=%u pong=%u last=%u/%u -> %s\n",
			SmokeRpc::LastPing, SmokeRpc::Pongs, SmokeRpc::LastPong, SmokeRpc::LastPing,
			ok ? "PASS" : "FAIL");
		std::fflush(stdout);
		return ok ? 0 : 2;
	}

	std::printf("[server] rpc pings=%u pongs=%u\n", SmokeRpc::Pings, SmokeRpc::Pongs);
	return 0;
}
