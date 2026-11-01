#include "vepch.h"
#include "Audio/AudioDecode.h"
#include "Audio/AudioInternal.h"

#include "stb_vorbis.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

namespace ve {

	namespace {

		// stb_vorbis data source. When it was opened from a path, stb owns the
		// FILE. When it was opened from miniaudio's read callbacks, the encoded
		// bytes are spooled here because stb keeps a pointer into them.
		struct VorbisSource {
			ma_data_source_base base;
			stb_vorbis* file;
			unsigned char* compressed;
			int channels;
			int sampleRate;
		};

		VorbisSource* asVorbis(ma_data_source* pDataSource) {
			return reinterpret_cast<VorbisSource*>(pDataSource);
		}

		FILE* openAudioFile(const char* path) {
#ifdef _WIN32
			// UTF-8 in, wide path out: non-ASCII asset names must still open.
			const std::filesystem::path filePath(
				std::u8string(reinterpret_cast<const char8_t*>(path)));
			FILE* file = nullptr;
			if (_wfopen_s(&file, filePath.c_str(), L"rb") != 0)
				return nullptr;
			return file;
#else
			return std::fopen(path, "rb");
#endif
		}

		ma_result vorbisRead(ma_data_source* pDataSource, void* pFramesOut,
			ma_uint64 frameCount, ma_uint64* pFramesRead) {
			VorbisSource* source = asVorbis(pDataSource);
			if (pFramesRead) *pFramesRead = 0;
			if (!source || !source->file) return MA_INVALID_ARGS;

			if (!pFramesOut) {
				// Forward seek only: the data source contract allows it and the
				// engine uses it to skip frames.
				const int cursor = stb_vorbis_get_sample_offset(source->file);
				if (cursor < 0) return MA_NOT_IMPLEMENTED;
				if (!stb_vorbis_seek_frame(source->file, (unsigned int)((ma_uint64)cursor + frameCount)))
					return MA_ERROR;
				if (pFramesRead) *pFramesRead = frameCount;
				return MA_SUCCESS;
			}

			const ma_uint64 requested = frameCount * (ma_uint64)source->channels;
			const int capacity = requested > 0x7FFFFFFFull ? 0x7FFFFFFF : (int)requested;
			const int read = stb_vorbis_get_samples_float_interleaved(source->file,
				source->channels, static_cast<float*>(pFramesOut), capacity);
			if (read <= 0) return MA_AT_END;
			if (pFramesRead) *pFramesRead = (ma_uint64)read;
			return MA_SUCCESS;
		}

		ma_result vorbisSeek(ma_data_source* pDataSource, ma_uint64 frameIndex) {
			VorbisSource* source = asVorbis(pDataSource);
			if (!source || !source->file) return MA_INVALID_ARGS;
			return stb_vorbis_seek_frame(source->file, (unsigned int)frameIndex)
				? MA_SUCCESS : MA_ERROR;
		}

		ma_result vorbisGetDataFormat(ma_data_source* pDataSource, ma_format* pFormat,
			ma_uint32* pChannels, ma_uint32* pSampleRate, ma_channel* pChannelMap,
			size_t channelMapCap) {
			VorbisSource* source = asVorbis(pDataSource);
			if (!source) return MA_INVALID_ARGS;
			if (pFormat) *pFormat = ma_format_f32;
			if (pChannels) *pChannels = (ma_uint32)source->channels;
			if (pSampleRate) *pSampleRate = (ma_uint32)source->sampleRate;
			if (pChannelMap) {
				ma_channel_map_init_standard(ma_standard_channel_map_vorbis, pChannelMap,
					channelMapCap, (ma_uint32)source->channels);
			}
			return MA_SUCCESS;
		}

		ma_result vorbisGetCursor(ma_data_source* pDataSource, ma_uint64* pCursor) {
			VorbisSource* source = asVorbis(pDataSource);
			if (!source || !source->file) return MA_INVALID_ARGS;
			const int offset = stb_vorbis_get_sample_offset(source->file);
			*pCursor = offset < 0 ? 0 : (ma_uint64)offset;
			return MA_SUCCESS;
		}

