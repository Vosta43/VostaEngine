#shader vertex
#version 410 core

layout(location = 0) in vec3 a_Position;

out vec3 v_TexCoord;

uniform mat4 u_ViewProjection;

void main()
{
    v_TexCoord = a_Position;
    vec4 clipPos = u_ViewProjection * vec4(a_Position, 1.0);
    // Force z = 0 so the depth is the far plane after the perspective divide.
    // Reverse-Z puts the far plane at 0, not 1, and 0 is what a freshly cleared
    // depth buffer holds — the skybox is drawn with GL_GEQUAL against it.
    gl_Position = vec4(clipPos.xy, 0.0, clipPos.w);
}

#shader fragment
#version 410 core

in vec3 v_TexCoord;

out vec4 FragColor;

uniform samplerCube u_Skybox;

void main()
{
    FragColor = texture(u_Skybox, v_TexCoord);
}