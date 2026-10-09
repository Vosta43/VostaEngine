#include "vepch.h"
#include "MCP/HttpClient.h"

#include <atomic>

#include <Windows.h>
#include <winhttp.h>

namespace ve {

	namespace {

		std::wstring widen(const std::string& s) {
			if (s.empty())
				return std::wstring();
			const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
			if (n <= 0)
				return std::wstring();
			std::wstring out(static_cast<size_t>(n), L'\0');
			MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), n);
			return out;
		}

		// Carry the URL in the message. WinHTTP reports a bad host as a bare 12007,
		// which says nothing about *which* name it could not resolve.
		std::string lastError(const char* where, const std::string& url) {
			const DWORD code = GetLastError();
			std::string message = std::string(where) + " failed for " + url +
								  " (WinHTTP error " + std::to_string(code) + ")";
			if (code == ERROR_WINHTTP_NAME_NOT_RESOLVED)
				message += " - that host could not be resolved";
			return message;
		}

		struct UrlParts {
			bool https = true;
			std::wstring host;
			INTERNET_PORT port = 0;
			std::wstring path;                  // always begins with '/'
		};

		// Split a URL into the pieces WinHTTP wants. WinHttpOpenRequest takes the
		// path alone, never the whole URL, so this split is mandatory.
		bool parseUrl(const std::string& url, UrlParts& out) {
			const std::string httpsPrefix = "https://";
			const std::string httpPrefix = "http://";

			std::string rest;
			if (url.rfind(httpsPrefix, 0) == 0) {
				out.https = true;
				rest = url.substr(httpsPrefix.size());
			}
			else if (url.rfind(httpPrefix, 0) == 0) {
				out.https = false;
				rest = url.substr(httpPrefix.size());
			}
			else {
				return false;                   // scheme is required
			}

			const size_t slash = rest.find('/');
			const std::string authority = (slash == std::string::npos) ? rest : rest.substr(0, slash);
			const std::string path = (slash == std::string::npos) ? "/" : rest.substr(slash);
			if (authority.empty())
				return false;

			const size_t colon = authority.rfind(':');
			if (colon != std::string::npos) {
				out.host = widen(authority.substr(0, colon));
				long port = 0;
				for (size_t i = colon + 1; i < authority.size(); ++i) {
					const char c = authority[i];
					if (c < '0' || c > '9')
						return false;
					port = port * 10 + (c - '0');
				}
				out.port = static_cast<INTERNET_PORT>(port);
			}
			else {
				out.host = widen(authority);
				out.port = out.https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
			}

			out.path = widen(path);
			return !out.host.empty();
		}

	}

	struct HttpClient::Impl {
		std::atomic<HINTERNET> inFlight{ nullptr };
		std::atomic<bool> cancelled{ false };
	};

	HttpClient::HttpClient() : m_impl(std::make_unique<Impl>()) {}
	HttpClient::~HttpClient() = default;
	HttpClient::HttpClient(HttpClient&&) noexcept = default;
	HttpClient& HttpClient::operator=(HttpClient&&) noexcept = default;

	void HttpClient::cancel() {
		m_impl->cancelled.store(true);
		// Closing the request handle is the documented way to break a synchronous
		// WinHTTP read. Exactly one of cancel()/post() wins the exchange, so the
		// handle is closed once and never twice.
		HINTERNET h = m_impl->inFlight.exchange(nullptr);
		if (h)
			WinHttpCloseHandle(h);
	}

	HttpClient::Response HttpClient::post(const std::string& url,
										  const std::vector<std::string>& headers,
										  const std::string& body,
										  const ChunkFn& onChunk) {
		Response response;
		m_impl->cancelled.store(false);

		HINTERNET hSession = nullptr;
		HINTERNET hConnect = nullptr;
		HINTERNET hRequest = nullptr;

		// After cancel() closes the request handle, touching it again would be a
		// use-after-close. This narrows that window to nothing (never a guarantee,
		// but the window is now a couple of instructions wide).
		const auto cancelledNow = [this] { return m_impl->cancelled.load(); };

		try {
			UrlParts parts;
			if (!parseUrl(url, parts)) {
				response.error = "invalid URL: " + url;
				return response;
			}

			hSession = WinHttpOpen(L"VostaEngine/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
								   WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
			if (!hSession) {
				response.error = lastError("WinHttpOpen", url);
				return response;
			}

			WinHttpSetTimeouts(hSession, 0, 15000, 30000, 120000);

			hConnect = WinHttpConnect(hSession, parts.host.c_str(), parts.port, 0);
			if (!hConnect) {
				response.error = lastError("WinHttpConnect", url);
			}
			else {
				hRequest = WinHttpOpenRequest(hConnect, L"POST", parts.path.c_str(), nullptr,
											  WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
											  parts.https ? WINHTTP_FLAG_SECURE : 0);
				if (!hRequest) {
					response.error = lastError("WinHttpOpenRequest", url);
				}
			}

			if (hRequest) {
				// Publish the handle before anything blocking, so cancel() has
				// something to close for as much of the call as possible.
				m_impl->inFlight.store(hRequest);

				std::wstring headerBlock;
				for (const std::string& h : headers)
					headerBlock += widen(h) + L"\r\n";
				if (!headerBlock.empty())
					WinHttpAddRequestHeaders(hRequest, headerBlock.c_str(), static_cast<DWORD>(-1),
											 WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);

				if (cancelledNow()) {
					response.error = "cancelled";
				}
				else if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
											 const_cast<char*>(body.data()),
											 static_cast<DWORD>(body.size()),
											 static_cast<DWORD>(body.size()), 0)) {
					response.error = lastError("WinHttpSendRequest", url);
				}
				else if (!WinHttpReceiveResponse(hRequest, nullptr)) {
					response.error = lastError("WinHttpReceiveResponse", url);
				}
				else {
					DWORD statusCode = 0;
					DWORD statusLen = sizeof(statusCode);
					WinHttpQueryHeaders(hRequest,
										WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
										WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusLen, nullptr);
					response.status = static_cast<int>(statusCode);

					for (;;) {
						if (cancelledNow()) {
							response.error = "cancelled";
							break;
						}
						DWORD available = 0;
						if (!WinHttpQueryDataAvailable(hRequest, &available) || available == 0)
							break;
						std::string chunk(available, '\0');
						DWORD read = 0;
						if (!WinHttpReadData(hRequest, chunk.data(), available, &read))
							break;
						response.body.append(chunk.data(), read);
						if (onChunk && read > 0)
							onChunk(chunk.data(), read);
					}
				}
			}
		}
		catch (const std::exception& e) {
			response.error = std::string("HttpClient exception: ") + e.what();
		}
		catch (...) {
			response.error = "HttpClient exception: unknown";
		}

		if (hRequest) {
			// If cancel() already took and closed it, exchange yields null and we
			// must not close it again.
			if (m_impl->inFlight.exchange(nullptr) == hRequest)
				WinHttpCloseHandle(hRequest);
		}
		if (hConnect)
			WinHttpCloseHandle(hConnect);
		if (hSession)
			WinHttpCloseHandle(hSession);

		return response;
	}

}
