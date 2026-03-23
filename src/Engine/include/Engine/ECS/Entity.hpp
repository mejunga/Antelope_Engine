#pragma once

#include <Engine/ECS/World.hpp>

#include <entt/entt.hpp>

#include <utility>


namespace Antelope {

    class Entity {
        public:
            Entity() = default;
            Entity(entt::entity handle, World* world)
                : m_EntityHandle(handle), m_World(world) {}
            Entity(const Entity& other) = default;

            template<typename T, typename... Args>
            T& AddComponent(Args&&... args) {
                return m_World->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
            }

            template<typename T>
            T& GetComponent() {
                return m_World->m_Registry.get<T>(m_EntityHandle);
            }

            template<typename T>
            bool HasComponent() {
                return m_World->m_Registry.all_of<T>(m_EntityHandle);
            }

            template<typename T>
            void RemoveComponent() {
                m_World->m_Registry.remove<T>(m_EntityHandle);
            }

            operator bool() const { return m_EntityHandle != entt::null; }
            operator entt::entity() const { return m_EntityHandle; }

        private:
            entt::entity m_EntityHandle{ entt::null };
            World* m_World = nullptr;
    };
}