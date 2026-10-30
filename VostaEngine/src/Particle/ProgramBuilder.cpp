#include "vepch.h"
#include "ProgramBuilder.h"

#include "AttributeRegistry.h"
#include "EmitterAsset.h"
#include "Core/Log.h"

namespace ve {

	ProgramBuilder::ProgramBuilder(const AttributeRegistry* layout) : m_layout(layout) {}

	void ProgramBuilder::fail(const std::string& message) {
		if (!m_valid)
			return;
		m_valid = false;
		m_error = message;
		VE_CORE_ERROR_PRINT("ProgramBuilder: %s", message.c_str());
	}

	void ProgramBuilder::emit(Op op, uint8_t comps, uint8_t dst, uint8_t a, uint8_t b, uint8_t flags, uint16_t aux) {
		code.push_back({ op, comps, dst, a, b, flags, aux });
	}

	Reg ProgramBuilder::newReg(uint8_t comps) {
		if (comps == 0 || comps > 4) {
			fail("register width outside 1..4");
			return { 0, 1 };
		}
		if (m_nextReg + comps > kRegisterCount) {
			fail("register file exhausted");
			return { 0, 1 };
		}

		Reg reg{ m_nextReg, comps };
		m_nextReg = static_cast<uint16_t>(m_nextReg + comps);
		return reg;
	}

	Reg ProgramBuilder::loadAttr(const std::string& name) {
		const auto written = m_lastWrite.find(name);
		if (written != m_lastWrite.end())
			return written->second;

		const AttributeDesc* desc = m_layout ? m_layout->find(name) : nullptr;
		if (!desc) {
			fail("read of attribute '" + name + "' that no earlier stage declared");
			return { 0, 1 };
		}

		const uint16_t slot = attrSlot(name, desc->comps);
		if (!m_valid)
			return { 0, 1 };

		const Reg dst = newReg(desc->comps);
		emit(Op::LoadAttr, desc->comps, static_cast<uint8_t>(dst.offset), 0, 0, 0, slot);
		return dst;
	}

	void ProgramBuilder::storeAttr(const std::string& name, const Reg& src) {
		if (src.comps == 0 || src.comps > 4) {
			fail("write of attribute '" + name + "' with an invalid width");
			return;
		}

		const uint16_t slot = attrSlot(name, src.comps);
		if (!m_valid)
			return;

		emit(Op::StoreAttr, src.comps, 0, static_cast<uint8_t>(src.offset), 0, 0, slot);
		m_lastWrite[name] = src;
	}

	Reg ProgramBuilder::param(const ModuleEntry& entry, const std::string& name) {
		if (!entry.module) {
			fail("parameter read from a null module");
			return { 0, 1 };
		}

		const ModuleParamDecl* decl = nullptr;
		for (const ModuleParamDecl& candidate : entry.module->params) {
			if (candidate.name == name) {
				decl = &candidate;
				break;
			}
		}
		if (!decl) {
			fail("module '" + entry.module->name + "' has no parameter '" + name + "'");
			return { 0, 1 };
		}

		const ParamKey key{ name, entry.guid };
		for (size_t i = 0; i < params.size(); ++i) {
			if (params[i].key == key)
				return m_paramRegs[i];
		}

		const uint16_t offset = m_paramComponents;
		params.push_back({ key, offset, decl->comps });
		m_paramComponents = static_cast<uint16_t>(m_paramComponents + decl->comps);

		const Reg dst = newReg(decl->comps);
		emit(Op::LoadParam, decl->comps, static_cast<uint8_t>(dst.offset), 0, 0, 0,
			static_cast<uint16_t>(params.size() - 1));
		m_paramRegs.push_back(dst);
		return dst;
	}

	Reg ProgramBuilder::imm(float value) {
		const Reg dst = newReg(1);
		if (!m_valid)
			return dst;

		const uint16_t index = static_cast<uint16_t>(consts.size());
		consts.push_back(value);
		emit(Op::LoadImm, 1, static_cast<uint8_t>(dst.offset), 0, 0, 0, index);
		return dst;
	}

	Reg ProgramBuilder::imm(const float* values, uint8_t comps) {
		const Reg dst = newReg(comps);
		if (!m_valid || !values)
			return dst;

		for (uint8_t c = 0; c < comps; ++c) {
			const uint16_t index = static_cast<uint16_t>(consts.size());
			consts.push_back(values[c]);
			emit(Op::LoadImm, 1, static_cast<uint8_t>(dst.offset + c), 0, 0, 0, index);
		}
		return dst;
	}

