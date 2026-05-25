#include <Engine/Scripting/Script.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/ECS/System/AudioSystem.hpp>
#include <Engine/Audio/AudioContext.hpp>
#include <Engine/Platform/Input.hpp>


namespace Antelope
{
    bool Script::IsKeyDown(Key key)
    {
        return Input::IsKeyPressed(static_cast<int>(key));
    }

    void Script::FireAnimationTrigger(const std::string& triggerName)
    {
        if (!HasComponent<AnimatorComponent>()) { return; }

        auto& anim { GetComponent<AnimatorComponent>() };

        for (uint32_t i { 0 }; i < static_cast<uint32_t>(anim.Controller.Parameters.size()); ++i)
        {
            if (anim.Controller.Parameters[i].Name == triggerName && i < static_cast<uint32_t>(anim.TriggerValues.size()))
            {
                anim.TriggerValues[i] = true;
                return;
            }
        }
    }

    void Script::PlayAudio(uint32_t clipIndex)
    {
        auto* context { m_Entity.GetWorld()->GetAudioContext() };

        if (!context) { return; }
        
        AudioSystem::PlayClip(*m_Entity.GetWorld(), *context, m_Entity, clipIndex);
    }

    void Script::StopAudio(uint32_t clipIndex)
    {
        AudioSystem::StopClip(*m_Entity.GetWorld(), m_Entity, clipIndex);
    }
}