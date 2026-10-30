#include "vepch.h"
#include "PipelineState.h"
#include "RenderCommand.h"
#include "Core/Log.h"

namespace ve {

    namespace {
        const RenderStateDesc s_defaultState{};
    }

    PipelineStateCache& PipelineStateCache::get() {
        static PipelineStateCache cache;
        return cache;
    }

    PipelineHandle PipelineStateCache::getOrCreate(const RenderStateDesc& desc) {
        for (size_t i = 0; i < m_states.size(); ++i) {
            if (m_states[i] == desc)
                return PipelineHandle{ static_cast<uint32_t>(i + 1) };
        }
        m_states.push_back(desc);
        return PipelineHandle{ static_cast<uint32_t>(m_states.size()) };
    }

    void PipelineStateCache::bind(PipelineHandle handle) {
        if (!handle.valid() || handle.id > m_states.size()) {
            VE_CORE_WARN_PRINT("PipelineStateCache::bind: invalid handle %u", handle.id);
            return;
        }
        RenderCommand::applyPipelineState(m_states[handle.id - 1]);
    }

    const RenderStateDesc& PipelineStateCache::desc(PipelineHandle handle) const {
        if (!handle.valid() || handle.id > m_states.size())
            return s_defaultState;
        return m_states[handle.id - 1];
    }

}
