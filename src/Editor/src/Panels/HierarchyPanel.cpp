#include <Editor/Panels/HierarchyPanel.hpp>
#include <Engine/ECS/BaseComponents.hpp>

#include <imgui.h>

#include <vector>


namespace Antelope::Editor
{
    static void DestroyRecursive(World* world, Entity entity)
    {
        if (!entity.HasComponent<RelationshipComponent>())
        {
            world->DestroyEntity(entity);
            return;
        }

        auto& rel { entity.GetComponent<RelationshipComponent>() };
        std::vector<entt::entity> children;
        entt::entity ch { rel.FirstChild };

        while (ch != entt::null)
        {
            Entity childEnt { ch, world };
            ch = childEnt.GetComponent<RelationshipComponent>().NextSibling;
            children.push_back(childEnt.GetHandle());
        }

        for (auto c : children) { DestroyRecursive(world, Entity { c, world }); }

        world->DestroyEntity(entity);
    }

    void HierarchyPanel::OnUIRender(World* world, Entity& selectedEntity)
    {
        ImGui::Begin("Hierarchy");

        m_SelectionChanged = (selectedEntity != m_LastSelectedEntity);
        if (m_SelectionChanged)
        {
            m_NodesToExpand.clear();
            if (selectedEntity)
            {
                Entity curr { selectedEntity };
                while (curr.HasComponent<RelationshipComponent>())
                {
                    entt::entity parentID { curr.GetComponent<RelationshipComponent>().Parent };
                    if (parentID != entt::null)
                    {
                        m_NodesToExpand.insert(parentID);
                        curr = { parentID, world };
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }

        auto view { world->GetRegistry().view<TagComponent>() };
        for (auto entityID : view)
        {
            Entity entity { entityID, world };
            bool isRoot { true };

            if (entity.HasComponent<RelationshipComponent>())
            {
                if (entity.GetComponent<RelationshipComponent>().Parent != entt::null)
                {
                    isRoot = false; 
                }
            }

            if (isRoot)
            {
                DrawEntityNode(entity, selectedEntity);
            }
        }

        if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
        {
            selectedEntity = {};
        }

        if (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete) && selectedEntity)
        {
            DestroyRecursive(world, selectedEntity);
            selectedEntity = {};
        }

        if (m_SelectionChanged)
        {
            m_NodesToExpand.clear();
            m_LastSelectedEntity = selectedEntity;
        }

        ImGui::End();
    }

    void HierarchyPanel::DrawEntityNode(Entity entity, Entity& selectedEntity)
    {
        auto& tag { entity.GetComponent<TagComponent>().Tag };
        
        ImGuiTreeNodeFlags flags { ((selectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow };
        flags |= ImGuiTreeNodeFlags_SpanAvailWidth; 

        bool hasChildren { false };
        if (entity.HasComponent<RelationshipComponent>())
        {
            hasChildren = (entity.GetComponent<RelationshipComponent>().FirstChild != entt::null);
        }

        if (!hasChildren) 
        {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        if (m_SelectionChanged && m_NodesToExpand.find(entity.GetHandle()) != m_NodesToExpand.end())
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        }

        bool opened { ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity.GetHandle(), flags, "%s", tag.c_str()) };
        
        if (ImGui::IsItemClicked())
        {
            selectedEntity = entity;
        }

        if (m_SelectionChanged && selectedEntity == entity)
        {
            ImGui::SetScrollHereY();
        }

        if (opened && hasChildren)
        {
            entt::entity childID { entity.GetComponent<RelationshipComponent>().FirstChild };
            
            while (childID != entt::null)
            {
                Entity child { childID, entity.GetWorld() };
                DrawEntityNode(child, selectedEntity);
                
                childID = child.GetComponent<RelationshipComponent>().NextSibling;
            }
            
            ImGui::TreePop();
        }
    }
}