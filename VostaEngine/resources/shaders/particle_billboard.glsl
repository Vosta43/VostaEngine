#shader vertex
#version 460 core
// Instanced particle billboards: one unit quad drawn once per particle.
// a_Corner is the only per-vertex attribute; everything else steps per instance.
layout(location = 0) in vec2 a_Corner;
layout(location = 1) in vec3 a_Center;
layout(location = 2) in float a_Size;
layout(location = 3) in vec4 a_Color;

uniform mat4 u_ViewProjection;
uniform vec3 u_CameraRight;
uniform vec3 u_CameraUp;

out vec2 v_TexCoord;
out vec4 v_Color;

void main() {
    vec3 world = a_Center + (u_CameraRight * a_Corner.x + u_CameraUp * a_Corner.y) * a_Size;
    gl_Position = u_ViewProjection * vec4(world, 1.0);
    v_TexCoord = a_Corner + 0.5;
    v_Color = a_Color;
}

#shader fragment
#version 460 core
in vec2 v_TexCoord;
in vec4 v_Color;
uniform sampler2D u_Texture;
out vec4 FragColor;

void main() {
    FragColor = texture(u_Texture, v_TexCoord) * v_Color;
}
