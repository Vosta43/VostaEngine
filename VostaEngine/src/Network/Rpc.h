#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "Network/BitReader.h"
#include "Network/BitWriter.h"
#include "Network/NetSession.h"
#include "Network/RepLayout.h"
#include "Scene/Scene.h"

namespace ve::rpc {

	enum class Direction : uint8_t { ClientToServer = 1, ServerToAll = 2 };

	// Compile-time name hash. The id is shared by both ends because the name is
	// written by hand, so a rename is the only thing that changes it.
	constexpr uint32_t IdOf(const char* name) {
		uint32_t hash = 2166136261u;
		while (*name) {
			hash ^= static_cast<uint8_t>(*name++);
			hash *= 16777619u;
		}
		return hash;
	}

	// Spelling used by VEPROPERTY and by RepLayoutRegistry::registerType.
	template<typename T>
	struct RepTypeName;

	template<> struct RepTypeName<bool>       { static constexpr const char* value = "bool"; };
	template<> struct RepTypeName<int32_t>    { static constexpr const char* value = "int"; };
	template<> struct RepTypeName<uint32_t>   { static constexpr const char* value = "uint32_t"; };
	template<> struct RepTypeName<float>      { static constexpr const char* value = "float"; };
	template<> struct RepTypeName<glm::vec2>  { static constexpr const char* value = "glm::vec2"; };
	template<> struct RepTypeName<glm::vec3>  { static constexpr const char* value = "glm::vec3"; };
	template<> struct RepTypeName<glm::dvec3> { static constexpr const char* value = "glm::dvec3"; };
	template<> struct RepTypeName<glm::vec4>  { static constexpr const char* value = "glm::vec4"; };
	template<> struct RepTypeName<glm::ivec2> { static constexpr const char* value = "glm::ivec2"; };
	template<> struct RepTypeName<glm::quat>  { static constexpr const char* value = "glm::quat"; };

	template<typename T>
	constexpr const char* TypeName() { return RepTypeName<T>::value; }

	// Separated hashing keeps ("ab","c") distinct from ("a","bc").
	constexpr uint64_t MixSignature(uint64_t hash, const char* text) {
		while (*text) {
			hash ^= static_cast<uint8_t>(*text++);
			hash *= 1099511628211ull;
		}
		hash ^= 0xFFull;
		hash *= 1099511628211ull;
		return hash;
	}

	// Local check only: it catches a call site whose argument types differ from
	// the declaration. It never goes on the wire and cannot catch a cross-build
	// mismatch.
	template<typename... A>
	constexpr uint64_t SignatureOf() {
		uint64_t hash = 1469598103934665603ull;
		((hash = MixSignature(hash, RepTypeName<std::decay_t<A>>::value)), ...);
		return hash;
	}

	struct RpcEntry {
		uint32_t id = 0;
		const char* name = nullptr;
		Direction direction = Direction::ClientToServer;
		uint64_t signatureHash = 0;
		bool ready = false;
		void* binding = nullptr;
		const char* const* argTypeNames = nullptr;
		size_t argCount = 0;
		void (*write)(void*, BitWriter&, const void* const*) = nullptr;
		bool (*invoke)(void*, BitReader&, Scene&) = nullptr;
		bool (*resolveTypes)(void*, const RepTypeOps* const*) = nullptr;
		void (*destroyBinding)(void*) = nullptr;
	};

	// The active session. NetSession::startServer/startClient install themselves
	// and stop() clears it, so a handler can send without threading a session
	// through every call. One live session per process.
	VE_API NetSession*& session();

	// Swaps the active session for the duration of a scope. A host that owns
	// several sessions (multi-world PIE) wraps each world's tick with one, so a
	// Send_ call inside a module resolves to that world's session rather than to
	// whichever session started last.
	class VE_API ScopedRpcSession {
	public:
		explicit ScopedRpcSession(NetSession* net);
		~ScopedRpcSession();

		ScopedRpcSession(const ScopedRpcSession&) = delete;
		ScopedRpcSession& operator=(const ScopedRpcSession&) = delete;

	private:
		NetSession* m_previous;
	};

	class VE_API RpcRegistry {
	public:
		static RpcRegistry& get();

		void add(const RpcEntry& entry);

		// Resolves every argument's type ops and freezes readiness. Run after the
		// builtin type table exists -- registration happens at module static-init,
		// which is earlier.
		void finalize();

		// Frees the bindings and drops every entry. Call before the module that
		// registered them unloads.
		void reset();

		const RpcEntry* find(uint32_t id) const;
		size_t size() const;

	private:
		RpcRegistry() = default;
		RpcRegistry(const RpcRegistry&) = delete;
		RpcRegistry& operator=(const RpcRegistry&) = delete;

		std::unordered_map<uint32_t, RpcEntry> m_entries;
	};

	template<typename C, typename R, typename... A>
	struct Binding {
		R (C::*fn)(A...) = nullptr;
		std::array<const RepTypeOps*, sizeof...(A)> ops{};

