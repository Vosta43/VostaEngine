#include "vepch.h"
#include "SceneRenderer.h"
#include "Scene/Components.h"
#include "Scene/Terrain/QuadTreeTerrain.h"

namespace ve {

	SceneRenderer::SceneRenderer(const Ref<Scene>& scene)
	{
		m_scene = scene;
	}
	void SceneRenderer::collectAllMesh(RenderContext& ctx){

        auto meshGroup = m_scene->getRegistry().group<TransformComponent, StaticMeshComponent>();///////////////////
        for (auto& et : meshGroup) {
            auto& transform = m_scene->getComponent<TransformComponent>(et);
            auto& meshComp = m_scene->getComponent<StaticMeshComponent>(et);

            auto mesh = ResourceManager::get<StaticMesh>(meshComp.staticMeshHandle);
            if (!mesh) continue;

            auto& submeshes = mesh->getSubmeshes();

            if (submeshes.empty()) {
                DrawMeshCommand cmd;
                cmd.transform = transform.transform;
                cmd.meshHandle = meshComp.staticMeshHandle;
                cmd.materialHandle = meshComp.materialHandle;

                ctx.drawMeshCommands.push_back(cmd);
            }
            else {
                // One submesh one command
                for (size_t i = 0; i < submeshes.size(); i++) {
                    DrawMeshCommand cmd;
                    cmd.transform = transform.transform;
                    cmd.meshHandle = meshComp.staticMeshHandle;
                    cmd.startIndex = submeshes[i].firstIndex;
                    cmd.indexCount = submeshes[i].indexCount;

                    //VE_CORE_ERROR_PRINT("Single mesh: startIndex=%u, indexCount=%u ",
                    //    cmd.startIndex, cmd.indexCount);
                    if (i < meshComp.submeshEntries.size()
                        && meshComp.submeshEntries[i].materialHandle.isValid()) {
                        cmd.materialHandle = meshComp.submeshEntries[i].materialHandle;
                    }
                    else {
                        cmd.materialHandle = submeshes[i].materialHandle;
                    }

                    ctx.drawMeshCommands.push_back(cmd);
                }
            }
        }//////////////////////////////////////////////////////////////////////////

        // Terrain meshes. The shared source/material/LOD parameters live on the
        // scene's TerrainSystem, so resolve it once here rather than per tile.
        TerrainSystemComponent* terrainSystem = Terrain::findSystem(*m_scene);
        auto terrainGroup = m_scene->getRegistry().group<TransformComponent, TerrainComponent>();
        for (auto& et : terrainGroup) {
            // Lazy build: the system may have been created after this tile, so bake
            // it here once the scene has settled. Cheap when already clean.
            Terrain::ensureBuilt(*m_scene, et);
            if (!terrainSystem) continue;

            auto& transform = m_scene->getComponent<TransformComponent>(et);
            auto& terrainComp = m_scene->getComponent<TerrainComponent>(et);

            if (!terrainComp.generatedMeshHandle.isValid()) continue;

            auto mesh = ResourceManager::get<StaticMesh>(terrainComp.generatedMeshHandle);
            if (!mesh) continue;

            // The per-instance view wraps the system material and carries this
            // terrain's control map; skip the tile if it could not build.
            const glm::vec2 terrainOrigin(transform.transform[3].x, transform.transform[3].z);
            AssetHandle terrainMaterial = Terrain::ensureMaterialInstance(*m_scene, et, terrainOrigin);
            if (!terrainMaterial.isValid()) continue;

            // Upload any brush edits accumulated since the last frame.
            Terrain::flushWeightMap(*m_scene, et);

            if (terrainComp.quadtree) {
                // Quadtree LOD: draw only the active chunks picked for this camera.
                const auto& actives = terrainComp.quadtree->update(
                    ctx.cameraPosition, terrainSystem->maxDepth,
                    terrainSystem->lodDetail, terrainSystem->renderDistance);
                for (const auto& chunk : actives) {
                    DrawMeshCommand cmd;
                    cmd.transform = transform.transform;
                    cmd.meshHandle = terrainComp.generatedMeshHandle;
                    cmd.materialHandle = terrainMaterial;
                    cmd.startIndex = chunk.firstIndex;
                    cmd.indexCount = chunk.indexCount;
                    ctx.drawMeshCommands.push_back(cmd);
                }
            }
            else {
                // No quadtree built yet: draw the whole mesh (pre-LOD behavior).
                DrawMeshCommand cmd;
                cmd.transform = transform.transform;
                cmd.meshHandle = terrainComp.generatedMeshHandle;
                cmd.materialHandle = terrainMaterial;
                ctx.drawMeshCommands.push_back(cmd);
            }
        }
	}
    void SceneRenderer::collectAllLight(RenderContext& ctx){

        auto lightView = m_scene->getRegistry().group<TransformComponent, LightComponent>();
        for (auto& et : lightView) {
            auto& transform = m_scene->getComponent<TransformComponent>(et);
            auto& lightComp = m_scene->getComponent<LightComponent>(et);
            DrawLightCommand cmd;
            cmd.position = glm::vec3(transform.transform[3]);
            cmd.light = lightComp.light;
            ctx.drawLightCommands.push_back(cmd);
        }
    }
    void SceneRenderer::collectAllSprites(RenderContext& ctx) {
        auto spriteGroup = m_scene->getRegistry().group<TransformComponent, SpriteRendererComponent>();
        for (auto& et : spriteGroup) {
            auto& transform = m_scene->getComponent<TransformComponent>(et);
            auto& sprite = m_scene->getComponent<SpriteRendererComponent>(et);
            DrawSpriteCommand cmd;
            cmd.transform = transform.transform;
            cmd.size = sprite.size;
            cmd.textureHandle = sprite.textureHandle;
            ctx.drawSpriteCommands.push_back(cmd);
        }
    }
}
