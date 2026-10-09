#include "vepch.h"
#include "RenderPipeline.h"

#include "Core/Application.h"
#include "Core/AssetConfig.h"
#include "Core/Log.h"
#include "Renderer/RenderPass/GBufferPass.h"
#include "Renderer/RenderPass/ShadowPass.h"
#include "Renderer/RenderPass/CloudPass.h"
#include "Renderer/RenderPass/CloudTAAPass.h"
#include "Renderer/RenderPass/HDRBufferPass.h"
#include "Renderer/RenderPass/TAAPass.h"
#include "Renderer/RenderPass/PostBufferPass.h"
#include "Renderer/RenderPass/PresentPass.h"

#include <algorithm>
#include <filesystem>

namespace ve {

    namespace {

        // Maps a pass "type" string from the asset to a concrete pass class.
        Ref<RenderPassBase> createPassByType(const std::string& type) {
            if (type == "gbuffer")  return CreateRef<GBufferPass>();
            if (type == "shadow")   return CreateRef<ShadowPass>();
            if (type == "cloud")    return CreateRef<CloudPass>();
            if (type == "cloudtaa") return CreateRef<CloudTAAPass>();
            if (type == "hdr")      return CreateRef<HDRBufferPass>();
            if (type == "taa")      return CreateRef<TAAPass>();
            if (type == "post")     return CreateRef<PostBufferPass>();
            if (type == "present")  return CreateRef<PresentPass>();
            VE_CORE_WARN_PRINT("RenderPipeline: unknown pass type '%s'", type.c_str());
            return nullptr;
        }

    } // namespace

    void RenderPipeline::init(uint32_t width, uint32_t height)
    {
        init(width, height, PipelineConfig::makeDefault());
    }

    void RenderPipeline::init(uint32_t width, uint32_t height, const PipelineConfig& config)
    {
        m_width = width;
        m_height = height;
        m_config = config;
        m_settings.load(config.settings);
        m_initialized = true;

        buildFramebuffers();
        buildPasses();
    }

    void RenderPipeline::init(uint32_t width, uint32_t height, const std::string& configPath)
    {
        PipelineConfig cfg;
        const std::string abs = toAbsolute(configPath);
        if (!cfg.load(abs)) {
            VE_CORE_WARN_PRINT("RenderPipeline: using built-in default pipeline (asset '%s' unavailable)", configPath.c_str());
            cfg = PipelineConfig::makeDefault();
        }
        init(width, height, cfg);
    }

    uint32_t RenderPipeline::fboDim(uint32_t absolute, float scale, uint32_t base) const {
        if (absolute > 0)
            return absolute;
        const uint32_t d = (uint32_t)((float)base * scale);
        return std::max(1u, d);   // ECSystem inits at 0x0; a 0-size FBO is invalid
    }

    Ref<Framebuffer> RenderPipeline::createFramebuffer(const FboDef& def) const {
        FramebufferSpec spec;
        const float scale = m_settings.resourceScale(def.name);
        spec.width  = fboDim(def.width,  def.widthScale  * scale, m_width);
        spec.height = fboDim(def.height, def.heightScale * scale, m_height);
        spec.hasDepthStencil = def.hasDepthStencil;
        spec.depthCompare = def.depthCompare;

        spec.colorAttachments.resize(def.attachments.size());
        for (size_t i = 0; i < def.attachments.size(); ++i) {
            const FboAttachmentDef& a = def.attachments[i];
            const size_t slot = (a.slot >= 0 && (size_t)a.slot < spec.colorAttachments.size())
                ? (size_t)a.slot : i;
            spec.colorAttachments[slot].internalFormat = a.internalFormat;
            spec.colorAttachments[slot].format = a.format;
            spec.colorAttachments[slot].type = a.type;
            spec.colorAttachments[slot].minFilter = a.minFilter;
            spec.colorAttachments[slot].magFilter = a.magFilter;
        }
        return Framebuffer::create(spec);
    }

