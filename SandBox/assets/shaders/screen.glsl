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

in vec2 v_TexCoord;

uniform sampler2D u_ScreenTexture;
uniform sampler2D u_DepthMap;
uniform int       u_DiscardBackground;

// Editor ground grid (y = 0). Composited here because this pass already has the
// final colour and the scene depth, and runs after TAA/post so thin lines stay
// crisp. u_ViewProj is the non-jittered matrix (a jittered one would shimmer).
uniform mat4 u_ViewProj;
uniform mat4 u_InvViewProj;
uniform vec3 u_CameraPos;
uniform int  u_GroundGrid;

layout(location = 0) out vec4 o_FragColor;

// Anti-aliased line coverage in [0,1] for a grid of the given spacing.
float gridLine(vec2 xz, float spacing) {
    vec2 c = xz / spacing;
    vec2 g = abs(fract(c - 0.5) - 0.5) / max(fwidth(c), vec2(1e-6));
    return 1.0 - min(min(g.x, g.y), 1.0);
}

void main()
{
    // Thumbnails render onto a transparent target, so drop the ray-marched sky
    // behind the geometry (depth cleared to 1.0). The scene view keeps it opaque
    // so the atmosphere shows through.
    if (u_DiscardBackground != 0 && texture(u_DepthMap, v_TexCoord).r >= 0.999)
        discard;

    vec3 color = texture(u_ScreenTexture, v_TexCoord).rgb;

    if (u_GroundGrid != 0) {
        vec2 ndc  = v_TexCoord * 2.0 - 1.0;
        vec4 far4 = u_InvViewProj * vec4(ndc, 1.0, 1.0);
        far4 /= far4.w;
        vec3 rd = normalize(far4.xyz - u_CameraPos);
        if (rd.y < -1e-4) {
            float t = -u_CameraPos.y / rd.y;                 // hit the y = 0 plane
            vec3  P = u_CameraPos + t * rd;
            vec4  clip = u_ViewProj * vec4(P, 1.0);
            float dPlane = clip.z / clip.w * 0.5 + 0.5;
            float dScene = texture(u_DepthMap, v_TexCoord).r;
            if (t > 0.0 && dPlane < dScene - 1e-5) {          // in front of the geometry
                float minor = gridLine(P.xz, 1.0);
                float major = gridLine(P.xz, 10.0);
                float fade  = 1.0 - smoothstep(120.0, 400.0, length(P.xz - u_CameraPos.xz));
                vec3  gc = mix(vec3(0.30), vec3(0.55), major);
                float ga = max(minor * 0.35, major * 0.60) * fade;
                color = mix(color, gc, ga);
            }
        }
    }

    o_FragColor = vec4(color, 1.0);
    gl_FragDepth = 0.0;
}