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

    struct ModelData
    {
        std::vector<SubMeshData> SubMeshes;
        ModelNode RootNode;
    };
}