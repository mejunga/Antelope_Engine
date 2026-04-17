#pragma once

#include <Engine/Renderer/Graphics/Mesh.hpp>
#include <Engine/Core/UUID.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <entt/entt.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <string>


namespace Antelope
{
    struct DisabledComponent {};
    
    struct TagComponent
    {
        std::string Tag;
        TagComponent() = default;
        TagComponent(const TagComponent&) = default;
        TagComponent(const std::string& tag) : Tag(tag) {}
    };

    struct IDComponent
    {
        UUID ID;

        IDComponent() = default;
        IDComponent(const IDComponent&) = default;
    };

    struct DirtyTransform {};

    struct TransformComponent
    {
        glm::vec3 Translation { 0.0f, 0.0f, 0.0f };
        glm::vec3 Rotation    { 0.0f, 0.0f, 0.0f };
        glm::vec3 Scale       { 1.0f, 1.0f, 1.0f };

        TransformComponent() = default;
        TransformComponent(const TransformComponent&) = default;
    };

    struct LocalMatrixComponent
    {
        glm::mat4 Matrix { 1.0f };

        LocalMatrixComponent() = default;
        LocalMatrixComponent(const LocalMatrixComponent&) = default;

        void Rebuild(const TransformComponent& t)
        {
            Matrix = glm::translate(glm::mat4(1.0f), t.Translation)
                * glm::toMat4(glm::quat(t.Rotation))
                * glm::scale(glm::mat4(1.0f), t.Scale);
        }
    };

    struct WorldMatrixComponent
    {
        glm::mat4 Matrix { 1.0f };

        WorldMatrixComponent() = default;
        WorldMatrixComponent(const WorldMatrixComponent&) = default;
    };

    struct NormalMatrixComponent
    {
        glm::mat3 Matrix { 1.0f };
    };

    struct RelationshipComponent
    {
        entt::entity Parent { entt::null };
        entt::entity FirstChild { entt::null };
        entt::entity PreviousSibling { entt::null };
        entt::entity NextSibling { entt::null };
        
        uint32_t Depth { 0 }; 
        
        RelationshipComponent() = default;
        RelationshipComponent(const RelationshipComponent&) = default;
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

    struct MaterialComponent
    {
        uint32_t MaterialIndex { 0 };

        MaterialComponent() = default;
        MaterialComponent(const MaterialComponent&) = default;
    };

    enum class RigidBodyType
    {
        Static = 0,
        Kinematic,
        Dynamic
    };

    struct RigidBodyComponent
    {
        RigidBodyType Type { RigidBodyType::Static };
        
        float Mass { 1.0f };
        float Friction { 0.2f };
        float Restitution { 0.0f };
        
        uint32_t RuntimeBodyID { 0xFFFFFFFF }; 
        
        RigidBodyComponent() = default;
        RigidBodyComponent(const RigidBodyComponent&) = default;
    };

    enum class ColliderType
    {
        Box = 0,
        Sphere,
        Capsule
    };

    struct ColliderComponent
    {
        ColliderType Type { ColliderType::Box };
        
        glm::vec3 Size { 0.5f, 0.5f, 0.5f }; 
        glm::vec3 Offset { 0.0f, 0.0f, 0.0f }; 
        
        ColliderComponent() = default;
        ColliderComponent(const ColliderComponent&) = default;
    };
}