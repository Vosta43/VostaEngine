#pragma once

#include "Core/Core.h"
#include "FrameBuffer.h"
#include "Renderer/RenderPass/RenderPassBase.h"
#include "Renderer/Pipeline/PipelineConfig.h"
#include "Renderer/Pipeline/SettingsRegistry.h"
#include "RenderContext.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace ve {

    // Data-driven render pipeline. The pipeline itself is described by a
    // PipelineConfig (loaded from a JSON asset, or makeDefault()); this class
    // only executes it: it owns the named-framebuffer registry, tracks which
    // framebuffer each pass wrote this frame, and walks the passes in order.
    class VE_API RenderPipeline {
    public:
        void init(uint32_t width, uint32_t height);
        void init(uint32_t width, uint32_t height, const PipelineConfig& config);
        // Loads the pipeline description from a JSON asset. Falls back to
        // makeDefault() (with a warning) if the file is missing or malformed.
        void init(uint32_t width, uint32_t height, const std::string& configPath);

        void render(RenderContext& renderContext);
        void shutdown();

        void resize(uint32_t width, uint32_t height);

        // Tone-mapped LDR output of the last pass that wrote a pipeline
        // framebuffer. Null before the first frame.
        Ref<Texture2D> getFinalColorTexture() const { return m_finalColor; }

        SettingsRegistry&       settings() { return m_settings; }
        const SettingsRegistry& settings() const { return m_settings; }

        const PipelineConfig& config() const { return m_config; }

    private:
        void buildFramebuffers();
        void buildPasses();
        void rebuild();

        uint32_t fboDim(uint32_t absolute, float scale, uint32_t base) const;
        Ref<Framebuffer> createFramebuffer(const FboDef& def) const;

        bool providerFlag(const std::string& name, const RenderContext& ctx) const;
        bool conditionMet(const ConditionDef& c, const RenderContext& ctx) const;

        std::unordered_map<std::string, Ref<Framebuffer>> m_fboRegistry;
        std::vector<Ref<RenderPassBase>>                  m_passes;
        PipelineConfig                                    m_config;
        SettingsRegistry                                  m_settings;

        // Per-frame: framebuffer written by each pass name this frame.
        std::unordered_map<std::string, Ref<Framebuffer>> m_passOutputs;
        Ref<Texture2D> m_finalColor;

        uint32_t m_width = 0;
        uint32_t m_height = 0;

        bool m_initialized = false;
    };

}
