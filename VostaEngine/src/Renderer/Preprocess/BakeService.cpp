#include "vepch.h"
#include "BakeService.h"

#include <cstring>

#include "Core/Log.h"
#include "Renderer/Preprocess/IBLBaker.h"
#include "Renderer/Preprocess/AtmosphereBaker.h"
#include "Renderer/Preprocess/AtmosphereSkyBaker.h"
#include "Renderer/Preprocess/WorleyNoiseBaker.h"
#include "Renderer/Preprocess/WeatherMapBaker.h"

#include <glm.hpp>

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

    Ref<TextureCubeMap> BakeService::getAtmosphereIrradianceMap(
        const AtmosphereParams& params,
        const Ref<Texture2D>& transmittance,
        const Ref<Texture3D>& scattering,
        const Ref<Texture3D>& mieScattering,
        const Ref<Texture3D>& multipleScattering,
        const glm::vec3& cameraPlanetRel) {

        // Nothing to bake until the async scattering LUTs have arrived.
        if (!transmittance || !scattering || !mieScattering || !multipleScattering) {
            return m_atmIrradiance;
        }

        const glm::vec3 sunDir = glm::normalize(params.sunDirection);

        AtmosphereParams key = params;
        key.sunDirection = glm::vec3(0.0f);
        key.exposure = 0.0f;

        constexpr float kSunCosEpsilon = 0.9999619f;  // ~0.5 degrees
        bool dirty = !m_hasCachedAtmIrradiance
            || memcmp(&key, &m_cachedAtmIrradianceParams, sizeof(AtmosphereParams)) != 0
            || glm::dot(sunDir, m_cachedAtmIrradianceSunDir) < kSunCosEpsilon;
        if (!dirty) return m_atmIrradiance;

        Ref<TextureCubeMap> sky = AtmosphereSkyBaker::bakeSkyCubemap(
            params, transmittance, scattering, mieScattering, multipleScattering,
            cameraPlanetRel);
        if (!sky) return m_atmIrradiance;

        Ref<TextureCubeMap> irradiance = IBLBaker::bakeIrradianceMap(sky);
        if (!irradiance) return m_atmIrradiance;

        m_atmIrradiance = irradiance;
        m_cachedAtmIrradianceParams = key;
        m_cachedAtmIrradianceSunDir = sunDir;
        m_hasCachedAtmIrradiance = true;

        VE_CORE_SUCCESS_PRINT("%s", "BakeService: atmosphere irradiance re-baked");
        return m_atmIrradiance;
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

    Ref<Texture3D> BakeService::getScatteringLUT(const AtmosphereParams& params) {
        if (m_scatteringTexture) return m_scatteringTexture;
        if (m_pendingScattering.task) return nullptr;
        m_pendingScattering = JobSystem::get().schedule([params] {
            return AtmosphereBaker::computeScatteringData(params);
        }, {});
        return nullptr;
    }

    Ref<Texture3D> BakeService::getMieScatteringLUT(const AtmosphereParams& params) {
        getScatteringLUT(params);
        return m_mieScatteringTexture;
    }

    Ref<Texture3D> BakeService::getMultipleScatteringLUT(const AtmosphereParams& params) {
        if (m_multipleScatteringTexture) return m_multipleScatteringTexture;
        if (m_pendingMultipleScattering.task) return nullptr;
        m_pendingMultipleScattering = JobSystem::get().schedule([params] {
            return AtmosphereBaker::computeMultipleScatteringData(params);
        }, {});
        return nullptr;
    }

    const CloudTextures& BakeService::getCloudTextures() {
        if (!m_clouds.noise && !m_pendingCloudNoise.task)
            m_pendingCloudNoise = JobSystem::get().schedule([] {
                return WorleyNoiseBaker::computeMultiOctave(8, 128, 0, 4);
            }, {});
        if (!m_clouds.detail && !m_pendingCloudDetail.task)
            m_pendingCloudDetail = JobSystem::get().schedule([] {
                return WorleyNoiseBaker::computeDetailWorley(4, 64, 1, 3);
            }, {});
        if (!m_clouds.warp && !m_pendingCloudWarp.task)
            m_pendingCloudWarp = JobSystem::get().schedule([] {
                return WorleyNoiseBaker::computeWarp(4, 64, 2);
            }, {});
        if (!m_clouds.weatherMap && !m_pendingCloudWeather.task)
            m_pendingCloudWeather = JobSystem::get().schedule([] {
                return WeatherMapBaker::compute(512, 4, 3);
            }, {});
        return m_clouds;
    }

    void BakeService::update() {
        if (m_pendingScattering.task && m_pendingScattering.task->done.load()) {
            const ScatteringLUTData& data = m_pendingScattering.future.get();
            AtmosphereBaker::ScatteringLUTPair pair = AtmosphereBaker::buildScatteringTextures(data);
            m_scatteringTexture = pair.rayleighTexture;
            m_mieScatteringTexture = pair.mieTexture;
            m_pendingScattering = {};
        }
        if (m_pendingMultipleScattering.task && m_pendingMultipleScattering.task->done.load()) {
            m_multipleScatteringTexture = AtmosphereBaker::buildMultipleScatteringTexture(m_pendingMultipleScattering.future.get());
            m_pendingMultipleScattering = {};
        }
        if (m_pendingCloudNoise.task && m_pendingCloudNoise.task->done.load()) {
            m_clouds.noise = WorleyNoiseBaker::buildMultiOctave(128, m_pendingCloudNoise.future.get());
            m_clouds.worleyCells = 8.0f;
            m_pendingCloudNoise = {};
        }
        if (m_pendingCloudDetail.task && m_pendingCloudDetail.task->done.load()) {
            m_clouds.detail = WorleyNoiseBaker::buildDetailWorley(64, m_pendingCloudDetail.future.get());
            m_clouds.detailCells = 4.0f;
            m_pendingCloudDetail = {};
        }
        if (m_pendingCloudWarp.task && m_pendingCloudWarp.task->done.load()) {
            m_clouds.warp = WorleyNoiseBaker::buildWarp(64, m_pendingCloudWarp.future.get());
            m_clouds.warpCells = 4.0f;
            m_pendingCloudWarp = {};
        }
        if (m_pendingCloudWeather.task && m_pendingCloudWeather.task->done.load()) {
            m_clouds.weatherMap = WeatherMapBaker::build(512, m_pendingCloudWeather.future.get());
            m_pendingCloudWeather = {};
        }
    }

}