		ma_result vorbisGetLength(ma_data_source* pDataSource, ma_uint64* pLength) {
			VorbisSource* source = asVorbis(pDataSource);
			if (!source || !source->file) return MA_INVALID_ARGS;
			const unsigned int frames = stb_vorbis_stream_length_in_samples(source->file);
			if (frames == 0) {
				*pLength = 0;
				return MA_NOT_IMPLEMENTED;
			}
			*pLength = frames;
			return MA_SUCCESS;
		}

		const ma_data_source_vtable g_vorbisDataSourceVTable = {
			vorbisRead,
			vorbisSeek,
			vorbisGetDataFormat,
			vorbisGetCursor,
			vorbisGetLength,
			nullptr,   // onSetLooping: the mixer owns loop behaviour
			0
		};

		ma_result initVorbisSource(stb_vorbis* file, unsigned char* compressed,
			const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend) {
			VorbisSource* source = static_cast<VorbisSource*>(
				ma_malloc(sizeof(VorbisSource), pAllocationCallbacks));
			if (!source) return MA_OUT_OF_MEMORY;
			std::memset(source, 0, sizeof(VorbisSource));

			source->file = file;
			source->compressed = compressed;
			const stb_vorbis_info info = stb_vorbis_get_info(file);
			source->channels = info.channels;
			source->sampleRate = info.sample_rate;

			ma_data_source_config config = ma_data_source_config_init();
			config.vtable = &g_vorbisDataSourceVTable;
			const ma_result result = ma_data_source_init(&config, &source->base);
			if (result != MA_SUCCESS) {
				ma_free(source, pAllocationCallbacks);
				return result;
			}

			*ppBackend = &source->base;
			return MA_SUCCESS;
		}

		ma_result vorbisInit(void*, ma_read_proc onRead, ma_seek_proc onSeek,
			ma_tell_proc, void* pReadSeekTellUserData, const ma_decoding_backend_config*,
			const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend) {
			if (!onRead || !ppBackend) return MA_INVALID_ARGS;

			// stb_vorbis needs random access, so the encoded stream is spooled
			// (a few MB for a whole song -- far less than its PCM).
			std::vector<unsigned char> bytes;
			unsigned char chunk[64 * 1024];
			for (;;) {
				size_t read = 0;
				const ma_result result = onRead(pReadSeekTellUserData, chunk, sizeof(chunk), &read);
				if (result != MA_SUCCESS && result != MA_AT_END) return result;
				if (read > 0) bytes.insert(bytes.end(), chunk, chunk + read);
				if (read < sizeof(chunk) || result == MA_AT_END) break;
			}
			if (bytes.empty() || bytes.size() > 0x7FFFFFFFull) return MA_INVALID_FILE;

			unsigned char* compressed = static_cast<unsigned char*>(
				ma_malloc(bytes.size(), pAllocationCallbacks));
			if (!compressed) return MA_OUT_OF_MEMORY;
			std::memcpy(compressed, bytes.data(), bytes.size());

			int error = 0;
			stb_vorbis* vorbis = stb_vorbis_open_memory(compressed, (int)bytes.size(), &error, nullptr);
			if (!vorbis) {
				ma_free(compressed, pAllocationCallbacks);
				return MA_INVALID_FILE;
			}

			const ma_result result = initVorbisSource(vorbis, compressed, pAllocationCallbacks, ppBackend);
			if (result != MA_SUCCESS) {
				stb_vorbis_close(vorbis);
				ma_free(compressed, pAllocationCallbacks);
			}
			if (onSeek) onSeek(pReadSeekTellUserData, 0, ma_seek_origin_start);
			return result;
		}

		ma_result vorbisInitFile(void*, const char* pFilePath, const ma_decoding_backend_config*,
			const ma_allocation_callbacks* pAllocationCallbacks, ma_data_source** ppBackend) {
			FILE* file = openAudioFile(pFilePath);
			if (!file) return MA_INVALID_FILE;

			// close_on_free: stb owns the FILE from here, including on failure.
			int error = 0;
			stb_vorbis* vorbis = stb_vorbis_open_file(file, 1, &error, nullptr);
			if (!vorbis)
				return MA_INVALID_FILE;

			const ma_result result = initVorbisSource(vorbis, nullptr, pAllocationCallbacks, ppBackend);
			if (result != MA_SUCCESS) stb_vorbis_close(vorbis);
			return result;
		}

