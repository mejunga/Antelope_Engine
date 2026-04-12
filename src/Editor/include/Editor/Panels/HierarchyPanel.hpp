#pragma once

#include <Engine/ECS/World.hpp>
#include <Engine/ECS/Entity.hpp>

#include <unordered_set>


namespace Antelope::Editor
{
    class HierarchyPanel
    {
        public:
            HierarchyPanel() = default;

            void OnUIRender(World* world, Entity& selectedEntity);

        private:
            void DrawEntityNode(Entity entity, Entity& selectedEntity);

        private:
            Entity m_LastSelectedEntity;
            std::unordered_set<entt::entity> m_NodesToExpand;
            bool m_SelectionChanged { false };
    };
}