	Reg ProgramBuilder::implicit(Implicit slot) {
		m_implicitUse |= implicitBit(slot);
		return { static_cast<uint16_t>(slot), 1 };
	}

	void ProgramBuilder::binary(Op op, const char* what, const Reg& dst, const Reg& a, const Reg& b) {
		if (dst.comps == 0 || dst.comps > 4 || a.comps != dst.comps || (b.comps != dst.comps && b.comps != 1)) {
			fail(std::string(what) + ": operand widths do not line up");
			return;
		}

		const uint8_t flags = (b.comps == 1 && dst.comps > 1) ? kFlagScalarB : 0;
		emit(op, dst.comps, static_cast<uint8_t>(dst.offset), static_cast<uint8_t>(a.offset),
			static_cast<uint8_t>(b.offset), flags, 0);
	}

	void ProgramBuilder::add(const Reg& dst, const Reg& a, const Reg& b) { binary(Op::Add, "add", dst, a, b); }
	void ProgramBuilder::sub(const Reg& dst, const Reg& a, const Reg& b) { binary(Op::Sub, "sub", dst, a, b); }
	void ProgramBuilder::mul(const Reg& dst, const Reg& a, const Reg& b) { binary(Op::Mul, "mul", dst, a, b); }
	void ProgramBuilder::div(const Reg& dst, const Reg& a, const Reg& b) { binary(Op::Div, "div", dst, a, b); }
	void ProgramBuilder::max(const Reg& dst, const Reg& a, const Reg& b) { binary(Op::Max, "max", dst, a, b); }
	void ProgramBuilder::less(const Reg& dst, const Reg& a, const Reg& b) { binary(Op::Less, "less", dst, a, b); }

	void ProgramBuilder::floor(const Reg& dst, const Reg& a) {
		if (a.comps != dst.comps) {
			fail("floor: operand widths do not line up");
			return;
		}
		emit(Op::Floor, dst.comps, static_cast<uint8_t>(dst.offset), static_cast<uint8_t>(a.offset), 0, 0, 0);
	}

	void ProgramBuilder::normalize(const Reg& dst, const Reg& a) {
		if (a.comps != dst.comps || dst.comps > 4) {
			fail("normalize: operand widths do not line up");
			return;
		}
		emit(Op::Normalize, dst.comps, static_cast<uint8_t>(dst.offset), static_cast<uint8_t>(a.offset), 0, 0, 0);
	}

	Reg ProgramBuilder::normalize(const Reg& a) { const Reg dst = newReg(a.comps); normalize(dst, a); return dst; }

	void ProgramBuilder::sin(const Reg& dst, const Reg& a) {
		if (a.comps != dst.comps) {
			fail("sin: operand widths do not line up");
			return;
		}
		emit(Op::Sin, dst.comps, static_cast<uint8_t>(dst.offset), static_cast<uint8_t>(a.offset), 0, 0, 0);
	}

	void ProgramBuilder::cos(const Reg& dst, const Reg& a) {
		if (a.comps != dst.comps) {
			fail("cos: operand widths do not line up");
			return;
		}
		emit(Op::Cos, dst.comps, static_cast<uint8_t>(dst.offset), static_cast<uint8_t>(a.offset), 0, 0, 0);
	}

	void ProgramBuilder::sqrt(const Reg& dst, const Reg& a) {
		if (a.comps != dst.comps) {
			fail("sqrt: operand widths do not line up");
			return;
		}
		emit(Op::Sqrt, dst.comps, static_cast<uint8_t>(dst.offset), static_cast<uint8_t>(a.offset), 0, 0, 0);
	}

	Reg ProgramBuilder::sin(const Reg& a) { const Reg dst = newReg(a.comps); sin(dst, a); return dst; }
	Reg ProgramBuilder::cos(const Reg& a) { const Reg dst = newReg(a.comps); cos(dst, a); return dst; }
	Reg ProgramBuilder::sqrt(const Reg& a) { const Reg dst = newReg(a.comps); sqrt(dst, a); return dst; }

