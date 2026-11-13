#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace ve {

	// Mirrors BitWriter: LSB-first within a byte, bytes consumed in write order.
	// Reads past the end return zero bits and set overflow, never throw.
	class BitReader {
	public:
		BitReader(const uint8_t* data, size_t size)
			: data(data), size(size) {}

		explicit BitReader(const std::vector<uint8_t>& bytes)
			: data(bytes.data()), size(bytes.size()) {}

		uint32_t readBit(uint32_t numBits) {
			if(numBits == 0)return 0;
			if(numBits > 32){
				overflow = true;
				return 0;
			}

			while(bitCount < numBits && bytePos < size){
				acc |= (uint64_t)data[bytePos++] << bitCount;
				bitCount += 8;
			}

			const uint32_t take = bitCount < numBits ? bitCount : numBits;
			const uint32_t value = (uint32_t)(acc & lowMask(take));
			acc >>= take;
			bitCount -= take;

			if(take < numBits)overflow = true;
			return value;
		}

		bool readBit() { return readBit(1) != 0; }

		void readBytes(void* out,uint32_t numBytes){
			uint8_t* p = (uint8_t*)out;
			if(bitCount == 0){
				const size_t avail = bytePos < size ? size - bytePos : 0;
				const size_t n = numBytes < avail ? numBytes : avail;
				if(n)std::memcpy(p,data + bytePos,n);
				bytePos += n;
				if(n < numBytes){
					std::memset(p + n,0,numBytes - n);
					overflow = true;
				}
				return;
			}
			for(uint32_t i = 0;i < numBytes;i ++){
				p[i] = (uint8_t)readBit(8);
			}
		}

		void alignToByte() {
			if (bitCount % 8) {
				readBit(bitCount % 8);
			}
		}

		bool isOverflow() const { return overflow; }

		size_t bitPosition() const { return bytePos * 8 - bitCount; }

		size_t bitsRemaining() const { return (size - bytePos) * 8 + bitCount; }

		bool atEnd() const { return bitsRemaining() == 0; }

	private:
		static uint64_t lowMask(uint32_t bits) {
			return bits >= 64 ? ~0ull : ((1ull << bits) - 1);
		}

		const uint8_t* data = nullptr;
		size_t size = 0;
		size_t bytePos = 0;
		uint64_t acc = 0;
		uint32_t bitCount = 0;
		bool overflow = false;

	};

}
