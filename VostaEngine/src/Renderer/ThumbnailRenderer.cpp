#include "vepch.h"
#include "ThumbnailRenderer.h"
#include "Core/ResourceManager.h"
#include "Renderer/Preprocess/BakeService.h"

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include "imgui.h"

namespace ve {

    std::unordered_map<uint32_t, ThumbnailRenderer::CachedThumbnail> ThumbnailRenderer::s_cache;
    std::unordered_map<uint32_t, ThumbnailRenderer::CachedThumbnail> ThumbnailRenderer::s_meshCache;

    namespace {

        // Give a thumbnail context the same lighting inputs the scene view uses.
        // Without hasAtmosphere the HDR pass never writes u_Exposure (or the sun
        // direction), so the PBR shader's "finalColor *= u_Exposure" collapses to
        // black and normalize(0) feeds NaN into the sky term. BakeService is a
        // singleton, so previews get real IBL and LUTs with no scene.
        void setupThumbnailLighting(RenderContext& ctx)
        {
            auto& bake = BakeService::get();

            ctx.hasAtmosphere = true;
            ctx.atmosphere = AtmosphereParams{};
            ctx.planetCenter = glm::vec3(0.0f);

            ctx.brdfLUT = bake.getBRDFLUT();
            ctx.irradianceMap = bake.getIrradianceMap();
            ctx.prefilteredEnvMap = bake.getPrefilteredEnvMap();

            ctx.transmittanceTexture = bake.getTransmittanceLUT(ctx.atmosphere);
            ctx.scatteringTexture = bake.getScatteringLUT(ctx.atmosphere);
            ctx.mieScatteringTexture = bake.getMieScatteringLUT(ctx.atmosphere);
            ctx.multipleScatteringTexture = bake.getMultipleScatteringLUT(ctx.atmosphere);
        }

    }

    Ref<Texture2D> ThumbnailRenderer::getMaterialThumbnail(AssetHandle materialHandle)
    {
        // Check cache
        auto it = s_cache.find(materialHandle.index());

        if (it != s_cache.end()) {
            return it->second.texture;
        }

        auto mat = ResourceManager::get<Material>(materialHandle);
        RenderPipeline ppl;
        ppl.init(512, 512);

        auto sphere = ResourceManager::find<StaticMesh>("SandBox/assets/models/sphere.obj");
        if (!sphere.isValid()) {
            sphere = ResourceManager::store<StaticMesh>("SandBox/assets/models/sphere.obj");
        }

        RenderContext ctx;
        ctx.cameraPosition = glm::vec3(-2.0f, 1.2f, 2.0f);
        ctx.viewMatrix = glm::lookAt(ctx.cameraPosition, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        ctx.projMatrix = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 10.0f);

        ctx.viewPortX = 0;
        ctx.viewPortY = 0;
        ctx.viewPortWidth = 512;
        ctx.viewPortHeight = 512;

        DrawMeshCommand dmcmd;
        dmcmd.transform = glm::mat4(1.0f);
        dmcmd.materialHandle = materialHandle;
        dmcmd.startIndex = 0;
        dmcmd.indexCount = 0;
        dmcmd.meshHandle = sphere;
        ctx.drawMeshCommands.push_back(dmcmd);


        DrawLightCommand dlcmd;
        Light defaultlight;
        dlcmd.light = defaultlight;
        dlcmd.light.intensity = 7.0f;
        defaultlight.position = glm::vec3(0.0f, 2.0f, 2.0f);
        dlcmd.position = glm::vec3(0.0f, 1.2f, 5.5f);
        ctx.drawLightCommands.push_back(dlcmd);

        setupThumbnailLighting(ctx);

        // The pipeline's present pass renders into a fresh output FBO, discarding
        // sky pixels so the preview sits on a transparent background.
        auto outputFBO = Framebuffer::create(512, 512);
        ctx.outputFrameBuffer = outputFBO;
        ctx.presentDiscardBackground = true;

        ppl.render(ctx);

        // Store in cache
        Ref<Texture2D> result = outputFBO->getColorTexture(0);
        s_cache[materialHandle.index()] = { result, outputFBO };

        return result;
    }

    Ref<Texture2D> ThumbnailRenderer::getStaticMeshThumbnail(AssetHandle staticMeshHandle)
    {
        // Check cache
        auto it = s_meshCache.find(staticMeshHandle.index());
        if (it != s_meshCache.end()) {
            return it->second.texture;
        }

        auto staticMesh = ResourceManager::get<StaticMesh>(staticMeshHandle);
        if (!staticMesh) return nullptr;

        // Ensure a default material exists in the resource manager for submeshes
        // without a valid material.
        static AssetHandle s_defaultMatHandle;
        if (!s_defaultMatHandle.isValid()) {
            s_defaultMatHandle = ResourceManager::store<Material>("_mesh_thumb_default");
        }

        const auto& submeshes = staticMesh->getSubmeshes();

        RenderPipeline ppl;
        ppl.init(512, 512);

        // Frame camera from AABB
        glm::vec3 aabbMin = staticMesh->getAabbMin();
        glm::vec3 aabbMax = staticMesh->getAabbMax();
        glm::vec3 center = (aabbMin + aabbMax) * 0.5f;
        glm::vec3 size = aabbMax - aabbMin;
        float radius = glm::length(size) * 0.5f;
        float distance = glm::max(radius * 3.0f, 0.5f);

        glm::vec3 camDir = glm::normalize(glm::vec3(-2.0f, 1.2f, 2.0f));
        glm::vec3 camPos = center + camDir * distance;

        RenderContext ctx;
        ctx.cameraPosition = camPos;
        ctx.viewMatrix = glm::lookAt(camPos, center, glm::vec3(0.0f, 1.0f, 0.0f));
        ctx.projMatrix = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, distance * 3.0f);

