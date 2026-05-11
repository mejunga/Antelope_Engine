#include <Editor/Core/AntelopeApp.hpp>

#include <Engine/Asset/ModelLoader.hpp>
#include <Engine/Asset/TextureManager.hpp>
#include <Engine/Asset/AssetManager.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/ECS/World.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/Platform/Input.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Core/FileSystem.hpp>
#include <Engine/Renderer/Graphics/Material.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <unordered_map>
#include <algorithm> 


namespace Antelope::Editor
{
    AntelopeApp::AntelopeApp(const std::string& projectRoot)
        : m_ProjectRoot(std::filesystem::absolute(projectRoot))
        , m_EditorCamera(glm::vec3(0.0f, 2.0f, 20.0f))
    {
        AE_CLIENT_INFO("Antelope Editor instance created. Project root: '{0}'", m_ProjectRoot.string());
    }

    AntelopeApp::~AntelopeApp()
    {
        AE_CLIENT_INFO("Antelope Editor instance destroyed.");
    }

    void AntelopeApp::OnInit()
    {
        FileSystem::Mount("engine", "Assets");

        std::filesystem::path assetsDir { m_ProjectRoot / "Assets" };
        FileSystem::Mount("assets", assetsDir);

        m_ProjectFilePath = Project::FindProjectFile(m_ProjectRoot);

        if (m_ProjectFilePath.empty())
        {
            AE_CLIENT_WARN("No .antelopeproject found in '{0}'. Using defaults.", m_ProjectRoot.string());
            m_ProjectFilePath = m_ProjectRoot / (m_ProjectRoot.filename().string() + ".antelopeproject");
            m_ProjectState = ProjectState {};
            m_ProjectState.ProjectName = m_ProjectRoot.filename().string();
        }
        else
        {
            Project::Load(m_ProjectFilePath, m_ProjectState);
            AE_CLIENT_INFO("Loaded project: '{0}'", m_ProjectState.ProjectName);
        }

        AssetManager::LoadAssetRegistry(assetsDir, m_ProjectState.LastKnownAssets);
        GetFileWatcher().BuildFrom(assetsDir, AssetManager::GetRegistry());

        m_EditorCamera.SetState(
            m_ProjectState.CameraPosition,
            m_ProjectState.CameraYaw,
            m_ProjectState.CameraPitch
        );

        if (!m_ProjectState.LastScene.empty())
        {
            std::filesystem::path scenePath { FileSystem::Resolve(m_ProjectState.LastScene) };

            if (std::filesystem::exists(scenePath))
            {
                LoadScene(m_ProjectState.LastScene);
                return;
            }

            AE_CLIENT_WARN("Last scene not found: '{0}'. Searching for another.", m_ProjectState.LastScene);
        }

        for (auto& [uuid, meta] : AssetManager::GetRegistry())
        {
            if (meta.Type != AssetType::Scene) { continue; }

            std::string relative { std::filesystem::relative(meta.FilePath, assetsDir).string() };
            std::replace(relative.begin(), relative.end(), '\\', '/');
            std::string virtualPath { "assets://" + relative };

            AE_CLIENT_INFO("Auto-loading scene from registry: '{0}'", virtualPath);
            LoadScene(virtualPath);
            return;
        }

        NewUnnamedScene();
    }

    void AntelopeApp::OnUpdate(float timeStep)
    {
        for (auto& change : GetFileWatcher().FlushChanges())
        {
            switch (change.Type)
            {
                case AssetChangeType::Modified: AE_CLIENT_INFO("Asset modified: '{0}'", change.NewPath.filename().string()); break;
                case AssetChangeType::Moved: AE_CLIENT_INFO("Asset moved: '{0}' -> '{1}'", change.OldPath.filename().string(), change.NewPath.string()); break;
                case AssetChangeType::Deleted:
                {
                    AE_CLIENT_WARN("Asset deleted: '{0}'", change.OldPath.filename().string());
                    std::error_code ec;
                    std::filesystem::remove(change.OldPath.string() + ".meta", ec);
                    break;
                }
                case AssetChangeType::Imported: AE_CLIENT_INFO("Asset imported: '{0}'", change.NewPath.filename().string()); break;
            }
        }

        if (m_DebounceTimer > 0.0f) { m_DebounceTimer -= timeStep; }

        if (Input::IsKeyPressed(GLFW_KEY_LEFT_CONTROL) && Input::IsKeyPressed(GLFW_KEY_S)
            && m_DebounceTimer <= 0.0f)
        {
            m_DebounceTimer = DEBOUNCE_DELAY;

            if (m_SceneIsUntitled || m_CurrentScenePath.empty())
            {
                m_OpenSaveAsPopup = true;
            }
            else
            {
                SaveScene(m_CurrentScenePath);
            }
        }

        if (Input::IsKeyPressed(GLFW_KEY_X) && m_DebounceTimer <= 0.0f)
        {
            m_DebounceTimer = DEBOUNCE_DELAY;

            if (GetWorld()->IsSimulating())
            {
                GetWorld()->OnSimulationStop();
                AE_CLIENT_INFO("Physics PAUSED via X key.");
            }
            else
            {
                GetWorld()->OnSimulationStart();
                AE_CLIENT_INFO("Physics STARTED via X key.");
            }
        }

        if (GetWorld()->IsSimulating()) { GetWorld()->StepSimulation(timeStep); }

        m_EditorCamera.OnUpdate(timeStep);
        GetWorld()->OnUpdateEditor(timeStep, m_EditorCamera);
    }

