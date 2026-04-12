#pragma once

#include <Engine/Renderer/Graphics/Mesh.hpp>
#include <glm/glm.hpp>

#include <cstdint>


namespace Antelope
{
    struct UniformBufferObject
    {
        glm::mat4 view;
        glm::mat4 proj;
    };

    struct RenderCommand 
    {
        glm::mat4 transform;
        glm::mat3 normalMatrix;
        MeshHandle mesh;

    #ifdef ANTELOPE_EDITOR_MODE
        uint32_t entityID { 0 };
    #endif
    };

    struct ObjectData
    {
        glm::mat4 model { 1.0f };
        glm::mat4 normalMatrix { 1.0f };
        uint32_t posOffset { 0 };
        uint32_t colorOffset { 0 };
        uint32_t normalOffset { 0 };
        uint32_t uvOffset { 0 };
        uint32_t faceOffset { 0 };
        uint32_t materialIndex { 0 };
    #ifdef ANTELOPE_EDITOR_MODE
        uint32_t entityID { 0 };
        uint32_t padding { 0 };
    #else
        uint32_t padding[2] { 0, 0 };
    #endif
    };
}