#include <Engine/ECS/Entity.hpp>
#include <Engine/ECS/BaseComponents.hpp>


namespace Antelope
{
    void Entity::SetParent(Entity parent)
    {
        auto& relation { GetComponent<RelationshipComponent>() };
        relation.Parent = parent.m_EntityHandle;

        if (parent)
        {
            auto& parentRel { parent.GetComponent<RelationshipComponent>() };
            relation.Depth = parentRel.Depth + 1;

            if (parentRel.FirstChild == entt::null)
            {
                parentRel.FirstChild = m_EntityHandle;
            }
            else
            {
                entt::entity current = parentRel.FirstChild;
                while (m_World->GetRegistry().get<RelationshipComponent>(current).NextSibling != entt::null)
                    current = m_World->GetRegistry().get<RelationshipComponent>(current).NextSibling;
                m_World->GetRegistry().get<RelationshipComponent>(current).NextSibling = m_EntityHandle;
                relation.PreviousSibling = current;
            }
        }

        m_World->MarkTransformDirty(*this);
        m_World->MarkHierarchyDirty();
    }
}