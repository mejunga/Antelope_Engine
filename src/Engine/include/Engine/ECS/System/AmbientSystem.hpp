#pragma once

namespace Antelope
{
    class World;

    class AmbientSystem
    {
        public:
            static void OnUpdate(World& world, float timeStep);
    };
}