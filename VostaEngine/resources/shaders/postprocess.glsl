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

uniform sampler2D u_HDRColor;
uniform float     u_Exposure = 1.0;

layout(location = 0) out vec4 o_FragColor;

// ============================================================================
// ACES Filmic Tone Mapping (Narkowicz 2016 approximation)
//
// Maps an HDR color to [0,1] with a natural "filmic" shoulder and toe.
// Unlike a simple clamp, ACES produces a smooth highlight roll-off and
// preserves color saturation in bright areas.
//
// Reference: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
// ============================================================================
vec3 ACESFilm(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 hdrColor = texture(u_HDRColor, v_TexCoord).rgb;

    // 1. Apply exposure control
    vec3 exposed = hdrColor * u_Exposure;

    // 2. ACES filmic tone mapping (HDR → LDR)
    vec3 mapped = ACESFilm(exposed);

    // 3. Linear → sRGB gamma correction
    mapped = pow(mapped, vec3(1.0 / 2.2));

    o_FragColor = vec4(mapped, 1.0);
}