    void AntelopeApp::OnUIRender()
    {
        static ImGuiDockNodeFlags dockspace_flags { ImGuiDockNodeFlags_None };
        ImGuiWindowFlags window_flags { ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking
                                      | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                                      | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                                      | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus };

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
        m_HierarchyPanel.OnUIRender(Application::Get().GetWorld().get(), m_ScenePanel.GetSelectedEntity());
        m_PropertiesPanel.OnUIRender(m_ScenePanel.GetSelectedEntity());
        m_ConsolePanel.OnUIRender();
        m_ProjectPanel.OnUIRender();

        RenderSaveAsPopup();

        ImGui::End();
    }

    void AntelopeApp::OnShutdown()
    {
        m_ProjectState.CameraPosition = m_EditorCamera.GetPosition();
        m_ProjectState.CameraYaw = m_EditorCamera.GetYaw();
        m_ProjectState.CameraPitch = m_EditorCamera.GetPitch();
        m_ProjectState.LastScene = m_CurrentScenePath;

        PopulateAssetRecords();
        Project::Save(m_ProjectFilePath, m_ProjectState);
        AE_CLIENT_INFO("Project state saved to '{0}'.", m_ProjectFilePath.string());

        FreeSceneMeshes();
    }

    void AntelopeApp::FreeSceneMeshes()
    {
        auto& registry { GetWorld()->GetRegistry() };
        auto view { registry.view<MeshComponent>() };
        uint32_t count { 0 };

        for (auto [entity, mesh] : view.each())
        {
            GetRenderer()->FreeMesh(mesh.Handle);
            ++count;
        }

        if (count > 0) { AE_CLIENT_INFO("{0} meshes freed from GPU.", count); }
    }

    void AntelopeApp::NewUnnamedScene()
    {
        FreeSceneMeshes();
        GetWorld()->Clear();
        m_AssetBindings.clear();
        m_CurrentScenePath = "";
        m_SceneIsUntitled = true;
        std::strncpy(m_SaveAsNameBuf, "Untitled", sizeof(m_SaveAsNameBuf) - 1);

        std::filesystem::path templatePath { FileSystem::Resolve("engine://TemplateScene.antelope") };

        if (std::filesystem::exists(templatePath))
        {
            m_AssetBindings = SceneSerializer::Deserialize("engine://TemplateScene.antelope", *GetWorld());
        }

        AE_CLIENT_INFO("New unnamed scene opened.");
    }

