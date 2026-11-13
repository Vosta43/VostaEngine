#pragma once

#include "Network/NetHandle.h"
#include "Network/RepLayout.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ve {

	using Bytes = std::vector<uint8_t>;

	struct RepChange {
		uint16_t handle = 0;   // index into RepLayout::fields
		Bytes value;           // byte copy of the field, never a pointer
	};

	struct ObjectBaseline {
		std::vector<Bytes> values;   // index == field index; empty == never sent
	};

	struct ConnectionRepState {
		std::unordered_map<NetHandle, ObjectBaseline, NetHandleHash> objects;
	};

	class VE_API RepConnection {
	public:
		bool collect(const NetHandle& handle,const RepLayout& layout,const void* object,std::vector<RepChange>& outChanges);
		void write(const NetHandle& handle, const RepLayout& layout, std::vector<RepChange>& changes,BitWriter& out);

		void commit(const NetHandle& handle, const RepLayout& layout, std::vector<RepChange>& changes);

		// Decodes one object block, resolving the layout from the classId on the wire.
		bool peek(BitReader& in, NetHandle& outHandle, std::vector<RepChange>& outChanges) const;
		bool apply(const RepLayout& layout, const std::vector<RepChange>& changes, void* object) const;

		// Drops every baseline: the next collect() resends all fields in full.
		void reset();

	private:
		ConnectionRepState m_state;
	};

}
