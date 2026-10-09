#pragma once

#include "Core/Core.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ve {

	// Blocking HTTP POST over WinHTTP. Transport only — no JSON knowledge, no
	// threads; callers run it on a worker. pimpl keeps <winhttp.h> out of every
	// public header, so the editor links no WinHTTP symbols.
	class VE_API HttpClient {
	public:
		struct Response {
			int status = 0;
			std::string body;
			std::string error;                  // non-empty => transport failure
			bool ok() const { return error.empty() && status >= 200 && status < 300; }
		};

		HttpClient();
		~HttpClient();

		HttpClient(HttpClient&&) noexcept;
		HttpClient& operator=(HttpClient&&) noexcept;

		HttpClient(const HttpClient&) = delete;
		HttpClient& operator=(const HttpClient&) = delete;

		// Called with each block of the body as it arrives, on the calling thread.
		using ChunkFn = std::function<void(const char*, size_t)>;

		// Blocking. Never throws — a failure comes back in Response::error. The body
		// is still accumulated in full, so onChunk is only for streamed consumers.
		Response post(const std::string& url,
					  const std::vector<std::string>& headers,
					  const std::string& body,
					  const ChunkFn& onChunk = {});

		// Abort an in-flight post() from another thread. Safe when idle.
		void cancel();

	private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};

}
