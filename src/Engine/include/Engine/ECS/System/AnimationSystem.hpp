#pragma once

#include <Engine/ECS/World.hpp>

#include <string>


namespace Antelope
{
    struct AnimatorComponent;

    class AnimationSystem
    {
    public:
        static void Update(World& world, float dt);

        static void SetFloat(AnimatorComponent& animator, const std::string& name, float value);
        static void SetInt(AnimatorComponent& animator, const std::string& name, int value);
        static void SetBool(AnimatorComponent& animator, const std::string& name, bool value);
        static void SetTrigger(AnimatorComponent& animator, const std::string& name);
    };
}