#include <Engine/ECS/System/TransformSystem.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/ECS/World.hpp>


namespace Antelope
{
    void TransformSystem::SortHierarchy(World& world)
    {
        auto& registry { world.GetRegistry() };

        registry.sort<TransformComponent>([&registry](const entt::entity lhs, const entt::entity rhs)
        {
            return registry.get<RelationshipComponent>(lhs).Depth < registry.get<RelationshipComponent>(rhs).Depth;
        });
    }

    void TransformSystem::OnUpdate(World& world)
    {
        auto& registry { world.GetRegistry() };
        
        auto dirtyView { registry.view<TransformComponent, DirtyTransform>() };
        for (auto [entity, transform] : dirtyView.each())
        {
            transform.RebuildLocal();
        }

        auto hierarchyView { registry.view<TransformComponent, RelationshipComponent>() };
        hierarchyView.use<TransformComponent>();

        for (auto [entity, transform, rel] : hierarchyView.each())
        {
            bool selfDirty { registry.all_of<DirtyTransform>(entity) };
            bool parentDirty { rel.Parent != entt::null && registry.all_of<DirtyTransform>(rel.Parent) };

            if (rel.Parent == entt::null)
            {
                if (selfDirty) 
                { 
                    transform.WorldMatrix = transform.LocalMatrix; 
                }
            }
            else
            {
                if (selfDirty || parentDirty)
                {
                    const auto& parentTransform { registry.get<TransformComponent>(rel.Parent) };
                    transform.WorldMatrix = parentTransform.WorldMatrix * transform.LocalMatrix;
                    registry.emplace_or_replace<DirtyTransform>(entity);
                }
            }
        }

        auto normalView { registry.view<TransformComponent, NormalMatrixComponent, DirtyTransform>() };
        for (auto [entity, transform, normalMat] : normalView.each())
        {
            normalMat.Matrix = glm::transpose(glm::inverse(glm::mat3(transform.WorldMatrix)));
        }

        registry.clear<DirtyTransform>();
    }
}