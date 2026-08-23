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

layout(location = 0) out vec4 o_FragColor;

void main()
{
    // TODO: discard temporarily disabled. It used to drop background pixels
    // (depth >= 0.999) so the forward skybox pass could fill them. Disabled now
    // so the ray-marched atmosphere from the HDR pass shows through. Re-enable via
    // a `u_HasAtmosphere` switch instead of an unconditional discard.
    // float gDepth = texture(u_DepthMap, v_TexCoord).r;
    // if (gDepth >= 0.999)
    //     discard;

    vec3 color = texture(u_ScreenTexture, v_TexCoord).rgb;
    o_FragColor = vec4(color, 1.0);
    gl_FragDepth = 0.0;
}