    void AntelopeApp::LoadScene(const std::string& virtualPath)
    {
        FreeSceneMeshes();
        GetWorld()->Clear();
        m_AssetBindings.clear();

        m_AssetBindings = SceneSerializer::Deserialize(virtualPath, *GetWorld());

        std::unordered_map<uint64_t, ModelData> loadedModels;
        auto& registry { GetWorld()->GetRegistry() };

        std::unordered_map<uint64_t, entt::entity> entityByUUID;
        for (auto e : registry.view<IDComponent>())
        {
            entityByUUID[(uint64_t)registry.get<IDComponent>(e).ID] = e;
        }

        std::unordered_map<std::string, std::filesystem::path> texturePaths;
        for (const auto& [assetUUID, meta] : AssetManager::GetRegistry())
        {
            if (meta.Type == AssetType::Texture2D)
            {
                texturePaths.emplace(meta.FilePath.filename().string(), meta.FilePath);
            }
        }

        auto loadTex { [&](const std::string& filename) -> uint32_t {
            if (filename.empty()) { return 0xFFFFFFFF; }
            auto it { texturePaths.find(filename) };
            if (it == texturePaths.end()) { return 0xFFFFFFFF; }
            return GetTextureManager()->LoadTexture(it->second.string());
        }};

        for (auto& binding : m_AssetBindings)
        {
            if (binding.ComponentType != "MeshComponent") { continue; }

            uint64_t uuid { (uint64_t)binding.AssetUUID };

            if (loadedModels.find(uuid) == loadedModels.end())
            {
                const auto& meta { AssetManager::GetMetadata(binding.AssetUUID) };
                if (!meta.IsValid()) { continue; }
                loadedModels[uuid] = ModelLoader::Load(meta.FilePath.string(), true);
            }

            const auto& modelData { loadedModels[uuid] };
            if (binding.MeshIndex >= modelData.SubMeshes.size()) { continue; }

            const auto& subMesh { modelData.SubMeshes[binding.MeshIndex] };
            MeshHandle handle { GetRenderer()->UploadMesh(subMesh.Data) };

            PBRMaterialData pbrMat {};
            uint32_t matIdx { subMesh.MaterialIndex };

            if (matIdx < modelData.Materials.size())
            {
                const auto& modelMat { modelData.Materials[matIdx] };
                pbrMat.AlbedoFactor = modelMat.AlbedoFactor;
                pbrMat.MetallicRoughnessFactors = modelMat.MetallicRoughnessFactors;

                pbrMat.AlbedoTexIndex = loadTex(modelMat.AlbedoTexPath);
                pbrMat.NormalTexIndex = loadTex(modelMat.NormalTexPath);
                pbrMat.MetRoughAOTexIndex = loadTex(modelMat.MetRoughAOTexPath);
                pbrMat.EmissiveTexIndex = loadTex(modelMat.EmissiveTexPath);
            }

            uint32_t matBufferIndex { GetRenderer()->AddMaterial(pbrMat) };

            auto entityIt { entityByUUID.find((uint64_t)binding.EntityID) };
            if (entityIt == entityByUUID.end()) { continue; }

            auto e { entityIt->second };
            if (registry.all_of<MeshComponent>(e)) { registry.get<MeshComponent>(e).Handle = handle; }
            registry.emplace_or_replace<MaterialComponent>(e).MaterialIndex = matBufferIndex;
        }

        m_CurrentScenePath = virtualPath;
        m_SceneIsUntitled = false;
        AE_CLIENT_INFO("Scene loaded: '{0}'", virtualPath);
    }

    void AntelopeApp::SaveScene(const std::string& virtualPath)
    {
        SceneSerializer::Serialize(virtualPath, *GetWorld(), m_AssetBindings);
        AE_CLIENT_INFO("Scene saved: '{0}'", virtualPath);
    }

    void AntelopeApp::RenderSaveAsPopup()
    {
        if (m_OpenSaveAsPopup)
        {
            ImGui::OpenPopup("Save Scene As");
            m_OpenSaveAsPopup = false;
        }

        ImVec2 center { ImGui::GetMainViewport()->GetCenter() };
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Scene Name:");
            ImGui::SetNextItemWidth(300.0f);
            ImGui::InputText("##scenename", m_SaveAsNameBuf, sizeof(m_SaveAsNameBuf));

            std::string previewPath { std::string("assets://Scenes/") + m_SaveAsNameBuf + ".antelope" };
            ImGui::TextDisabled("%s", previewPath.c_str());
            ImGui::Spacing();

            bool nameValid { m_SaveAsNameBuf[0] != '\0' };

            if (!nameValid) { ImGui::BeginDisabled(); }

            if (ImGui::Button("Save", ImVec2(120.0f, 0.0f)))
            {
                SaveScene(previewPath);
                m_CurrentScenePath = previewPath;
                m_ProjectState.LastScene = previewPath;
                m_SceneIsUntitled = false;
                ImGui::CloseCurrentPopup();
            }

            if (!nameValid) { ImGui::EndDisabled(); }

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) { ImGui::CloseCurrentPopup(); }

            ImGui::EndPopup();
        }
    }

    void AntelopeApp::PopulateAssetRecords()
    {
        std::filesystem::path assetsDir { m_ProjectRoot / "Assets" };
        std::error_code ec;

        m_ProjectState.LastKnownAssets.clear();

        for (const auto& [uuid, meta] : AssetManager::GetRegistry())
        {
            if (meta.Type == AssetType::None) { continue; }

            AssetRecord record;
            record.Handle = uuid;
            record.Type = meta.Type;
            record.LastModified = std::filesystem::last_write_time(meta.FilePath, ec).time_since_epoch().count();

            std::string rel { std::filesystem::relative(meta.FilePath, assetsDir, ec).string() };
            std::replace(rel.begin(), rel.end(), '\\', '/');
            record.RelativePath = rel;

            m_ProjectState.LastKnownAssets.push_back(record);
        }
    }
}