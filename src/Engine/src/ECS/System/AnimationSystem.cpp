#include <Engine/ECS/System/AnimationSystem.hpp>
#include <Engine/ECS/BaseComponents.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <unordered_map>
#include <string>


namespace Antelope
{

    static glm::mat4 InterpolatePosition(float time, const BoneAnimationNode& node)
    {
        if (node.Positions.size() == 1)
        {
            return glm::translate(glm::mat4(1.0f), node.Positions[0].position);
        }

        uint32_t i { 0 };

        for (; i < node.Positions.size() - 2; ++i)
        {
            if (time < node.Positions[i + 1].timeStamp) { break; }
        }

        float dt { node.Positions[i + 1].timeStamp - node.Positions[i].timeStamp };
        float t { (dt > 0.0f) ? (time - node.Positions[i].timeStamp) / dt : 0.0f };
        t = glm::clamp(t, 0.0f, 1.0f);
        return glm::translate(glm::mat4(1.0f), glm::mix(node.Positions[i].position, node.Positions[i + 1].position, t));
    }

    static glm::mat4 InterpolateRotation(float time, const BoneAnimationNode& node)
    {
        if (node.Rotations.size() == 1)
        {
            return glm::mat4_cast(glm::normalize(node.Rotations[0].orientation));
        }

        uint32_t i { 0 };

        for (; i < node.Rotations.size() - 2; ++i)
        {
            if (time < node.Rotations[i + 1].timeStamp) { break; }
        }

        float dt { node.Rotations[i + 1].timeStamp - node.Rotations[i].timeStamp };
        float t { (dt > 0.0f) ? (time - node.Rotations[i].timeStamp) / dt : 0.0f };
        t = glm::clamp(t, 0.0f, 1.0f);
        return glm::mat4_cast(glm::normalize(glm::slerp(node.Rotations[i].orientation, node.Rotations[i + 1].orientation, t)));
    }

    static glm::mat4 InterpolateScale(float time, const BoneAnimationNode& node)
    {
        if (node.Scales.size() == 1)
        {
            return glm::scale(glm::mat4(1.0f), node.Scales[0].scale);
        }

        uint32_t i { 0 };
        for (; i < node.Scales.size() - 2; ++i)
        {
            if (time < node.Scales[i + 1].timeStamp) { break; }
        }

        float dt { node.Scales[i + 1].timeStamp - node.Scales[i].timeStamp };
        float t { (dt > 0.0f) ? (time - node.Scales[i].timeStamp) / dt : 0.0f };
        t = glm::clamp(t, 0.0f, 1.0f);
        return glm::scale(glm::mat4(1.0f), glm::mix(node.Scales[i].scale, node.Scales[i + 1].scale, t));
    }

    static void CalculateBoneTransform(const ModelNode& node, glm::mat4 parentTransform, const AnimationClip& clip, float time,
                                       const std::unordered_map<std::string, BoneInfo>& boneMap, std::vector<glm::mat4>& out)
    {
        glm::mat4 nodeTransform { node.LocalTransform };

        for (const auto& channel : clip.Channels)
        {
            if (channel.NodeName != node.Name) { continue; }

            nodeTransform = InterpolatePosition(time, channel) * InterpolateRotation(time, channel) * InterpolateScale(time, channel);
            break;
        }

        glm::mat4 global { parentTransform * nodeTransform };

        auto it { boneMap.find(node.Name) };

        if (it != boneMap.end())
        {
            out[it->second.id] = global * it->second.offsetMatrix;
        }

        for (const auto& child : node.Children)
        {
            CalculateBoneTransform(child, global, clip, time, boneMap, out);
        }
    }

