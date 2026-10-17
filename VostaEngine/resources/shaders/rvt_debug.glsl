#shader vertex
#version 460 core

layout(location = 0) in vec2 a_Position;

out vec2 v_TexCoord;

void main()
{
    gl_Position = vec4(a_Position, 0.0, 1.0);
    v_TexCoord  = a_Position * 0.5 + 0.5;
}

#shader fragment
#version 460 core

// The view covers exactly the grid, so the screen quad's UV is already the virtual
// UV; the world-min/size uniforms go unused.
#include "rvt_lookup.glsl"

in vec2 v_TexCoord;

layout(location = 0) out vec4 o_Color;

uniform int u_DebugView;      // 1 = physical albedo, 2 = occupancy, 3 = resident mip

void main()
{
    // Screen -> mip-0 virtual grid.
    vec2 page0 = v_TexCoord * vec2(float(u_PagesX), float(u_PagesY));

    // The same walk a consumer's resident check does, fallbacks included. Magenta
    // means the page is resident nowhere, which should only happen outside the baked
    // bounds.
    uint slot;
    vec2 inPage;
    int  level = veRvtResolve(page0, 0, slot, inPage);

    vec3 col;
    if (level < 0) {
        col = vec3(1.0, 0.0, 1.0);
    }
    else if (u_DebugView == 3) {
        // Resident mip level as grey: black is mip 0, white the coarsest.
        col = vec3(float(level) / max(1.0, float(u_MipCount - 1)));
    }
    else if (u_DebugView == 2) {
        // A stable colour per slot, so two pages aliasing onto one slot is visible.
        col = fract(vec3(float(slot) * 0.6180340, float(slot) * 0.3819660, float(slot) * 0.1545085));
    }
    else {
        // The bake writes linear albedo; encode for display.
        col = pow(texture(u_Physical0, vec3(veRvtPageToPhysicalUV(inPage), float(slot))).rgb, vec3(1.0 / 2.2));
    }

    o_Color = vec4(col, 1.0);
}
