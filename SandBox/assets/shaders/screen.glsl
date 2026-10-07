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

layout(location = 0) out vec4 o_FragColor;

void main()
{
    // Thumbnails render onto a transparent target, so drop the ray-marched sky
    // behind the geometry (depth cleared to 1.0). The scene view keeps it opaque
    // so the atmosphere shows through.
    if (u_DiscardBackground != 0 && texture(u_DepthMap, v_TexCoord).r >= 0.999)
        discard;

    vec3 color = texture(u_ScreenTexture, v_TexCoord).rgb;
    o_FragColor = vec4(color, 1.0);
    gl_FragDepth = 0.0;
}