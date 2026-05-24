#pragma once

#include <entt/entt.hpp>
#include <cstdint>


namespace Antelope
{
    class World;
    class AudioContext;

    class AudioSystem
    {
        public:
            static void OnRuntimeStart(World& world, AudioContext& context);
            static void OnRuntimeStop(World& world, AudioContext& context);
            static void PlayClip(World& world, AudioContext& context, entt::entity entity, uint32_t clipIndex);
            static void StopClip(World& world, entt::entity entity, uint32_t clipIndex);
    };
}