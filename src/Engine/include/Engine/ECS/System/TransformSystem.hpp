#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>


namespace Antelope
{
    class World;

    class TransformSystem
    {
        public:
            static void OnUpdate(World& world);
            static void SortHierarchy(World& world);
    };
}