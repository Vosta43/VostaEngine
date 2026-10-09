
/* Intended for use within the sandbox application only.*/

#pragma warning(push)
#pragma warning(disable : 4251)

#include "src/Core/Application.h"
#include "src/Core/Log.h"
#include "src/Core/Reflection.h"


#include "src/Input.h"
#include "src/KeyCodes.h"
#include "src/MouseButtonCodes.h"
#include "src/Core/Events/EventDispatcher.h"
#include "src/Core/Events/WindowResizeEvent.h"
#include "src/Core/Events/MouseButtonReleasedEvent.h"
#include "src/Core/Events/MouseButtonPressedEvent.h"
#include "src/Core/Events/MouseScrolledEvent.h"
#include "src/Core/Events/MouseMovedEvent.h"

#include "src/Core/Window.h"

#include "src/Renderer/CameraController.h"
#include "src/Core/Deltatime.h"
#include "src/Core/JobSystem.h"

#include "src/Asset/ImportManager.h"

#include "src/Scene/Components.h"
#include "src/Scene/Scene.h"
#include "src/Scene/SceneSerializer.h"
#include "src/Scene/PrefabRegistry.h"

#include "src/MCP/ToolRegistry.h"
#include "src/MCP/ToolDispatchQueue.h"
#include "src/MCP/CommandRegistry.h"
#include "src/MCP/LlmClient.h"
#include "src/MCP/AgentSession.h"

#include "src/Core/ResourceManager.h"

#include "src/Renderer/StaticMesh.h"
#include "src/Renderer/Material.h"
#include "src/Asset/StaticMeshResource.h"

#include "src/Gui/Gui.h"
#include "src/Renderer/Renderer.h"
#include "src/Renderer/Renderer2D.h"
#include "src/Renderer/Renderer3D.h"
#include "src/Renderer/RenderCommand.h"
#include "src/Renderer/Shader.h"
#include "src/Renderer/Buffer.h"
#include "src/Renderer/VertexArray.h"
#include "src/Renderer/FrameBuffer.h"
#include "src/Renderer/RenderPipeline.h"
#include "src/Renderer/SceneRenderer.h"
#include "src/Renderer/SceneViewRenderer.h"
#include "src/Renderer/ThumbnailRenderer.h"
#include "src/Renderer/Preprocess/BakeService.h"

#pragma warning(pop)