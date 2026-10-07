#shader vertex
#version 460 core

layout(location = 0) in vec3 a_Position;

out vec3 v_Direction;

uniform mat4 u_ViewProjection;

void main()
{
    v_Direction = a_Position;
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#shader fragment
#version 460 core

in vec3 v_Direction;

out vec4 FragColor;

// Camera position relative to the planet center.
uniform vec3 u_PlanetRelOrigin;

#include "atmosphere.glsl"

void main()
{
    // Scattered sky only. The sun disk is deliberately excluded (the direct sun
    // is added analytically in the lighting pass) and no exposure is applied, so
    // the convolved result lives in the same pre-exposure radiance space as the
    // HDR lighting pass.
    vec3 sky = computeAtmosphereLUT(u_PlanetRelOrigin, normalize(v_Direction));
    FragColor = vec4(sky, 1.0);
}
