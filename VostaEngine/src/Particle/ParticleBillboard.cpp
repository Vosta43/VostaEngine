#include "vepch.h"
#include "ParticleBillboard.h"

#include "Core/Application.h"
#include "ParticleTypes.h"
#include "Renderer/Buffer.h"
#include "Renderer/RenderCommand.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexArray.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ve {

	namespace ParticleBillboard {

		namespace {

			// One instance per particle: 32 bytes against the 144 a CPU-expanded
			// quad costs in the sprite batch.
			struct Instance {
				glm::vec3 center;
				float size;
				glm::vec4 color;
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

				Instance& instance = s.instances[s.count++];
				instance.center = toRenderSpace(local);
				instance.size = size;
				instance.color = color;
				++appended;
			}
			return appended;
		}

		void flush(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
			State& s = state();
			if (!s.initialized || s.count == 0)
				return;

			const uint32_t count = s.count;
			s.count = 0;

			const Ref<Texture2D> texture = sprite();
			if (!texture || !s.shader || !s.shader->isValid())
				return;

			const glm::mat4 inverseView = glm::inverse(viewMatrix);
			const glm::vec3 right(inverseView[0]);
			const glm::vec3 up(inverseView[1]);

			s.instanceBuffer->setData(s.instances.data(), static_cast<uint32_t>(count * sizeof(Instance)));

			s.shader->bind();
			s.shader->setMat4("u_ViewProjection", projectionMatrix * viewMatrix);
			s.shader->setFloat3("u_CameraRight", right);
			s.shader->setFloat3("u_CameraUp", up);
			s.shader->setInt("u_Texture", 0);
			texture->bind(0);

			RenderCommand::drawIndexedInstanced(s.vertexArray, count);
		}

		glm::mat3 spaceToWorld() {
			return glm::mat3(
				glm::vec3(1.0f, 0.0f, 0.0f),
				glm::vec3(0.0f, 0.0f, -1.0f),
				glm::vec3(0.0f, 1.0f, 0.0f));
		}

	}

}
