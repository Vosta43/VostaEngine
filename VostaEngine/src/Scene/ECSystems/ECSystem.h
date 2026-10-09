#pragma once

#include "Core/Core.h"
#include "Core/Log.h"
#include "Renderer/RenderContext.h"
#include "Renderer/RenderPipeline.h"

namespace ve {
	
	class Scene;
	class SceneRenderer;

	class VE_API ECSystemBase {
	public:
		
		virtual void onUpdate(float deltaTime) = 0;

	private:
		
	};


	class VE_API RenderSystem : public ECSystemBase {
	public:
		RenderSystem(const Ref<Scene>& scene);

		void onUpdate(float deltaTime) override;
	
	private:
		RenderPipeline m_renderPipeline;
		Ref<Scene> m_scene;
		// Draw-command collection lives in SceneRenderer; this system only owns
		// the pipeline that consumes it.
		Ref<SceneRenderer> m_collector;
	};


	class VE_API ECSystemRegistry {
	public:
		
		void registerSystem(const std::string& name, Ref<ECSystemBase> system) {
			
			if (m_systemRegistry.find(name) != m_systemRegistry.end()) {
				VE_CORE_ERROR_PRINT("System %s register failed,can not use same name in ECSystem registry!",name);
				return;
			}

			m_systemRegistry[name] = system;

		}

		Ref<ECSystemBase> getSystem(const std::string& name) {
			if (m_systemRegistry.find(name) == m_systemRegistry.end()) {
				VE_CORE_ERROR_PRINT("Can not found System %s", name);
				return nullptr;
			}

			return m_systemRegistry[name];
		}

		static ECSystemRegistry* getInstance();


	private:
		std::unordered_map<std::string,Ref<ECSystemBase>> m_systemRegistry;
		
		ECSystemRegistry() {};
		static ECSystemRegistry* instance;
	};

}