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

        registry.sort<LocalMatrixComponent, TransformComponent>();
        registry.sort<WorldMatrixComponent, TransformComponent>();
    }

    void TransformSystem::OnUpdate(World& world)
    {
        auto& registry { world.GetRegistry() };

        auto dirtyView { registry.view<TransformComponent, LocalMatrixComponent, DirtyTransform>() };
        
        for (auto [entity, transform, local] : dirtyView.each())
        {
            local.Rebuild(transform);
        }

        auto hierarchyView { registry.view<LocalMatrixComponent, WorldMatrixComponent, RelationshipComponent>() };
        hierarchyView.use<LocalMatrixComponent>();

        for (auto [entity, local, world, rel] : hierarchyView.each())
        {
            bool selfDirty   { registry.all_of<DirtyTransform>(entity) };
            bool parentDirty { rel.Parent != entt::null && registry.all_of<DirtyTransform>(rel.Parent) };

            if (rel.Parent == entt::null)
            {
                if (selfDirty) { world.Matrix = local.Matrix; }
            }
            else
            {
                if (selfDirty || parentDirty)
                {
                    const auto& parentWorld { registry.get<WorldMatrixComponent>(rel.Parent) };
                    world.Matrix = parentWorld.Matrix * local.Matrix;
                    registry.emplace_or_replace<DirtyTransform>(entity);
                }
            }
        }

        auto normalView { registry.view<WorldMatrixComponent, NormalMatrixComponent, DirtyTransform>() };
        
        for (auto [entity, worldMat, normalMat] : normalView.each())
        {
            normalMat.Matrix = glm::transpose(glm::inverse(glm::mat3(worldMat.Matrix)));
        }

        registry.clear<DirtyTransform>();
    }
}