    void RenderPipeline::buildFramebuffers()
    {
        m_fboRegistry.clear();
        for (const auto& def : m_config.framebuffers) {
            if (def.name.empty()) continue;
            // A depth-only framebuffer (a shadow map) is valid; only skip when it
            // has neither colour nor depth to attach.
            if (def.attachments.empty() && !def.hasDepthStencil) {
                VE_CORE_WARN_PRINT("RenderPipeline: framebuffer '%s' has no attachments", def.name.c_str());
                continue;
            }
            m_fboRegistry[def.name] = createFramebuffer(def);
        }
    }

    void RenderPipeline::buildPasses()
    {
        m_passes.clear();

        auto& shaderLib = Application::get().getShaderLibrary();
        for (const auto& s : m_config.shaders) {
            if (s.path.empty()) continue;
            const std::string stem = std::filesystem::path(s.path).stem().string();
            if (!s.name.empty() && s.name != stem)
                VE_CORE_WARN_PRINT("RenderPipeline: shader name '%s' != file stem '%s'", s.name.c_str(), stem.c_str());
            shaderLib.load(s.path);
        }

        for (const auto& pd : m_config.passes) {
            auto pass = createPassByType(pd.type);
            if (!pass) continue;

            Ref<Shader> shader;
            if (!pd.shader.empty())
                shader = shaderLib.get(pd.shader);

            pass->configure(pd, m_fboRegistry, shader);
            pass->init();
            m_passes.push_back(pass);
        }
    }

    void RenderPipeline::rebuild()
    {
        buildFramebuffers();
        buildPasses();
    }

    Ref<Framebuffer> RenderPipeline::framebuffer(const std::string& name) const {
        auto it = m_fboRegistry.find(name);
        return (it != m_fboRegistry.end()) ? it->second : nullptr;
    }

    bool RenderPipeline::providerFlag(const std::string& name, const RenderContext& ctx) const {
        if (name == "hasAtmosphere") return ctx.hasAtmosphere;
        if (name == "hasClouds")     return ctx.cloudNoiseTexture != nullptr;
        if (name == "hasSkybox")     return ctx.skyboxTexture != nullptr;
        if (name == "wireframe")     return ctx.wireframe;
        VE_CORE_WARN_PRINT("RenderPipeline: unknown condition provider '%s' (treated as false)", name.c_str());
        return false;
    }

    bool RenderPipeline::conditionMet(const ConditionDef& c, const RenderContext& ctx) const {
        const bool v = providerFlag(c.provider, ctx);
        return c.negate ? !v : v;
    }

    void RenderPipeline::render(RenderContext& ctx) {
        ctx.fboRegistry = &m_fboRegistry;
        ctx.passOutputs = &m_passOutputs;
        m_passOutputs.clear();
        m_finalColor.reset();

        Ref<Framebuffer> lastTarget;

        for (auto& pass : m_passes) {
            if (pass->hasCondition() && !conditionMet(pass->when(), ctx))
                continue;
            if (!m_settings.passEnabled(pass->name()))
                continue;

            ctx.previousTarget = lastTarget;

            pass->execute(ctx);

            // A pass reports what it actually wrote via setWritten(); a pass that
            // early-outed writes nothing, so @previous keeps pointing at the last
            // pass that did. No target() fallback -- a history pass that skips must
            // not advertise a stale ping-pong half.
            Ref<Framebuffer> out = pass->written();
            if (out) {
                m_passOutputs[pass->name()] = out;
                lastTarget = out;
                if (!pass->targetsDefault())
                    m_finalColor = out->getColorTexture(0);
            }
        }
    }

    void RenderPipeline::shutdown() {
        m_passes.clear();
        m_fboRegistry.clear();
        m_passOutputs.clear();
        m_finalColor.reset();
        m_initialized = false;
    }

    void RenderPipeline::resize(uint32_t width, uint32_t height) {
        if (width == m_width && height == m_height) return;
        m_width = width;
        m_height = height;

        for (const auto& def : m_config.framebuffers) {
            auto it = m_fboRegistry.find(def.name);
            if (it == m_fboRegistry.end() || !it->second) continue;
            const float scale = m_settings.resourceScale(def.name);
            it->second->resize(fboDim(def.width,  def.widthScale  * scale, m_width),
                               fboDim(def.height, def.heightScale * scale, m_height));
        }
    }
}
