#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace Antelope
{
    struct VertexPosition
    {
        alignas(16) glm::vec3 pos;
    };

    struct VertexColor
    {
        alignas(16) glm::vec3 color;
    };
    
    struct VertexNormal
    {
        alignas(16) glm::vec3 normal;
    };
    
    struct Face 
    { 
        uint32_t v0, v1, v2; 
        uint32_t normalIndex; 
    };

    struct MeshData
    {
        std::vector<VertexPosition> positions;
        std::vector<VertexColor> colors;
        std::vector<VertexNormal> normals;
        std::vector<Face> faces;
    };

    struct MeshHandle
    {
        uint32_t posOffset;
        uint32_t colorOffset;
        uint32_t normalOffset;
        uint32_t faceOffset;
        uint32_t faceCount;
    };

    struct RenderCommand 
    {
        glm::mat4 transform;
        MeshHandle mesh;
    };

    struct ObjectData
    {
        glm::mat4 model;
        
        uint32_t posOffset;
        uint32_t colorOffset;
        uint32_t normalOffset;
        uint32_t faceOffset;
    };
}