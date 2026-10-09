#include "vepch.h"
#include "MCP/CommandRegistry.h"

#include <utility>

namespace ve {

	CommandRegistry& CommandRegistry::get() {
		static CommandRegistry instance;
		return instance;
	}

	void CommandRegistry::add(CommandInfo command) {
		for (CommandInfo& existing : m_commands) {
			if (existing.name == command.name) {
				existing = std::move(command);
				return;
			}
		}
		m_commands.push_back(std::move(command));
	}

	void CommandRegistry::remove(const std::string& name) {
		for (size_t i = 0; i < m_commands.size(); ++i) {
			if (m_commands[i].name == name) {
				m_commands.erase(m_commands.begin() + i);
				return;
			}
		}
	}

	const std::vector<CommandInfo>& CommandRegistry::list() const {
		return m_commands;
	}

	const CommandInfo* CommandRegistry::find(const std::string& name) const {
		for (const CommandInfo& command : m_commands) {
			if (command.name == name)
				return &command;
		}
		return nullptr;
	}

}
