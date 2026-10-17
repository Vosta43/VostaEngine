#shader vertex
#version 460 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_Model;
uniform mat4 u_ViewProj;

out vec4 v_WorldPosition;   // render space: world minus u_WorldOrigin

void main()
{
    v_WorldPosition = u_Model * vec4(a_Position, 1.0);
    gl_Position = u_ViewProj * v_WorldPosition;
}

#shader fragment
#version 460 core

// Grid + page table + physical array uniforms and the lookup.
#include "rvt_lookup.glsl"

in vec4 v_WorldPosition;

// The virtual grid is anchored in absolute world XZ, and v_WorldPosition is relative
// to the camera, so add the render origin back.
uniform vec3 u_WorldOrigin;

// The baked pages already hold the blended surface, so the material that produced
// them is never re-evaluated here.
layout(location = 0) out vec4 o_Albedo;
layout(location = 1) out vec4 o_Normal;
layout(location = 2) out vec4 o_Material;
layout(location = 3) out vec4 o_Emissive;

void main()
{
    vec2 worldXZ = v_WorldPosition.xz + u_WorldOrigin.xz;
    vec2 page0 = veRvtPageXY(worldXZ);

    int level = veRvtLod(page0);
    uint slot;
    vec2 inPage;
    if (veRvtResolve(page0, level, slot, inPage) < 0) {
        o_Albedo   = vec4(0.5, 0.5, 0.5, 1.0);
        o_Normal   = vec4(0.5, 0.5, 1.0, 1.0);
        o_Material = vec4(0.0, 0.5, 1.0, 1.0);
        o_Emissive = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Explicit LOD: the physical arrays have no mip chain.
    vec3 c = vec3(veRvtPageToPhysicalUV(inPage), float(slot));
    o_Albedo   = textureLod(u_Physical0, c, 0.0);
    o_Normal   = textureLod(u_Physical1, c, 0.0);
    o_Material = textureLod(u_Physical2, c, 0.0);
    o_Emissive = textureLod(u_Physical3, c, 0.0);
}
