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
#include <Jolt/Physics/Body/BodyInterface.h>


namespace Antelope
{
    inline JPH::Vec3 ToJoltVec3(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
    inline JPH::Quat ToJoltQuat(const glm::quat& q) { return JPH::Quat(q.x, q.y, q.z, q.w); }
    
    inline glm::vec3 ToGlmVec3(const JPH::Vec3& v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }
    inline glm::quat ToGlmQuat(const JPH::Quat& q) { return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); }

    void PhysicsSystem::OnRuntimeStart(World& world, PhysicsContext& physicsContext)
    {
        auto& registry { world.GetRegistry() };
        JPH::BodyInterface& bodyInterface { physicsContext.GetBodyInterface() };

        auto view { registry.view<TransformComponent, RigidBodyComponent>() };

        for (auto [entity, transform, rb] : view.each())
        {
            JPH::ShapeRefC shape;
            
            std::vector<std::pair<glm::vec3, JPH::ShapeRefC>> childShapes;

            if (registry.all_of<RelationshipComponent>(entity))
            {
                entt::entity child { registry.get<RelationshipComponent>(entity).FirstChild };
                
                while (child != entt::null)
                {
                    if (registry.all_of<ColliderComponent>(child) && registry.all_of<TransformComponent>(child))
                    {
                        auto& childCol { registry.get<ColliderComponent>(child) };
                        auto& childTrans { registry.get<TransformComponent>(child) };
                        
                        JPH::ShapeRefC childShape;

                        if (childCol.Type == ColliderType::Box)
                        {
                            childShape = JPH::BoxShapeSettings(ToJoltVec3(childCol.Size * childTrans.Scale)).Create().Get();
                        }
                        else if (childCol.Type == ColliderType::Sphere)
                        {
                            childShape = JPH::SphereShapeSettings(childCol.Size.x * childTrans.Scale.x).Create().Get();
                        }
                        else if (childCol.Type == ColliderType::Capsule)
                        {
                            childShape = JPH::CapsuleShapeSettings(childCol.Size.y, childCol.Size.x * childTrans.Scale.x).Create().Get();
                        }
                            
                        if (childShape)
                        {
                            childShapes.push_back({ childTrans.Translation + childCol.Offset, childShape });
                        }
                    }
                    
                    child = registry.get<RelationshipComponent>(child).NextSibling;
                }
            }

            if (!childShapes.empty())
            {
                JPH::StaticCompoundShapeSettings compoundSettings;
                
                for (const auto& cs : childShapes)
                {
                    compoundSettings.AddShape(ToJoltVec3(cs.first), JPH::Quat::sIdentity(), cs.second);
                }
                
                shape = compoundSettings.Create().Get();
            } 
            else if (registry.all_of<ColliderComponent>(entity))
            {
                auto& collider { registry.get<ColliderComponent>(entity) };
                
                if (collider.Type == ColliderType::Box)
                {
                    shape = JPH::BoxShapeSettings(ToJoltVec3(collider.Size * transform.Scale)).Create().Get();
                }
                else if (collider.Type == ColliderType::Sphere)
                {
                    shape = JPH::SphereShapeSettings(collider.Size.x * transform.Scale.x).Create().Get();
                }
                else if (collider.Type == ColliderType::Capsule)
                {
                    shape = JPH::CapsuleShapeSettings(collider.Size.y, collider.Size.x * transform.Scale.x).Create().Get();
                }

                if (shape && glm::length(collider.Offset) > 0.001f)
                {
                    JPH::StaticCompoundShapeSettings offsetSettings;
                    offsetSettings.AddShape(ToJoltVec3(collider.Offset), JPH::Quat::sIdentity(), shape);
                    shape = offsetSettings.Create().Get();
                }
            } 
            else
            {
                shape = JPH::BoxShapeSettings(ToJoltVec3(transform.Scale * 0.5f)).Create().Get();
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

            glm::vec3 worldPosition { transform.WorldMatrix[3] };
            glm::quat worldRotation { glm::normalize(glm::quat_cast(transform.WorldMatrix)) };
            
            JPH::BodyCreationSettings bodySettings(
                shape, 
                ToJoltVec3(worldPosition), 
                ToJoltQuat(worldRotation), 
                motionType, 
                layer
            );

            bodySettings.mRestitution = rb.Restitution;
            bodySettings.mFriction = rb.Friction;

            if (rb.Type == RigidBodyType::Dynamic && rb.Mass > 0.0f)
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
            if (rb.RuntimeBodyID != 0xFFFFFFFF)
            {
                JPH::BodyID bodyID(rb.RuntimeBodyID);
                
                bool isActive { !registry.all_of<DisabledComponent>(entity) };
                bool isAdded { bodyInterface.IsAdded(bodyID) };

                if (isActive && !isAdded)
                {
                    bodyInterface.AddBody(bodyID, JPH::EActivation::Activate);
                }
                else if (!isActive && isAdded)
                {
                    bodyInterface.RemoveBody(bodyID);
                }
            }
        }

        physicsContext.GetSystem().Update(timeStep, 1, physicsContext.GetTempAllocator(), physicsContext.GetJobSystem());

        auto view { registry.view<TransformComponent, RigidBodyComponent>() };

        for (auto [entity, transform, rb] : view.each())
        {
            if (registry.all_of<DisabledComponent>(entity)) { continue; }

            if (rb.Type != RigidBodyType::Static && rb.RuntimeBodyID != 0xFFFFFFFF)
            {
                JPH::BodyID bodyID(rb.RuntimeBodyID);
                
                if (bodyInterface.IsActive(bodyID))
                {
                    JPH::Vec3 position { bodyInterface.GetPosition(bodyID) };
                    JPH::Quat rotation { bodyInterface.GetRotation(bodyID) };

                    transform.Translation = ToGlmVec3(position);
                    transform.Rotation = glm::eulerAngles(ToGlmQuat(rotation)); 

                    registry.emplace_or_replace<DirtyTransform>(entity);
                }
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
                rbEntity = registry.get<RelationshipComponent>(rbEntity).Parent;
            else
                rbEntity = entt::null;
        }

        if (rbEntity == entt::null) { return; }

        auto& rb { registry.get<RigidBodyComponent>(rbEntity) };
        if (rb.RuntimeBodyID == 0xFFFFFFFF) { return; }

        auto& transform { registry.get<TransformComponent>(rbEntity) };
        JPH::BodyInterface& bodyInterface { physicsContext.GetBodyInterface() };
        JPH::BodyID bodyID(rb.RuntimeBodyID);

        glm::vec3 worldPosition { transform.Translation };
        glm::quat worldRotation { glm::normalize(glm::quat(transform.Rotation)) };

        bodyInterface.SetPositionAndRotation(
            bodyID,
            ToJoltVec3(worldPosition),
            ToJoltQuat(worldRotation),
            JPH::EActivation::Activate
        );
    }
}