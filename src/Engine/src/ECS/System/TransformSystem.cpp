#include <Engine/ECS/System/TransformSystem.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/ECS/World.hpp>


namespace Antelope
{
    void TransformSystem::OnUpdate(World& world)
    {
        auto& registry { world.GetRegistry() };
        auto view { registry.view<TransformComponent, RelationshipComponent, DirtyTransform>() };

        for (auto [entity, transform, rel] : view.each())
        {
            bool isRootDirty { (rel.Parent == entt::null) || !registry.all_of<DirtyTransform>(rel.Parent) };

            if (isRootDirty)
            {
                glm::mat4 parentMat { 1.0f };

                if (rel.Parent != entt::null) { parentMat = registry.get<TransformComponent>(rel.Parent).WorldMatrix; }

                UpdateNodeCascade(registry, entity, parentMat);
            }
        }

        registry.clear<DirtyTransform>(); 
    }

    void TransformSystem::UpdateNodeCascade(entt::registry& registry, entt::entity entity, const glm::mat4& parentMatrix)
    {
        auto& transform { registry.get<TransformComponent>(entity) };
        auto& rel { registry.get<RelationshipComponent>(entity) };

        transform.WorldMatrix = parentMatrix * transform.GetLocalTransform();
        transform.NormalMatrix = glm::transpose(glm::inverse(transform.WorldMatrix));

        entt::entity child { rel.FirstChild };
        while (child != entt::null)
        {
            UpdateNodeCascade(registry, child, transform.WorldMatrix);
            child = registry.get<RelationshipComponent>(child).NextSibling;
        }
    }
}