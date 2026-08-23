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

uniform sampler2D u_CloudColor;    // current raw cloud frame (quarter-res)
uniform sampler2D u_History;       // previous accumulated cloud (quarter-res)
uniform mat4 u_InvViewProj;        // inverse(current jittered VP)
uniform mat4 u_PrevViewProj;       // previous jittered VP
uniform vec3 u_CameraPos;
uniform vec3 u_PlanetCenter;
uniform float u_BlendAlpha;        // current-frame weight 0.07 (1.0 on first frame)
uniform vec2  u_TexelSize;         // 1/quarter-res, 3x3 neighborhood offset

// u_PlanetRadius / u_CloudInnerRadius / u_CloudOuterRadius are NOT redeclared
// here: they come from the includes below, and CloudTAAPass sets them.

// Includes are inlined by the engine BEFORE the stage split, so they must sit
// after #shader fragment to land only here. atmosphere.glsl first — the cloud
// library reuses its raySphere + u_PlanetRadius.
#include "atmosphere.glsl"
#include "volume_cloud.glsl"

void main()
{
    vec2 uv = v_TexCoord;
    vec4 cur = texture(u_CloudColor, uv);

    // Full transmittance = no cloud on this pixel. Nothing to accumulate.
    if (cur.a >= 1.0) { gl_FragColor = cur; return; }

    // The cloud's reprojection depth is NOT the GBuffer depth: cloud pixels read
    // 1.0 (sky) there, which is exactly why the screen TAA's far-plane guard
    // drops them. Instead recompute the cloud ENTRY distance from the view ray +
    // shell — the same value the cloud pass raymarched under — so the history
    // reprojects with the correct cloud parallax.
    vec4 ndc      = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    vec4 farPlane = u_InvViewProj * ndc;
    vec3 rayDir   = normalize(farPlane.xyz / farPlane.w - u_CameraPos);
    vec2 t        = rayCloudLayer(u_CameraPos - u_PlanetCenter, rayDir);
    if (t.x < 0.0) { gl_FragColor = cur; return; }

    vec3 worldPos = u_CameraPos + rayDir * t.x;
    vec4 prevClip = u_PrevViewProj * vec4(worldPos, 1.0);
    vec2 prevUV   = prevClip.xy / prevClip.w * 0.5 + 0.5;

    vec4 result = cur;
    if (prevUV == clamp(prevUV, 0.0, 1.0)) {             // inside previous frame
        vec4 hist = texture(u_History, prevUV);
        // 3x3 neighborhood AABB clamp: cuts history colors that no longer exist
        // this frame (cloud animates, camera moves) — prevents ghosting.
        vec3 mn = vec3(1e9), mx = vec3(-1e9);
        for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++) {
            vec3 n = texture(u_CloudColor, uv + vec2(i, j) * u_TexelSize).rgb;
            mn = min(mn, n); mx = max(mx, n);
        }
        result.rgb = mix(clamp(hist.rgb, mn, mx), cur.rgb, u_BlendAlpha);
        result.a   = mix(hist.a, cur.a, u_BlendAlpha);   // transmittance accumulates too
    }
    gl_FragColor = result;
}
