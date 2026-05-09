#pragma once

#include <Engine/ECS/World.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/Debug/Log.hpp>

#include <entt/entt.hpp>

#include <utility>
#include <stdexcept>


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
            inline World* GetWorld() const { return m_World; }
            
            void SetLocalPosition(const glm::vec3& pos)
            {
                GetComponent<TransformComponent>().Translation = pos;
                m_World->MarkTransformDirty(*this);
            }

            void SetLocalRotation(const glm::vec3& euler)
            {
                GetComponent<TransformComponent>().Rotation = euler;
                m_World->MarkTransformDirty(*this);
            }

            void SetLocalScale(const glm::vec3& scale)
            {
                GetComponent<TransformComponent>().Scale = scale;
                m_World->MarkTransformDirty(*this);
            }

            void SetLocalPositionAndRotation(const glm::vec3& pos, const glm::vec3& euler)
            {
                auto& t { GetComponent<TransformComponent>() };
                t.Translation = pos;
                t.Rotation = euler;
                m_World->MarkTransformDirty(*this);
            }

            void SetLocalTransform(const glm::vec3& pos, const glm::vec3& euler, const glm::vec3& scale)
            {
                auto& t { GetComponent<TransformComponent>() };
                t.Translation = pos;
                t.Rotation = euler;
                t.Scale = scale;
                m_World->MarkTransformDirty(*this);
            }

            const glm::vec3& GetLocalPosition() const { return GetComponent<TransformComponent>().Translation; }
            const glm::vec3& GetLocalRotation() const { return GetComponent<TransformComponent>().Rotation; }
            const glm::vec3& GetLocalScale() const { return GetComponent<TransformComponent>().Scale; }
            glm::vec3 GetWorldPosition() const { return glm::vec3(GetComponent<WorldMatrixComponent>().Matrix[3]); }

            template<typename T, typename... Args>
            T& AddComponent(Args&&... args)
            {
                if (m_EntityHandle == entt::null || !m_World)
                {
                    AE_ENGINE_ERROR("AddComponent called on a null entity!");
                    throw std::runtime_error("AddComponent called on a null entity!");
                }
                return m_World->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
            }

            template<typename T>
            T& GetComponent()
            {
                return m_World->m_Registry.get<T>(m_EntityHandle);
            }

            template<typename T>
            const T& GetComponent() const
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