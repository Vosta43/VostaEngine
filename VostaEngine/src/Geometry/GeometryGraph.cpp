#include "vepch.h"
#include "Geometry/GeometryGraph.h"
#include "Geometry/GeometryNodes.h"
#include "Sdf/SdfNodes.h"

namespace ve {

	const NodeRegistry& getGeometryNodeRegistry() {
		static const NodeRegistry registry = [] {
			NodeRegistry r;
			r.registerNode<IcosphereNode>((uint8_t)GeometryNodeTag::Icosphere);
			r.registerNode<SubdivideNode>((uint8_t)GeometryNodeTag::Subdivide);
			r.registerNode<NoiseDisplaceNode>((uint8_t)GeometryNodeTag::NoiseDisplace);
			r.registerNode<TransformNode>((uint8_t)GeometryNodeTag::Transform);
			r.registerNode<MeshOutputNode>((uint8_t)GeometryNodeTag::MeshOutput);
			r.registerNode<FieldDisplaceNode>((uint8_t)GeometryNodeTag::FieldDisplace);
			r.registerNode<SdfSphereNode>((uint8_t)GeometryNodeTag::SdfSphere);
			r.registerNode<MarchingCubesNode>((uint8_t)GeometryNodeTag::MarchingCubes);
			r.registerNode<SdfNoiseDisplaceNode>((uint8_t)GeometryNodeTag::SdfNoiseDisplace);
			r.registerNode<SdfBooleanNode>((uint8_t)GeometryNodeTag::SdfBoolean);
			return r;
		}();
		return registry;
	}

	void serializeGeometryGraph(const GeometryGraph& graph, Archive& ar) {
		graph.serialize(getGeometryNodeRegistry(), ar);
	}

	void deserializeGeometryGraph(GeometryGraph& graph, Archive& ar) {
		graph.deserialize(getGeometryNodeRegistry(), ar);
	}

	GeometryNode* findGeometryOutput(GeometryGraph& graph) {
		for (auto& node : graph.nodes)
			if (auto* out = dynamic_cast<MeshOutputNode*>(node.get()))
				return out;
		return nullptr;
	}

}
