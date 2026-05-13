#pragma once

#include <Engine/Renderer/Graphics/Animation.hpp>

#include <glm/glm.hpp>

#include <string>
#include <vector>
#include <cstdint>


namespace Antelope
{
    struct AnimatorParameter
    {
        enum class Type { Float, Int, Bool, Trigger };
        std::string Name;
        Type ParamType { Type::Float };
        float FloatValue { 0.0f };
        int IntValue { 0 };
        bool BoolValue { false };
        bool TriggerFired { false };
    };

    struct TransitionCondition
    {
        enum class Op { Greater, Less, Equal, NotEqual, True, False };
        uint32_t ParameterIndex { 0 };
        Op Operation { Op::True };
        float Threshold { 0.0f };
    };

    struct AnimationTransition
    {
        static constexpr uint32_t ENTRY = UINT32_MAX;
        uint32_t FromState { ENTRY };
        uint32_t ToState { 0 };
        std::vector<TransitionCondition> Conditions;
        float BlendDuration { 0.15f };
        bool HasExitTime { false };
        float ExitTime { 1.0f };
    };

    struct AnimationStateNode
    {
        std::string Name;
        uint32_t ClipIndex { 0 };
        float Speed { 1.0f };
        bool Loop { true };
        glm::vec2 EditorPos { 0.0f, 0.0f };
    };

    struct AnimatorController
    {
        std::vector<AnimationClip> Clips;
        std::vector<AnimationStateNode> States;
        std::vector<AnimationTransition> Transitions;
        std::vector<AnimatorParameter> Parameters;
        uint32_t DefaultStateIndex { 0 };
    };
}