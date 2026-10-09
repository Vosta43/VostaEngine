#include "vepch.h"
#include "LayerStack.h"

namespace ve {
    LayerStack::LayerStack()
    {
    }
    LayerStack::~LayerStack()
    {
        // Layers are owned elsewhere (raw pointers), so the stack never deletes
        // them — but it must still let each one tear down. Without this, onDetach()
        // only ever ran for explicitly popped layers and was effectively dead.
        for (Layer* layer : m_layers)
            layer->onDetach();
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