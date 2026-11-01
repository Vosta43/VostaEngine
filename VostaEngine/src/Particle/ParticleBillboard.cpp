#include "vepch.h"
#include "ParticleBillboard.h"

#include "Core/Application.h"
#include "Core/ResourceManager.h"
#include "ParticleTypes.h"
#include "Renderer/Buffer.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexArray.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_set>
#include <vector>

namespace ve {

	namespace ParticleBillboard {

		namespace {

			// One instance per particle against the 144 bytes a CPU-expanded quad
			// costs in the sprite batch.
			struct Instance {
				glm::vec3 center;
				float size;
				glm::vec4 color;
				// Sprite-sheet tile index; 0 when the particle has no SubUVIndex.
				float subUV;
			};

			// One instanced draw's worth of instances. Emitters only share a batch
			// when they agree on both the texture and the sheet grid.
			struct Batch {
				Ref<Texture2D> texture;
				glm::vec2 subImageSize{ 1.0f, 1.0f };
				uint32_t begin = 0;
				uint32_t count = 0;
			};

			struct State {
				// Sized to the simulation's per-emitter ceiling, so a full emitter is
				// never truncated: the dataset is age-ordered, and drawing a prefix
				// would show only the oldest (fallen, fading) particles.
				static constexpr uint32_t kMaxInstances = kMaxParticles;

				bool initialized = false;
				Ref<Shader> shader;
				Ref<VertexArray> vertexArray;
				Ref<VertexBuffer> instanceBuffer;
				std::vector<Instance> instances;
				std::vector<Batch> batches;
				// A texture path that does not load stays failed for the session, so
				// a missing asset does not retry disk access (and log an error) at
				// frame rate.
				std::unordered_set<std::string> failedTextures;
				uint32_t count = 0;
			};

			State& state() {
				static State s;
				return s;
			}

			void ensureInitialized() {
				State& s = state();
				if (s.initialized)
					return;
				s.initialized = true;

				auto& shaderLib = Application::get().getShaderLibrary();
				shaderLib.load("VostaEngine/resources/shaders/particle_billboard.glsl");
				s.shader = shaderLib.get("particle_billboard");

				// Unit quad corners in [-0.5, 0.5]; the vertex shader orients them
				// towards the camera and scales them by the instance size.
				float corners[4][2] = {
					{ -0.5f, -0.5f }, { 0.5f, -0.5f },
					{  0.5f,  0.5f }, { -0.5f,  0.5f },
				};
				uint32_t indices[6] = { 0, 1, 2, 2, 3, 0 };

				auto quadBuffer = VertexBuffer::create(&corners[0][0], static_cast<uint32_t>(sizeof(corners)));
				quadBuffer->setLayout({ { ShaderDataType::Float2, "a_Corner" } });

				s.instanceBuffer = VertexBuffer::create(nullptr,
					static_cast<uint32_t>(State::kMaxInstances * sizeof(Instance)));
				s.instanceBuffer->setLayout({
					{ ShaderDataType::Float3, "a_Center", false, true },
					{ ShaderDataType::Float,  "a_Size",   false, true },
					{ ShaderDataType::Float4, "a_Color",  false, true },
					{ ShaderDataType::Float,  "a_SubUV",  false, true },
					});

				s.vertexArray = VertexArray::create();
				s.vertexArray->addVertexBuffer(quadBuffer);
				s.vertexArray->addVertexBuffer(s.instanceBuffer);
				s.vertexArray->setIndexBuffer(IndexBuffer::create(indices, 6));

				s.instances.resize(State::kMaxInstances);
			}

		}

		Ref<Texture2D> sprite() {
			static const Ref<Texture2D> texture = [] {
				constexpr uint32_t size = 64;
				std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4, 0);
				for (uint32_t y = 0; y < size; ++y) {
					for (uint32_t x = 0; x < size; ++x) {
						const float u = ((float)x + 0.5f) / (float)size * 2.0f - 1.0f;
						const float v = ((float)y + 0.5f) / (float)size * 2.0f - 1.0f;
						const float falloff = std::clamp(1.0f - std::sqrt(u * u + v * v), 0.0f, 1.0f);
						const size_t i = (static_cast<size_t>(y) * size + x) * 4;
						pixels[i + 0] = 255;
						pixels[i + 1] = 255;
						pixels[i + 2] = 255;
						pixels[i + 3] = static_cast<uint8_t>(falloff * falloff * 255.0f + 0.5f);
					}
				}

				Ref<Texture2D> tex = Texture2D::create(size, size);
				if (tex)
					tex->setData(pixels.data(), (uint32_t)pixels.size());
				return tex;
			}();
			return texture;
		}

