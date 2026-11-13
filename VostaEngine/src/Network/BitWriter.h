#pragma once

#include <cstdint>
#include <vector>

namespace ve {

	class BitWriter {
	public:
		void writeBit(uint32_t value,uint32_t numBits){
			if(numBits == 0)return;
			
			uint64_t mask = (1ull << numBits) - 1;
			acc |= (uint64_t)(value & mask) << bitCount;
			bitCount += numBits;
			
			while(bitCount >= 8){
				buf.push_back((uint8_t)(acc & 0xFF));
				acc >>= 8;
				bitCount -= 8;
			}
		}

		void writeBit(bool b) { writeBit(b ? 1u : 0u,1); }

		void writeBytes(const void* data,uint32_t numBytes){
			const uint8_t* p = (const uint8_t*)data;
			if(bitCount == 0){
				buf.insert(buf.end(),p,p + numBytes);
				return;
			}
			else {
				for(int i = 0;i < numBytes;i ++){
					writeBit(p[i],8);
				}
			}
		}

		void alignToByte() {
			if (bitCount != 0) {
				writeBit(0,8 - bitCount);
			}
		}

		void flush() { alignToByte(); }

		const std::vector<uint8_t>& bytes() { return buf; }

		// Includes the partial trailing byte, so a size budget can be checked
		// mid-stream without flushing (a flush would pad between object blocks).
		size_t sizeBytes() const { return buf.size() + (bitCount != 0 ? 1 : 0); }

	private:
		std::vector<uint8_t> buf;
		uint64_t acc = 0;
		uint32_t  bitCount = 0;

	};

}