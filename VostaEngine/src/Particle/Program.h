#pragma once

#include "ParticleTypes.h"

#include <cstdint>
#include <vector>

namespace ve {

	enum class Op : uint8_t {
		LoadAttr,
		StoreAttr,
		LoadParam,
		LoadImm,
		Add,
		Sub,
		Mul,
		Div,
		Floor,
		Max,
		Less,
		Normalize,
		SetAlive,
		Insert,     // r[dst + aux] = r[a]
		Extract,    // r[dst] = r[a + aux]
		Random,     // [0,1) hash of the seed register and the stream id in aux
		Sin,
		Cos,
		Sqrt,
		Length      // r[dst] = |r[a .. a + comps)|
	};

	constexpr uint8_t kFlagScalarB = 1u << 0;

	struct Instruction {
		Op op = Op::Add;
		uint8_t comps = 1;
		uint8_t dst = 0;
		uint8_t a = 0;
		uint8_t b = 0;
		uint8_t flags = 0;
		uint16_t aux = 0;   // index into Program::attrs, params or consts
	};

	struct ProgramAttr {
		uint16_t slot = 0;
		uint16_t offset = 0;
		uint8_t comps = 1;
	};

	struct ProgramParam {
		ParamKey key;
		uint16_t offset = 0;
		uint8_t comps = 1;
	};

	struct Program {
		std::vector<Instruction> code;
		std::vector<ProgramAttr> attrs;
		std::vector<ProgramParam> params;
		std::vector<float> consts;
		uint16_t regCount = kUserRegBase;
		uint32_t requiredComponents = 0;
		uint64_t structureHash = 0;
		Stage stage = Stage::ParticleUpdate;
	};

}
