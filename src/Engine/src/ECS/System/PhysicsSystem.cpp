#include <Engine/ECS/System/PhysicsSystem.hpp>
#include <Engine/ECS/World.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/Physics/PhysicsContext.hpp>
#include <Engine/Debug/Log.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Body/BodyInterface.h>


namespace Antelope
{
    inline JPH::Vec3 ToJoltVec3(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
    inline JPH::Quat ToJoltQuat(const glm::quat& q) { return JPH::Quat(q.x, q.y, q.z, q.w); }
    
    inline glm::vec3 ToGlmVec3(const JPH::Vec3& v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }
    inline glm::quat ToGlmQuat(const JPH::Quat& q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

    static JPH::ShapeRefC CreateEntityShape(World& world, entt::entity entity)
    {
        auto& registry { world.GetRegistry() };

        if (registry.all_of<MeshColliderComponent>(entity))
        {
            auto& mc { registry.get<MeshColliderComponent>(entity) };

            if (mc.CachedShape)
            {
                return *static_cast<JPH::ShapeRefC*>(mc.CachedShape.get());
            }

            if (!mc.Vertices.empty() && mc.Indices.size() >= 3)
            {
                glm::vec3 scale { 1.0f };

                if (registry.all_of<WorldMatrixComponent>(entity))
                {
                    const auto& wm { registry.get<WorldMatrixComponent>(entity).Matrix };
                    scale = { glm::length(glm::vec3(wm[0])), glm::length(glm::vec3(wm[1])), glm::length(glm::vec3(wm[2])) };
                }

                JPH::VertexList verts;
                verts.reserve(mc.Vertices.size());

                for (const auto& v : mc.Vertices)
                {
                    verts.push_back({ v.x * scale.x, v.y * scale.y, v.z * scale.z });
                }

                JPH::IndexedTriangleList tris;
                tris.reserve(mc.Indices.size() / 3);

                for (size_t i { 0 }; i + 2 < mc.Indices.size(); i += 3)
                {
                    tris.push_back(JPH::IndexedTriangle(mc.Indices[i], mc.Indices[i + 1], mc.Indices[i + 2]));
                }

                auto result { JPH::MeshShapeSettings(std::move(verts), std::move(tris)).Create() };

                if (result.IsValid())
                {
                    if (registry.all_of<RigidBodyComponent>(entity))
                    {
                        auto type { registry.get<RigidBodyComponent>(entity).Type };
                        
                        if (type == RigidBodyType::Dynamic || type == RigidBodyType::Kinematic)
                        {
                            AE_ENGINE_WARN("MeshCollider on Dynamic/Kinematic body is not supported — falling back to box.");
                        }
                        else
                        {
                            auto* ref { new JPH::ShapeRefC(result.Get()) };
                            mc.CachedShape = std::shared_ptr<void>(ref, [](void* p){ delete static_cast<JPH::ShapeRefC*>(p); });
                            return result.Get();
                        }
                    }
                    else
                    {
                        auto* ref { new JPH::ShapeRefC(result.Get()) };
                        mc.CachedShape = std::shared_ptr<void>(ref, [](void* p){ delete static_cast<JPH::ShapeRefC*>(p); });
                        return result.Get();
                    }
                }
            }
        }

        JPH::ShapeRefC shape;

        std::vector<glm::vec3> childOffsets;
        std::vector<JPH::Quat> childRotations;
        std::vector<JPH::ShapeRefC> childShapeRefs;

        const auto& parentWorldMat { registry.get<WorldMatrixComponent>(entity).Matrix };
        glm::vec3 parentWorldPos { parentWorldMat[3] };
        glm::mat3 parentRotMat { parentWorldMat };

        if (glm::length(parentRotMat[0]) > 0.0001f)
        {
            parentRotMat[0] = glm::normalize(parentRotMat[0]);
        }

        if (glm::length(parentRotMat[1]) > 0.0001f)
        {
            parentRotMat[1] = glm::normalize(parentRotMat[1]);
        }
        
        if (glm::length(parentRotMat[2]) > 0.0001f)
        {
            parentRotMat[2] = glm::normalize(parentRotMat[2]);
        }

        glm::quat parentRot { glm::quat_cast(parentRotMat) };
        glm::quat invParentRot { glm::inverse(parentRot) };

        if (registry.all_of<RelationshipComponent>(entity))
        {
            entt::entity child { registry.get<RelationshipComponent>(entity).FirstChild };
            
            while (child != entt::null)
            {
                if (registry.all_of<ColliderComponent>(child) && registry.all_of<WorldMatrixComponent>(child))
                {
                    auto& childCol { registry.get<ColliderComponent>(child) };
                    const auto& childWorldMat { registry.get<WorldMatrixComponent>(child).Matrix };
                    glm::vec3 childWorldPos { childWorldMat[3] };
                    glm::mat3 childRotMat { childWorldMat };

                    if (glm::length(childRotMat[0]) > 0.0001f)
                    {
                        childRotMat[0] = glm::normalize(childRotMat[0]);
                    }

                    if (glm::length(childRotMat[1]) > 0.0001f)
                    {
                        childRotMat[1] = glm::normalize(childRotMat[1]);
                    }
                    
                    if (glm::length(childRotMat[2]) > 0.0001f)
                    {
                        childRotMat[2] = glm::normalize(childRotMat[2]);
                    }

                    glm::quat childRot { glm::quat_cast(childRotMat) };
                    JPH::ShapeRefC childShape;

                    if (childCol.Type == ColliderType::Box)
                    {
                        childShape = JPH::BoxShapeSettings(ToJoltVec3(childCol.Size * 0.5f)).Create().Get();
                    }
                    else if (childCol.Type == ColliderType::Sphere)
                    {
                        childShape = JPH::SphereShapeSettings(childCol.Size.x).Create().Get();
                    }
                    else if (childCol.Type == ColliderType::Capsule)
                    {
                        childShape = JPH::CapsuleShapeSettings(childCol.Size.y, childCol.Size.x).Create().Get();
                    }

                    if (childShape)
                    {
                        glm::vec3 childColliderWorldPos { childWorldPos + childRot * childCol.Offset };
                        glm::vec3 localOffset { invParentRot * (childColliderWorldPos - parentWorldPos) };
                        glm::quat localRot { invParentRot * childRot };
                        childOffsets.push_back(localOffset);
                        childRotations.push_back(ToJoltQuat(localRot));
                        childShapeRefs.push_back(childShape);
                    }
                }
                child = registry.get<RelationshipComponent>(child).NextSibling;
            }
        }

        if (!childOffsets.empty())
        {
            JPH::StaticCompoundShapeSettings compoundSettings {};

            for (size_t i { 0 }; i < childOffsets.size(); ++i)
            {
                compoundSettings.AddShape(ToJoltVec3(childOffsets[i]), childRotations[i], childShapeRefs[i]);
            }
            
            shape = compoundSettings.Create().Get();
        }
        else if (registry.all_of<ColliderComponent>(entity))
        {
            auto& collider { registry.get<ColliderComponent>(entity) }
            ;
            if (collider.Type == ColliderType::Box)
            {
                shape = JPH::BoxShapeSettings(ToJoltVec3(collider.Size * 0.5f)).Create().Get();
            }
            else if (collider.Type == ColliderType::Sphere)
            {
                shape = JPH::SphereShapeSettings(collider.Size.x).Create().Get();
            }
            else if (collider.Type == ColliderType::Capsule)
            {
                shape = JPH::CapsuleShapeSettings(collider.Size.y, collider.Size.x).Create().Get();
            }
        }
        else
        {
            auto& transform { registry.get<TransformComponent>(entity) };
            shape = JPH::BoxShapeSettings(ToJoltVec3(transform.Scale * 0.5f)).Create().Get();
        }

        return shape;
    }

    void PhysicsSystem::OnRuntimeStart(World& world, PhysicsContext& physicsContext)
    {
        auto& registry { world.GetRegistry() };
        JPH::BodyInterface& bodyInterface { physicsContext.GetBodyInterface() };

        auto view { registry.view<TransformComponent, RigidBodyComponent>() };

        for (auto [entity, transform, rb] : view.each())
        {
            JPH::ShapeRefC shape { CreateEntityShape(world, entity) };

            if (!shape)
            {
                AE_ENGINE_ERROR("PhysicsSystem: Shape creation failed for entity, skipping body.");
                continue;
            }

            JPH::ObjectLayer layer { rb.Type == RigidBodyType::Static ? Layers::NON_MOVING : Layers::MOVING };
            JPH::EMotionType motionType { JPH::EMotionType::Dynamic };

            if (rb.Type == RigidBodyType::Static)
            {
                motionType = JPH::EMotionType::Static;
            }
            else if (rb.Type == RigidBodyType::Kinematic)
            {
                motionType = JPH::EMotionType::Kinematic;
            }

            const auto& worldMat { registry.get<WorldMatrixComponent>(entity) };
            glm::mat3 rotMat { worldMat.Matrix };
            
            glm::vec3 globalScale
            {
                glm::length(rotMat[0]),
                glm::length(rotMat[1]),
                glm::length(rotMat[2])
            };

            if (globalScale.x > 0.0001f) { rotMat[0] /= globalScale.x; }
            if (globalScale.y > 0.0001f) { rotMat[1] /= globalScale.y; }
            if (globalScale.z > 0.0001f) { rotMat[2] /= globalScale.z; }

            glm::quat worldRotation { glm::quat_cast(rotMat) };
            glm::vec3 worldPosition { worldMat.Matrix[3] };

            glm::vec3 colliderOffset { glm::vec3(0.0f) };

            if (registry.all_of<ColliderComponent>(entity))
            {
                colliderOffset = registry.get<ColliderComponent>(entity).Offset;
            }

            JPH::BodyCreationSettings bodySettings(
                shape,
                ToJoltVec3(worldPosition + worldRotation * colliderOffset),
                ToJoltQuat(worldRotation),
                motionType,
                layer
            );

            bodySettings.mRestitution = rb.Restitution;
            bodySettings.mFriction = rb.Friction;

            if ((rb.Type == RigidBodyType::Dynamic || rb.Type == RigidBodyType::Kinematic) && rb.Mass > 0.0f)
            {
                bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
                bodySettings.mMassPropertiesOverride.mMass = rb.Mass;
            }

            JPH::Body* body { bodyInterface.CreateBody(bodySettings) };

            if (body)
            {
                rb.RuntimeBodyID = body->GetID().GetIndexAndSequenceNumber();

                if (!registry.all_of<DisabledComponent>(entity))
                {
                    bodyInterface.AddBody(body->GetID(), JPH::EActivation::Activate);
                }
            }
            else
            {
                AE_ENGINE_ERROR("Failed to create Jolt physics body for an entity!");
            }
        }
        
        AE_ENGINE_TRACE("PhysicsSystem: Runtime started, bodies created.");
    }

    void PhysicsSystem::OnUpdate(World& world, PhysicsContext& physicsContext, float timeStep)
    {
        auto& registry { world.GetRegistry() };
        JPH::BodyInterface& bodyInterface { physicsContext.GetBodyInterface() };

        auto rbView { registry.view<RigidBodyComponent>() };
        for (auto [entity, rb] : rbView.each())
        {
            if (rb.RuntimeBodyID == 0xFFFFFFFF) { continue; }

            JPH::BodyID bodyID(rb.RuntimeBodyID);
            bool isActive { !registry.all_of<DisabledComponent>(entity) };
            bool isAdded { bodyInterface.IsAdded(bodyID) };

            if (isActive && !isAdded) { bodyInterface.AddBody(bodyID, JPH::EActivation::Activate); }
            else if (!isActive && isAdded) { bodyInterface.RemoveBody(bodyID); }
        }

        physicsContext.GetSystem().Update(timeStep, 1, physicsContext.GetTempAllocator(), physicsContext.GetJobSystem());

        for (auto [entity, rb] : rbView.each())
        {
            if (registry.all_of<DisabledComponent>(entity)) { continue; }
            if (rb.Type == RigidBodyType::Static) { continue; }
            if (rb.RuntimeBodyID == 0xFFFFFFFF) { continue; }

            JPH::BodyID bodyID(rb.RuntimeBodyID);

            if (bodyInterface.IsActive(bodyID))
            {
                auto& transform { registry.get<TransformComponent>(entity) };
                glm::quat bodyRot { ToGlmQuat(bodyInterface.GetRotation(bodyID)) };
                glm::vec3 bodyPos { ToGlmVec3(bodyInterface.GetPosition(bodyID)) };

                if (registry.all_of<ColliderComponent>(entity))
                {
                    bodyPos -= bodyRot * registry.get<ColliderComponent>(entity).Offset;
                }

                transform.Translation = bodyPos;
                transform.Rotation = glm::eulerAngles(ToGlmQuat(bodyInterface.GetRotation(bodyID)));
                registry.emplace_or_replace<DirtyTransform>(entity);
            }
        }
    }

    void PhysicsSystem::OnRuntimeStop(World& world, PhysicsContext& physicsContext)
    {
        auto& registry { world.GetRegistry() };
        JPH::BodyInterface& bodyInterface { physicsContext.GetBodyInterface() };

        auto view { registry.view<RigidBodyComponent>() };

        for (auto [entity, rb] : view.each())
        {
            if (rb.RuntimeBodyID != 0xFFFFFFFF)
            {
                JPH::BodyID bodyID(rb.RuntimeBodyID);
                bodyInterface.RemoveBody(bodyID);
                bodyInterface.DestroyBody(bodyID);
                
                rb.RuntimeBodyID = 0xFFFFFFFF;
            }
        }
        
        AE_ENGINE_TRACE("PhysicsSystem: Runtime stopped, bodies destroyed.");
    }

    void PhysicsSystem::SetBodyTransform(World& world, PhysicsContext& physicsContext, entt::entity entity)
    {
        auto& registry { world.GetRegistry() };
        entt::entity rbEntity { entity };

        while (rbEntity != entt::null && !registry.all_of<RigidBodyComponent>(rbEntity))
        {
            if (registry.all_of<RelationshipComponent>(rbEntity))
            {
                rbEntity = registry.get<RelationshipComponent>(rbEntity).Parent;
            }
            else
            {
                rbEntity = entt::null;
            }
        }

        if (rbEntity == entt::null) { return; }

        auto& rb { registry.get<RigidBodyComponent>(rbEntity) };

        if (rb.RuntimeBodyID == 0xFFFFFFFF) { return; }

        auto& transform { registry.get<TransformComponent>(rbEntity) };
        auto& localMat { registry.get<LocalMatrixComponent>(rbEntity) };
        localMat.Rebuild(transform);

        glm::mat4 worldMat { localMat.Matrix };

        if (registry.all_of<RelationshipComponent>(rbEntity))
        {
            entt::entity parent { registry.get<RelationshipComponent>(rbEntity).Parent };
            
            if (parent != entt::null)
            {
                const auto& parentWorld { registry.get<WorldMatrixComponent>(parent) };
                worldMat = parentWorld.Matrix * localMat.Matrix;
            }
        }

        JPH::BodyInterface& bodyInterface { physicsContext.GetBodyInterface() };
        JPH::BodyID bodyID(rb.RuntimeBodyID);

        glm::vec3 worldPosition { worldMat[3] };
        glm::quat worldRotation { glm::normalize(glm::quat_cast(glm::mat3(worldMat))) };

        glm::vec3 colliderOffset { 0.0f };

        if (registry.all_of<ColliderComponent>(rbEntity))
        {
            colliderOffset = registry.get<ColliderComponent>(rbEntity).Offset;
        }

        JPH::ShapeRefC newShape { CreateEntityShape(world, rbEntity) };

        if (newShape)
        {
            bodyInterface.SetShape(bodyID, newShape, false, JPH::EActivation::Activate);
        }

        bodyInterface.SetPositionAndRotation(
            bodyID,
            ToJoltVec3(worldPosition + worldRotation * colliderOffset),
            ToJoltQuat(worldRotation),
            JPH::EActivation::Activate
        );

        bodyInterface.SetLinearVelocity(bodyID, JPH::Vec3::sZero());
        bodyInterface.SetAngularVelocity(bodyID, JPH::Vec3::sZero());
    }
}