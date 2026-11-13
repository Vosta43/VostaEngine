#include "vepch.h"
#include "Network/Rpc.h"

#include <vector>

namespace ve::rpc {

	NetSession*& session() {
		static NetSession* instance = nullptr;
		return instance;
	}

	ScopedRpcSession::ScopedRpcSession(NetSession* net)
		: m_previous(session()) {
		session() = net;
	}

	ScopedRpcSession::~ScopedRpcSession() {
		session() = m_previous;
	}

	RpcRegistry& RpcRegistry::get() {
		static RpcRegistry instance;
		return instance;
	}

	void RpcRegistry::add(const RpcEntry& entry) {
		auto [it, inserted] = m_entries.try_emplace(entry.id, entry);
		if (inserted)
			return;

		if (entry.binding && entry.destroyBinding)
			entry.destroyBinding(entry.binding);

		VE_CORE_ERROR_PRINT("RpcRegistry: id collision between '%s' and '%s'",
			it->second.name ? it->second.name : "?", entry.name ? entry.name : "?");
	}

	void RpcRegistry::finalize() {
		size_t ready = 0;
		for (auto& pair : m_entries) {
			RpcEntry& entry = pair.second;
			entry.ready = false;
			if (!entry.binding || !entry.write || !entry.invoke || !entry.resolveTypes) {
				VE_CORE_ERROR_PRINT("RpcRegistry: '%s' is missing its binding", entry.name);
				continue;
			}

			std::vector<const RepTypeOps*> ops(entry.argCount, nullptr);
			bool resolved = true;
			for (size_t i = 0; i < entry.argCount; ++i) {
				ops[i] = RepLayoutRegistry::get().findType(entry.argTypeNames[i]);
				if (!ops[i]) {
					VE_CORE_ERROR_PRINT("RpcRegistry: '%s' argument %zu has no ops for '%s'",
						entry.name, i, entry.argTypeNames[i]);
					resolved = false;
				}
			}
			if (!resolved)
				continue;

			if (!entry.resolveTypes(entry.binding, ops.data())) {
				VE_CORE_ERROR_PRINT("RpcRegistry: '%s' failed to resolve its argument types", entry.name);
				continue;
			}

			entry.ready = true;
			++ready;
		}
		VE_CORE_INFO_PRINT("RpcRegistry: %zu/%zu RPCs ready", ready, m_entries.size());
	}

	void RpcRegistry::reset() {
		for (auto& pair : m_entries) {
			RpcEntry& entry = pair.second;
			if (entry.binding && entry.destroyBinding)
				entry.destroyBinding(entry.binding);
			entry.binding = nullptr;
		}
		m_entries.clear();
	}

	const RpcEntry* RpcRegistry::find(uint32_t id) const {
		const auto it = m_entries.find(id);
		return it == m_entries.end() ? nullptr : &it->second;
	}

	size_t RpcRegistry::size() const {
		return m_entries.size();
	}

}
