#pragma once

#include <Engine/ECS/World.hpp>

#include <entt/entt.hpp>
#include <glm/glm.hpp>


namespace Antelope
{
    class TransformSystem
    {
        public:
            static void OnUpdate(World& world);

        private:
            static void UpdateNodeCascade(entt::registry& registry, entt::entity entity, const glm::mat4& parentMatrix);
    };
}