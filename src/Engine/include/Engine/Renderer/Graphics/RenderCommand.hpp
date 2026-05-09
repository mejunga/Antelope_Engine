#pragma once

#include <Engine/Renderer/Graphics/Mesh.hpp>

#include <glm/glm.hpp>

#include <cstdint>


namespace Antelope
{
    constexpr uint32_t MAX_POINT_LIGHTS = 32;
    constexpr uint32_t MAX_SPOT_LIGHTS = 32;

    struct PointLightData 
    {
        glm::vec4 positionAndRadius;
        glm::vec4 colorAndIntensity;
    };

    struct SpotLightData 
    {
        glm::vec4 positionAndRadius;
        glm::vec4 directionAndCutOff;
        glm::vec4 colorAndIntensity;
        glm::vec4 outerCutOffAndPad;
    };

    struct UniformBufferObject
    {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec4 cameraPos;

        glm::vec4 sunDirection;
        glm::vec4 sunColor;
        glm::vec4 moonDirection;
        glm::vec4 moonColor;

        glm::mat4 lightSpaceMatrices[2];
        glm::vec4 cascadeSplits;

        glm::vec4 skyColorDayAndStar;
        glm::vec4 horizonColorDay;
        glm::vec4 skyColorNight;
        glm::vec4 horizonColorNight;
        glm::vec4 groundColor;

        uint32_t pointLightCount;
        uint32_t spotLightCount;
        float time;
        float ambientEnabled;
        uint32_t shadowCaster { 0 };
        float _pad1 { 0.0f };
        float _pad2 { 0.0f };
        float _pad3 { 0.0f };

        PointLightData pointLights[MAX_POINT_LIGHTS];
        SpotLightData spotLights[MAX_SPOT_LIGHTS];
    };

    struct RenderCommand 
    {
        glm::mat4 transform;
        glm::mat3 normalMatrix;
        MeshHandle mesh;
        uint32_t materialIndex { 0 };
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
        uint32_t tangentOffset { 0 };
    #ifdef ANTELOPE_EDITOR_MODE
        uint32_t entityID { 0 };
    #else
        uint32_t padding { 0 };
    #endif
    };
}