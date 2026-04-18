#pragma once

#include <glm/glm.hpp>

#include <cstdint>


namespace Antelope
{
    struct PBRMaterialData 
    {
        glm::vec4 AlbedoFactor { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec4 MetallicRoughnessFactors { 0.0f, 0.85f, 1.0f, 0.0f };
        uint32_t AlbedoTexIndex { 0xFFFFFFFF };
        uint32_t NormalTexIndex { 0xFFFFFFFF };
        uint32_t MetRoughAOTexIndex { 0xFFFFFFFF };
        uint32_t EmissiveTexIndex { 0xFFFFFFFF };
    };

    static constexpr uint32_t MAX_MATERIALS = 1024;
}