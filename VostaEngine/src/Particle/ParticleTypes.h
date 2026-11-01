#pragma once

#include <cstdint>
#include <string>

namespace ve {

	enum class Stage : uint8_t {
		EmitterSpawn,
		EmitterUpdate,
		ParticleSpawn,
		ParticleUpdate
	};

	constexpr uint32_t kStageCount = 4;

	inline constexpr uint8_t stageBit(Stage stage) {
		return static_cast<uint8_t>(1u << static_cast<uint8_t>(stage));
	}

	// 0..7 are runtime-filled; module registers start at 8. These numbers are ABI.
	enum class Implicit : uint8_t {
		ExecIndex,
		Dt,
		SpawnTime,
		SpawnIndex,
		SpawnCount,
		SpawnGroup,
		Alive,
		SpawnRandom
	};

	constexpr uint16_t kUserRegBase = 8;
	constexpr uint16_t kAliveReg = static_cast<uint16_t>(Implicit::Alive);
	constexpr uint16_t kRegisterCount = 256;

	constexpr uint32_t kMaxSpawnSlots = 4;
	constexpr uint32_t kMaxParticles = 1'000'000;

	inline bool isParticleStage(Stage stage) {
		return stage == Stage::ParticleSpawn || stage == Stage::ParticleUpdate;
	}

	inline uint8_t implicitBit(Implicit slot) {
		return static_cast<uint8_t>(1u << static_cast<uint8_t>(slot));
	}

	// Which implicit registers a stage may read; anything else is a compile error.
	inline uint8_t implicitStageMask(Stage stage) {
		switch (stage) {
		case Stage::EmitterSpawn:
		case Stage::EmitterUpdate:
			return implicitBit(Implicit::Dt);
		case Stage::ParticleSpawn:
			return static_cast<uint8_t>(
				implicitBit(Implicit::ExecIndex) | implicitBit(Implicit::Dt) |
				implicitBit(Implicit::SpawnTime) | implicitBit(Implicit::SpawnIndex) |
				implicitBit(Implicit::SpawnCount) | implicitBit(Implicit::SpawnGroup) |
				implicitBit(Implicit::SpawnRandom) | implicitBit(Implicit::Alive));
		case Stage::ParticleUpdate:
			// Alive is readable so a later kill module can compose with the decision
			// an earlier one (Lifetime) already made instead of overwriting it.
			return static_cast<uint8_t>(implicitBit(Implicit::ExecIndex) | implicitBit(Implicit::Dt) |
				implicitBit(Implicit::Alive));
		}
		return 0;
	}

	struct Guid16 {
		uint64_t a = 0;
		uint64_t b = 0;
	};

	// The only interface between EmitterUpdate and ParticleSpawn.
	struct SpawnInfo {
		int32_t count = 0;
		float interpStartDt = 0.0f;
		float intervalDt = 0.0f;
		int32_t spawnGroup = 0;
	};

	struct ParamKey {
		std::string name;
		Guid16 guid;

		bool operator==(const ParamKey& other) const {
			return guid.a == other.guid.a && guid.b == other.guid.b && name == other.name;
		}
	};

}
