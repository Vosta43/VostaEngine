#shader vertex
#version 410 core

layout(location = 0) in vec2 a_Position;

out vec2 v_TexCoord;

void main()
{
    gl_Position = vec4(a_Position, 0.0, 1.0);
    v_TexCoord  = a_Position * 0.5 + 0.5;
}

#shader fragment
#version 410 core

in vec2 v_TexCoord;

out vec2 FragColor;

const float PI = 3.14159265359;
const uint  SAMPLE_COUNT = 1024u;

// Van der Corput radical inverse (base 2), used by Hammersley sequence.
float radicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 hammersley(uint i, uint N)
{
    return vec2(float(i) / float(N), radicalInverse_VdC(i));
}

// GGX importance sampling: maps a 2D uniform point to a half-vector in tangent space.
// z = normal direction.
vec3 importanceSampleGGX(vec2 Xi, float roughness)
{
    float a = roughness * roughness;

    // Avoid numerical degeneracy at exactly zero roughness (same fix as prefilter).
    a = max(a, 0.001);

    float phi      = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

// Smith geometry function for GGX (Schlick-Beckmann approximation)
float geometrySchlickGGX(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(float NdotV, float NdotL, float roughness)
{
    return geometrySchlickGGX(NdotV, roughness) *
           geometrySchlickGGX(NdotL, roughness);
}

// Integrates the specular BRDF term over the hemisphere at a given NdotV and
// roughness. The result is stored in the R (scale) and G (bias) channels.
// The Fresnel term is handled analytically (linear in F0), so only two
// coefficients need to be stored.
void main()
{
    float NdotV = v_TexCoord.x;
    float roughness = v_TexCoord.y;

    // Build view vector in tangent space where N = (0, 0, 1)
    vec3 V = vec3(sqrt(1.0 - NdotV * NdotV), 0.0, NdotV);

    float scale = 0.0;
    float bias  = 0.0;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        vec2 Xi = hammersley(i, SAMPLE_COUNT);
        vec3 H  = importanceSampleGGX(Xi, roughness);
        vec3 L  = reflect(-V, H);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0)
        {
            float G = geometrySmith(NdotV, NdotL, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.0 - VdotH, 5.0);

            // scale: Fresnel weight for F0 = 1.0
            scale += (1.0 - Fc) * G_Vis;
            // bias:  Fresnel weight for F0 = 0.0
            bias  += Fc * G_Vis;
        }
    }

    FragColor = vec2(scale, bias) / float(SAMPLE_COUNT);
}
