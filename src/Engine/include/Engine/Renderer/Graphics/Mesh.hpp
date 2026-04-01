#pragma once

#include <Engine/Renderer/Vulkan/GpuMemoryAllocator.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace Antelope
{
    struct VertexPosition { alignas(16) glm::vec3 pos; };
    struct VertexColor { alignas(16) glm::vec3 color; };
    struct VertexNormal { alignas(16) glm::vec3 normal; };
    struct VertexUV { alignas(8) glm::vec2 uv; };
    struct Face { uint32_t v0, v1, v2; uint32_t normalIndex; };

    struct MeshData
    {
        std::vector<VertexPosition> positions;
        std::vector<VertexColor> colors;
        std::vector<VertexNormal> normals;
        std::vector<VertexUV> uvs;
        std::vector<Face> faces;
    };

    struct MeshHandle
    {
        uint32_t MeshID { 0 };
        VirtualAllocation posAllocation;
        VirtualAllocation colorAllocation;
        VirtualAllocation normalAllocation;
        VirtualAllocation faceAllocation;
        VirtualAllocation uvAllocation;
        uint32_t faceCount { 0 };
        uint32_t materialIndex { 0 };
    };
}