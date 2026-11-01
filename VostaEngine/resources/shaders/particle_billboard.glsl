#shader vertex
#version 460 core
// Instanced particle billboards: one unit quad drawn once per particle.
// a_Corner is the only per-vertex attribute; everything else steps per instance.
layout(location = 0) in vec2 a_Corner;
layout(location = 1) in vec3 a_Center;
layout(location = 2) in float a_Size;
layout(location = 3) in vec4 a_Color;
layout(location = 4) in float a_SubUV;

uniform mat4 u_ViewProjection;
uniform vec3 u_CameraRight;
uniform vec3 u_CameraUp;
// Sprite-sheet grid (columns, rows); tiles are numbered left to right, top row first.
uniform vec2 u_SubImageSize;

out vec2 v_TexCoord;
out vec4 v_Color;

void main() {
    vec3 world = a_Center + (u_CameraRight * a_Corner.x + u_CameraUp * a_Corner.y) * a_Size;
    gl_Position = u_ViewProjection * vec4(world, 1.0);

    vec2 grid = max(floor(u_SubImageSize), vec2(1.0));
    float index = clamp(floor(a_SubUV), 0.0, grid.x * grid.y - 1.0);
    vec2 tile = vec2(mod(index, grid.x), floor(index / grid.x));
    // Textures are imported flipped, so v = 0 is the sheet's bottom row while
    // tile 0 is authored at the top.
    tile.y = grid.y - 1.0 - tile.y;
    v_TexCoord = (a_Corner + 0.5 + tile) / grid;
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
