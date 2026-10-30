#pragma once

#include "Core/Core.h"
#include "ParticleTypes.h"
#include "Program.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace ve {

	class AttributeRegistry;
	struct ModuleEntry;

	struct Reg {
		uint16_t offset = 0;
		uint8_t comps = 1;
	};

	class VE_API ProgramBuilder {
	public:
		explicit ProgramBuilder(const AttributeRegistry* layout = nullptr);

		Reg newReg(uint8_t comps);

		Reg loadAttr(const std::string& name);
		void storeAttr(const std::string& name, const Reg& src);

		Reg param(const ModuleEntry& entry, const std::string& name);
		Reg imm(float value);
		Reg imm(const float* values, uint8_t comps);
		Reg implicit(Implicit slot);

		void add(const Reg& dst, const Reg& a, const Reg& b);
		void sub(const Reg& dst, const Reg& a, const Reg& b);
		void mul(const Reg& dst, const Reg& a, const Reg& b);
		void div(const Reg& dst, const Reg& a, const Reg& b);
		void floor(const Reg& dst, const Reg& a);
		void max(const Reg& dst, const Reg& a, const Reg& b);
		void less(const Reg& dst, const Reg& a, const Reg& b);
		// Unit-length copy of a vector; a zero-length input yields zero, not NaN.
		void normalize(const Reg& dst, const Reg& a);
		void sin(const Reg& dst, const Reg& a);
		void cos(const Reg& dst, const Reg& a);
		void sqrt(const Reg& dst, const Reg& a);

		Reg add(const Reg& a, const Reg& b);
		Reg sub(const Reg& a, const Reg& b);
		Reg mul(const Reg& a, const Reg& b);
		Reg div(const Reg& a, const Reg& b);
		Reg floor(const Reg& a);
		Reg max(const Reg& a, const Reg& b);
		Reg less(const Reg& a, const Reg& b);

		Reg normalize(const Reg& a);
		Reg sin(const Reg& a);
		Reg cos(const Reg& a);
		Reg sqrt(const Reg& a);

		// Uniform draw in [0,1). stream keeps consecutive draws from a single seed
		// independent, so one per-particle seed can feed many random values.
		Reg random(const Reg& seed, uint8_t stream);
		// Packs three scalars into a fresh float3 register.
		Reg compose(const Reg& x, const Reg& y, const Reg& z);

		void setAlive(const Reg& value);

		// Resolves attribute offsets against the registry, appends new attributes, and
		// commits everything into out. Fails on a stage/register mismatch or layout error.
		bool finalize(Stage stage, AttributeRegistry& layout, Program& out);

		bool valid() const { return m_valid; }
		const std::string& error() const { return m_error; }
		uint16_t registerCount() const { return m_nextReg; }
		const AttributeRegistry* layout() const { return m_layout; }

		// Tables consumed by Finalize, which resolves registry offsets.
		std::vector<Instruction> code;
		std::vector<ProgramAttr> attrs;
		std::vector<ProgramParam> params;
		std::vector<float> consts;

	private:
		void fail(const std::string& message);
		void emit(Op op, uint8_t comps, uint8_t dst, uint8_t a, uint8_t b, uint8_t flags, uint16_t aux);
		void binary(Op op, const char* what, const Reg& dst, const Reg& a, const Reg& b);
		void insertComponent(const Reg& dst, uint8_t component, const Reg& src);
		uint16_t attrSlot(const std::string& name, uint8_t comps);

		const AttributeRegistry* m_layout = nullptr;
		uint16_t m_nextReg = kUserRegBase;
		bool m_valid = true;
		bool m_finalized = false;
		bool m_writeAlive = false;
		uint8_t m_implicitUse = 0;
		std::string m_error;
		std::unordered_map<std::string, Reg> m_lastWrite;
		std::unordered_map<std::string, uint16_t> m_attrSlots;
		std::vector<std::string> m_attrNames;
		std::vector<Reg> m_paramRegs;
		uint16_t m_paramComponents = 0;
	};

}
