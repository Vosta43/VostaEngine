#pragma once

#include <string>

namespace ve {
	
	class Entity {
	public:
		Entity() {};
		~Entity() {};
		
		bool operator==(const Entity& other) const { return m_id == other.m_id; }
		bool operator!=(const Entity& other) const { return m_id != other.m_id; }

		uint32_t getId() const {return m_id;}
		std::string getName() const {return m_name;}
	
	public:
		uint32_t m_id = 0xFFFFFFFF;
		// incremented on slot reuse, for handle validation
		uint32_t m_generation = 0;
		std::string m_name;   
	};

}