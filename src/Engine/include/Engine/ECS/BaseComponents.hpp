#pragma once

#include <Engine/Renderer/Graphics/Mesh.hpp>
#include <Engine/Core/UUID.hpp>
#include <Engine/ECS/AnimatorController.hpp>
#include <Engine/Renderer/Graphics/Model.hpp>

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
        glm::vec3 Rotation { 0.0f, 0.0f, 0.0f };
        glm::vec3 Scale { 1.0f, 1.0f, 1.0f };

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
            glm::quat rx = glm::angleAxis(t.Rotation.x, glm::vec3(1, 0, 0));
            glm::quat ry = glm::angleAxis(t.Rotation.y, glm::vec3(0, 1, 0));
            glm::quat rz = glm::angleAxis(t.Rotation.z, glm::vec3(0, 0, 1));
            Matrix = glm::translate(glm::mat4(1.0f), t.Translation)
                   * glm::toMat4(ry * rx * rz)
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

        uint32_t prio { 0 };

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
        glm::vec3 Offset { 0.0f, 0.0f, 0.0f };
        glm::vec3 Scale { 1.0f, 1.0f, 1.0f };
        
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

    struct DirectionalLightComponent
    {
        glm::vec3 Color { 1.0f, 1.0f, 1.0f };
        float Intensity { 1.0f };

        DirectionalLightComponent() = default;
        DirectionalLightComponent(const DirectionalLightComponent&) = default;
    };

    struct PointLightComponent
    {
        glm::vec3 Color { 1.0f, 0.9f, 0.7f };
        float Intensity { 10.0f };
        float Radius { 10.0f };

        PointLightComponent() = default;
        PointLightComponent(const PointLightComponent&) = default;
    };

    struct SpotLightComponent
    {
        glm::vec3 Color { 1.0f, 1.0f, 1.0f };
        float Intensity { 15.0f };
        float Radius { 15.0f }; 
        float InnerCutOff { glm::cos(glm::radians(12.5f)) };
        float OuterCutOff { glm::cos(glm::radians(17.5f)) };

        SpotLightComponent() = default;
        SpotLightComponent(const SpotLightComponent&) = default;
    };

    struct AmbientComponent
    {
        glm::vec3 SkyColorDay { 0.26108402f, 0.42027727f, 0.7784616f  };
        glm::vec3 HorizonColorDay { 0.6553657f,  0.8003316f,  0.9815385f  };
        glm::vec3 GroundColor { 0.23529412f, 0.22745098f, 0.23137255f };

        glm::vec3 SkyColorNight { 0.050073378f, 0.06377917f, 0.13230771f };
        glm::vec3 HorizonColorNight { 0.030769223f, 0.040615413f, 0.07999998f };

        float StarIntensity { 1.5f };

        entt::entity SunEntity { entt::null };
        entt::entity MoonEntity { entt::null };
        float SunMaxIntensity { 2.5f };
        float MoonMaxIntensity { 0.15f };

        AmbientComponent() = default;
        AmbientComponent(const AmbientComponent&) = default;
    };

    struct TimeCycleComponent
    {
        float TimeOfDay { 12.0f };
        float TimeScale { 600.0f };
        uint32_t CurrentDay { 0 };
        float MoonPhase { 0.0f };

        TimeCycleComponent() = default;
        TimeCycleComponent(const TimeCycleComponent&) = default;
    };

    struct AnimatorComponent
    {
        AnimatorController Controller;
        ModelData* Model { nullptr };

        float Speed { 1.0f };

        uint32_t ActiveState { UINT32_MAX };
        uint32_t BlendingFrom { UINT32_MAX };
        float StateTime { 0.0f };
        float BlendFromTime { 0.0f };
        float BlendTimer { 0.0f };
        float BlendDuration { 0.0f };

        std::vector<float> FloatValues;
        std::vector<int> IntValues;
        std::vector<bool> BoolValues;
        std::vector<bool> TriggerValues;

        std::vector<glm::mat4> FinalBoneMatrices;
        std::vector<std::string> FiredEvents;

        AnimatorComponent() = default;
        AnimatorComponent(const AnimatorComponent&) = default;
    };

    struct SkinnedMeshComponent
    {
        UUID ModelAssetUUID { 0 };
        ModelData* Model { nullptr };

        SkinnedMeshComponent() = default;
        SkinnedMeshComponent(const SkinnedMeshComponent&) = default;
    };

    struct MeshColliderComponent
    {
        std::vector<glm::vec3> Vertices;
        std::vector<uint32_t> Indices;
        std::shared_ptr<void> CachedShape;

        MeshColliderComponent() = default;
        MeshColliderComponent(const MeshColliderComponent&) = default;
    };

    struct AudioClip
    {
        UUID AudioAssetUUID { 0 };
        float Volume { 1.0f };
        bool Loop { false };
        bool PlayOnStart { false };
        std::shared_ptr<void> RuntimeSound;

        AudioClip() = default;
        AudioClip(const AudioClip& o) : AudioAssetUUID(o.AudioAssetUUID), Volume(o.Volume), Loop(o.Loop), PlayOnStart(o.PlayOnStart) {}
        AudioClip& operator=(const AudioClip& o)
        {
            AudioAssetUUID = o.AudioAssetUUID; Volume = o.Volume; Loop = o.Loop; PlayOnStart = o.PlayOnStart;
            return *this;
        }
    };

    struct AudioPlayerComponent
    {
        std::vector<AudioClip> Clips;

        AudioPlayerComponent() = default;
        AudioPlayerComponent(const AudioPlayerComponent&) = default;
    };
}