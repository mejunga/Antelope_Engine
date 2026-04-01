#include <Editor/Core/AntelopeApp.hpp>

#include <Engine/AssetImport/ModelLoader.hpp>
#include <Engine/AssetImport/TextureManager.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/ECS/World.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/Platform/Input.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Renderer/UI/UIContext.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <GLFW/glfw3.h>


namespace Antelope::Editor
{
    AntelopeApp::AntelopeApp() 
        : m_EditorCamera(glm::vec3(0.0f, 2.0f, 20.0f))
    {
        AE_CLIENT_INFO("Antelope Editor instance created.");
    }

    AntelopeApp::~AntelopeApp()
    {
        AE_CLIENT_INFO("Antelope Editor instance destroyed.");
    }

    void AntelopeApp::SetupMockData()
    {
        m_BearTexID = GetTextureManager()->LoadTexture("Assets/Bear.png");
        m_BearMesh = ModelLoader::Load("Assets/Bear_DEMO.fbx");

        m_GorillaTexID = GetTextureManager()->LoadTexture("Assets/gorilla.png");
        m_GorillaMesh = ModelLoader::Load("Assets/Gorilla_hd.fbx");

        GetRenderer()->UpdateTextureDescriptors(GetTextureManager()->GetGlobalTextures());
    }

    void AntelopeApp::OnInit()
    {
        SetupMockData();

        m_BearRoot = GetWorld()->CreateEntity("BearRoot");

        for (const auto& subMesh : m_BearMesh.SubMeshes)
        {
            MeshHandle handle { GetRenderer()->UploadMesh(subMesh.Data) };
            handle.materialIndex = m_BearTexID;

            Entity part { GetWorld()->CreateEntity(subMesh.Name.empty() ? "BearPart" : subMesh.Name) };
            part.AddComponent<MeshComponent>(handle);
            part.GetComponent<TransformComponent>().Scale = glm::vec3(0.005f);
            part.SetParent(m_BearRoot);
        }

        m_GorillaRoot = GetWorld()->CreateEntity("GorillaRoot");
        m_GorillaRoot.SetParent(m_BearRoot); 
        m_GorillaRoot.GetComponent<TransformComponent>().Translation = glm::vec3(10.0f, 0.0f, 0.0f);
        GetWorld()->MarkTransformDirty(m_GorillaRoot);

        for (const auto& subMesh : m_GorillaMesh.SubMeshes)
        {
            MeshHandle handle { GetRenderer()->UploadMesh(subMesh.Data) };
            handle.materialIndex = m_GorillaTexID;

            Entity part { GetWorld()->CreateEntity(subMesh.Name.empty() ? "GorillaPart" : subMesh.Name) };
            part.AddComponent<MeshComponent>(handle);
            part.GetComponent<TransformComponent>().Scale = glm::vec3(7.5f);
            part.SetParent(m_GorillaRoot);
            m_GorillaParts.push_back(part);
        }
    }

    void AntelopeApp::OnUpdate(float timeStep)
    {
        if (m_DebounceTimer > 0.0f) { m_DebounceTimer -= timeStep; }

        if (Input::IsKeyPressed(GLFW_KEY_SPACE) && m_DebounceTimer <= 0.0f)
        {
            m_DebounceTimer = DEBOUNCE_DELAY;
            bool isActive { m_GorillaRoot.IsActive() };
            
            m_GorillaRoot.SetActive(!isActive); 
            
            for (auto& part : m_GorillaParts)
            {
                part.SetActive(!isActive);
            }
            
            AE_CLIENT_TRACE("Gorilla Active State: {0}", !isActive);
        }

        m_EditorCamera.OnUpdate(timeStep);
        GetWorld()->OnUpdateEditor(timeStep, m_EditorCamera);
    }

    void AntelopeApp::OnUIRender()
    {
        static ImGuiDockNodeFlags dockspace_flags { ImGuiDockNodeFlags_None };
        ImGuiWindowFlags window_flags { ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | 
                                        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | 
                                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | 
                                        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus };

        ImGuiViewport* viewport { ImGui::GetMainViewport() };
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        
        ImGui::Begin("Antelope Editor", nullptr, window_flags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspace_id { ImGui::GetID("AntelopeDockSpace") };
        
        static bool first_time { true };
        if (first_time)
        {
            first_time = false;
            ImGui::DockBuilderRemoveNode(dockspace_id); 
            ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

            auto dock_id_main { dockspace_id };
            auto dock_id_bottom { ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, 0.25f, nullptr, &dock_id_main) };
            auto dock_id_left { ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f, nullptr, &dock_id_main) };
            auto dock_id_right { ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.25f, nullptr, &dock_id_main) };

            ImGui::DockBuilderDockWindow("Hierarchy", dock_id_left);
            ImGui::DockBuilderDockWindow("Properties", dock_id_right);
            ImGui::DockBuilderDockWindow("Console", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Project", dock_id_bottom);
            ImGui::DockBuilderDockWindow("Scene", dock_id_main);

            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

        m_ScenePanel.OnUIRender(m_EditorCamera);
        m_HierarchyPanel.OnUIRender();
        m_PropertiesPanel.OnUIRender(m_ScenePanel.GetSelectedEntity());
        m_ConsolePanel.OnUIRender();
        m_ProjectPanel.OnUIRender();

        ImGui::End();
    }

    void AntelopeApp::OnShutdown()
    {
        auto& registry { GetWorld()->GetRegistry() };
        auto view { registry.view<MeshComponent>() };

        for (auto [entity, mesh] : view.each()) 
        {
            GetRenderer()->FreeMesh(mesh.Handle); 
        }

        AE_CLIENT_INFO("All meshes freed from GPU.");
    }
}