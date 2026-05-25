#pragma once

namespace Antelope
{
    class World;

    class GameSystem
    {
        public:
            virtual ~GameSystem() = default;

            virtual void OnStart(World& world) {}
            virtual void OnUpdate(World& world, float dt) = 0;
            virtual void OnStop(World& world) {}
    };
}