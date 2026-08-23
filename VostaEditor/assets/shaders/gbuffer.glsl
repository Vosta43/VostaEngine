#shader vertex
#version 460 core

// Vertex attributes
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec3 a_Normal;
layout(location = 3) in vec3 a_Tangent;

// Uniforms
uniform mat4 u_Model;
uniform mat4 u_ViewProj;

// Output to fragment shader
out vec2 v_TexCoord;
out vec3 v_Normal;
out vec3 v_Tangent;
out vec3 v_Bitangent;
out vec4 v_WorldPosition;

void main()
{
    // Calculate world position
    v_WorldPosition = u_Model * vec4(a_Position, 1.0);
    
    // Transform to clip space
    gl_Position = u_ViewProj * v_WorldPosition;
    
    // Pass texture coordinates
    v_TexCoord = a_TexCoord;
    
    // Transform normals and tangents to world space (assuming uniform scale)
    v_Normal = normalize(mat3(u_Model) * a_Normal);
    v_Tangent = normalize(mat3(u_Model) * a_Tangent);
    v_Bitangent = cross(v_Normal, v_Tangent);
}

#shader fragment
#version 460 core

// ============================================================================
// Input from vertex shader
// ============================================================================
in vec2 v_TexCoord;
in vec3 v_Normal;
in vec3 v_Tangent;
in vec3 v_Bitangent;
in vec4 v_WorldPosition;

// ============================================================================
// Material uniforms (set by CPU per draw call)
// ============================================================================

// Albedo
uniform sampler2D u_AlbedoMap;
uniform vec3 u_AlbedoColor;
uniform int u_UseAlbedoMap;     // 1 = use texture, 0 = use color

// Normal map
uniform sampler2D u_NormalMap;
uniform int u_UseNormalMap;     // 1 = use texture, 0 = use geometric normal

// Material properties
uniform sampler2D u_MetallicMap;
uniform float u_Metallic;
uniform int u_UseMetallicMap;   // 1 = use texture, 0 = use uniform value

uniform sampler2D u_RoughnessMap;
uniform float u_Roughness;
uniform int u_UseRoughnessMap;  // 1 = use texture, 0 = use uniform value

uniform sampler2D u_AOMap;
uniform float u_AO;
uniform int u_UseAOMap;         // 1 = use texture, 0 = use uniform value

// ============================================================================
// GBuffer outputs (Multiple Render Targets)
// ============================================================================
layout(location = 0) out vec4 o_Albedo;      // RT0: Diffuse color (RGB)
layout(location = 1) out vec4 o_Normal;      // RT1: World space normal (RGB)
layout(location = 2) out vec4 o_Material;    // RT2: Metallic (R), Roughness (G), AO (B)
layout(location = 3) out vec4 o_Emissive;    // RT3: Emissive color (RGB) - reserved for future use

// ============================================================================
// Helper functions
// ============================================================================

// Sample normal map and convert from tangent space to world space
vec3 getNormalFromMap(vec2 texCoord, vec3 normal, vec3 tangent, vec3 bitangent)
{
    // Sample normal map (values in [0,1])
    vec3 tangentNormal = texture(u_NormalMap, texCoord).rgb;

    // Convert from [0,1] to [-1,1]
    tangentNormal = tangentNormal * 2.0 - 1.0;

    // Build orthonormal TBN via Gram-Schmidt.
    // Vertex T, B, N are no longer orthogonal after interpolation, so using
    // them directly skews the world-space normal — visible as "bald top" /
    // missing hemisphere on curved surfaces.
    vec3 T = normalize(tangent);
    vec3 N = normalize(normal);
    // Re-orthogonalize T against N
    T = normalize(T - dot(T, N) * N);
    // B = cross(N, T) is guaranteed orthogonal to both
    vec3 B = cross(N, T);

    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tangentNormal);
}

// ============================================================================
// Main fragment shader
// ============================================================================
void main()
{
    // ========================================================================
    // 1. Albedo (diffuse color)
    // ========================================================================
    vec3 albedo;
    if (u_UseAlbedoMap == 1) {
        albedo = texture(u_AlbedoMap, v_TexCoord).rgb;
    } else {
        albedo = u_AlbedoColor;
    }
    
    // ========================================================================
    // 2. Normal (world space)
    // ========================================================================
    vec3 normal;
    if (u_UseNormalMap == 1) {
        // Sample normal map and convert to world space
        normal = getNormalFromMap(v_TexCoord, v_Normal, v_Tangent, v_Bitangent);
    } else {
        normal = normalize(v_Normal);
    }
    
    // ========================================================================
    // 3. Metallic
    // ========================================================================
    float metallic;
    if (u_UseMetallicMap == 1) {
        metallic = texture(u_MetallicMap, v_TexCoord).r;
    } else {
        metallic = u_Metallic;
    }
    
    // ========================================================================
    // 4. Roughness
    // ========================================================================
    float roughness;
    if (u_UseRoughnessMap == 1) {
        roughness = texture(u_RoughnessMap, v_TexCoord).r;
    } else {
        roughness = u_Roughness;
    }
    
    // ========================================================================
    // 5. Ambient Occlusion
    // ========================================================================
    float ao;
    if (u_UseAOMap == 1) {
        ao = texture(u_AOMap, v_TexCoord).r;
    } else {
        ao = u_AO;
    }
    
    // ========================================================================
    // 6. Output to GBuffer
    // ========================================================================
    
    // RT0: Albedo (RGB), alpha unused
    o_Albedo = vec4(albedo, 1.0);
    
    // RT1: World space normal encoded from [-1,1] to [0,1] for storage
    // This allows us to store negative values in unsigned texture formats
    o_Normal = vec4(normal * 0.5 + 0.5, 1.0);
    
    // RT2: Material properties
    // R channel = Metallic, G channel = Roughness, B channel = AO
    o_Material = vec4(metallic, roughness, ao, 1.0);
    
    // RT3: Emissive (reserved for future use, currently black)
    o_Emissive = vec4(0.0, 0.0, 0.0, 1.0);
    
    // Depth is automatically written by OpenGL to the depth attachment
}