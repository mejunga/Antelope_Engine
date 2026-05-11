#pragma once

#include <glm/glm.hpp>

#include <vector>
#include <cstdint>
#include <memory_resource>


namespace Antelope
{
    struct VertexPosition { alignas(16) glm::vec3 pos; };
    struct VertexColor { alignas(16) glm::vec3 color; };
    struct VertexNormal { alignas(16) glm::vec3 normal; };
    struct VertexUV { alignas(8)  glm::vec2 uv; };
    struct VertexTangent { alignas(16) glm::vec3 tangent; };
    struct Face { uint32_t v0, v1, v2; uint32_t normalIndex; };

    struct MeshData
    {
        std::pmr::vector<VertexPosition> positions;
        std::pmr::vector<VertexColor> colors;
        std::pmr::vector<VertexNormal> normals;
        std::pmr::vector<VertexUV> uvs;
        std::pmr::vector<VertexTangent> tangents;
        std::pmr::vector<Face> faces;
    };

    struct MeshHandle
    {
        uint32_t MeshID { 0 };
        uint32_t posOffset { 0 };
        uint32_t colorOffset { 0 };
        uint32_t normalOffset { 0 };
        uint32_t uvOffset { 0 };
        uint32_t tangentOffset { 0 };
        uint32_t faceOffset { 0 };
        uint32_t faceCount { 0 };
    };
}