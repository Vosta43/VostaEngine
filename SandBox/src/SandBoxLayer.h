#pragma once
#include <VostaEngine.h>

class SandboxLayer : public ve::Layer {
public:
    SandboxLayer();

    void onAttach() override;
    void onUpdate() override;

private:
    ve::Ref<ve::Scene> m_scene;
    ve::Ref<ve::SceneViewRenderer> m_viewRenderer;
    ve::Ref<ve::RenderPipeline> m_renderPipeline;
    ve::Ref<ve::Framebuffer> m_framebuffer;

    ve::CameraController m_cameraController = ve::CameraController(-1.6f, 1.6f, -0.9f, 0.9f);
};
