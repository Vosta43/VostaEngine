#include "vepch.h"
#include "BakeService.h"

#include <cstring>

#include "Core/Log.h"
#include "Renderer/Preprocess/IBLBaker.h"
#include "Renderer/Preprocess/AtmosphereBaker.h"
#include "Renderer/Preprocess/WorleyNoiseBaker.h"
#include "Renderer/Preprocess/WeatherMapBaker.h"

namespace ve {

    BakeService& BakeService::get() {
        static BakeService instance;
        return instance;
    }

    Ref<Texture2D> BakeService::getBRDFLUT() {
        if (!m_brdfLUT) {
            m_brdfLUT = IBLBaker::bakeBRDFLUT();
            if (!m_brdfLUT) {
                VE_CORE_ERROR_PRINT("%s", "BakeService: BRDF LUT bake failed, specular IBL will be missing");
            }
        }
        return m_brdfLUT;
    }

    void BakeService::setSkybox(const Ref<TextureCubeMap>& skybox) {
        if (!skybox || skybox == m_skybox) return;

        m_skybox = skybox;
        VE_CORE_SUCCESS_PRINT("%s", "BakeService: skybox changed, re-baking IBL...");
        m_irradianceMap = IBLBaker::bakeIrradianceMap(skybox);
        m_prefilteredEnvMap = IBLBaker::bakePrefilteredEnvMap(skybox);
        if (!m_irradianceMap) m_irradianceMap = skybox;
        if (!m_prefilteredEnvMap) m_prefilteredEnvMap = skybox;
    }

    Ref<TextureCubeMap> BakeService::getIrradianceMap() const {
        return m_irradianceMap;
    }

    Ref<TextureCubeMap> BakeService::getPrefilteredEnvMap() const {
        return m_prefilteredEnvMap;
    }

    Ref<Texture2D> BakeService::getTransmittanceLUT(const AtmosphereParams& params) {
        if (!m_hasCachedAtmosphere ||
            memcmp(&m_cachedAtmosphereParams, &params, sizeof(AtmosphereParams)) != 0) {
            m_cachedAtmosphereParams = params;
            m_hasCachedAtmosphere = true;
            m_transmittanceTexture = AtmosphereBaker::bakeTransmittanceLUT(params);
        }
        return m_transmittanceTexture;
    }

    void BakeService::ensureScatteringLUTs(const AtmosphereParams& params) {
        if (m_scatteringTexture && m_mieScatteringTexture) return;

        AtmosphereBaker::ScatteringLUTPair pair = AtmosphereBaker::bakeScatteringLUT(params);
        m_scatteringTexture = pair.rayleighTexture;
        m_mieScatteringTexture = pair.mieTexture;
    }

    Ref<Texture3D> BakeService::getScatteringLUT(const AtmosphereParams& params) {
        ensureScatteringLUTs(params);
        return m_scatteringTexture;
    }

    Ref<Texture3D> BakeService::getMieScatteringLUT(const AtmosphereParams& params) {
        ensureScatteringLUTs(params);
        return m_mieScatteringTexture;
    }

    Ref<Texture3D> BakeService::getMultipleScatteringLUT(const AtmosphereParams& params) {
        if (!m_multipleScatteringTexture) {
            m_multipleScatteringTexture = AtmosphereBaker::bakeMultipleScatteringLUT(params);
        }
        return m_multipleScatteringTexture;
    }

    const CloudTextures& BakeService::getCloudTextures() {
        // The cell-count params are output params (outCells): the baker writes
        // the base cell count actually used, which the shader needs to normalize.
        if (!m_clouds.noise) {
            uint32_t outCells = 1;
            m_clouds.noise = WorleyNoiseBaker::bakeMultiOctave(8, 128, 0, 4, outCells);
            m_clouds.worleyCells = static_cast<float>(outCells);
        }
        if (!m_clouds.detail) {
            uint32_t outCells = 1;
            m_clouds.detail = WorleyNoiseBaker::bakeDetailWorley(4, 64, 1, 3, outCells);
            m_clouds.detailCells = static_cast<float>(outCells);
        }
        if (!m_clouds.warp) {
            uint32_t outCells = 1;
            m_clouds.warp = WorleyNoiseBaker::bakeWarp(4, 64, 2, outCells);
            m_clouds.warpCells = static_cast<float>(outCells);
        }
        if (!m_clouds.weatherMap) {
            m_clouds.weatherMap = WeatherMapBaker::bake(512, 4, 3);
        }
        return m_clouds;
    }

}
