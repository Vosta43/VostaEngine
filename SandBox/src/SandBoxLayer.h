#pragma once
#include <VostaEngine.h>

class SandboxLayer : public ve::Layer {
public:
    SandboxLayer();

    void onAttach() override;
    void onUpdate() override;

private:
    void bakeIBL(ve::AssetHandle skyboxHandle);

    ve::Ref<ve::Scene> m_scene;
    ve::Ref<ve::SceneRenderer> m_sceneRenderer;
    ve::Ref<ve::RenderPipeline> m_renderPipeline;
    ve::Ref<ve::Framebuffer> m_framebuffer;

    ve::CameraController m_cameraController = ve::CameraController(-1.6f, 1.6f, -0.9f, 0.9f);

    // IBL
    ve::Ref<ve::TextureCubeMap> m_irradianceMap;
    ve::Ref<ve::TextureCubeMap> m_prefilteredEnvMap;
    ve::Ref<ve::Texture2D> m_brdfLUT;
    ve::AssetHandle m_bakedSkyboxHandle;

    // Cloud noise volumes (baked once, same role as IBL). The shape volume is a
    // multi-octave RGBA Worley field; the detail volume is a second independent
    // Worley field (finer cells + different seed); the warp volume holds two
    // pre-summed FBM fields that bend the shape sample position.
    ve::Ref<ve::Texture3D> m_cloudNoiseTexture;
    uint32_t m_cloudWorleyCells = 1;
    ve::Ref<ve::Texture3D> m_cloudDetailTexture;
    uint32_t m_cloudDetailCells = 1;
    ve::Ref<ve::Texture3D> m_cloudWarpTexture;
    uint32_t m_cloudWarpCells = 1;
    // 2D weather map (R = per-position coverage, see WeatherMapBaker)
    ve::Ref<ve::Texture2D> m_cloudWeatherMap;

    // Atmospheric LUTs (baked once; editor re-bakes transmittance on param edits).
    // m_scatteringTexture holds the Rayleigh single-scattering LUT; the Mie
    // component lives in its own texture so the dusk sun keeps its reddening.
    ve::Ref<ve::Texture2D> m_transmittanceTexture;
    ve::Ref<ve::Texture3D> m_scatteringTexture;
    ve::Ref<ve::Texture3D> m_mieScatteringTexture;
    ve::Ref<ve::Texture3D> m_multipleScatteringTexture;

    // Light icon billboard
    ve::AssetHandle m_pointLightIcon;
    float m_lightIconSize = 0.5f;
};
