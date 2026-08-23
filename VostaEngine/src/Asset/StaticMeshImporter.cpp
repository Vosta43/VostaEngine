#include "vepch.h"
#include "Core/Log.h"
#include "StaticMeshImporter.h"
#include "StaticMeshResource.h"
#include "Renderer/Material.h"
#include "Core/ResourceManager.h"
#include "Scene/Archive.h"
#include "Core/AssetConfig.h"
#include "Utils.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>

namespace ve {
    
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
        return stem + "_" + materialName;       // "car_body_paint"
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
        struct TriangleRecord {
            Vertex v0, v1, v2;
            std::string materialName;
        };
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

        // Group triangles by material
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

        // Emit vertices and indices grouped by material order.
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

        resource->vertexBuffer = std::move(sortedVertices);
        resource->indexBuffer = std::move(sortedIndices);
        resource->subMeshes = std::move(sortedSubMeshes);
        // ------------------------------------

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
                auto material = ResourceManager::get<Material>(matHandle);

                auto it = mtlMaterials.find(rawMtlName);
                if (it != mtlMaterials.end()) {
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
                {
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
    Ref<StaticMeshResource> StaticMeshImporter::importFromFile(const std::string& filePath) {

        Ref<StaticMeshResource> resource;

        if (utils::getExtension(filePath) == ".obj") {
            resource = importObj(filePath);
        }
        else if (utils::getExtension(filePath) == ".fbx") {
            // TODO: importFbx
        }

        return resource;
    }

}

