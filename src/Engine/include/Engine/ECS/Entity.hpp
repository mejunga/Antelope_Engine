#pragma once

#include <Engine/ECS/World.hpp>
#include <Engine/ECS/BaseComponents.hpp>

#include <entt/entt.hpp>

#include <utility>


namespace Antelope
{
    class Entity
    {
        public:
            Entity() = default;
            Entity(entt::entity handle, World* world)
                : m_EntityHandle(handle), m_World(world) {}
            Entity(const Entity& other) = default;

            void SetParent(Entity parent);

            void SetActive(bool isActive)
            {
                if (isActive) { m_World->GetRegistry().remove<DisabledComponent>(m_EntityHandle); }
                else { m_World->GetRegistry().emplace_or_replace<DisabledComponent>(m_EntityHandle); }
            }

            inline bool IsActive() { return !m_World->GetRegistry().all_of<DisabledComponent>(m_EntityHandle); }
            inline entt::entity GetHandle() const { return m_EntityHandle; }

            template<typename T, typename... Args>
            T& AddComponent(Args&&... args)
            {
                return m_World->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
            }

            template<typename T>
            T& GetComponent()
            {
                return m_World->m_Registry.get<T>(m_EntityHandle);
            }

            template<typename T>
            bool HasComponent()
            {
                return m_World->m_Registry.all_of<T>(m_EntityHandle);
            }

            template<typename T>
            void RemoveComponent()
            {
                m_World->m_Registry.remove<T>(m_EntityHandle);
            }

            bool operator==(const Entity& other) const { return m_EntityHandle == other.m_EntityHandle && m_World == other.m_World; }
            bool operator!=(const Entity& other) const { return !(*this == other); }

            operator bool() const { return m_EntityHandle != entt::null; }
            operator entt::entity() const { return m_EntityHandle; }

        private:
            entt::entity m_EntityHandle { entt::null };
            World* m_World { nullptr };
    };
}