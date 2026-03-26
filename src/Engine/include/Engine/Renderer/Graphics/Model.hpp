#pragma once

#include <Engine/Renderer/Graphics/Mesh.hpp>
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

    struct ModelData
    {
        std::vector<SubMeshData> SubMeshes;
    };
}