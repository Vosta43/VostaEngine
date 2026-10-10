#shader vertex
#version 410 core

layout(location = 0) in vec3 a_Position;

out vec3 v_TexCoord;

uniform mat4 u_ViewProjection;

void main()
{
    v_TexCoord = a_Position;
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#shader fragment
#version 410 core

in vec3 v_TexCoord;

out vec4 FragColor;

uniform samplerCube u_InputCubemap;
uniform float u_Roughness;

const float PI = 3.14159265359;
const uint  SAMPLE_COUNT = 1024u;

// Van der Corput radical inverse (base 2), used by Hammersley sequence.
// Generates a low-discrepancy value in [0,1) from an integer.
float radicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

// Hammersley sequence: maps sample index i (of N total) to a uniform 2D point.
vec2 hammersley(uint i, uint N)
{
    return vec2(float(i) / float(N), radicalInverse_VdC(i));
}

// Maps a 2D Hammersley point to a GGX half-vector in tangent space.
// Assumes z = surface normal direction.
vec3 importanceSampleGGX(vec2 Xi, float roughness)
{
    float a = roughness * roughness;

    // Avoid numerical degeneracy when roughness is exactly zero:
    // a = 0 causes the cosTheta formula to collapse to sqrt(0/0) = NaN
    // for Xi.y close to 1.0. Clamping `a` keeps the GGX distribution
    // stable while being visually indistinguishable from a mirror.
    a = max(a, 0.001);

    float phi      = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

void main()
{
    vec3 N = normalize(v_TexCoord);
    vec3 V = N; // For prefiltering we assume V = N = R (split-sum simplification)

    // Build tangent space around N, avoiding the singularity at the poles
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up = cross(N, right);

    vec3 prefilteredColor = vec3(0.0);
    float totalWeight = 0.0;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        vec2 Xi = hammersley(i, SAMPLE_COUNT);
        vec3 H  = importanceSampleGGX(Xi, u_Roughness);

        // Transform H from tangent space to world space
        vec3 Hw = H.x * right + H.y * up + H.z * N;

        // Incoming light direction that reflects V into Hw
        vec3 L = reflect(-V, Hw);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            prefilteredColor += texture(u_InputCubemap, L).rgb * NdotL;
            totalWeight      += NdotL;
        }
    }

    prefilteredColor = prefilteredColor / max(totalWeight, 0.001);
    FragColor = vec4(prefilteredColor, 1.0);
}
