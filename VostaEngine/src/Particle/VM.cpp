#include "vepch.h"
#include "VM.h"

#include <cmath>
#include <cstring>

namespace ve {

	namespace {

		uint32_t hash32(uint32_t x) {
			x ^= x >> 16;
			x *= 0x7feb352dU;
			x ^= x >> 15;
			x *= 0x846ca68bU;
			x ^= x >> 16;
			return x;
		}

	}

	bool VM::run(const ExecContext& context) {
		const Program& program = *context.program;
		float r[kRegisterCount];

		r[static_cast<uint16_t>(Implicit::ExecIndex)] = static_cast<float>(context.srcIndex);
		r[static_cast<uint16_t>(Implicit::Dt)] = context.dt;
		r[static_cast<uint16_t>(Implicit::SpawnTime)] = context.spawnTime;
		r[static_cast<uint16_t>(Implicit::SpawnIndex)] = static_cast<float>(context.spawnIndex);
		r[static_cast<uint16_t>(Implicit::SpawnCount)] = static_cast<float>(context.spawnCount);
		r[static_cast<uint16_t>(Implicit::SpawnGroup)] = static_cast<float>(context.spawnGroup);
		r[static_cast<uint16_t>(Implicit::SpawnRandom)] = context.spawnRandom;
		r[kAliveReg] = 1.0f;

		for (const Instruction& i : program.code) {
			switch (i.op) {
			case Op::LoadAttr: {
				const ProgramAttr& attr = program.attrs[i.aux];
				for (uint8_t c = 0; c < attr.comps; ++c)
					r[i.dst + c] = *context.src->attr(static_cast<uint16_t>(attr.offset + c), context.srcIndex);
				break;
			}
			case Op::StoreAttr: {
				const ProgramAttr& attr = program.attrs[i.aux];
				for (uint8_t c = 0; c < attr.comps; ++c)
					*context.dst->attr(static_cast<uint16_t>(attr.offset + c), context.dstIndex) = r[i.a + c];
				break;
			}
			case Op::LoadParam: {
				const ProgramParam& param = program.params[i.aux];
				for (uint8_t c = 0; c < param.comps; ++c)
					r[i.dst + c] = context.paramBlock[param.offset + c];
				break;
			}
			case Op::LoadImm:
				r[i.dst] = program.consts[i.aux];
				break;
			case Op::Add:
			case Op::Sub:
			case Op::Mul:
			case Op::Div: {
				const bool scalarB = (i.flags & kFlagScalarB) != 0;
				for (uint8_t c = 0; c < i.comps; ++c) {
					const float b = r[i.b + (scalarB ? 0 : c)];
					switch (i.op) {
					case Op::Add: r[i.dst + c] = r[i.a + c] + b; break;
					case Op::Sub: r[i.dst + c] = r[i.a + c] - b; break;
					case Op::Mul: r[i.dst + c] = r[i.a + c] * b; break;
					default:      r[i.dst + c] = r[i.a + c] / b; break;
					}
				}
				break;
			}
			case Op::Floor:
				for (uint8_t c = 0; c < i.comps; ++c)
					r[i.dst + c] = std::floor(r[i.a + c]);
				break;
			case Op::Max:
			case Op::Less: {
				const bool scalarB = (i.flags & kFlagScalarB) != 0;
				for (uint8_t c = 0; c < i.comps; ++c) {
					const float b = r[i.b + (scalarB ? 0 : c)];
					r[i.dst + c] = i.op == Op::Max ? std::max(r[i.a + c], b) : (r[i.a + c] < b ? 1.0f : 0.0f);
				}
				break;
			}
			case Op::Normalize: {
				float lengthSquared = 0.0f;
				for (uint8_t c = 0; c < i.comps; ++c)
					lengthSquared += r[i.a + c] * r[i.a + c];

				// A zero vector has no direction; leave the result at zero rather than
				// dividing by zero (particles spawned exactly on a point-force center).
				const float inverseLength = lengthSquared > 1e-12f ? 1.0f / std::sqrt(lengthSquared) : 0.0f;
				for (uint8_t c = 0; c < i.comps; ++c)
					r[i.dst + c] = r[i.a + c] * inverseLength;
				break;
			}
			case Op::SetAlive:
				r[kAliveReg] = r[i.a];
				break;
			case Op::Insert:
				r[i.dst + i.aux] = r[i.a];
				break;
			case Op::Extract:
				r[i.dst] = r[i.a + i.aux];
				break;
			case Op::Random: {
				uint32_t seedBits;
				std::memcpy(&seedBits, &r[i.a], sizeof(seedBits));
				const uint32_t stream = static_cast<uint32_t>(i.aux) * 0x9E3779B9U;
				const uint32_t h = hash32(seedBits ^ stream);
				// 24 bits keep the result exact in float.
				r[i.dst] = static_cast<float>(h >> 8) * (1.0f / 16777216.0f);
				break;
			}
			case Op::Sin:
				for (uint8_t c = 0; c < i.comps; ++c)
					r[i.dst + c] = std::sin(r[i.a + c]);
				break;
			case Op::Cos:
				for (uint8_t c = 0; c < i.comps; ++c)
					r[i.dst + c] = std::cos(r[i.a + c]);
				break;
			case Op::Sqrt:
				for (uint8_t c = 0; c < i.comps; ++c)
					r[i.dst + c] = std::sqrt(r[i.a + c]);
				break;
			case Op::Length: {
				float lengthSquared = 0.0f;
				for (uint8_t c = 0; c < i.comps; ++c)
					lengthSquared += r[i.a + c] * r[i.a + c];
				r[i.dst] = std::sqrt(lengthSquared);
				break;
			}
			}
		}

		return r[kAliveReg] != 0.0f;
	}

}