		void vorbisUninit(void*, ma_data_source* pBackend, const ma_allocation_callbacks* pAllocationCallbacks) {
			VorbisSource* source = reinterpret_cast<VorbisSource*>(pBackend);
			if (!source) return;
			if (source->file) stb_vorbis_close(source->file);
			if (source->compressed) ma_free(source->compressed, pAllocationCallbacks);
			ma_data_source_uninit(pBackend);
			ma_free(source, pAllocationCallbacks);
		}

		ma_decoding_backend_vtable g_vorbisBackendVTable = {
			vorbisInit,
			vorbisInitFile,
			nullptr,   // onInitFileW
			nullptr,   // onInitMemory
			vorbisUninit
		};

		ma_decoding_backend_vtable* g_customBackends[] = { &g_vorbisBackendVTable };

		ma_decoder_config makeDecoderConfig() {
			ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
			config.ppCustomBackendVTables = g_customBackends;
			config.customBackendCount = 1;
			return config;
		}

	}

	void attachAudioCustomDecoders(ma_resource_manager_config& config) {
		config.ppCustomDecodingBackendVTables = g_customBackends;
		config.customDecodingBackendCount = 1;
	}

	bool probeAudioFile(const std::string& absolutePath, double& outDurationSeconds,
		uint32_t& outSampleRate, uint32_t& outChannels) {
		ma_decoder decoder;
		ma_decoder_config config = makeDecoderConfig();
		if (ma_decoder_init_file(absolutePath.c_str(), &config, &decoder) != MA_SUCCESS)
			return false;

		ma_format format = ma_format_unknown;
		ma_uint32 channels = 0;
		ma_uint32 sampleRate = 0;
		ma_uint64 lengthInFrames = 0;
		const ma_result formatResult = ma_decoder_get_data_format(&decoder, &format,
			&channels, &sampleRate, nullptr, 0);
		const ma_result lengthResult = ma_decoder_get_length_in_pcm_frames(&decoder, &lengthInFrames);
		ma_decoder_uninit(&decoder);
		(void)format;

		if (formatResult != MA_SUCCESS || lengthResult != MA_SUCCESS)
			return false;
		if (lengthInFrames == 0 || sampleRate == 0 || channels == 0)
			return false;

		outDurationSeconds = (double)lengthInFrames / (double)sampleRate;
		outSampleRate = sampleRate;
		outChannels = channels;
		return true;
	}

	bool decodeAudioFile(const std::string& absolutePath, DecodedAudio& out) {
		ma_decoder decoder;
		ma_decoder_config config = makeDecoderConfig();
		if (ma_decoder_init_file(absolutePath.c_str(), &config, &decoder) != MA_SUCCESS)
			return false;

		ma_format format = ma_format_unknown;
		ma_uint32 channels = 0;
		ma_uint32 sampleRate = 0;
		if (ma_decoder_get_data_format(&decoder, &format, &channels, &sampleRate, nullptr, 0) != MA_SUCCESS
			|| channels == 0 || sampleRate == 0) {
			ma_decoder_uninit(&decoder);
			return false;
		}
		(void)format;

		ma_uint64 lengthInFrames = 0;
		if (ma_decoder_get_length_in_pcm_frames(&decoder, &lengthInFrames) == MA_SUCCESS
			&& lengthInFrames > 0) {
			out.samples.reserve((size_t)lengthInFrames * channels);
		}

		constexpr ma_uint64 kChunkFrames = 4096;
		std::vector<float> chunk((size_t)kChunkFrames * channels);
		for (;;) {
			ma_uint64 framesRead = 0;
			const ma_result result = ma_decoder_read_pcm_frames(&decoder, chunk.data(),
				kChunkFrames, &framesRead);
			if (framesRead > 0) {
				const size_t count = (size_t)framesRead * channels;
				out.samples.insert(out.samples.end(), chunk.begin(), chunk.begin() + count);
			}
			if (framesRead < kChunkFrames)
				break;
			if (result != MA_SUCCESS && result != MA_AT_END)
				break;
		}
		ma_decoder_uninit(&decoder);

		if (out.samples.empty())
			return false;

		out.channels = channels;
		out.sampleRate = sampleRate;
		out.durationSeconds = (double)out.frameCount() / (double)sampleRate;
		return true;
	}

}