    static bool EvaluateCondition(const TransitionCondition& cond, const AnimatorComponent& anim)
    {
        if (cond.ParameterIndex >= anim.Controller.Parameters.size()) { return false; }

        const auto& param { anim.Controller.Parameters[cond.ParameterIndex] };

        switch (param.ParamType)
        {
            case AnimatorParameter::Type::Float:
            {
                float v { anim.FloatValues[cond.ParameterIndex] };

                if (cond.Operation == TransitionCondition::Op::Greater) { return v >  cond.Threshold; }
                if (cond.Operation == TransitionCondition::Op::Less) { return v <  cond.Threshold; }
                if (cond.Operation == TransitionCondition::Op::Equal) { return glm::abs(v - cond.Threshold) < 0.0001f; }
                if (cond.Operation == TransitionCondition::Op::NotEqual) { return glm::abs(v - cond.Threshold) >= 0.0001f; }
                
                return false;
            }
            case AnimatorParameter::Type::Int:
            {
                int v { anim.IntValues[cond.ParameterIndex] };
                int th { static_cast<int>(cond.Threshold) };

                if (cond.Operation == TransitionCondition::Op::Greater) { return v >  th; }
                if (cond.Operation == TransitionCondition::Op::Less) { return v <  th; }
                if (cond.Operation == TransitionCondition::Op::Equal) { return v == th; }
                if (cond.Operation == TransitionCondition::Op::NotEqual) { return v != th; }

                return false;
            }
            case AnimatorParameter::Type::Bool:
            {
                bool v { anim.BoolValues[cond.ParameterIndex] };

                if (cond.Operation == TransitionCondition::Op::True) { return  v; }
                if (cond.Operation == TransitionCondition::Op::False) { return !v; }

                return false;
            }
            case AnimatorParameter::Type::Trigger:
            {
                return anim.TriggerValues[cond.ParameterIndex];
            }
        }

        return false;
    }

    static bool EvaluateTransition(const AnimationTransition& t, const AnimatorComponent& anim)
    {
        for (const auto& cond : t.Conditions)
        {
            if (!EvaluateCondition(cond, anim)) { return false; }
        }

        return true;
    }

    static void ConsumeTriggers(const AnimationTransition& t, AnimatorComponent& anim)
    {
        for (const auto& cond : t.Conditions)
        {
            if (cond.ParameterIndex >= anim.Controller.Parameters.size()) { continue; }
            if (anim.Controller.Parameters[cond.ParameterIndex].ParamType == AnimatorParameter::Type::Trigger)
            {
                anim.TriggerValues[cond.ParameterIndex] = false;
            }
        }
    }

    void AnimationSystem::SetFloat(AnimatorComponent& anim, const std::string& name, float value)
    {
        for (uint32_t i { 0 }; i < anim.Controller.Parameters.size(); ++i)
        {
            if (anim.Controller.Parameters[i].Name == name && i < anim.FloatValues.size())
            {
                anim.FloatValues[i] = value;
                return;
            }
        }
    }

    void AnimationSystem::SetInt(AnimatorComponent& anim, const std::string& name, int value)
    {
        for (uint32_t i { 0 }; i < anim.Controller.Parameters.size(); ++i)
        {
            if (anim.Controller.Parameters[i].Name == name && i < anim.IntValues.size())
            {
                anim.IntValues[i] = value;
                return;
            }
        }
    }

    void AnimationSystem::SetBool(AnimatorComponent& anim, const std::string& name, bool value)
    {
        for (uint32_t i { 0 }; i < anim.Controller.Parameters.size(); ++i)
        {
            if (anim.Controller.Parameters[i].Name == name && i < anim.BoolValues.size())
            {
                anim.BoolValues[i] = value;
                return;
            }
        }

    }

    void AnimationSystem::SetTrigger(AnimatorComponent& anim, const std::string& name)
    {
        for (uint32_t i { 0 }; i < anim.Controller.Parameters.size(); ++i)
        {
            if (anim.Controller.Parameters[i].Name == name && i < anim.TriggerValues.size())
            {
                anim.TriggerValues[i] = true;
                return;
            }
        }
    }

