#include "vepch.h"
#include "Scene/PrefabRegistry.h"

#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "Scene/Terrain/Terrain.h"

#include "Asset/BuiltinReousrces.h"
#include "Core/ResourceManager.h"
#include "Renderer/StaticMesh.h"

#include <glm.hpp>

namespace ve {

	// The built-in templates. Registration order is the order the Create menu shows
	// them, so keep it grouped: Core, Rendering, Environment.

	PrefabRegistrar reg_prefabEmpty{ "empty", "Empty", "Core",
		[](Scene& scene) {
			Entity entity = scene.createEntity();
			scene.assignComponent<TransformComponent>(entity);
			return entity;
		} };

	PrefabRegistrar reg_prefabLight{ "light", "Light", "Rendering",
		[](Scene& scene) {
			Entity entity = scene.createEntity();
			scene.assignComponent<NameComponent>(entity, "Light");
			scene.assignComponent<TransformComponent>(entity);
			scene.assignComponent<LightComponent>(entity);
			return entity;
		} };

	PrefabRegistrar reg_prefabStaticMesh{ "static_mesh", "Static Mesh", "Rendering",
		[](Scene& scene) {
			Entity entity = scene.createEntity();
			scene.assignComponent<NameComponent>(entity, "Mesh");
			scene.assignComponent<TransformComponent>(entity);
			scene.assignComponent<StaticMeshComponent>(entity);
			return entity;
		} };

	// The mesh is 1x1x1, so halfExtents 0.5 lines the collider up with it; the
	// collider does not follow TransformComponent's scale (see RigidBodyComponent).
	PrefabRegistrar reg_prefabPhysicsCube{ "physics_cube", "Physics Cube", "Physics",
		[](Scene& scene) {
			Entity entity = scene.createEntity();
			scene.assignComponent<NameComponent>(entity, "Cube");
			scene.assignComponent<TransformComponent>(entity);

			StaticMeshComponent mesh;
			mesh.staticMeshHandle = ResourceManager::load<StaticMesh>(
				"VostaEngine/resources/models/cube.obj");
			mesh.materialHandle = BuiltinResources::getDefaultMaterial();
			scene.assignComponent<StaticMeshComponent>(entity, mesh);

			RigidBodyComponent body;
			body.bodyType = 2; // Dynamic
			body.shape = 0;    // Box
			body.halfExtents = glm::vec3(0.5f);
			scene.assignComponent<RigidBodyComponent>(entity, body);
			return entity;
		} };

	PrefabRegistrar reg_prefabSkybox{ "skybox", "Skybox", "Rendering",
		[](Scene& scene) {
			Entity entity = scene.createEntity();
			scene.assignComponent<NameComponent>(entity, "Skybox");
			scene.assignComponent<TransformComponent>(entity);
			scene.assignComponent<SkyBoxComponent>(entity);
			return entity;
		} };

	PrefabRegistrar reg_prefabAtmosphere{ "atmosphere", "Atmosphere", "Environment",
		[](Scene& scene) {
			Entity entity = scene.createEntity();
			scene.assignComponent<NameComponent>(entity, "Atmosphere");

			// Planet centre sits directly below the scene origin, so y = 0 is the surface.
			// Kept in the authoritative precision: at planetary radius a float32 y would
			// quantize the centre by ~0.5 m.
			TransformComponent transform;
			transform.position.y = -(ve::Vec3::value_type)AtmosphereComponent().atmosphere.planetRadius;
			scene.assignComponent<TransformComponent>(entity, transform);

			scene.assignComponent<AtmosphereComponent>(entity);
			return entity;
		}, false };

	PrefabRegistrar reg_prefabSprite{ "sprite", "Sprite", "Rendering",
		[](Scene& scene) {
			Entity entity = scene.createEntity();
			scene.assignComponent<NameComponent>(entity, "Sprite");
			scene.assignComponent<TransformComponent>(entity);
			scene.assignComponent<SpriteRendererComponent>(entity);
			return entity;
		} };

	PrefabRegistrar reg_prefabParticle{ "particle", "Particle", "Effects",
		[](Scene& scene) {
			Entity entity = scene.createEntity();
			scene.assignComponent<NameComponent>(entity, "Particle");
			scene.assignComponent<TransformComponent>(entity);
			scene.assignComponent<ParticleComponent>(entity);
			return entity;
		} };

	PrefabRegistrar reg_prefabSpaceBackground{ "space_background", "Space Background", "Environment",
		[](Scene& scene) {
			Entity entity = scene.createEntity();
			scene.assignComponent<NameComponent>(entity, "Space Background");
			scene.assignComponent<SpaceBackgroundComponent>(entity);
			return entity;
		} };

	PrefabRegistrar reg_prefabTerrain{ "terrain", "Terrain", "Environment",
		[](Scene& scene) {
			// A tile only draws once the scene has a system to bake from, so make
			// sure one exists before adding the first tile.
			TerrainSubsystem(scene).ensureSystem();
			return scene.getEntity(TerrainSubsystem(scene).addTile(glm::ivec2(0)));
		}, false };

	PrefabRegistrar reg_prefabTerrainSystem{ "terrain_system", "Terrain System", "Environment",
		[](Scene& scene) {
			return scene.getEntity(TerrainSubsystem(scene).ensureSystem());
		} };

}
