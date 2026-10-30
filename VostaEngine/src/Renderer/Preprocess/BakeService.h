#pragma once

#include "Core/Core.h"
#include "Renderer/Texture.h"
#include "Renderer/Atmosphere.h"
#include "Scene/SpaceBackground.h"
#include "Core/JobSystem.h"
#include "Renderer/Preprocess/AtmosphereBaker.h"

#include <vector>

namespace ve {

    // Result of the one-time cloud volume bakes. The cell counts are floats that
    // must reach the shader so it can normalize the Worley sample coordinates.
    struct CloudTextures {
        Ref<Texture3D> noise;
        float worleyCells;
        Ref<Texture3D> detail;
        float detailCells;
        Ref<Texture3D> warp;
        float warpCells;
        Ref<Texture2D> weatherMap;
    };

    // Owns every precomputed bake result (IBL maps, atmosphere LUTs, cloud
    // volumes) together with its cache/dirty logic. App layers (editor, sandbox)
    // request results through this facade instead of calling the concrete bakers,
    // so a baker signature change never touches UI code and the cache is kept in
    // one place instead of being reimplemented per consumer.
    class VE_API BakeService {
    public:
        static BakeService& get();

        // Scene-independent PBR LUT. Baked lazily on first request, cached forever.
        Ref<Texture2D> getBRDFLUT();

        // Records the skybox currently in the scene. Re-bakes the IBL maps
        // internally when a different non-null skybox is provided; a null skybox
        // keeps the previous result so a temporarily missing skybox doesn't wipe
        // the cached lighting.
        void setSkybox(const Ref<TextureCubeMap>& skybox);
        Ref<TextureCubeMap> getIrradianceMap() const;
        Ref<TextureCubeMap> getPrefilteredEnvMap() const;

        // Diffuse ambient from the analytic atmosphere: renders the sky into a
        // cubemap and convolves it, re-baking only when the sun direction or the
        // atmosphere parameters change. Returns the previous result while the
        // async scattering LUTs are still in flight, or nullptr if they never
        // arrived.
        //
        // A non-null `space` adds the star field and nebula to that baked sky, so
        // the diffuse ambient matches what the HDR pass draws; changing it
        // re-bakes.
        Ref<TextureCubeMap> getAtmosphereIrradianceMap(
            const AtmosphereParams& params,
            const SpaceBackgroundParams* space,
            const Ref<Texture2D>& transmittance,
            const Ref<Texture3D>& scattering,
            const Ref<Texture3D>& mieScattering,
            const Ref<Texture3D>& multipleScattering,
            const glm::vec3& cameraPlanetRel);

        // Diffuse ambient from a space background with no atmosphere to carry it:
        // the stars and nebula are baked to a cubemap and convolved, re-baking
        // when the component changes. Without this, a scene that has only a Space
        // Background gets no diffuse ambient at all.
        Ref<TextureCubeMap> getSpaceIrradianceMap(const SpaceBackgroundParams& space);

        // Transmittance re-bakes when the params change (a single bake is <10ms),
        // so inspector edits stay live. The single-scattering pair and the
        // multiple-scattering LUT are baked asynchronously and re-baked when the
        // params change (sun direction and exposure excluded: neither reaches
        // them).
        Ref<Texture2D> getTransmittanceLUT(const AtmosphereParams& params);
        Ref<Texture3D> getScatteringLUT(const AtmosphereParams& params);
        Ref<Texture3D> getMieScatteringLUT(const AtmosphereParams& params);
        Ref<Texture3D> getMultipleScatteringLUT(const AtmosphereParams& params);

        // True when every LUT SceneViewRenderer forwards (transmittance, the
        // scattering pair and the multiple-scattering LUT) has landed. Until it
        // is, the atmosphere shaders would sample unbound 3D samplers and read
        // black, so callers keep the atmosphere off instead of letting it appear
        // mid-session the moment a background bake finishes.
        bool isAtmosphereReady();

        // True once the async scattering LUTs have been uploaded by update(). A
        // preview that renders on demand polls this so it keeps re-rendering until
        // its first frame actually has sky, instead of rendering every frame
        // forever or freezing on a black first frame.
        bool hasAtmosphereLUTs() const;

        // Cloud volumes, baked once on first request.
        const CloudTextures& getCloudTextures();

        // Polls in-flight async bakes; uploads the finished ones to the GPU.
        // Call once per frame on the render thread.
        void update();

    private:
        BakeService() = default;
        ~BakeService() = default;
        BakeService(const BakeService&) = delete;
        BakeService& operator=(const BakeService&) = delete;

        TaskHandle<ScatteringLUTData> m_pendingScattering;
        TaskHandle<std::vector<float>> m_pendingMultipleScattering;
        TaskHandle<std::vector<float>> m_pendingCloudNoise;
        TaskHandle<std::vector<float>> m_pendingCloudDetail;
        TaskHandle<std::vector<float>> m_pendingCloudWarp;
        TaskHandle<std::vector<float>> m_pendingCloudWeather;

        Ref<TextureCubeMap> m_skybox;
        Ref<TextureCubeMap> m_irradianceMap;
        Ref<TextureCubeMap> m_prefilteredEnvMap;
        Ref<Texture2D> m_brdfLUT;

        Ref<TextureCubeMap> m_atmIrradiance;
        // sunDirection and exposure are zeroed here so the memcmp dirty test
        // ignores them: the sun is tracked separately (angle threshold below) and
        // exposure is not applied by the bake.
        AtmosphereParams m_cachedAtmIrradianceParams;
        glm::vec3 m_cachedAtmIrradianceSunDir = glm::vec3(0.0f);
        bool m_hasCachedAtmIrradiance = false;

        // The space background that was folded into m_atmIrradiance, so editing
        // it re-bakes the sky.
        SpaceBackgroundParams m_cachedAtmSpaceParams;
        bool m_hasCachedAtmSpace = false;

        Ref<TextureCubeMap> m_spaceIrradiance;
        SpaceBackgroundParams m_cachedSpaceParams;
        bool m_hasCachedSpaceIrradiance = false;

        AtmosphereParams m_cachedAtmosphereParams;
        bool m_hasCachedAtmosphere = false;
        Ref<Texture2D> m_transmittanceTexture;

        // What the scattering pair and the multiple-scattering LUT were baked
        // from. These used to be "first caller wins, forever": whichever
        // AtmosphereParams came first defined the sky for the rest of the
        // session. A thumbnail preview (ThumbnailRenderer) passes default params,
        // so opening a mesh before the scene's own sky was baked pinned the whole
        // session to the preview's atmosphere.
        AtmosphereParams m_cachedScatteringParams;
        AtmosphereParams m_cachedMultipleParams;
        Ref<Texture3D> m_scatteringTexture;
        Ref<Texture3D> m_mieScatteringTexture;
        Ref<Texture3D> m_multipleScatteringTexture;

        CloudTextures m_clouds;
    };

}
