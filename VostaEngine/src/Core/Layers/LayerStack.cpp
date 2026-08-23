#include "vepch.h"
#include "LayerStack.h"

namespace ve {
    LayerStack::LayerStack()
    {
    }
    LayerStack::~LayerStack()
    {
    }
    void LayerStack::pushLayer(Layer* layer) {
        m_layers.emplace(m_layers.begin() + m_layerInsertIndex, layer);
        ++m_layerInsertIndex;
        layer->onAttach();
    }

    void LayerStack::popLayer(Layer* layer) {
        auto it = std::find(m_layers.begin(), m_layers.begin() + m_layerInsertIndex, layer);
        if (it != m_layers.begin() + m_layerInsertIndex) {
            layer->onDetach();
            m_layers.erase(it);
            --m_layerInsertIndex;
        }
    }

    void LayerStack::pushOverlay(Layer* overlay) {
        m_layers.emplace_back(overlay);
        overlay->onAttach();
    }

    void LayerStack::popOverlay(Layer* overlay) {
        auto it = std::find(m_layers.begin() + m_layerInsertIndex, m_layers.end(), overlay);
        if (it != m_layers.end()) {
            overlay->onDetach();
            m_layers.erase(it);
        }
    }

}