	Reg ProgramBuilder::random(const Reg& seed, uint8_t stream) {
		const Reg dst = newReg(1);
		if (!m_valid)
			return dst;
		if (seed.comps != 1) {
			fail("random: seed must be a scalar");
			return dst;
		}
		emit(Op::Random, 1, static_cast<uint8_t>(dst.offset), static_cast<uint8_t>(seed.offset), 0, 0, stream);
		return dst;
	}

	void ProgramBuilder::insertComponent(const Reg& dst, uint8_t component, const Reg& src) {
		if (!m_valid)
			return;
		if (src.comps != 1 || component >= dst.comps) {
			fail("compose: expects scalar sources and an in-range component");
			return;
		}
		emit(Op::Insert, 1, static_cast<uint8_t>(dst.offset), static_cast<uint8_t>(src.offset), 0, 0, component);
	}

	Reg ProgramBuilder::compose(const Reg& x, const Reg& y, const Reg& z) {
		const Reg dst = newReg(3);
		insertComponent(dst, 0, x);
		insertComponent(dst, 1, y);
		insertComponent(dst, 2, z);
		return dst;
	}

	Reg ProgramBuilder::add(const Reg& a, const Reg& b) { const Reg dst = newReg(a.comps); add(dst, a, b); return dst; }
	Reg ProgramBuilder::sub(const Reg& a, const Reg& b) { const Reg dst = newReg(a.comps); sub(dst, a, b); return dst; }
	Reg ProgramBuilder::mul(const Reg& a, const Reg& b) { const Reg dst = newReg(a.comps); mul(dst, a, b); return dst; }
	Reg ProgramBuilder::div(const Reg& a, const Reg& b) { const Reg dst = newReg(a.comps); div(dst, a, b); return dst; }
	Reg ProgramBuilder::floor(const Reg& a) { const Reg dst = newReg(a.comps); floor(dst, a); return dst; }
	Reg ProgramBuilder::max(const Reg& a, const Reg& b) { const Reg dst = newReg(a.comps); max(dst, a, b); return dst; }
	Reg ProgramBuilder::less(const Reg& a, const Reg& b) { const Reg dst = newReg(a.comps); less(dst, a, b); return dst; }

	void ProgramBuilder::setAlive(const Reg& value) {
		if (value.comps != 1) {
			fail("setAlive expects a scalar");
			return;
		}
		m_writeAlive = true;
		emit(Op::SetAlive, 1, static_cast<uint8_t>(kAliveReg), static_cast<uint8_t>(value.offset), 0, 0, 0);
	}

	uint16_t ProgramBuilder::attrSlot(const std::string& name, uint8_t comps) {
		const auto it = m_attrSlots.find(name);
		if (it != m_attrSlots.end()) {
			if (attrs[it->second].comps != comps) {
				fail("attribute '" + name + "' used with two different widths");
				return 0;
			}
			return it->second;
		}

		const uint16_t slot = static_cast<uint16_t>(attrs.size());
		attrs.push_back({ 0, 0, comps });
		m_attrNames.push_back(name);
		m_attrSlots.emplace(name, slot);
		return slot;
	}

	bool ProgramBuilder::finalize(Stage stage, AttributeRegistry& layout, Program& out) {
		if (m_finalized)
			return false;
		m_finalized = true;

		if (!m_valid)
			return false;

		const uint8_t allowed = implicitStageMask(stage);
		if ((m_implicitUse & ~allowed) != 0) {
			fail("stage reads an implicit register it does not own");
			return false;
		}
		if (m_writeAlive && !isParticleStage(stage)) {
			fail("setAlive is only valid in particle stages");
			return false;
		}

		out = Program{};
		out.attrs.resize(attrs.size());
		uint32_t required = 0;
		for (size_t i = 0; i < attrs.size(); ++i) {
			const uint16_t offset = layout.getOrAdd(m_attrNames[i], attrs[i].comps);
			if (offset == kInvalidComponentOffset) {
				fail("layout rejected attribute '" + m_attrNames[i] + "'");
				return false;
			}

			out.attrs[i].slot = layout.findSlot(m_attrNames[i]);
			out.attrs[i].offset = offset;
			out.attrs[i].comps = attrs[i].comps;
			required = std::max(required, static_cast<uint32_t>(offset) + attrs[i].comps);
		}

		out.code = std::move(code);
		out.params = std::move(params);
		out.consts = std::move(consts);
		out.regCount = m_nextReg;
		out.requiredComponents = required;
		out.stage = stage;
		return true;
	}

}
