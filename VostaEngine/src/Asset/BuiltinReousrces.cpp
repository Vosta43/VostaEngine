#include "vepch.h"
#include "BuiltinReousrces.h"
#include "Core/ResourceManager.h"
#include "Renderer/Material.h"
#include "Renderer/SingleMaterial.h"
#include "Renderer/MaterialNodes.h"
#include "Renderer/Texture.h"
#include "Renderer/StaticMesh.h"

#include <cmath>

namespace ve {

	namespace {
		AssetHandle s_defaultMaterial;
		AssetHandle s_defaultWhiteTexture;
		AssetHandle s_builtinSphere;

		const char* kDefaultMaterialKey = "__builtin_default_white";
		const char* kDefaultWhiteTextureKey = "__builtin_white_1x1";
		const char* kBuiltinSphereKey = "__builtin_sphere";

		constexpr float kPi = 3.14159265358979323846f;

		// Indexed UV sphere: every vertex is shared by the quads around it, so
		// normals stay smooth across the whole surface (the flat-shaded OBJ it
		// replaces duplicated 3 vertices per triangle). Columns are duplicated at
		// the phi seam so u runs 0..1 without wrapping, and each pole keeps one
		// vertex per column — the usual UV-sphere trade, which costs a few
		// degenerate triangles at the caps in exchange for a correct seam.
		void buildUvSphere(StaticMeshResource& out, int segments, int rings, float radius) {
			out.vertexBuffer.reserve((size_t)(segments + 1) * (rings + 1));
			out.indexBuffer.reserve((size_t)segments * rings * 6);

			for (int r = 0; r <= rings; ++r) {
				const float v     = (float)r / (float)rings;
				const float theta = v * kPi;              // 0 at the +Y pole
				const float sinT  = std::sin(theta);
				const float cosT  = std::cos(theta);

				for (int s = 0; s <= segments; ++s) {
					const float u    = (float)s / (float)segments;
					const float phi  = u * 2.0f * kPi;
					const float sinP = std::sin(phi);
					const float cosP = std::cos(phi);

					Vertex vert{};
					vert.position = glm::vec3(sinT * cosP, cosT, sinT * sinP) * radius;
					// Unit sphere: the outward normal is the normalized position.
					vert.normal = glm::vec3(sinT * cosP, cosT, sinT * sinP);
					// uv.y tracks theta (not 1-theta) so that cross(normal, tangent)
					// from the shader points along +v, matching the UV-gradient
					// convention the OBJ/FBX importers produce.
					vert.uv = glm::vec2(u, v);
					// d(position)/d(phi), normalized. Degenerate only at the poles.
					vert.tangent = glm::vec3(-sinP, 0.0f, cosP);
					out.vertexBuffer.push_back(vert);
				}
			}

			for (int r = 0; r < rings; ++r) {
				for (int s = 0; s < segments; ++s) {
					const uint32_t a = (uint32_t)(r * (segments + 1) + s);
					const uint32_t b = a + 1;
					const uint32_t c = a + (uint32_t)(segments + 1);
					const uint32_t d = c + 1;
					// Wound so the quad's normal points outward.
					out.indexBuffer.insert(out.indexBuffer.end(), { (int)a, (int)b, (int)c });
					out.indexBuffer.insert(out.indexBuffer.end(), { (int)b, (int)d, (int)c });
				}
			}

			SubMeshResource sub;
			sub.name = "sphere";
			sub.firstIndex = 0;
			sub.indexCount = (uint32_t)out.indexBuffer.size();
			out.subMeshes.push_back(std::move(sub));
		}
	}

	void BuiltinResources::init() {
		if (s_defaultMaterial.isValid())
			return;

		// Built directly instead of through Material::create(path): that path
		// writes a .veasset to disk, which a built-in must not do.
		auto material = CreateRef<SingleMaterial>();
		material->name = "default_white";
		material->albedoColor = glm::vec3(1.0f);
		material->metallic = 0.0f;
		material->roughness = 0.8f;
		material->ao = 1.0f;
		// Same shape as an editor-created material: the lone output node keeps
		// compile() in PBR-fallback mode (no custom shader).
		material->graph.addNode(CreateRef<MaterialOutputNode>());

		s_defaultMaterial = ResourceManager::getStorage<Material>().store(kDefaultMaterialKey, material);
	}

	AssetHandle BuiltinResources::getDefaultMaterial() {
		return s_defaultMaterial;
	}

	AssetHandle BuiltinResources::getDefaultWhiteTexture() {
		// Created on first use, not in init(): a Texture2D needs a live GL context,
		// and init() runs before the window (and GLAD) exist. Every caller is on
		// the draw path, so the context is current by the time this is reached.
		if (!s_defaultWhiteTexture.isValid()) {
			auto white = CreateRef<TextureResource>();
			white->width = 1;
			white->height = 1;
			white->format = TextureFormat::RGBA;
			white->pixels = { 255, 255, 255, 255 };

			s_defaultWhiteTexture = ResourceManager::getStorage<Texture2D>().store(
				kDefaultWhiteTextureKey, Texture2D::create(white));
		}
		return s_defaultWhiteTexture;
	}

	AssetHandle BuiltinResources::getBuiltinSphere() {
		// Same lazy-create reason as the white texture: a StaticMesh owns GPU
		// buffers, and init() runs before the window exists.
		if (!s_builtinSphere.isValid()) {
			auto resource = CreateRef<StaticMeshResource>();
			buildUvSphere(*resource, 32, 16, 1.0f);

			auto mesh = StaticMesh::create(resource);
			if (!mesh)
				return INVALID_ASSET_HANDLE;

			s_builtinSphere = ResourceManager::getStorage<StaticMesh>().store(
				kBuiltinSphereKey, mesh);
		}
		return s_builtinSphere;
	}

}
