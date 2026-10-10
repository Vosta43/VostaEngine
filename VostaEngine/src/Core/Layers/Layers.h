#ifndef LAYERS_H
#define LAYERS_H

#include "../Core.h"
#include "../Events/Event.h"

namespace ve {
	
	class VE_API Layer {
	public:
		Layer();
		virtual ~Layer();
		virtual void onAttach() {};
		virtual void onDetach() {};
		virtual void onUpdate() {};
		virtual void onEvent(Event& event) {};

		virtual void onUIRender() {};

	private:

		std::string m_debugName;
	};
}

#endif // !LAYERS_H
