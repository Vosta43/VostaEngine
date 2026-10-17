#include "vepch.h"
#include "Terrain/RuleGraph.h"
#include "Terrain/RuleNodes.h"

namespace ve {

	const NodeRegistry& getRuleNodeRegistry() {
		static const NodeRegistry registry = [] {
			NodeRegistry r;
			r.registerNode<FieldSourceNode>((uint8_t)RuleNodeTag::FieldSource);
			r.registerNode<LayerBandsNode>((uint8_t)RuleNodeTag::LayerBands);
			return r;
		}();
		return registry;
	}

	void serializeRuleGraph(const RuleGraph& graph, Archive& ar) {
		graph.serialize(getRuleNodeRegistry(), ar);
	}

	void deserializeRuleGraph(RuleGraph& graph, Archive& ar) {
		graph.deserialize(getRuleNodeRegistry(), ar);
	}

}
