#ifndef LAYERSTACK_H
#define LAYERSTACK_H

#include <vector>

#include "Layers.h"

namespace ve {
	class VE_API LayerStack {
	public:
		LayerStack();
		~LayerStack();

		void pushLayer(Layer* layer);
		void popLayer(Layer* layer);

		void pushOverlay(Layer* overlay);
		void popOverlay(Layer* overlay);

		std::vector<Layer*>::iterator begin() { return m_layers.begin(); }
		std::vector<Layer*>::iterator end() { return m_layers.end(); }
		std::vector<Layer*>::reverse_iterator rbegin() { return m_layers.rbegin(); }
		std::vector<Layer*>::reverse_iterator rend() { return m_layers.rend(); }

	private:
		std::vector<Layer*> m_layers;
		unsigned int m_layerInsertIndex = 0;
	};



}

#endif // !LAYERSTACK_H

