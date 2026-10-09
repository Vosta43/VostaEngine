#include "vepch.h"
#include "TerrainMeshBuilder.h"
#include "Core/Log.h"


namespace ve {

	Ref<StaticMeshResource> TerrainMeshBuilder::buildMeshResourceFromHeightMap(const Ref<TextureResource>& heightMap,float tileSize,float heightScale,float uvTileMeters,const glm::vec2& worldOrigin) {

		const int w = (int)heightMap->width;
		const int h = (int)heightMap->height;
		const int channels = getChannelCount(heightMap->format);
		// Supports HDR and LDR
		const bool useFloat = !heightMap->floatPixels.empty();

		std::vector<float> heights(w * h);
		for (int i = 0; i < w; i++) {
			for (int j = 0; j < h; j++) {
				int pixelIdx = i * h + j;
				if (useFloat) {
					heights[i * h + j] = heightMap->floatPixels[pixelIdx * channels] * heightScale;
				} else {
					heights[i * h + j] = (heightMap->pixels[pixelIdx * channels] / 255.0f) * heightScale;
				}
			}
		}

		// Helper: get height with edge clamping
		auto getHeight = [&](int i, int j) -> float {
			i = std::clamp(i, 0, w - 1);
			j = std::clamp(j, 0, h - 1);
			return heights[i * h + j];
		};

		// Second pass: build vertices with normals
		Ref<StaticMeshResource> mesh = CreateRef<StaticMeshResource>();

		for (int i = 0; i < w; i++) {
			float x = i * tileSize;
			for (int j = 0; j < h; j++) {
				float z = j * tileSize;
				float height = heights[i * h + j];

				// Central differences for normal
				float hL = getHeight(i - 1, j);
				float hR = getHeight(i + 1, j);
				float hD = getHeight(i, j - 1);
				float hU = getHeight(i, j + 1);

				float hx = (hR - hL) / (2.0f * tileSize);
				float hz = (hU - hD) / (2.0f * tileSize);

				glm::vec3 normal = glm::normalize(glm::vec3(-hx, 1.0f, -hz));

				Vertex vertex;
				vertex.position = glm::vec3(x, height, z);
				vertex.normal = normal;
				// World-space UV, so a tile set keeps one continuous texture across
				// its seams instead of restarting at every tile's local zero.
				vertex.uv = (glm::vec2(x, z) + worldOrigin) / uvTileMeters;
				mesh->vertexBuffer.push_back(vertex);
			}
		}

		int vertexCount = w * h;
		if (vertexCount != (int)mesh->vertexBuffer.size()) {
			VE_CORE_WARN_PRINT("Terrain mesh build failed: vertex buffer size mismatch");
		}

		// Index buffer
		for (int i = 0; i < w - 1; i++) {
			for (int j = 0; j < h - 1; j++) {
				int leftUp    = i * h + j;
				int rightUp   = i * h + j + 1;
				int leftDown  = (i + 1) * h + j;
				int rightDown = (i + 1) * h + j + 1;

				mesh->indexBuffer.push_back(leftUp);
				mesh->indexBuffer.push_back(rightUp);
				mesh->indexBuffer.push_back(leftDown);

				mesh->indexBuffer.push_back(rightUp);
				mesh->indexBuffer.push_back(rightDown);
				mesh->indexBuffer.push_back(leftDown);
			}
		}

		return mesh;
	}

	Ref<StaticMesh> TerrainMeshBuilder::buildMeshFromHeightMap(const Ref<TextureResource>& heightMap,float tileSize,float heightScale,float uvTileMeters,const glm::vec2& worldOrigin) {
		return StaticMesh::create(buildMeshResourceFromHeightMap(heightMap, tileSize, heightScale, uvTileMeters, worldOrigin));
	}

	Ref<StaticMeshResource> TerrainMeshBuilder::buildFlatMeshResource(float worldSize, int segments, float uvTileMeters, const glm::vec2& worldOrigin) {

		if (segments < 1) segments = 1;

		Ref<StaticMeshResource> mesh = CreateRef<StaticMeshResource>();

		const int n = segments + 1;
		const float step = worldSize / (float)segments;

		// Same vertex/index layout and corner origin as the heightmap path, just
		// with a constant height and straight-up normals, so both meshes face the
		// same way and cover the same footprint.
		for (int i = 0; i < n; i++) {
			for (int j = 0; j < n; j++) {
				Vertex vertex;
				vertex.position = glm::vec3(i * step, 0.0f, j * step);
				vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
				vertex.uv = (glm::vec2(vertex.position.x, vertex.position.z) + worldOrigin) / uvTileMeters;
				mesh->vertexBuffer.push_back(vertex);
			}
		}

		for (int i = 0; i < segments; i++) {
			for (int j = 0; j < segments; j++) {
				int leftUp    = i * n + j;
				int rightUp   = i * n + j + 1;
				int leftDown  = (i + 1) * n + j;
				int rightDown = (i + 1) * n + j + 1;

				mesh->indexBuffer.push_back(leftUp);
				mesh->indexBuffer.push_back(rightUp);
				mesh->indexBuffer.push_back(leftDown);

				mesh->indexBuffer.push_back(rightUp);
				mesh->indexBuffer.push_back(rightDown);
				mesh->indexBuffer.push_back(leftDown);
			}
		}

		return mesh;
	}

	Ref<StaticMesh> TerrainMeshBuilder::buildFlatMesh(float worldSize, int segments, float uvTileMeters, const glm::vec2& worldOrigin) {
		return StaticMesh::create(buildFlatMeshResource(worldSize, segments, uvTileMeters, worldOrigin));
	}

	std::vector<Ref<StaticMesh>> TerrainMeshBuilder::buildMeshesFromHeightMap(const Ref<TextureResource>& heightMap, float tileSize, float heightScale, int blocks){
		
		std::vector<Ref<StaticMesh>> ret;

		const int w = (int)heightMap->width;
		const int h = (int)heightMap->height;
		const int channels = getChannelCount(heightMap->format);
		// Supports HDR and LDR
		const bool useFloat = !heightMap->floatPixels.empty();

		int eachBlockW = w / blocks;
		int eachBlockH = h / blocks;

		for (int i = 0;i < blocks;i ++) {
			for (int j = 0;j < blocks;j++) {
				
				Ref<TextureResource> textureResource = CreateRef<TextureResource>();

				for (int k = eachBlockW * blocks;k < eachBlockW * (blocks + 1);k++) {
					for (int l = eachBlockH * blocks;l < eachBlockH * (blocks + 1);l++) {

						int index = k * (eachBlockH * (blocks + 1)) + l;
						if (useFloat) {
							textureResource->floatPixels.push_back(heightMap->floatPixels[index * channels]);
						}
						else {
							textureResource->pixels.push_back(heightMap->pixels[index * channels]);
						}
						
					}
				}
				//auto texture = Texture2D::create(textureResource);
				auto mesh = buildMeshFromHeightMap(textureResource, tileSize, heightScale);
				ret.push_back(mesh);
			}
		}
		return ret;
	}
}
