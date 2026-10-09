#include "vepch.h"
#include "Core/Log.h"
#include "StaticMeshImporter.h"
#include "StaticMeshResource.h"
#include "Renderer/Material.h"
#include "Renderer/SingleMaterial.h"
#include "Core/ResourceManager.h"
#include "Scene/Archive.h"
#include "Core/AssetConfig.h"
#include "Utils.h"

#include "ufbx.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <array>
#include <unordered_map>

namespace ve {

    // One face-triangle and the material name it belongs to, before the
    // vertices are grouped into per-material sub-meshes.
    struct TriangleRecord {
        Vertex v0, v1, v2;
        std::string materialName;
    };

    static glm::vec3 toGlm3(const ufbx_vec3& v) {
        return glm::vec3(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
    }
    static glm::vec2 toGlm2(const ufbx_vec2& v) {
        return glm::vec2(static_cast<float>(v.x), static_cast<float>(v.y));
    }
    static std::string toStdString(const ufbx_string& s) {
        return s.data ? std::string(s.data, s.length) : std::string();
    }

    // Group triangles by material, emit vertices/indices/sub-meshes, and derive
    // UV-gradient tangents. Shared by the OBJ and FBX importers.
    static void buildGeometry(StaticMeshResource& resource, const std::vector<TriangleRecord>& triangles) {
        // Collect unique material names, preserving the order of first occurrence.
        std::vector<std::string> materialNames;
        std::unordered_map<std::string, std::vector<size_t>> materialTriangleIndices;
        for (size_t i = 0; i < triangles.size(); i++) {
            const auto& name = triangles[i].materialName;
            if (materialTriangleIndices.find(name) == materialTriangleIndices.end()) {
                materialNames.push_back(name);
            }
            materialTriangleIndices[name].push_back(i);
        }

        std::vector<Vertex> sortedVertices;
        std::vector<int> sortedIndices;
        std::vector<SubMeshResource> sortedSubMeshes;

        for (const auto& matName : materialNames) {
            SubMeshResource sub;
            sub.name = matName;
            sub.firstIndex = static_cast<uint32_t>(sortedIndices.size());
            sub.indexCount = 0;

            const auto& triIndices = materialTriangleIndices[matName];
            for (size_t triIdx : triIndices) {
                const auto& tri = triangles[triIdx];

                uint32_t base = static_cast<uint32_t>(sortedVertices.size());

                sortedVertices.push_back(tri.v0);
                sortedVertices.push_back(tri.v1);
                sortedVertices.push_back(tri.v2);

                sortedIndices.push_back(base);
                sortedIndices.push_back(base + 1);
                sortedIndices.push_back(base + 2);

                sub.indexCount += 3;
            }

            sortedSubMeshes.push_back(sub);
        }

        // Compute tangents from UV gradients (required for normal mapping).
        // Standard algorithm: for each triangle, solve for the tangent direction
        // from the vertex positions and UV deltas, then average per-vertex.
        for (size_t i = 0; i < sortedIndices.size(); i += 3) {
            uint32_t i0 = sortedIndices[i];
            uint32_t i1 = sortedIndices[i + 1];
            uint32_t i2 = sortedIndices[i + 2];

            Vertex& v0 = sortedVertices[i0];
            Vertex& v1 = sortedVertices[i1];
            Vertex& v2 = sortedVertices[i2];

            glm::vec3 e1 = v1.position - v0.position;
            glm::vec3 e2 = v2.position - v0.position;

            float du1 = v1.uv.x - v0.uv.x;
            float du2 = v2.uv.x - v0.uv.x;
            float dv1 = v1.uv.y - v0.uv.y;
            float dv2 = v2.uv.y - v0.uv.y;

            float r = du1 * dv2 - du2 * dv1;
            if (std::abs(r) < 1e-8f) continue;

            glm::vec3 tangent = (e1 * dv2 - e2 * dv1) / r;

            v0.tangent += tangent;
            v1.tangent += tangent;
            v2.tangent += tangent;
        }

        // Normalize accumulated tangents; assign a default for degenerate UVs.
        for (auto& v : sortedVertices) {
            if (glm::length(v.tangent) > 1e-8f) {
                v.tangent = glm::normalize(v.tangent);
            } else {
                // Fallback: local +X direction (perpendicular to normal).
                v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            }
        }

        resource.vertexBuffer = std::move(sortedVertices);
        resource.indexBuffer = std::move(sortedIndices);
        resource.subMeshes = std::move(sortedSubMeshes);
    }

    struct MtlData {
        // Colors
        glm::vec3 Ka = glm::vec3(0.0f);     // ambient
        glm::vec3 Kd = glm::vec3(1.0f);     // diffuse (albedo)
        glm::vec3 Ks = glm::vec3(0.0f);     // specular
        glm::vec3 Ke = glm::vec3(0.0f);     // emissive

        // Scalars
        float Ns   = 32.0f;                 // specular exponent (shininess)
        float Ni   = 1.0f;                  // index of refraction
        float d    = 1.0f;                  // dissolve (opacity)
        int   illum = 2;                    // illumination model

        // PBR extensions (commonly exported by Blender, 3ds Max, etc.)
        float Pr   = 0.5f;                  // roughness
        float Pm   = 0.0f;                  // metallic

        // Texture map paths (resolved to absolute paths during parse)
        std::string map_Kd;     // diffuse / albedo
        std::string map_Ks;     // specular
        std::string map_Ka;     // ambient
        std::string map_Ns;     // glossiness
        std::string map_d;      // opacity
        std::string map_bump;   // normal ("map_Bump" or "bump" token)
        std::string map_Pr;     // roughness (PBR)
        std::string map_Pm;     // metallic (PBR)
        std::string map_Ke;     // emissive
        std::string map_disp;   // displacement
        std::string map_refl;   // reflection
        std::string map_ao;     // ambient occlusion (non-standard but common)
    };
    using MtlDataMap = std::unordered_map<std::string, MtlData>;

    static std::string makeMaterialName(const std::string& objFilePath, const std::string& materialName) {
        std::filesystem::path p(objFilePath);
        std::string stem = p.stem().string();  // "car"
        // Next to the mesh, not the process cwd: materials used to land in the
        // repo root because this was a bare name.
        return (p.parent_path() / (stem + "_" + materialName)).string();  // .../car_body_paint
    }

    // Minimal trim: removes leading and trailing whitespace in-place.
    static void trim(std::string& s) {
        auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
        s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    }

    // Parse a single MTL file into the given map.
    static void parseMtlFile(const std::string& mtlPath, MtlDataMap& materials) {
        std::ifstream file(mtlPath);
        if (!file.is_open()) {
            VE_CORE_WARN_PRINT("Failed to open MTL file: %s", mtlPath.c_str());
            return;
        }

        VE_CORE_INFO_PRINT("Parsing MTL file: %s", mtlPath.c_str());

        std::filesystem::path mtlDir = std::filesystem::path(mtlPath).parent_path();
        std::string currentMtl;
        std::string line;

        while (std::getline(file, line)) {
            // Skip empty and comment lines
            auto firstNonSpace = line.find_first_not_of(" \t\r");
            if (firstNonSpace == std::string::npos) continue;
            if (line[firstNonSpace] == '#') continue;

            std::istringstream iss(line.substr(firstNonSpace));
            std::string token;
            iss >> token;

            if (token == "newmtl") {
                iss >> currentMtl;
                if (!currentMtl.empty()) {
                    materials[currentMtl] = MtlData();
                }
            }
            else if (token == "Ka") { iss >> materials[currentMtl].Ka.x >> materials[currentMtl].Ka.y >> materials[currentMtl].Ka.z; }
            else if (token == "Kd") { iss >> materials[currentMtl].Kd.x >> materials[currentMtl].Kd.y >> materials[currentMtl].Kd.z; }
            else if (token == "Ks") { iss >> materials[currentMtl].Ks.x >> materials[currentMtl].Ks.y >> materials[currentMtl].Ks.z; }
            else if (token == "Ke") { iss >> materials[currentMtl].Ke.x >> materials[currentMtl].Ke.y >> materials[currentMtl].Ke.z; }
            else if (token == "Ns") { iss >> materials[currentMtl].Ns; }
            else if (token == "Ni") { iss >> materials[currentMtl].Ni; }
            else if (token == "d")  { iss >> materials[currentMtl].d; }
            else if (token == "Tr") {
                float tr;
                iss >> tr;
                materials[currentMtl].d = 1.0f - tr;
            }
            else if (token == "illum") { iss >> materials[currentMtl].illum; }
            else if (token == "Pr")    { iss >> materials[currentMtl].Pr; }
            else if (token == "Pm")    { iss >> materials[currentMtl].Pm; }

            // --- Texture maps ---
            // Each map line may include -options before the filename.
            // Strategy: collect tokens, skip -option + its arguments, keep last non-option token as path.
            else if (token == "map_Kd" || token == "map_Ks" || token == "map_Ka" ||
                     token == "map_Ns" || token == "map_d"  || token == "map_Ke" ||
                     token == "map_Pr" || token == "map_Pm" ||
                     token == "map_Bump" || token == "bump" ||
                     token == "map_disp" || token == "disp" ||
                     token == "map_refl" ||
                     token == "map_ao" || token == "map_AO")
            {
                bool isBump = (token == "map_Bump" || token == "bump");
                bool isDisp = (token == "map_disp" || token == "disp");
                bool isAO   = (token == "map_ao" || token == "map_AO");
                std::string mapType = token;

                // Collect all remaining tokens on this line
                std::vector<std::string> args;
                std::string arg;
                while (iss >> arg) args.push_back(arg);

                // Walk tokens; skip known -options and their parameters.
                std::string texFile;
                for (size_t i = 0; i < args.size(); ++i) {
                    if (args[i][0] == '-') {
                        const auto& opt = args[i];
                        if (opt == "-s" || opt == "-o")       i += 3;
                        else if (opt == "-mm")                i += 2;
                        else if (opt == "-bm" || opt == "-texres") i += 1;
                        // -blendu, -blendv, -clamp, -cc, -type etc. take no numeric args; just skip them.
                    } else {
                        texFile = args[i];
                    }
                }

                if (texFile.empty()) continue;

                // Resolve: some exporters quote filenames with spaces
                std::filesystem::path texPath(texFile);
                std::string resolvedPath = (mtlDir / texPath.filename()).string();

                if (mapType == "map_Kd")        materials[currentMtl].map_Kd    = resolvedPath;
                else if (mapType == "map_Ks")   materials[currentMtl].map_Ks   = resolvedPath;
                else if (mapType == "map_Ka")   materials[currentMtl].map_Ka   = resolvedPath;
                else if (mapType == "map_Ns")   materials[currentMtl].map_Ns   = resolvedPath;
                else if (mapType == "map_d")    materials[currentMtl].map_d    = resolvedPath;
                else if (mapType == "map_Ke")   materials[currentMtl].map_Ke   = resolvedPath;
                else if (mapType == "map_Pr")   materials[currentMtl].map_Pr   = resolvedPath;
                else if (mapType == "map_Pm")   materials[currentMtl].map_Pm   = resolvedPath;
                else if (isBump)                materials[currentMtl].map_bump = resolvedPath;
                else if (isDisp)                materials[currentMtl].map_disp = resolvedPath;
                else if (mapType == "map_refl") materials[currentMtl].map_refl = resolvedPath;
                else if (isAO)                  materials[currentMtl].map_ao   = resolvedPath;
            }
        }

        VE_CORE_INFO_PRINT("Parsed %d materials from MTL: %s", (int)materials.size(), mtlPath.c_str());
        for (auto& [name, data] : materials) {
            VE_CORE_INFO_PRINT("  Material: '%s', Kd=(%.2f,%.2f,%.2f)",
                name.c_str(), data.Kd.x, data.Kd.y, data.Kd.z);
        }
    }

    // Parse one or more MTL files referenced by an OBJ.
    static MtlDataMap parseMtl(const std::vector<std::string>& mtlPaths) {
        MtlDataMap materials;
        for (const auto& path : mtlPaths) {
            parseMtlFile(path, materials);
        }
        return materials;
    }

    // Scan OBJ lines for all "mtllib" references and resolve them to absolute paths.
    static std::vector<std::string> findMtlFiles(const std::string& objPath, const std::vector<std::string>& objLines) {
        std::vector<std::string> mtlPaths;
        std::filesystem::path objDir = std::filesystem::path(objPath).parent_path();

        for (const auto& line : objLines) {
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;
            if (prefix == "mtllib") {
                std::string mtlFile;
                std::getline(iss, mtlFile);
                trim(mtlFile);
                if (!mtlFile.empty()) {
                    mtlPaths.push_back((objDir / mtlFile).string());
                }
            }
        }
        return mtlPaths;
    }

    static Ref<StaticMeshResource> importObj(const std::string& filePath) {
        std::ifstream file(filePath);
        VE_CORE_INFO_PRINT("Try to open OBJ file: %s", filePath.c_str());
        if (!file.is_open()) {
            VE_CORE_ERROR_PRINT("Failed to open OBJ file: %s", filePath.c_str());
            return nullptr;
        }

        auto resource = CreateRef<StaticMeshResource>();

        std::vector<glm::vec3> positions;
        std::vector<glm::vec2> uvs;
        std::vector<glm::vec3> normals;

        std::vector<std::string> allLines;
        std::string line;
        while (std::getline(file, line)) {
            allLines.push_back(line);
        }
        file.close();
        auto mtlPaths = findMtlFiles(filePath, allLines);
        auto mtlMaterials = parseMtl(mtlPaths);

        // Temporary struct: maps each triangle to its source material
        std::vector<TriangleRecord> triangles;

        std::string currentMaterialName = "default";
        std::string currentObjectName = "";

        for (const auto& line : allLines) {
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;

            if (prefix == "v") {
                glm::vec3 pos;
                iss >> pos.x >> pos.y >> pos.z;
                positions.push_back(pos);
            }
            else if (prefix == "vt") {
                glm::vec2 uv;
                iss >> uv.x >> uv.y;
                uvs.push_back(uv);
            }
            else if (prefix == "vn") {
                glm::vec3 normal;
                iss >> normal.x >> normal.y >> normal.z;
                normals.push_back(normal);
            }
            else if (prefix == "o") {
                std::string objName;
                iss >> objName;
                if (!objName.empty()) {
                    currentObjectName = objName;
                }
            }
            else if (prefix == "usemtl") {
                iss >> currentMaterialName;
                if (currentMaterialName.empty()) {
                    currentMaterialName = "default";
                }
            }
            else if (prefix == "f") {
                std::vector<std::string> faceVertices;
                std::string v;
                while (iss >> v) {
                    faceVertices.push_back(v);
                }

                // Parse obj face into engine format
                // Ex: f 7498/7527/12052 7501/7528/12053 7499/7525/12054
                // (index   position           uv              normal    
                auto parseVertex = [&](const std::string& vtx, Vertex& out) {
                    std::istringstream viss(vtx);
                    std::string idxStr;
                    int idx = 0;

                    // position
                    std::getline(viss, idxStr, '/');
                    if (!idxStr.empty()) {
                        idx = std::stoi(idxStr) - 1;
                        if (idx < (int)positions.size()) out.position = positions[idx];
                    }
                    // uv
                    if (std::getline(viss, idxStr, '/') && !idxStr.empty()) {
                        idx = std::stoi(idxStr) - 1;
                        if (idx < (int)uvs.size()) out.uv = uvs[idx];
                    }
                    // normal
                    if (std::getline(viss, idxStr, '/') && !idxStr.empty()) {
                        idx = std::stoi(idxStr) - 1;
                        if (idx < (int)normals.size()) out.normal = normals[idx];
                    }
                 };

                std::string fullMaterialName;
                if (currentObjectName.empty()) {
                    fullMaterialName = currentMaterialName;
                }
                else {

                    fullMaterialName = currentObjectName + ":" + currentMaterialName;
                }

                // Triangle fan
                for (size_t i = 1; i + 1 < faceVertices.size(); i++) {
                    TriangleRecord tri;
                    tri.materialName = fullMaterialName;
                    parseVertex(faceVertices[0], tri.v0);
                    parseVertex(faceVertices[i], tri.v1);
                    parseVertex(faceVertices[i + 1], tri.v2);
                    triangles.push_back(tri);
                }
            }
        }


        if (triangles.empty()) {
            VE_CORE_ERROR_PRINT("OBJ file has no faces: %s", filePath.c_str());
            return nullptr;
        }

        buildGeometry(*resource, triangles);

        // Create materials for each sub-mesh, mapping MTL properties to engine PBR material.
        for (auto& sub : resource->subMeshes) {
            // sub.name is "ObjectName|MaterialName" (or just "MaterialName" if no 'o' token).
            // Extract the raw material name for MTL lookup.
            std::string rawMtlName = sub.name;
            
            auto pipePos = rawMtlName.find(':');
            if (pipePos != std::string::npos) {
                rawMtlName = rawMtlName.substr(pipePos + 1);
            }

            std::string uniqueMaterialName = makeMaterialName(filePath, rawMtlName);
            AssetHandle matHandle = ResourceManager::store<Material>(uniqueMaterialName);

            if (matHandle != INVALID_ASSET_HANDLE) {
                // Imported materials are always single-surface PBR.
                auto material = std::dynamic_pointer_cast<SingleMaterial>(ResourceManager::get<Material>(matHandle));

                auto it = mtlMaterials.find(rawMtlName);
                if (material && it != mtlMaterials.end()) {
                    const auto& mtl = it->second;

                    // --- Texture maps ---
                    if (!mtl.map_Kd.empty())
                        material->albedoMapHandle   = ResourceManager::store<Texture2D>(mtl.map_Kd);
                    if (!mtl.map_bump.empty())
                        material->normalMapHandle   = ResourceManager::store<Texture2D>(mtl.map_bump);
                    if (!mtl.map_Pr.empty())
                        material->roughnessMapHandle = ResourceManager::store<Texture2D>(mtl.map_Pr);
                    if (!mtl.map_Pm.empty())
                        material->metallicMapHandle  = ResourceManager::store<Texture2D>(mtl.map_Pm);
                    if (!mtl.map_ao.empty())
                        material->aoMapHandle       = ResourceManager::store<Texture2D>(mtl.map_ao);
                    if (!mtl.map_Ke.empty())
                        material->emissiveMapHandle = ResourceManager::store<Texture2D>(mtl.map_Ke);

                    // --- Scalar fallbacks (applied to the material, maps override) ---
                    material->albedoColor   = mtl.Kd;
                    material->roughness     = mtl.Pr;
                    material->metallic      = mtl.Pm;
                    material->emissiveColor = mtl.Ke;
                }

                // Persist populated material to .veasset.
                if (material) {
                    TextArchive ar(toAbsolute(material->name), ArchiveMode::write);
                    if (ar.isGood()) {
                        material->serialize(ar);
                    }
                }

                sub.materialHandle = matHandle;
            }

            VE_CORE_INFO_PRINT("subMesh '%s' -> rawMtlName='%s'", sub.name.c_str(), rawMtlName.c_str());
            auto it = mtlMaterials.find(rawMtlName);
            if (it != mtlMaterials.end()) {
                VE_CORE_INFO_PRINT("  -> FOUND, Kd=(%.2f,%.2f,%.2f)", it->second.Kd.x, it->second.Kd.y, it->second.Kd.z);
            }
            else {
                VE_CORE_WARN_PRINT("  -> NOT FOUND in MTL map!");
            }
        }

        VE_CORE_SUCCESS_PRINT("OBJ loaded: %s (%d vertices, %d indices, %d submeshes)",
            filePath.c_str(),
            static_cast<int>(resource->vertexBuffer.size()),
            static_cast<int>(resource->indexBuffer.size()),
            static_cast<int>(resource->subMeshes.size()));

        return resource;
    }
    // --- FBX (ufbx) --------------------------------------------------------

    // The FBX PBR maps this importer binds, in a fixed order so callers can
    // pair them with matching destination handles.
    static std::array<const ufbx_material_map*, 6> fbxPbrMaps(const ufbx_material& um) {
        return {
            &um.pbr.base_color,
            &um.pbr.normal_map,
            &um.pbr.roughness,
            &um.pbr.metalness,
            &um.pbr.emission_color,
            &um.pbr.ambient_occlusion,
        };
    }

    // Path of a file-backed texture as referenced by the FBX (relative to the
    // mesh file), or "" for none / embedded ones (embedded pixel data is not
    // extracted yet).
    static std::string fbxTextureFile(const ufbx_material_map& map) {
        ufbx_texture* tex = map.texture;
        if (!tex || !map.texture_enabled || tex->type != UFBX_TEXTURE_FILE)
            return {};
        if (tex->content.size > 0)
            return {};

        std::string file = toStdString(tex->filename);
        if (file.empty())
            file = toStdString(tex->relative_filename);
        return file;
    }

    static void applyFbxMaterial(SingleMaterial& material, const ufbx_material& um, const std::filesystem::path& meshDir) {
        // Colors: prefer the PBR maps ufbx derives from the FBX non-physical values.
        auto setColor = [](const ufbx_material_map& map, glm::vec3& out) {
            if (!map.has_value) return;
            out = (map.value_components >= 3)
                ? toGlm3(map.value_vec3)
                : glm::vec3(static_cast<float>(map.value_real));
        };
        setColor(um.pbr.base_color, material.albedoColor);
        setColor(um.pbr.emission_color, material.emissiveColor);

        if (um.pbr.metalness.has_value)
            material.metallic = static_cast<float>(um.pbr.metalness.value_real);
        if (um.pbr.roughness.has_value)
            material.roughness = static_cast<float>(um.pbr.roughness.value_real);

        AssetHandle* targets[] = {
            &material.albedoMapHandle,
            &material.normalMapHandle,
            &material.roughnessMapHandle,
            &material.metallicMapHandle,
            &material.emissiveMapHandle,
            &material.aoMapHandle,
        };
        auto maps = fbxPbrMaps(um);
        for (size_t i = 0; i < maps.size(); i++) {
            std::string file = fbxTextureFile(*maps[i]);
            if (!file.empty()) {
                // Textures are imported flat next to the mesh, so drop any
                // sub-directory the FBX recorded.
                std::string flat = (meshDir / std::filesystem::path(file).filename()).string();
                *targets[i] = ResourceManager::store<Texture2D>(flat);
            }
        }
    }

    static Ref<StaticMeshResource> importFbx(const std::string& filePath) {
        VE_CORE_INFO_PRINT("Try to open FBX file: %s", filePath.c_str());

        ufbx_load_opts opts = {};
        // Engine geometry is right-handed Y-up; convert from FBX's own axes so
        // meshes land in the same space as the OBJ path.
        opts.target_axes = ufbx_axes_right_handed_y_up;
        opts.generate_missing_normals = true;
        opts.ignore_animation = true;

        ufbx_error error;
        ufbx_scene* scene = ufbx_load_file(filePath.c_str(), &opts, &error);
        if (!scene) {
            char desc[256];
            ufbx_format_error(desc, sizeof(desc), &error);
            VE_CORE_ERROR_PRINT("Failed to load FBX: %s (%s)", filePath.c_str(), desc);
            return nullptr;
        }

        auto resource = CreateRef<StaticMeshResource>();
        std::vector<TriangleRecord> triangles;
        std::unordered_map<std::string, ufbx_material*> materialByName;

        // Flatten the node hierarchy: every mesh instance is baked into world
        // space and appended to a single buffer, grouped by material.
        for (size_t ni = 0; ni < scene->nodes.count; ni++) {
            ufbx_node* node = scene->nodes.data[ni];
            ufbx_mesh* mesh = node->mesh;
            if (!mesh || mesh->num_faces == 0) continue;

            const ufbx_matrix& xform = node->geometry_to_world;
            std::vector<uint32_t> faceIndices(mesh->max_face_triangles * 3);

            for (size_t fi = 0; fi < mesh->num_faces; fi++) {
                ufbx_face face = mesh->faces.data[fi];
                if (face.num_indices < 3) continue;   // skip points and lines

                uint32_t numTris = ufbx_triangulate_face(faceIndices.data(), faceIndices.size(), mesh, face);

                std::string matName = "default";
                if (fi < mesh->face_material.count) {
                    uint32_t mi = mesh->face_material.data[fi];
                    if (mi < mesh->materials.count && mesh->materials.data[mi]) {
                        ufbx_material* um = mesh->materials.data[mi];
                        std::string n = toStdString(um->name);
                        if (!n.empty()) {
                            matName = n;
                            materialByName[matName] = um;
                        }
                    }
                }

                for (uint32_t t = 0; t < numTris; t++) {
                    TriangleRecord rec;
                    rec.materialName = matName;
                    Vertex* dst[3] = { &rec.v0, &rec.v1, &rec.v2 };
                    for (int k = 0; k < 3; k++) {
                        uint32_t vi = faceIndices[t * 3 + k];
                        Vertex& v = *dst[k];

                        v.position = toGlm3(ufbx_transform_position(&xform, ufbx_get_vertex_vec3(&mesh->vertex_position, vi)));

                        v.uv = (mesh->vertex_uv.values.count > 0)
                            ? toGlm2(ufbx_get_vertex_vec2(&mesh->vertex_uv, vi))
                            : glm::vec2(0.0f);

                        if (mesh->vertex_normal.values.count > 0) {
                            glm::vec3 n = toGlm3(ufbx_transform_direction(&xform, ufbx_get_vertex_vec3(&mesh->vertex_normal, vi)));
                            float len = glm::length(n);
                            v.normal = (len > 1e-8f) ? n / len : glm::vec3(0.0f, 1.0f, 0.0f);
                        } else {
                            v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                        }

                        v.tangent = glm::vec3(0.0f);
                    }
                    triangles.push_back(rec);
                }
            }
        }

        if (triangles.empty()) {
            VE_CORE_ERROR_PRINT("FBX file has no geometry: %s", filePath.c_str());
            ufbx_free_scene(scene);
            return nullptr;
        }

        buildGeometry(*resource, triangles);

        // One engine material per sub-mesh, mapped from the ufbx material.
        std::filesystem::path meshDir = std::filesystem::path(filePath).parent_path();
        for (auto& sub : resource->subMeshes) {
            std::string uniqueMaterialName = makeMaterialName(filePath, sub.name);
            AssetHandle matHandle = ResourceManager::store<Material>(uniqueMaterialName);
            if (matHandle == INVALID_ASSET_HANDLE) continue;

            auto material = std::dynamic_pointer_cast<SingleMaterial>(ResourceManager::get<Material>(matHandle));
            if (material) {
                auto it = materialByName.find(sub.name);
                if (it != materialByName.end())
                    applyFbxMaterial(*material, *it->second, meshDir);

                TextArchive ar(toAbsolute(material->name), ArchiveMode::write);
                if (ar.isGood())
                    material->serialize(ar);
            }
            sub.materialHandle = matHandle;
        }

        VE_CORE_SUCCESS_PRINT("FBX loaded: %s (%d vertices, %d indices, %d submeshes)",
            filePath.c_str(),
            static_cast<int>(resource->vertexBuffer.size()),
            static_cast<int>(resource->indexBuffer.size()),
            static_cast<int>(resource->subMeshes.size()));

        ufbx_free_scene(scene);
        return resource;
    }

    std::vector<std::string> StaticMeshImporter::referencedTextures(const std::string& filePath) {
        std::vector<std::string> textures;
        if (utils::getExtension(filePath) != ".fbx")
            return textures;

        ufbx_load_opts opts = {};
        opts.ignore_geometry = true;    // only the material graph is needed
        opts.ignore_animation = true;

        ufbx_error error;
        ufbx_scene* scene = ufbx_load_file(filePath.c_str(), &opts, &error);
        if (!scene)
            return textures;

        for (size_t i = 0; i < scene->materials.count; i++) {
            for (const ufbx_material_map* map : fbxPbrMaps(*scene->materials.data[i])) {
                std::string file = fbxTextureFile(*map);
                if (!file.empty())
                    textures.push_back(file);
            }
        }

        ufbx_free_scene(scene);
        return textures;
    }

    Ref<StaticMeshResource> StaticMeshImporter::importFromFile(const std::string& filePath) {

        Ref<StaticMeshResource> resource;

        if (utils::getExtension(filePath) == ".obj") {
            resource = importObj(filePath);
        }
        else if (utils::getExtension(filePath) == ".fbx") {
            resource = importFbx(filePath);
        }

        return resource;
    }

    // --- Baked mesh asset (.veasset, binary) -------------------------------
    //
    // Layout:
    //   [string] "staticmesh"
    //   [int32]  vertexCount, indexCount, submeshCount
    //   [bytes]  vertexCount * sizeof(Vertex)
    //   [bytes]  indexCount  * sizeof(int)
    //   per submesh: [string] name, [int32] firstIndex, [int32] indexCount,
    //                [string] materialPath ("" when unassigned)
    //
    // Material handles travel as asset paths and are re-stored on load.

    void StaticMeshResource::serialize(Archive& ar) const {
        ar << std::string("staticmesh");
        ar << static_cast<int32_t>(vertexBuffer.size());
        ar << static_cast<int32_t>(indexBuffer.size());
        ar << static_cast<int32_t>(subMeshes.size());

        if (!vertexBuffer.empty())
            ar.writeBytes(vertexBuffer.data(), vertexBuffer.size() * sizeof(Vertex));
        if (!indexBuffer.empty())
            ar.writeBytes(indexBuffer.data(), indexBuffer.size() * sizeof(int));

        for (const auto& sub : subMeshes) {
            ar << sub.name;
            ar << static_cast<int32_t>(sub.firstIndex);
            ar << static_cast<int32_t>(sub.indexCount);
            ar << ResourceManager::getPath<Material>(sub.materialHandle);
        }
    }

    bool StaticMeshResource::deserialize(Archive& ar) {
        std::string token;
        ar >> token;
        if (token != "staticmesh") {
            VE_CORE_ERROR_PRINT("Not a static mesh asset (token='%s')", token.c_str());
            return false;
        }

        int32_t vertexCount = 0, indexCount = 0, submeshCount = 0;
        ar >> vertexCount >> indexCount >> submeshCount;
        if (vertexCount < 0 || indexCount < 0 || submeshCount < 0)
            return false;

        vertexBuffer.resize(static_cast<size_t>(vertexCount));
        if (vertexCount > 0)
            ar.readBytes(vertexBuffer.data(), static_cast<size_t>(vertexCount) * sizeof(Vertex));

        indexBuffer.resize(static_cast<size_t>(indexCount));
        if (indexCount > 0)
            ar.readBytes(indexBuffer.data(), static_cast<size_t>(indexCount) * sizeof(int));

        subMeshes.resize(static_cast<size_t>(submeshCount));
        for (auto& sub : subMeshes) {
            std::string materialPath;
            int32_t firstIndex = 0, count = 0;
            ar >> sub.name >> firstIndex >> count >> materialPath;
            sub.firstIndex = static_cast<uint32_t>(firstIndex);
            sub.indexCount = static_cast<uint32_t>(count);
            sub.materialHandle = materialPath.empty()
                ? INVALID_ASSET_HANDLE
                : ResourceManager::store<Material>(materialPath);
        }

        return true;
    }

    bool StaticMeshImporter::saveToAsset(const Ref<StaticMeshResource>& resource, const std::string& filePath) {
        if (!resource) {
            VE_CORE_ERROR_PRINT("saveToAsset: null resource (%s)", filePath.c_str());
            return false;
        }

        BinaryArchive ar(filePath, ArchiveMode::write);
        if (!ar.isGood()) {
            VE_CORE_ERROR_PRINT("saveToAsset: cannot open %s", filePath.c_str());
            return false;
        }

        resource->serialize(ar);
        ar.flush();
        return true;
    }

    Ref<StaticMeshResource> StaticMeshImporter::loadFromAsset(const std::string& filePath) {
        BinaryArchive ar(filePath, ArchiveMode::read);
        if (!ar.isGood()) {
            VE_CORE_ERROR_PRINT("loadFromAsset: cannot open %s", filePath.c_str());
            return nullptr;
        }

        auto resource = CreateRef<StaticMeshResource>();
        if (!resource->deserialize(ar) || !ar.isGood() || resource->vertexBuffer.empty()) {
            VE_CORE_ERROR_PRINT("loadFromAsset: invalid mesh asset %s", filePath.c_str());
            return nullptr;
        }
        return resource;
    }

}

