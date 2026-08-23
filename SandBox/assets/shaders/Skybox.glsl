#shader vertex
#version 410 core

layout(location = 0) in vec3 a_Position;

out vec3 v_TexCoord;

uniform mat4 u_ViewProjection;

void main()
{
    v_TexCoord = a_Position;
    vec4 clipPos = u_ViewProjection * vec4(a_Position, 1.0);
    // Force w = z so depth is 1.0 after perspective divide
    gl_Position = clipPos.xyww;
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