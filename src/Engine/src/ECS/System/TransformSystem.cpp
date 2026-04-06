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
        auto view { registry.view<TransformComponent, RelationshipComponent>() };
        view.use<TransformComponent>();

        for (auto [entity, transform, rel] : view.each())
        {
            if (registry.all_of<DirtyTransform>(entity))
            {
                transform.RebuildLocal();
            }
                
            if (rel.Parent == entt::null)
            {
                transform.WorldMatrix = transform.LocalMatrix;
            }
            else
            {
                const auto& parentTransform { registry.get<TransformComponent>(rel.Parent) };
                transform.WorldMatrix = parentTransform.WorldMatrix * transform.LocalMatrix;
            }
        }

        auto normalView { registry.view<TransformComponent, NormalMatrixComponent>() };
        
        for (auto [entity, transform, normalMat] : normalView.each())
        {
            normalMat.Matrix = glm::transpose(glm::inverse(glm::mat3(transform.WorldMatrix)));
        }

        registry.clear<DirtyTransform>();
    }
}