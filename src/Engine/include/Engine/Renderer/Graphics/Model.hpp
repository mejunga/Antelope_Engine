#pragma once

#include <Engine/Renderer/Graphics/Mesh.hpp>

#include <glm/glm.hpp>

#include <vector>
#include <string>


namespace Antelope
{
    struct SubMeshData
    {
        MeshData Data;
        uint32_t MaterialIndex { 0 };
        std::string Name;
    };

    struct ModelNode
    {
        std::string Name;
        glm::mat4 LocalTransform { 1.0f };
        std::vector<uint32_t> MeshIndices;
        std::vector<ModelNode> Children;
    };

    struct ModelMaterial 
    {
        glm::vec4 AlbedoFactor { 1.0f };
        glm::vec4 MetallicRoughnessFactors { 0.0f, 0.5f, 1.0f, 0.0f };
        
        std::string AlbedoTexPath;
        std::string NormalTexPath;
        std::string MetRoughAOTexPath;
        std::string EmissiveTexPath;
    };

    struct ModelData
    {
        std::vector<SubMeshData> SubMeshes;
        std::vector<ModelMaterial> Materials;
        ModelNode RootNode;
    };
}