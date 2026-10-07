#include "vepch.h"
#include "TerrainMeshBuilder.h"
#include "Core/Log.h"


namespace ve {

	static int getChannelCount(TextureFormat format) {
		switch (format) {
			case TextureFormat::R8:       return 1;
			case TextureFormat::R16F:     return 1;
			case TextureFormat::RGB:      return 3;
			case TextureFormat::RGBA:     return 4;
			case TextureFormat::RG16F:    return 2;
			case TextureFormat::RGB16F:   return 3;
			case TextureFormat::RGBA16F:  return 4;
			default:                      return 3;
		}
	}

	Ref<StaticMeshResource> TerrainMeshBuilder::buildMeshResourceFromHeightMap(const Ref<TextureResource>& heightMap,float tileSize,float heightScale) {

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
				vertex.uv = glm::vec2((float)i / (w - 1), (float)j / (h - 1));
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

	Ref<StaticMesh> TerrainMeshBuilder::buildMeshFromHeightMap(const Ref<TextureResource>& heightMap,float tileSize,float heightScale) {
		return StaticMesh::create(buildMeshResourceFromHeightMap(heightMap, tileSize, heightScale));
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