		namespace {

			Ref<Texture2D> resolveSpriteTexture(const std::string& path) {
				if (path.empty())
					return sprite();

				State& s = state();
				if (s.failedTextures.contains(path))
					return sprite();

				AssetHandle handle = ResourceManager::find<Texture2D>(path);
				if (!handle.isValid())
					handle = ResourceManager::load<Texture2D>(path);

				Ref<Texture2D> texture = ResourceManager::get<Texture2D>(handle);
				if (!texture) {
					// A registered slot that is merely unloaded comes back on a later
					// frame; only a path that never registered is permanently failed.
					if (!ResourceManager::contains<Texture2D>(handle))
						s.failedTextures.insert(path);
					return sprite();
				}
				return texture;
			}

		}

		uint32_t draw(const EmitterAsset& emitter, const ParticleDataSet& particles,
			const SpaceMap& toRenderSpace, const glm::vec4& tint, float size) {
			if (particles.num == 0)
				return 0;

			const EmitterCompiled* compiled = emitter.compiled.get();
			if (!compiled || !compiled->particleProperties.valid)
				return 0;

			ensureInitialized();
			State& s = state();
			if (!s.shader || !s.shader->isValid())
				return 0;

			const ParticlePropertiesOffsetsLayout& properties = compiled->particleProperties;
			const ParticleRenderer& renderer = emitter.renderer;

			const Ref<Texture2D> texture = resolveSpriteTexture(renderer.spriteTexturePath);
			const glm::vec2 subImageSize(
				(float)std::max(renderer.subImageSizeX, 1u),
				(float)std::max(renderer.subImageSizeY, 1u));

			uint32_t appended = 0;
			for (uint32_t i = 0; i < particles.num; ++i) {
				if (s.count >= State::kMaxInstances)
					break;

				glm::vec4 color = tint;
				if (properties.hasFade) {
					const float life = std::max(*particles.attr(properties.lifetime, i), 1e-3f);
					color.a *= std::clamp(1.0f - *particles.attr(properties.age, i) / life, 0.05f, 1.0f);
				}
				if (color.a <= 0.0f)
					continue;

				const glm::vec3 local(*particles.attr(properties.position + 0, i),
					*particles.attr(properties.position + 1, i),
					*particles.attr(properties.position + 2, i));

				Batch* batch = s.batches.empty() ? nullptr : &s.batches.back();
				if (!batch || batch->texture != texture ||
					batch->subImageSize.x != subImageSize.x || batch->subImageSize.y != subImageSize.y) {
					s.batches.push_back(Batch{ texture, subImageSize, s.count, 0 });
					batch = &s.batches.back();
				}

				Instance& instance = s.instances[s.count++];
				instance.center = toRenderSpace(local);
				instance.size = size;
				instance.color = color;
				instance.subUV = properties.hasSubUVIndex
					? std::max(*particles.attr(properties.subUVIndex, i), 0.0f)
					: 0.0f;
				++batch->count;
				++appended;
			}
			return appended;
		}

		void flush(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
			State& s = state();
			if (!s.initialized || s.count == 0) {
				s.batches.clear();
				return;
			}

			const std::vector<Batch> batches = std::move(s.batches);
			s.batches.clear();
			s.count = 0;

			if (!s.shader || !s.shader->isValid())
				return;

			const glm::mat4 inverseView = glm::inverse(viewMatrix);
			const glm::vec3 right(inverseView[0]);
			const glm::vec3 up(inverseView[1]);

			s.shader->bind();
			s.shader->setMat4("u_ViewProjection", projectionMatrix * viewMatrix);
			s.shader->setFloat3("u_CameraRight", right);
			s.shader->setFloat3("u_CameraUp", up);
			s.shader->setInt("u_Texture", 0);

			for (const Batch& batch : batches) {
				if (batch.count == 0 || !batch.texture)
					continue;
				s.instanceBuffer->setData(s.instances.data() + batch.begin,
					static_cast<uint32_t>(batch.count * sizeof(Instance)));
				s.shader->setFloat2("u_SubImageSize", batch.subImageSize);
				batch.texture->bind(0);
				RenderCommand::drawIndexedInstanced(s.vertexArray, batch.count);
			}
		}

		glm::mat3 spaceToWorld() {
			return glm::mat3(
				glm::vec3(1.0f, 0.0f, 0.0f),
				glm::vec3(0.0f, 0.0f, -1.0f),
				glm::vec3(0.0f, 1.0f, 0.0f));
		}

	}

}
