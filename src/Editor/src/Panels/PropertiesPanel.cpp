#include <Editor/Panels/PropertiesPanel.hpp>

#include <Engine/Core/Application.hpp>
#include <Engine/ECS/World.hpp>

#include <glm/gtc/type_ptr.hpp>


namespace Antelope::Editor
{
    static void SetEntityActiveRecursive(Entity entity, bool active)
    {
        entity.SetActive(active);
        
        if (entity.HasComponent<RelationshipComponent>())
        {
            auto& rel { entity.GetComponent<RelationshipComponent>() };
            entt::entity childID { rel.FirstChild };
            
            while (childID != entt::null)
            {
                Entity child { childID, Application::Get().GetWorld().get() };
                SetEntityActiveRecursive(child, active);
                childID = child.GetComponent<RelationshipComponent>().NextSibling;
            }
        }
    }

    void PropertiesPanel::OnUIRender(Entity entity)
    {
        ImGui::Begin("Properties");

        if (entity)
        {
            DrawComponents(entity);
        }

        ImGui::End();
    }

    void PropertiesPanel::DrawComponents(Entity entity)
    {
        if (entity.HasComponent<TagComponent>())
        {
            auto& tag { entity.GetComponent<TagComponent>().Tag };

            bool isActive { entity.IsActive() };
            if (ImGui::Checkbox("##Active", &isActive))
            {
                SetEntityActiveRecursive(entity, isActive);
            }

            ImGui::SameLine();

            char buffer[256];
            memset(buffer, 0, sizeof(buffer));
            std::strncpy(buffer, tag.c_str(), sizeof(buffer));

            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
            {
                tag = std::string(buffer);
            }
            ImGui::PopItemWidth();
        }

        DrawComponent<TransformComponent>("Transform", entity, [this, entity](auto& component)
        {
            bool changed { false };
            changed |= this->DrawVec3Control("Translation", component.Translation);

            glm::vec3 rotation { glm::degrees(component.Rotation) };
            bool rotChanged { this->DrawVec3Control("Rotation", rotation) };
            if (rotChanged) { component.Rotation = glm::radians(rotation); changed = true; }

            changed |= this->DrawVec3Control("Scale", component.Scale, 1.0f);

            if (changed)
                Application::Get().GetWorld()->MarkTransformDirty(entity);
        });

        DrawComponent<MeshComponent>("Mesh Renderer", entity, [](auto& component)
        {
            ImGui::Text("Mesh ID: %d", component.Handle.MeshID);
            ImGui::Text("Polygons: %d", component.Handle.faceCount);
        });

        DrawComponent<MaterialComponent>("Material", entity, [](auto& component)
        {
            ImGui::Text("Texture Index: %d", component.MaterialIndex);
        });

        DrawComponent<RigidBodyComponent>("Rigid Body", entity, [](auto& component)
        {
            const char* bodyTypes[] = { "Static", "Dynamic", "Kinematic" };
            int currentBodyType = static_cast<int>(component.Type);
            
            if (ImGui::Combo("Body Type", &currentBodyType, bodyTypes, IM_ARRAYSIZE(bodyTypes)))
            {
                component.Type = static_cast<RigidBodyType>(currentBodyType);
            }

            if (component.Type == RigidBodyType::Dynamic)
            {
                ImGui::DragFloat("Mass", &component.Mass, 0.1f, 0.0f, 10000.0f, "%.2f");
            }
        });

        DrawComponent<ColliderComponent>("Collider", entity, [this](auto& component)
        {
            const char* colliderTypes[] = { "Box", "Sphere", "Capsule" };
            int currentColliderType = static_cast<int>(component.Type);
            
            if (ImGui::Combo("Collider Type", &currentColliderType, colliderTypes, IM_ARRAYSIZE(colliderTypes)))
            {
                component.Type = static_cast<ColliderType>(currentColliderType);
            }

            if (component.Type == ColliderType::Box)
            {
                this->DrawVec3Control("Size", component.Size, 1.0f);
            }
        });

        DrawComponent<DirectionalLightComponent>("Directional Light", entity, [](auto& component)
        {
            ImGui::ColorEdit3("Color", glm::value_ptr(component.Color));
            ImGui::DragFloat("Intensity", &component.Intensity, 0.1f, 0.0f, 100.0f, "%.2f");
        });
    }

    bool PropertiesPanel::DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue, float columnWidth)
    {
        bool changed { false };
        ImGuiIO& io { ImGui::GetIO() };
        auto boldFont { io.Fonts->Fonts[1] };

        ImGui::PushID(label.c_str());

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, columnWidth);
        ImGui::Text("%s", label.c_str());
        ImGui::NextColumn();

        ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 4 });

        float lineHeight { ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f };
        ImVec2 buttonSize { lineHeight + 3.0f, lineHeight };

        ImGui::PushFont(boldFont);
        if (ImGui::Button("X", buttonSize)) { values.x = resetValue; changed = true; }
        ImGui::PopFont();
        ImGui::SameLine();
        if (ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f")) { changed = true; }
        ImGui::PopItemWidth();
        ImGui::SameLine();

        ImGui::PushFont(boldFont);
        if (ImGui::Button("Y", buttonSize)) { values.y = resetValue; changed = true; }
        ImGui::PopFont();
        ImGui::SameLine();
        if (ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f")) { changed = true; }
        ImGui::PopItemWidth();
        ImGui::SameLine();

        ImGui::PushFont(boldFont);
        if (ImGui::Button("Z", buttonSize)) { values.z = resetValue; changed = true; }
        ImGui::PopFont();
        ImGui::SameLine();
        if (ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f")) { changed = true; }
        ImGui::PopItemWidth();

        ImGui::PopStyleVar();
        ImGui::Columns(1);
        ImGui::PopID();
        return changed;
    }
}