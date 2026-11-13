#include "vepch.h"
#include "RepConnection.h"

#include <cstring>

namespace ve {


	static uint32_t bitsNeeded(uint32_t val){
		uint32_t bits = 0;
		while(val){
			bits++;
			val >>= 1;
		}
		return bits;
	}

	bool RepConnection::collect(const NetHandle& handle, const RepLayout& layout, const void* object, std::vector<RepChange>& outChanges) {
		outChanges.clear();

		ObjectBaseline& base = m_state.objects[handle];
		base.values.resize(layout.fields.size());
		
		for(size_t i = 0;i < layout.fields.size();i ++){
			const RepField& field = layout.fields[i];
			void* current = field.getter(const_cast<void*>(object));

			const Bytes& known = base.values[i];
			if(!known.empty() && field.compare(current,known.data()))
				continue;
			
			RepChange change;
			change.handle = (uint16_t)i;
			change.value.assign((const uint8_t*)current, (const uint8_t*)current + field.size);
			outChanges.push_back(change);
		}

		return !outChanges.empty();
	}

	void RepConnection::write(const NetHandle& handle, const RepLayout& layout, std::vector<RepChange>& changes, BitWriter& out) {
	
		// [ netId:32 ][ classId:32 ][ count:Nb(F) ] { [ handle:Nb(F-1) ][ value ] } x count
		// Bit-packed: flush() before bytes() or the dangling tail bits are lost.
		out.writeBytes(&handle.netId,(uint32_t)sizeof(handle.netId));
		out.writeBytes(&handle.classId, (uint32_t)sizeof(handle.classId));

		const uint32_t fieldCount = (uint32_t)layout.fields.size();
		out.writeBit((uint32_t)changes.size(),bitsNeeded(fieldCount));

		for (const RepChange& change : changes) {
			out.writeBit(change.handle,bitsNeeded(fieldCount - 1));	
			layout.fields[change.handle].write(out,change.value.data());
		}

	}

	void RepConnection::commit(const NetHandle& handle, const RepLayout& layout, std::vector<RepChange>& changes) {
		ObjectBaseline& base = m_state.objects[handle];
		base.values.resize(layout.fields.size());

		for (RepChange& change : changes)
			base.values[change.handle] = std::move(change.value);
		changes.clear();
	}

	bool RepConnection::peek(BitReader& in, NetHandle& outHandle, std::vector<RepChange>& outChanges) const {
		NetHandle handle;
		in.readBytes(&handle.netId, (uint32_t)sizeof(handle.netId));
		in.readBytes(&handle.classId, (uint32_t)sizeof(handle.classId));
		if(in.isOverflow())return false;

		const RepLayout* layout = RepLayoutRegistry::get().find(handle.classId);
		if(!layout)return false;

		const uint32_t fieldCount = (uint32_t)layout->fields.size();
		const uint32_t count = in.readBit(bitsNeeded(fieldCount));
		if(count > fieldCount)return false;

		const uint32_t handleBits = bitsNeeded(fieldCount - 1);

		std::vector<RepChange> parsed;
		parsed.reserve(count);

		for(uint32_t i = 0;i < count;i ++){
			const uint32_t index = in.readBit(handleBits);
			if(index >= fieldCount || in.isOverflow())return false;

			const RepField& field = layout->fields[index];
			RepChange change;
			change.handle = (uint16_t)index;
			change.value.resize(field.size);
			field.read(in, change.value.data());
			if(in.isOverflow())return false;

			parsed.push_back(std::move(change));
		}

		outHandle = handle;
		outChanges = std::move(parsed);
		return true;
	}

	bool RepConnection::apply(const RepLayout& layout, const std::vector<RepChange>& changes, void* object) const {
		if(!object)return false;

		const uint32_t fieldCount = (uint32_t)layout.fields.size();
		for(const RepChange& change : changes){
			if(change.handle >= fieldCount)return false;
			if(change.value.size() != layout.fields[change.handle].size)return false;
		}

		for(const RepChange& change : changes){
			const RepField& field = layout.fields[change.handle];
			std::memcpy(field.getter(object), change.value.data(), field.size);
		}

		return true;
	}

	void RepConnection::reset() {
		m_state.objects.clear();
	}

}

