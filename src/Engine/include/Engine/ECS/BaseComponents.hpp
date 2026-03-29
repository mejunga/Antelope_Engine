#pragma once

#include <Engine/Renderer/Graphics/Mesh.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <string>


namespace Antelope
{
    struct TagComponent
    {
        std::string Tag;
        TagComponent() = default;
        TagComponent(const TagComponent&) = default;
        TagComponent(const std::string& tag) : Tag(tag) {}
    };

    struct RelationshipComponent
    {
        entt::entity Parent { entt::null };
        entt::entity FirstChild { entt::null };
        entt::entity PreviousSibling { entt::null };
        entt::entity NextSibling { entt::null };
        
        RelationshipComponent() = default;
        RelationshipComponent(const RelationshipComponent&) = default;
    };

    struct TransformComponent
    {
        glm::vec3 Translation { 0.0f, 0.0f, 0.0f };
        glm::vec3 Rotation { 0.0f, 0.0f, 0.0f };
        glm::vec3 Scale { 1.0f, 1.0f, 1.0f };

        glm::mat4 WorldMatrix { 1.0f };
        glm::mat4 NormalMatrix { 1.0f };

        bool IsDirty { true };

        TransformComponent() = default;
        TransformComponent(const TransformComponent&) = default;
        TransformComponent(const glm::vec3& translation) : Translation(translation) {}

        glm::mat4 GetLocalTransform() const 
        {
            glm::mat4 rotation { glm::toMat4(glm::quat(Rotation)) };
            return glm::translate(glm::mat4(1.0f), Translation)
                 * rotation
                 * glm::scale(glm::mat4(1.0f), Scale);
        }
    };

    struct CameraComponent
    {
        glm::mat4 Projection { 1.0f };
        float PerspectiveFOV { glm::radians(45.0f) };
        float PerspectiveNear { 0.1f };
        float PerspectiveFar { 1000.0f };

        bool IsPrimary { true };

        CameraComponent() = default;
        CameraComponent(const CameraComponent&) = default;

        void CalculateProjection(float aspectRatio)
        {
            Projection = glm::perspective(PerspectiveFOV, aspectRatio, PerspectiveNear, PerspectiveFar);
            Projection[1][1] *= -1.0f;
        }
    };

    struct MeshComponent
    {
        MeshHandle Handle; 
        
        MeshComponent() = default;
        MeshComponent(const MeshComponent&) = default;
        MeshComponent(MeshHandle handle) : Handle(handle) {}
    };
}