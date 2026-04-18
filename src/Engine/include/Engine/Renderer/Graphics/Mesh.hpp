#pragma once

#include <glm/glm.hpp>

#include <vector>
#include <cstdint>


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
        std::vector<VertexPosition> positions;
        std::vector<VertexColor> colors;
        std::vector<VertexNormal> normals;
        std::vector<VertexUV> uvs;
        std::vector<VertexTangent> tangents;
        std::vector<Face> faces;
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