    void AnimationSystem::Update(World& world, float dt)
    {
        auto view { world.GetRegistry().view<AnimatorComponent>() };

        for (auto entity : view)
        {
            auto& anim { view.get<AnimatorComponent>(entity) };

            if (!anim.Model || anim.Controller.States.empty() || anim.Controller.Clips.empty()) { continue; }

            const uint32_t paramCount { static_cast<uint32_t>(anim.Controller.Parameters.size()) };

            if (anim.FloatValues.size() != paramCount) { anim.FloatValues.assign(paramCount, 0.0f); }
            if (anim.IntValues.size() != paramCount) { anim.IntValues.assign(paramCount, 0); }
            if (anim.BoolValues.size() != paramCount) { anim.BoolValues.assign(paramCount, false); }
            if (anim.TriggerValues.size() != paramCount) { anim.TriggerValues.assign(paramCount, false); }

            if (anim.ActiveState == UINT32_MAX)
            {
                anim.ActiveState = anim.Controller.DefaultStateIndex;
                anim.StateTime = 0.0f;
                anim.BlendingFrom = UINT32_MAX;
                anim.FinalBoneMatrices.assign(anim.Model->BoneCount, glm::mat4(1.0f));
            }

            if (anim.ActiveState >= anim.Controller.States.size()) { continue; }
            const auto& activeState { anim.Controller.States[anim.ActiveState] };
            if (activeState.ClipIndex >= anim.Controller.Clips.size()) { continue; }
            const auto& activeClip { anim.Controller.Clips[activeState.ClipIndex] };
            if (activeClip.Channels.empty()) { continue; }

            float effectiveDt { dt * anim.Speed * activeState.Speed };
            anim.StateTime += activeClip.TicksPerSecond * effectiveDt;

            if (activeState.Loop)
            {
                anim.StateTime = std::fmod(anim.StateTime, activeClip.Duration);
            }
            else
            {
                anim.StateTime = std::min(anim.StateTime, activeClip.Duration);
            }

            if (anim.BlendingFrom != UINT32_MAX)
            {
                const auto& fromState { anim.Controller.States[anim.BlendingFrom] };
                const auto& fromClip { anim.Controller.Clips[fromState.ClipIndex] };
                anim.BlendFromTime += fromClip.TicksPerSecond * effectiveDt;

                if (fromState.Loop)
                {
                    anim.BlendFromTime = std::fmod(anim.BlendFromTime, fromClip.Duration);
                }

                anim.BlendTimer += dt;

                if (anim.BlendTimer >= anim.BlendDuration)
                {
                    anim.BlendingFrom = UINT32_MAX;
                    anim.BlendTimer = 0.0f;
                    anim.BlendDuration = 0.0f;
                }
            }

            if (anim.BlendingFrom == UINT32_MAX)
            {
                for (const auto& transition : anim.Controller.Transitions)
                {
                    if (transition.FromState != anim.ActiveState) { continue; }

                    if (transition.HasExitTime)
                    {
                        float normalizedTime { (activeClip.Duration > 0.0f) ? anim.StateTime / activeClip.Duration : 1.0f };
                        
                        if (normalizedTime < transition.ExitTime) { continue; }
                    }

                    if (!EvaluateTransition(transition, anim)) { continue; }

                    ConsumeTriggers(transition, anim);

                    anim.BlendingFrom = anim.ActiveState;
                    anim.BlendFromTime = anim.StateTime;
                    anim.ActiveState = transition.ToState;
                    anim.StateTime = 0.0f;
                    anim.BlendDuration = transition.BlendDuration;
                    anim.BlendTimer = 0.0f;
                    break;
                }
            }

            if (anim.FinalBoneMatrices.size() != anim.Model->BoneCount)
            {
                anim.FinalBoneMatrices.assign(anim.Model->BoneCount, glm::mat4(1.0f));
            }

            if (anim.BlendingFrom != UINT32_MAX)
            {
                const auto& fromState { anim.Controller.States[anim.BlendingFrom] };
                const auto& fromClip { anim.Controller.Clips[fromState.ClipIndex] };

                std::vector<glm::mat4> fromMatrices(anim.Model->BoneCount, glm::mat4(1.0f));
                std::vector<glm::mat4> toMatrices(anim.Model->BoneCount, glm::mat4(1.0f));

                CalculateBoneTransform(anim.Model->RootNode, glm::mat4(1.0f), fromClip, anim.BlendFromTime, anim.Model->BoneMapping, fromMatrices);
                CalculateBoneTransform(anim.Model->RootNode, glm::mat4(1.0f), activeClip, anim.StateTime,    anim.Model->BoneMapping, toMatrices);

                float blendFactor { (anim.BlendDuration > 0.0f) ? anim.BlendTimer / anim.BlendDuration : 1.0f };
                
                for (uint32_t i { 0 }; i < anim.Model->BoneCount; ++i)
                {
                    anim.FinalBoneMatrices[i] = fromMatrices[i] * (1.0f - blendFactor) + toMatrices[i] * blendFactor;
                }
            }
            else
            {
                CalculateBoneTransform(anim.Model->RootNode, glm::mat4(1.0f), activeClip, anim.StateTime, anim.Model->BoneMapping, anim.FinalBoneMatrices);
            }
        }
    }
}