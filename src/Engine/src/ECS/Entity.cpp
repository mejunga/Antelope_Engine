#include <Engine/ECS/Entity.hpp>
#include <Engine/ECS/BaseComponents.hpp>

namespace Antelope
{
    void Entity::SetParent(Entity parentEntity)
    {
        auto& myRel { GetComponent<RelationshipComponent>() };

        if (myRel.Parent != entt::null)
        {
            auto& oldParentRel { m_World->GetRegistry().get<RelationshipComponent>(myRel.Parent) };
            
            if (oldParentRel.FirstChild == m_EntityHandle)
                oldParentRel.FirstChild = myRel.NextSibling;

            if (myRel.PreviousSibling != entt::null)
            {
                auto& prevRel { m_World->GetRegistry().get<RelationshipComponent>(myRel.PreviousSibling) };
                prevRel.NextSibling = myRel.NextSibling;
            }
            if (myRel.NextSibling != entt::null)
            {
                auto& nextRel { m_World->GetRegistry().get<RelationshipComponent>(myRel.NextSibling) };
                nextRel.PreviousSibling = myRel.PreviousSibling;
            }
        }

        myRel.Parent = parentEntity ? parentEntity.m_EntityHandle : entt::null;
        myRel.PreviousSibling = entt::null;
        myRel.NextSibling = entt::null;

        if (parentEntity)
        {
            auto& newParentRel { parentEntity.GetComponent<RelationshipComponent>() };
            
            if (newParentRel.FirstChild != entt::null)
            {
                myRel.NextSibling = newParentRel.FirstChild;
                auto& oldFirstChildRel { m_World->GetRegistry().get<RelationshipComponent>(newParentRel.FirstChild) };
                oldFirstChildRel.PreviousSibling = m_EntityHandle;
            }
            
            newParentRel.FirstChild = m_EntityHandle;
        }
    }
}