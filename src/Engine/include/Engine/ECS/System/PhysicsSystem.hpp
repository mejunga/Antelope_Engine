#pragma once

#include <entt/entt.hpp>


namespace Antelope
{
    class World;
    class PhysicsContext;

    class PhysicsSystem
    {
        public:
            static void OnRuntimeStart(World& world, PhysicsContext& physicsContext);
            static void OnUpdate(World& world, PhysicsContext& physicsContext, float timeStep);
            static void OnRuntimeStop(World& world, PhysicsContext& physicsContext);
            static void SetBodyTransform(World& world, PhysicsContext& physicsContext, entt::entity entity);
    };
}