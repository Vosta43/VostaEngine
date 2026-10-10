#shader vertex
#version 450 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec4 a_Color;  

uniform mat4 u_ViewProjection;

flat out int v_EntityID;

void main() {
    v_EntityID = floatBitsToInt(a_Color.r); 
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#shader fragment
#version 450 core
flat in int v_EntityID;

layout(location = 0) out int o_Color;       

void main() {
    o_Color = v_EntityID;
}