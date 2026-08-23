#pragma once

#include "Core/Core.h"
#include "Renderer/RenderContext.h"

namespace ve {

	class RenderPassBase {
	public:
		virtual ~RenderPassBase() = default;
		virtual void init(); //TODO: = 0
		//TODO: RenderContext should not be & but const&,split RenderContext into changeble part and inchangeble part.
		virtual void execute(RenderContext& ctx) = 0;

	private:
		
	};

}