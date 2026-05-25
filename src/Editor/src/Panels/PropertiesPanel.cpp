#include <Editor/Panels/PropertiesPanel.hpp>

#include <Engine/Core/Application.hpp>
#include <Engine/ECS/World.hpp>
#include <Engine/ECS/System/PhysicsSystem.hpp>
#include <Engine/Physics/PhysicsContext.hpp>
#include <Engine/ECS/AnimatorController.hpp>
#include <Engine/Renderer/Graphics/EditorCamera.hpp>
#include <Engine/Asset/AssetManager.hpp>

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

    void PropertiesPanel::OnUIRender(Entity entity, const EditorCamera& camera)
    {
        ImGui::Begin("Properties");

        if (entity)
        {
            DrawComponents(entity, camera);
        }

        ImGui::End();
    }

    void PropertiesPanel::DrawComponents(Entity entity, const EditorCamera& camera)
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
            {
                Application::Get().GetWorld()->MarkTransformDirty(entity);
                if (Application::Get().GetWorld()->IsSimulating())
                {
                    PhysicsSystem::SetBodyTransform(
                        *Application::Get().GetWorld(),
                        *Application::Get().GetWorld()->GetPhysicsContext(),
                        entity
                    );
                }
            }
        });

        DrawComponent<MeshComponent>("Mesh Renderer", entity, [this](auto& component)
        {
            ImGui::Text("Mesh ID: %d", component.Handle.MeshID);
            ImGui::Text("Polygons: %d", component.Handle.faceCount);
            this->DrawVec3Control("Offset", component.Offset);
            this->DrawVec3Control("Scale", component.Scale, 1.0f);
        });

        DrawComponent<MaterialComponent>("Material", entity, [](auto& component)
        {
            ImGui::Text("Texture Index: %d", component.MaterialIndex);
        });

        DrawComponent<RigidBodyComponent>("Rigid Body", entity, [](auto& component)
        {
            const char* bodyTypes[] { "Static", "Kinematic", "Dynamic" };
            int currentBodyType { static_cast<int>(component.Type) };

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
            const char* colliderTypes[] { "Box", "Sphere", "Capsule" };
            int currentColliderType { static_cast<int>(component.Type) };

            if (ImGui::Combo("Collider Type", &currentColliderType, colliderTypes, IM_ARRAYSIZE(colliderTypes)))
            {
                component.Type = static_cast<ColliderType>(currentColliderType);
            }

            this->DrawVec3Control("Offset", component.Offset);

            if (component.Type == ColliderType::Box)
            {
                this->DrawVec3Control("Size", component.Size, 1.0f);
            }
            else if (component.Type == ColliderType::Sphere)
            {
                ImGui::DragFloat("Radius", &component.Size.x, 0.05f, 0.0f, 1000.0f, "%.2f");
            }
            else if (component.Type == ColliderType::Capsule)
            {
                ImGui::DragFloat("Radius", &component.Size.x, 0.05f, 0.0f, 1000.0f, "%.2f");
                ImGui::DragFloat("Height", &component.Size.y, 0.05f, 0.0f, 1000.0f, "%.2f");
            }
        });

        DrawComponent<MeshColliderComponent>("Mesh Collider", entity, [](auto& component)
        {
            ImGui::Text("Vertices: %zu | Triangles: %zu", component.Vertices.size(), component.Indices.size() / 3);

            if (component.Vertices.empty())
            {
                ImGui::TextDisabled("Waiting for wiring — will build on next frame.");
            }
        });

        DrawComponent<AudioPlayerComponent>("Audio Player", entity, [](auto& component)
        {
            for (uint32_t i { 0 }; i < static_cast<uint32_t>(component.Clips.size()); ++i)
            {
                auto& clip { component.Clips[i] };
                ImGui::PushID(static_cast<int>(i));

                ImGui::Text("Clip %u", i);
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetFontSize());
                if (ImGui::SmallButton("x"))
                {
                    component.Clips.erase(component.Clips.begin() + i);
                    ImGui::PopID();
                    return;
                }

                std::string label { "None" };
                if ((uint64_t)clip.AudioAssetUUID != 0)
                {
                    const auto& meta { AssetManager::GetMetadata(clip.AudioAssetUUID) };
                    label = meta.IsValid() ? meta.FilePath.filename().string() : "Missing";
                }

                ImGui::SetNextItemWidth(-1.0f);
                ImGui::Button(label.c_str(), ImVec2(-1.0f, 0.0f));

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload { ImGui::AcceptDragDropPayload("ASSET_AUDIO") })
                    {
                        clip.AudioAssetUUID = *static_cast<const UUID*>(payload->Data);
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::SliderFloat("Volume", &clip.Volume, 0.0f, 1.0f, "%.2f");
                ImGui::Checkbox("Loop", &clip.Loop);
                ImGui::SameLine();
                ImGui::Checkbox("Play on Start", &clip.PlayOnStart);
                ImGui::Separator();

                ImGui::PopID();
            }

            if (ImGui::Button("+ Add Clip")) { component.Clips.emplace_back(); }
        });

        DrawComponent<CameraComponent>("Camera", entity, [&camera, entity](auto& component) mutable
        {
            float fovDeg { glm::degrees(component.PerspectiveFOV) };
            
            if (ImGui::DragFloat("FOV", &fovDeg, 0.5f, 1.0f, 179.0f, "%.1f deg"))
            {
                component.PerspectiveFOV = glm::radians(fovDeg);
            }
            
            ImGui::DragFloat("Near", &component.PerspectiveNear, 0.01f, 0.001f, 10.0f, "%.3f");
            ImGui::DragFloat("Far",  &component.PerspectiveFar,  1.0f,  10.0f,  10000.0f, "%.1f");
            int prio { static_cast<int>(component.prio) };
            
            if (ImGui::DragInt("Priority", &prio, 1, 0, 255))
            {
                component.prio = static_cast<uint32_t>(prio);
            }
            
            if (ImGui::Button("Align with View"))
            {
                auto& tc { entity.GetComponent<TransformComponent>() };
                tc.Translation = camera.GetPosition();
                tc.Rotation = glm::vec3(
                    glm::radians(camera.GetPitch()),
                    -glm::radians(camera.GetYaw()) - glm::radians(90.0f),
                    0.0f
                );
                Application::Get().GetWorld()->MarkTransformDirty(entity);
            }
        });

        DrawComponent<DirectionalLightComponent>("Directional Light", entity, [](auto& component)
        {
            ImGui::ColorEdit3("Color", glm::value_ptr(component.Color));
            ImGui::DragFloat("Intensity", &component.Intensity, 0.1f, 0.0f, 100.0f, "%.2f");
        });

        DrawComponent<PointLightComponent>("Point Light", entity, [](auto& component)
        {
            ImGui::ColorEdit3("Color", glm::value_ptr(component.Color));
            ImGui::DragFloat("Intensity", &component.Intensity, 0.1f, 0.0f, 500.0f, "%.2f");
            ImGui::DragFloat("Radius", &component.Radius,    0.5f, 0.0f, 500.0f, "%.2f");
        });

        DrawComponent<SpotLightComponent>("Spot Light", entity, [](auto& component)
        {
            ImGui::ColorEdit3("Color", glm::value_ptr(component.Color));
            ImGui::DragFloat("Intensity", &component.Intensity, 0.1f, 0.0f, 500.0f, "%.2f");
            ImGui::DragFloat("Radius", &component.Radius,    0.5f, 0.0f, 500.0f, "%.2f");

            float innerDeg { glm::degrees(glm::acos(component.InnerCutOff)) };
            float outerDeg { glm::degrees(glm::acos(component.OuterCutOff)) };

            if (ImGui::DragFloat("Inner Angle", &innerDeg, 0.5f, 1.0f, 89.0f, "%.1f deg"))
            {
                component.InnerCutOff = glm::cos(glm::radians(innerDeg));
            }

            if (ImGui::DragFloat("Outer Angle", &outerDeg, 0.5f, 1.0f, 89.0f, "%.1f deg"))
            {
                component.OuterCutOff = glm::cos(glm::radians(outerDeg));
            }
        });

        DrawComponent<AmbientComponent>("Ambient", entity, [](auto& component)
        {
            ImGui::ColorEdit3("Sky Day", glm::value_ptr(component.SkyColorDay));
            ImGui::ColorEdit3("Horizon Day", glm::value_ptr(component.HorizonColorDay));
            ImGui::ColorEdit3("Ground", glm::value_ptr(component.GroundColor));
            ImGui::Separator();
            ImGui::ColorEdit3("Sky Night", glm::value_ptr(component.SkyColorNight));
            ImGui::ColorEdit3("Horizon Night", glm::value_ptr(component.HorizonColorNight));
            ImGui::Separator();
            ImGui::DragFloat("Star Intensity", &component.StarIntensity, 0.01f, 0.0f,  5.0f, "%.2f");
            ImGui::DragFloat("Sun Intensity", &component.SunMaxIntensity, 0.1f, 0.0f, 20.0f, "%.2f");
            ImGui::DragFloat("Moon Intensity", &component.MoonMaxIntensity, 0.01f, 0.0f, 2.0f, "%.3f");
        });

        DrawComponent<TimeCycleComponent>("Time Cycle", entity, [](auto& component)
        {
            ImGui::DragFloat("Time of Day", &component.TimeOfDay, 0.05f, 0.0f, 24.0f, "%.2f h");
            ImGui::DragFloat("Time Scale", &component.TimeScale, 10.0f, 0.0f, 10000.0f, "%.0f");

            int day { static_cast<int>(component.CurrentDay) };

            if (ImGui::DragInt("Day", &day, 1, 0, 100000))
            {
                component.CurrentDay = static_cast<uint32_t>(day < 0 ? 0 : day);
            }

            ImGui::SameLine();

            if (ImGui::SmallButton("Reset"))
            {
                component.CurrentDay = 0;
                component.TimeOfDay  = 12.0f;
            }
        });

        DrawComponent<AnimatorComponent>("Animator", entity, [](auto& component)
        {
            ImGui::DragFloat("Speed", &component.Speed, 0.01f, 0.0f, 10.0f, "%.2f");

            const char* stateName { "None" };
            
            if (component.ActiveState != UINT32_MAX && component.ActiveState < component.Controller.States.size())
            {
                stateName = component.Controller.States[component.ActiveState].Name.c_str();
            }

            ImGui::LabelText("State", "%s", stateName);
            ImGui::LabelText("State Time", "%.2f", component.StateTime);

            if (!component.Controller.Parameters.empty())
            {
                ImGui::Separator();
                ImGui::TextDisabled("Parameters");

                for (uint32_t i { 0 }; i < component.Controller.Parameters.size(); ++i)
                {
                    auto& p { component.Controller.Parameters[i] };
                    ImGui::PushID(static_cast<int>(i));

                    switch (p.ParamType)
                    {
                        case AnimatorParameter::Type::Float:
                        {
                            if (i < component.FloatValues.size())
                            {
                                ImGui::DragFloat(p.Name.c_str(), &component.FloatValues[i], 0.01f);
                            }

                            break;
                        }
                        case AnimatorParameter::Type::Int:
                        {
                            if (i < component.IntValues.size())
                            {
                                int v { component.IntValues[i] };

                                if (ImGui::DragInt(p.Name.c_str(), &v)) { component.IntValues[i] = v; }
                            }

                            break;
                        }
                        case AnimatorParameter::Type::Bool:
                        {
                            if (i < component.BoolValues.size())
                            {
                                bool v { component.BoolValues[i] };

                                if (ImGui::Checkbox(p.Name.c_str(), &v)) { component.BoolValues[i] = v; }
                            }

                            break;
                        }
                        case AnimatorParameter::Type::Trigger:
                        {
                            if (ImGui::Button(p.Name.c_str()))
                            {
                                if (i < component.TriggerValues.size())
                                {
                                    component.TriggerValues[i] = true;
                                }
                            }

                            break;
                        }
                    }
                    ImGui::PopID();
                }
            }
        });

        DrawComponent<ScriptComponent>("Script", entity, [entity](auto& sc)
        {
            ImGui::LabelText("Class", "%s", sc.ScriptClassName.empty() ? "None" : sc.ScriptClassName.c_str());

            std::string dragLabel { sc.ScriptClassName.empty() ? "Drop .hpp to attach" : sc.ScriptClassName };
            ImGui::Button(dragLabel.c_str(), ImVec2(-1.0f, 0.0f));

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload { ImGui::AcceptDragDropPayload("ASSET_SCRIPT") })
                {
                    UUID uuid { *static_cast<const UUID*>(payload->Data) };
                    const auto& meta { AssetManager::GetMetadata(uuid) };
                    if (meta.IsValid() && meta.FilePath.extension() == ".hpp")
                    {
                        sc.ScriptClassName = meta.FilePath.stem().string();
                    }
                }
                ImGui::EndDragDropTarget();
            }
        });

        DrawCustomComponents(entity);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float buttonWidth { 160.0f };
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - buttonWidth) * 0.5f + ImGui::GetCursorPosX());

        if (ImGui::Button("Add Component", ImVec2(buttonWidth, 0.0f)))
        {
            ImGui::OpenPopup("AddComponentPopup");
        }

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            if (!entity.HasComponent<CameraComponent>())
            {
                if (ImGui::MenuItem("Camera"))
                {
                    entity.AddComponent<CameraComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!entity.HasComponent<DirectionalLightComponent>())
            {
                if (ImGui::MenuItem("Directional Light"))
                {
                    entity.AddComponent<DirectionalLightComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!entity.HasComponent<PointLightComponent>())
            {
                if (ImGui::MenuItem("Point Light"))
                {
                    entity.AddComponent<PointLightComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!entity.HasComponent<SpotLightComponent>())
            {
                if (ImGui::MenuItem("Spot Light"))
                {
                    entity.AddComponent<SpotLightComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!entity.HasComponent<AmbientComponent>())
            {
                if (ImGui::MenuItem("Ambient"))
                {
                    entity.AddComponent<AmbientComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!entity.HasComponent<AnimatorComponent>())
            {
                if (ImGui::MenuItem("Animator"))
                {
                    entity.AddComponent<AnimatorComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!entity.HasComponent<RigidBodyComponent>())
            {
                if (ImGui::MenuItem("Rigid Body"))
                {
                    entity.AddComponent<RigidBodyComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!entity.HasComponent<ColliderComponent>())
            {
                if (ImGui::MenuItem("Collider"))
                {
                    entity.AddComponent<ColliderComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!entity.HasComponent<MeshColliderComponent>())
            {
                if (ImGui::MenuItem("Mesh Collider"))
                {
                    entity.AddComponent<MeshColliderComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }

            if (!entity.HasComponent<AudioPlayerComponent>())
            {
                if (ImGui::MenuItem("Audio Player"))
                {
                    entity.AddComponent<AudioPlayerComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::Separator();

            if (!entity.HasComponent<ScriptComponent>())
            {
                if (ImGui::MenuItem("Script"))
                {
                    entity.AddComponent<ScriptComponent>();
                    ImGui::CloseCurrentPopup();
                }
            }

            for (const auto& name : ComponentRegistry::GetRegisteredNames())
            {
                if (ComponentRegistry::Has(entity.GetWorld()->GetRegistry(), entity.GetHandle(), name)) { continue; }
                if (ImGui::MenuItem(name.c_str()))
                {
                    ComponentRegistry::Add(entity.GetWorld()->GetRegistry(), entity.GetHandle(), name);
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndPopup();
        }
    }

    void PropertiesPanel::DrawCustomComponents(Entity entity)
    {
        const ImGuiTreeNodeFlags treeNodeFlags {
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
            ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap |
            ImGuiTreeNodeFlags_FramePadding };

        auto& registry { entity.GetWorld()->GetRegistry() };
        entt::entity handle { entity.GetHandle() };

        for (const auto& name : ComponentRegistry::GetRegisteredNames())
        {
            if (!ComponentRegistry::Has(registry, handle, name)) { continue; }

            const ComponentDescriptor* desc { ComponentRegistry::GetDescriptor(name) };
            if (!desc) { continue; }

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
            ImGui::Separator();

            float lineHeight { ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f };
            bool open { ImGui::TreeNodeEx((void*)std::hash<std::string>{}(name), treeNodeFlags, "%s", name.c_str()) };
            ImGui::PopStyleVar();
            ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - lineHeight);

            ImGui::PushID(name.c_str());

            if (ImGui::Button("x", ImVec2{ lineHeight, lineHeight }))
            {
                ComponentRegistry::Remove(registry, handle, name);
                if (open) { ImGui::TreePop(); }
                ImGui::PopID();
                continue;
            }

            if (open)
            {
                void* ptr { ComponentRegistry::Get(registry, handle, name) };
                if (ptr)
                {
                    for (const auto& field : desc->Fields)
                    {
                        DrawScriptField(ptr, field);
                    }
                }
                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }

    void PropertiesPanel::DrawScriptField(void* componentPtr, const ScriptFieldDescriptor& field)
    {
        char* base { static_cast<char*>(componentPtr) };

        ImGui::PushID(field.Name.c_str());

        switch (field.Type)
        {
            case ScriptFieldType::Float:
            {
                float* v { reinterpret_cast<float*>(base + field.Offset) };
                ImGui::DragFloat(field.Name.c_str(), v, 0.1f, 0.0f, 0.0f, "%.3f");
                break;
            }
            case ScriptFieldType::Int:
            {
                int* v { reinterpret_cast<int*>(base + field.Offset) };
                ImGui::DragInt(field.Name.c_str(), v);
                break;
            }
            case ScriptFieldType::Bool:
            {
                bool* v { reinterpret_cast<bool*>(base + field.Offset) };
                ImGui::Checkbox(field.Name.c_str(), v);
                break;
            }
            case ScriptFieldType::Vec3:
            {
                glm::vec3* v { reinterpret_cast<glm::vec3*>(base + field.Offset) };
                DrawVec3Control(field.Name, *v);
                break;
            }
            case ScriptFieldType::String:
            {
                std::string* v { reinterpret_cast<std::string*>(base + field.Offset) };
                char buf[256] {};
                std::strncpy(buf, v->c_str(), sizeof(buf) - 1);
                if (ImGui::InputText(field.Name.c_str(), buf, sizeof(buf))) { *v = buf; }
                break;
            }
            default: break;
        }

        ImGui::PopID();
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