        ctx.viewPortX = 0;
        ctx.viewPortY = 0;
        ctx.viewPortWidth = 512;
        ctx.viewPortHeight = 512;

        // One draw command per submesh
        if (!submeshes.empty()) {
            for (auto& sub : submeshes) {
                DrawMeshCommand cmd;
                cmd.meshHandle = staticMeshHandle;
                cmd.materialHandle = sub.materialHandle.isValid() ? sub.materialHandle : s_defaultMatHandle;
                cmd.transform = glm::mat4(1.0f);
                cmd.startIndex = sub.firstIndex;
                cmd.indexCount = sub.indexCount;
                ctx.drawMeshCommands.push_back(cmd);
            }
        } else {
            // No submeshes — draw entire mesh as one piece
            DrawMeshCommand cmd;
            cmd.meshHandle = staticMeshHandle;
            cmd.materialHandle = s_defaultMatHandle;
            cmd.transform = glm::mat4(1.0f);
            cmd.startIndex = 0;
            cmd.indexCount = 0;
            ctx.drawMeshCommands.push_back(cmd);
        }

        // Lighting
        DrawLightCommand dlcmd;
        dlcmd.light = Light();
        dlcmd.light.intensity = 7.0f;
        dlcmd.position = glm::vec3(0.0f, 1.2f, 5.5f);
        ctx.drawLightCommands.push_back(dlcmd);

        setupThumbnailLighting(ctx);

        // The pipeline's present pass renders into a fresh output FBO, discarding
        // sky pixels so the preview sits on a transparent background.
        auto outputFBO = Framebuffer::create(512, 512);
        ctx.outputFrameBuffer = outputFBO;
        ctx.presentDiscardBackground = true;

        ppl.render(ctx);

        // Store in cache
        Ref<Texture2D> result = outputFBO->getColorTexture(0);
        s_meshCache[staticMeshHandle.index()] = { result, outputFBO };

        return result;
    }

    void ThumbnailRenderer::bakeAllLoaded()
    {
        // Collect first: baking can store new resources, and the callback runs
        // while the storage's map is being walked.
        std::vector<AssetHandle> materials;
        std::vector<AssetHandle> meshes;
        ResourceManager::forEach<Material>([&](AssetHandle h, const std::string&) { materials.push_back(h); });
        ResourceManager::forEach<StaticMesh>([&](AssetHandle h, const std::string&) { meshes.push_back(h); });

        for (AssetHandle h : materials) getMaterialThumbnail(h);
        for (AssetHandle h : meshes) getStaticMeshThumbnail(h);
    }

    void ThumbnailRenderer::invalidate(AssetHandle materialHandle)
    {
        s_cache.erase(materialHandle.index());
    }

    void ThumbnailRenderer::invalidateMesh(AssetHandle staticMeshHandle)
    {
        s_meshCache.erase(staticMeshHandle.index());
    }

}

//namespace ve {
//	
//	Ref<Texture2D> ThumbnailRenderer::getMaterialThumbnail(AssetHandle materialHandle){
//		
//		auto mat = ResourceManager::get<Material>(materialHandle);
//		
//		RenderPipeline ppl;
//		ppl.init(512,512);
//		
//		//TODO: Cache
//		auto sphere = ResourceManager::store<StaticMesh>("SandBox/assets/models/sphere.obj");
//
//		RenderContext ctx;
//
//		ctx.cameraPosition = glm::vec3(0, 1, 3);
//		ctx.viewMatrix = glm::lookAt(ctx.cameraPosition, glm::vec3(0), glm::vec3(0, 1, 0));
//		ctx.projMatrix = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 10.0f);
//		
//		// Use default material sphere.
//		DrawMeshCommand dmcmd;
//		dmcmd.materialHandle = materialHandle;
//		dmcmd.meshHandle = sphere;
//		ctx.drawMeshCommands.push_back(dmcmd);// Build ctx
//		
//		DrawLightCommand dlcmd;
//		Light defaultlight;
//		dlcmd.light = defaultlight;
//		dlcmd.position = glm::vec3(5,5,5);
//		ctx.drawLightCommands.push_back(dlcmd);// Build ctx
//
//		ppl.render(ctx);
//
//		auto retTexture = ctx.inputTextures["hdrColor"];
//
//		return retTexture;
//	}
//}

