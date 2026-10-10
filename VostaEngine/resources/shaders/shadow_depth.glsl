#shader vertex
#version 460 core

layout(location = 0) in vec3 a_Position;

uniform mat4 u_Model;
uniform mat4 u_ShadowVP;

void main()
{
    gl_Position = u_ShadowVP * u_Model * vec4(a_Position, 1.0);
}

#shader fragment
#version 460 core

// Depth-only: the shadow map is the depth attachment, so there is no colour output.
void main()
{
}