		static void writeArgs(void* self, BitWriter& w, const void* const* argv) {
			auto* binding = static_cast<Binding*>(self);
			for (size_t i = 0; i < binding->ops.size(); ++i) {
				if (!binding->ops[i])
					return;
				binding->ops[i]->write(w, argv[i]);
			}
		}

		static bool invoke(void* self, BitReader& r, Scene& scene) {
			auto* binding = static_cast<Binding*>(self);

			const uint32_t entityId = r.readBit(32);
			// getComponent returns a per-type placeholder for an absent component,
			// so has<C> is required or the handler writes into a fake object.
			if (r.isOverflow() || !scene.getRegistry().has<C>(entityId))
				return false;
			C& object = scene.getComponent<C>(entityId);

			std::tuple<std::decay_t<A>...> args{};
			bool ok = true;
			size_t index = 0;
			auto readOne = [&](auto& arg) {
				if (ok && binding->ops[index])
					binding->ops[index]->read(r, &arg);
				else
					ok = false;
				++index;
			};
			std::apply([&](auto&... a) { (readOne(a), ...); }, args);
			if (!ok || r.isOverflow())
				return false;

			std::apply([&](auto&... a) { (object.*binding->fn)(a...); }, args);
			return true;
		}

		static bool resolveTypes(void* self, const RepTypeOps* const* types) {
			auto* binding = static_cast<Binding*>(self);
			for (size_t i = 0; i < binding->ops.size(); ++i) {
				if (!types[i])
					return false;
				binding->ops[i] = types[i];
			}
			return true;
		}

		static void destroy(void* self) {
			delete static_cast<Binding*>(self);
		}
	};

	template<typename Expected, typename R, typename C, typename... A>
	void RegisterMemberRpc(uint32_t id, const char* name, Direction dir, R (C::*fn)(A...)) {
		static_assert(std::is_same_v<C, Expected>,
			"VE_RPC class name does not match the member function");
		static_assert((!std::is_reference_v<A> && ...),
			"RPC arguments cannot be references");

		static const std::array<const char*, sizeof...(A)> typeNames{
			RepTypeName<std::decay_t<A>>::value... };

		auto* binding = new Binding<C, R, A...>{ fn, {} };

		RpcRegistry::get().add({
			id, name, dir,
			SignatureOf<std::decay_t<A>...>(), false,
			binding, typeNames.data(), typeNames.size(),
			&Binding<C, R, A...>::writeArgs,
			&Binding<C, R, A...>::invoke,
			&Binding<C, R, A...>::resolveTypes,
			&Binding<C, R, A...>::destroy });
	}

	inline bool canSend(Direction direction, NetSession::Role role) {
		if (direction == Direction::ClientToServer)
			return role == NetSession::Role::Client;
		return role == NetSession::Role::Server;
	}

	inline bool canReceive(Direction direction, NetSession::Role role) {
		if (direction == Direction::ClientToServer)
			return role == NetSession::Role::Server;
		return role == NetSession::Role::Client;
	}

	template<uint32_t Id, typename... Args>
	bool Call(NetSession& net, uint32_t entityId, Args&&... args) {
		const RpcEntry* entry = RpcRegistry::get().find(Id);
		if (!entry || !entry->ready)
			return false;
		if (!canSend(entry->direction, net.role()))
			return false;
		if (entry->signatureHash != SignatureOf<std::decay_t<Args>...>())
			return false;

		BitWriter w;
		w.writeBit(static_cast<uint32_t>(NetPacketType::Rpc), 8);
		w.writeBit(Id, 32);
		w.writeBit(entityId, 32);
		const std::array<const void*, sizeof...(Args)> argv{ &args... };
		entry->write(entry->binding, w, argv.data());
		w.flush();

		const std::vector<uint8_t>& bytes = w.bytes();
		return net.sendRpcPacket(bytes.data(), static_cast<uint32_t>(bytes.size()), entry->direction);
	}

	// Sends an event; the result still travels back through replication. Never
	// executes the local function, even when the caller owns the entity.
	template<uint32_t Id, typename... Args>
	bool Call(uint32_t entityId, Args&&... args) {
		NetSession* net = session();
		if (!net)
			return false;
		return Call<Id>(*net, entityId, std::forward<Args>(args)...);
	}

}

#define VE_RPC(ClassName, FnName, Dir)                                            \
	static constexpr uint32_t RpcId_##FnName =                                    \
		::ve::rpc::IdOf(#ClassName "." #FnName);                                  \
	static inline struct RpcReg_##FnName {                                        \
		RpcReg_##FnName() {                                                       \
			::ve::rpc::RegisterMemberRpc<ClassName>(                              \
				ClassName::RpcId_##FnName, #ClassName "." #FnName,                \
				Dir, &ClassName::FnName);                                         \
		}                                                                         \
	} s_rpcReg_##FnName;                                                          \
	template<typename... A>                                                       \
	static bool Send_##FnName(uint32_t entityId, A&&... a) {                      \
		return ::ve::rpc::Call<RpcId_##FnName>(                                   \
			entityId, std::forward<A>(a)...);                                     \
	}
