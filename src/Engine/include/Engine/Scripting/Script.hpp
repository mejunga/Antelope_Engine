#pragma once

#include <Engine/ECS/Entity.hpp>
#include <Engine/Platform/Input.hpp>

#include <string>


namespace Antelope
{
    class ScriptSystem;

    class Script
    {
        public:
            virtual ~Script() = default;

            virtual void OnCreate() {}
            virtual void OnUpdate(float dt) {}
            virtual void OnDestroy() {}

            template<typename T>
            T& GetComponent() { return m_Entity.GetComponent<T>(); }

            template<typename T>
            bool HasComponent() { return m_Entity.HasComponent<T>(); }

            Entity GetEntity() const { return m_Entity; }
            World* GetWorld() const { return m_Entity.GetWorld(); }

            bool IsKeyDown(Key key);
            void FireAnimationTrigger(const std::string& triggerName);
            void PlayAudio(uint32_t clipIndex);
            void StopAudio(uint32_t clipIndex);

        private:
            Entity m_Entity;

            friend class ScriptSystem;
    };
}