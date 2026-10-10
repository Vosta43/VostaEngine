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

layout(location = 0) out vec4 o_FragColor;

uniform sampler2D u_HDRColor;   // current frame, linear HDR
uniform sampler2D u_DepthMap;   // current frame depth (DEPTH24_STENCIL8, .r = [0,1])
uniform sampler2D u_History;    // previous TAA output (RGBA16F)
uniform mat4 u_InvViewProj;     // inverse(current jittered VP)
uniform mat4 u_PrevViewProj;    // previous jittered VP
uniform float u_BlendAlpha;     // current-frame weight 0.05~0.1 (1.0 on first frame)
uniform vec2  u_TexelSize;      // vec2(1/w, 1/h), 3x3 neighborhood offset
uniform sampler2D u_CloudTex;   // accumulated cloud (quarter-res), .a = transmittance, 1.0 = no cloud

// Reverse reprojection: depth + current jittered VP -> world, then previous VP -> prev UV.
vec3 reconstructWorld(vec2 uv, float depth) {
    float z = depth * 2.0 - 1.0;                                   // [0,1] -> NDC
    vec4 clip = vec4(uv * 2.0 - 1.0, z, 1.0);
    vec4 world = u_InvViewProj * clip;
    return world.xyz / world.w;
}

void main() {
    vec2 uv = v_TexCoord;
    float depth = texture(u_DepthMap, uv).r;
    vec3 cur = texture(u_HDRColor, uv).rgb;

    // Sky (far plane) is not accumulated: reprojecting at the far plane is
    // unstable due to depth precision, which would flicker at the horizon.
    // Cloud pixels are skipped the same way — CloudTAAPass accumulates them at
    // their true entry depth; reprojecting the composite at geometry depth here
    // would apply the wrong parallax and ghost. cloudT >= 0.999 = no cloud.
    float cloudT = texture(u_CloudTex, uv).a;

    vec3 outCol = cur;
    float alpha = u_BlendAlpha;
    if (depth < 0.999999 && cloudT >= 0.999) {
        vec3 worldPos = reconstructWorld(uv, depth);
        vec4 prevClip = u_PrevViewProj * vec4(worldPos, 1.0);
        vec2 prevUV = prevClip.xy / prevClip.w * 0.5 + 0.5;
        if (prevUV == clamp(prevUV, 0.0, 1.0)) {                   // inside previous frame
            vec3 hist = texture(u_History, prevUV).rgb;
            // 3x3 neighborhood AABB clamp: cuts history colors that no longer
            // exist this frame, preventing ghosting.
            vec3 mn = vec3(1e9), mx = vec3(-1e9);
            for (int i = -1; i <= 1; i++)
            for (int j = -1; j <= 1; j++) {
                vec3 n = texture(u_HDRColor, uv + vec2(i, j) * u_TexelSize).rgb;
                mn = min(mn, n); mx = max(mx, n);
            }
            hist = clamp(hist, mn, mx);
            outCol = mix(hist, cur, alpha);                        // history holds 90-95%
        }
    }
    o_FragColor = vec4(outCol, 1.0);
}
