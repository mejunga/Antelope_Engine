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
#include <Engine/Renderer/UI/UIContext.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <GLFW/glfw3.h>
#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif


namespace Antelope::Editor
{
    static std::filesystem::path FindCompiler()
    {
        namespace fs = std::filesystem;

        const char* vcTools { std::getenv("VCToolsInstallDir") };
        if (vcTools)
        {
            fs::path cl { fs::path(vcTools) / "bin" / "HostX64" / "x64" / "cl.exe" };
            if (fs::exists(cl)) { return cl; }
        }

        const char* pf86 { std::getenv("ProgramFiles(x86)") };
        if (pf86)
        {
            fs::path vswhere { fs::path(pf86) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe" };

            if (fs::exists(vswhere))
            {
                std::string cmd { "\"\"" + vswhere.string()
                    + "\" -latest -products * -find \"VC\\Tools\\MSVC\\**\\bin\\HostX64\\x64\\cl.exe\"\"" };

                FILE* p { _popen(cmd.c_str(), "r") };
                if (p)
                {
                    char buf[512] {};
                    if (fgets(buf, sizeof(buf), p))
                    {
                        _pclose(p);
                        std::string path { buf };
                        path.erase(path.find_last_not_of(" \t\r\n") + 1);
                        if (!path.empty() && fs::exists(path)) { return path; }
                    }
                    else { _pclose(p); }
                }
            }
        }

        FILE* pipe { _popen("where cl.exe", "r") };
        if (pipe)
        {
            char buf[512] {};
            if (fgets(buf, sizeof(buf), pipe))
            {
                _pclose(pipe);
                std::string path { buf };
                path.erase(path.find_last_not_of(" \t\r\n") + 1);
                if (!path.empty() && fs::exists(path)) { return path; }
            }
            else { _pclose(pipe); }
        }

        return {};
    }

    static std::filesystem::path SetupCompileEnvironment(const std::filesystem::path& projectRoot)
    {
        namespace fs = std::filesystem;
        const char* pf86 { std::getenv("ProgramFiles(x86)") };

        std::error_code ec;
        fs::create_directories(projectRoot / "Libraries", ec);
        std::ofstream header(projectRoot / "Libraries" / "AntelopeScript.hpp");
        header << "#pragma once\n"
            << "#include <Engine/Scripting/Script.hpp>\n"
            << "#include <Engine/Scripting/GameSystem.hpp>\n"
            << "#include <Engine/Scripting/ScriptMacros.hpp>\n"
            << "#include <Engine/ECS/World.hpp>\n"
            << "#include <Engine/Platform/Input.hpp>\n";

        wchar_t buf[MAX_PATH] {};
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        fs::path exeDir { fs::path(buf).parent_path() };
        fs::path engineInclude { exeDir / "Engine" / "include" };
        fs::path engineLib     { exeDir / "Engine" / "lib" };

        fs::path compilerPath { FindCompiler() };

        if (compilerPath.empty())
        {
            AE_CLIENT_ERROR("Script compiler: cl.exe not found. Add it to PATH or install MSVC Build Tools.");
            return {};
        }

        auto yp { [](const fs::path& p) {
            std::string s { p.string() };
            std::string out;
            out.reserve(s.size());
            for (char c : s) { if (c == '\\') { out += '/'; } else { out += c; } }
            return out;
        }};

        fs::path msvcRoot { compilerPath.parent_path().parent_path().parent_path().parent_path() };
        fs::path msvcInclude { msvcRoot / "include" };
        fs::path msvcLib { msvcRoot / "lib" / "x64" };

        fs::path winKitsInclude;
        fs::path winKitsUcrtLib;
        fs::path winKitsUmLib;

        if (pf86)
        {
            fs::path kitsBase { fs::path(pf86) / "Windows Kits" / "10" };
            fs::path kitsInc { kitsBase / "include" };
            fs::path kitsLib { kitsBase / "lib" };

            std::string latestSdk;
            std::error_code ec2;
            for (const auto& entry : fs::directory_iterator(kitsInc, ec2))
            {
                std::string name { entry.path().filename().string() };
                if (entry.is_directory() && name > latestSdk) { latestSdk = name; }
            }

            if (!latestSdk.empty())
            {
                winKitsInclude = kitsInc / latestSdk;
                winKitsUcrtLib = kitsLib / latestSdk / "ucrt" / "x64";
                winKitsUmLib = kitsLib / latestSdk / "um" / "x64";
            }
        }

        std::ofstream cfg(exeDir / "compile_config.yaml");
        cfg << "Compiler: \"" << yp(compilerPath) << "\"\n";
    #ifdef NDEBUG
        cfg << "Flags: \"/std:c++20 /EHsc /MD /utf-8 /DANTELOPE_EDITOR_MODE\"\n";
    #else
        cfg << "Flags: \"/std:c++20 /EHsc /MDd /utf-8 /DANTELOPE_EDITOR_MODE\"\n";
    #endif
        cfg << "IncludeDirs:\n";
        cfg << "- \"" << yp(engineInclude) << "\"\n";
        cfg << "- \"" << yp(projectRoot / "Libraries") << "\"\n";

        if (fs::exists(msvcInclude)) { cfg << "- \"" << yp(msvcInclude) << "\"\n"; }
        if (!winKitsInclude.empty())
        {
            cfg << "- \"" << yp(winKitsInclude / "ucrt") << "\"\n";
            cfg << "- \"" << yp(winKitsInclude / "um") << "\"\n";
            cfg << "- \"" << yp(winKitsInclude / "shared") << "\"\n";
        }

        cfg << "LibDirs:\n";
        cfg << "- \"" << yp(engineLib) << "\"\n";

        if (fs::exists(msvcLib)) { cfg << "- \"" << yp(msvcLib) << "\"\n"; }
        if (!winKitsUcrtLib.empty())
        {
            cfg << "- \"" << yp(winKitsUcrtLib) << "\"\n";
            cfg << "- \"" << yp(winKitsUmLib) << "\"\n";
        }

        cfg << "Libs:\n";
        cfg << "- Antelope.lib\n";

        fs::path configPath { exeDir / "compile_config.yaml" };
        AE_CLIENT_INFO("Compile config written: '{0}'", configPath.string());
        return configPath;
    }

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

        m_ProjectPanel.SetAssetsRoot(assetsDir);
        m_ProjectPanel.SetModelCache(&m_ModelCache);
        m_ScenePanel.SetOnMeshDropped([this](UUID uuid, glm::vec3 pos) -> Entity { return SpawnModel(uuid, pos); });
        m_AnimatorPanel.SetModelCache(&m_ModelCache);
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

        m_CompileConfigPath = SetupCompileEnvironment(m_ProjectRoot);
    #ifdef NDEBUG
        std::filesystem::path dllPath { m_ProjectRoot / ".generated" / "Scripts.dll" };
    #else
        std::filesystem::path dllPath { m_ProjectRoot / ".generated" / "Scripts_d.dll" };
    #endif


        if (std::filesystem::exists(dllPath))
        {
            ScriptSystem::LoadDLL(dllPath.string());
            AE_CLIENT_INFO("Loaded existing script DLL: '{0}'", dllPath.string());
        }
        else
        {
            bool hasScripts { false };

            for (auto& [uuid, meta] : AssetManager::GetRegistry())
            {
                if (meta.Type == AssetType::Script) { hasScripts = true; break; }
            }
            
            if (hasScripts) { TriggerScriptRecompile(); }
        }

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
            AssetType changeAssetType { AssetManager::GetMetadata(change.AssetUUID).Type };

            switch (change.Type)
            {
                case AssetChangeType::Modified:
                {
                    AE_CLIENT_INFO("Asset modified: '{0}'", change.NewPath.filename().string());
                    if (changeAssetType == AssetType::Script)
                    {
                        m_ScriptRecompilePending = true;
                        m_ScriptDebounce = SCRIPT_RECOMPILE_DELAY;
                    }
                    break;
                }
                case AssetChangeType::Moved:
                {
                    AE_CLIENT_INFO("Asset moved: '{0}' -> '{1}'", change.OldPath.filename().string(), change.NewPath.string());
                    break;
                }
                case AssetChangeType::Deleted:
                {
                    AE_CLIENT_WARN("Asset deleted: '{0}'", change.OldPath.filename().string());
                    std::error_code ec;
                    std::filesystem::remove(change.OldPath.string() + ".meta", ec);
                    auto deletedType { AssetManager::GetAssetTypeFromFileExtension(change.OldPath.extension()) };
                    
                    if (deletedType == AssetType::Script)
                    {
                        m_ScriptRecompilePending = true;
                        m_ScriptDebounce = SCRIPT_RECOMPILE_DELAY;
                    }
                    
                    break;
                }
                case AssetChangeType::Imported:
                {
                    AE_CLIENT_INFO("Asset imported: '{0}'", change.NewPath.filename().string());
                    if (changeAssetType == AssetType::Script)
                    {
                        m_ScriptRecompilePending = true;
                        m_ScriptDebounce = SCRIPT_RECOMPILE_DELAY;
                    }
                    break;
                }
            }
        }

        if (m_ScriptRecompilePending)
        {
            m_ScriptDebounce -= timeStep;
            if (m_ScriptDebounce <= 0.0f)
            {
                m_ScriptRecompilePending = false;
                TriggerScriptRecompile();
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

        GetWorld()->SetGameViewActive(m_GameViewPanel.IsGameViewActive());
        GetWorld()->SetPaused(m_GameViewPanel.IsPaused());

        if (GetWorld()->IsSimulating() && !m_GameViewPanel.IsPaused())
        {
            GetWorld()->StepSimulation(timeStep);
        }

        {
            auto& registry { GetWorld()->GetRegistry() };

            for (auto [entity, anim, smc] : registry.view<AnimatorComponent, SkinnedMeshComponent>().each())
            {
                if (anim.Model) { continue; }

                auto it { m_ModelCache.find((uint64_t)smc.ModelAssetUUID) };

                if (it != m_ModelCache.end())
                {
                    smc.Model = &it->second;
                    anim.Model = smc.Model;
                    for (auto& clip : anim.Controller.Clips)
                    {
                        if (!clip.Channels.empty()) { continue; }
                        for (const auto& src : anim.Model->Animations)
                        {
                            if (src.Name == clip.Name) { clip = src; break; }
                        }
                    }
                }
            }

            for (auto [entity, anim, rel] : registry.view<AnimatorComponent, RelationshipComponent>().each())
            {
                if (anim.Model) { continue; }

                entt::entity child { rel.FirstChild };

                while (child != entt::null && !anim.Model)
                {
                    if (registry.all_of<IDComponent>(child))
                    {
                        UUID childUUID { registry.get<IDComponent>(child).ID };

                        for (const auto& binding : m_AssetBindings)
                        {
                            if (binding.EntityID != childUUID || binding.ComponentType != "MeshComponent") { continue; }

                            auto it { m_ModelCache.find((uint64_t)binding.AssetUUID) };
                            
                            if (it != m_ModelCache.end())
                            {
                                anim.Model = &it->second;
                                for (auto& clip : anim.Controller.Clips)
                                {
                                    if (!clip.Channels.empty()) { continue; }
                                    for (const auto& src : anim.Model->Animations)
                                    {
                                        if (src.Name == clip.Name) { clip = src; break; }
                                    }
                                }
                            }

                            break;
                        }
                    }

                    auto* cRel { registry.try_get<RelationshipComponent>(child) };
                    child = cRel ? cRel->NextSibling : entt::null;
                }
            }

            for (auto [entity, mc, rel] : registry.view<MeshColliderComponent, RelationshipComponent>().each())
            {
                if (!mc.Vertices.empty()) { continue; }

                UUID modelUUID { 0 };
                entt::entity child { rel.FirstChild };

                while (child != entt::null && (uint64_t)modelUUID == 0)
                {
                    if (registry.all_of<IDComponent>(child))
                    {
                        UUID childUUID { registry.get<IDComponent>(child).ID };

                        for (const auto& binding : m_AssetBindings)
                        {
                            if (binding.EntityID == childUUID && binding.ComponentType == "MeshComponent")
                            {
                                modelUUID = binding.AssetUUID;
                                break;
                            }
                        }
                    }

                    auto* cRel { registry.try_get<RelationshipComponent>(child) };
                    child = cRel ? cRel->NextSibling : entt::null;
                }

                if ((uint64_t)modelUUID == 0) { continue; }

                auto it { m_ModelCache.find((uint64_t)modelUUID) };
                if (it == m_ModelCache.end()) { continue; }

                const ModelData& model { it->second };

                struct NodeEntry { const ModelNode* node; glm::mat4 transform; };
                std::vector<NodeEntry> stack;
                stack.push_back({ &model.RootNode, glm::mat4(1.0f) });

                while (!stack.empty())
                {
                    auto [pNode, parentTransform] { stack.back() };
                    stack.pop_back();

                    glm::mat4 global { parentTransform * pNode->LocalTransform };

                    for (uint32_t meshIdx : pNode->MeshIndices)
                    {
                        if (meshIdx >= model.SubMeshes.size()) { continue; }

                        const SubMeshData& sub { model.SubMeshes[meshIdx] };
                        uint32_t base { static_cast<uint32_t>(mc.Vertices.size()) };

                        for (const auto& vp : sub.Data.positions)
                        {
                            glm::vec4 t { global * glm::vec4(vp.pos, 1.0f) };
                            mc.Vertices.push_back(glm::vec3(t));
                        }

                        for (const auto& face : sub.Data.faces)
                        {
                            mc.Indices.push_back(base + face.v0);
                            mc.Indices.push_back(base + face.v1);
                            mc.Indices.push_back(base + face.v2);
                        }
                    }

                    for (const auto& childNode : pNode->Children)
                    {
                        stack.push_back({ &childNode, global });
                    }
                }
            }
        }

        m_EditorCamera.OnUpdate(timeStep);
        GetWorld()->OnUpdateEditor(timeStep, m_EditorCamera);
    }

    void AntelopeApp::OnUIRender()
    {
        ImGui::SetCurrentContext(Application::Get().GetUIContext()->GetContext());
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
            ImGui::DockBuilderDockWindow("Animator", dock_id_main);
            ImGui::DockBuilderDockWindow("Game", dock_id_main);
            ImGui::DockBuilderDockWindow("Scene", dock_id_main);
            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

        m_ScenePanel.OnUIRender(m_EditorCamera, m_GameViewPanel);
        m_AnimatorPanel.OnUIRender(m_ScenePanel.GetSelectedEntity());
        m_HierarchyPanel.OnUIRender(Application::Get().GetWorld().get(), m_ScenePanel.GetSelectedEntity());
        m_PropertiesPanel.OnUIRender(m_ScenePanel.GetSelectedEntity(), m_EditorCamera);
        m_ConsolePanel.OnUIRender();
        m_ProjectPanel.OnUIRender();
        m_GameViewPanel.OnUIRender(GetWorld().get());

        static bool s_FocusScene { true };
        
        if (s_FocusScene && ImGui::FindWindowByName("Scene"))
        {
            ImGui::SetWindowFocus("Scene");
            s_FocusScene = false;
        }

        RenderSaveAsPopup();

        if (ImGui::GetDragDropPayload() != nullptr && !ImGui::IsDragDropPayloadBeingAccepted())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);
        }

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
        GetTextureManager()->Clear();
        GetWorld()->Clear();
        m_AssetBindings.clear();
        m_ModelCache.clear();   
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
        GetTextureManager()->Clear();
        GetWorld()->Clear();
        m_AssetBindings.clear();

        m_AssetBindings = SceneSerializer::Deserialize(virtualPath, *GetWorld());

        m_ModelCache.clear();
        auto& registry { GetWorld()->GetRegistry() };

        std::unordered_map<uint64_t, entt::entity> entityByUUID;
        for (auto e : registry.view<IDComponent>())
        {
            entityByUUID[(uint64_t)registry.get<IDComponent>(e).ID] = e;
        }

        std::unordered_map<std::string, std::filesystem::path> texturePaths;
        
        for (const auto& [assetUUID, meta] : AssetManager::GetRegistry())
        {
            if (meta.Type != AssetType::Texture2D) { continue; }
            auto [it, inserted] { texturePaths.emplace(meta.FilePath.filename().string(), meta.FilePath) };
            if (!inserted)
            {
                AE_ENGINE_WARN("LoadScene: Duplicate texture filename '{0}': '{1}' vs '{2}'. Consider renaming one.",
                    meta.FilePath.filename().string(), it->second.string(), meta.FilePath.string());
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

            if (m_ModelCache.find(uuid) == m_ModelCache.end())
            {
                const auto& meta { AssetManager::GetMetadata(binding.AssetUUID) };
                if (!meta.IsValid()) { continue; }
                m_ModelCache[uuid] = ModelLoader::Load(meta.FilePath.string(), true);
            }

            const auto& modelData { m_ModelCache[uuid] };

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

    void AntelopeApp::TriggerScriptRecompile()
    {
        namespace fs = std::filesystem;

        bool hasScripts { false };

        for (auto& [uuid, meta] : AssetManager::GetRegistry())
        {
            if (meta.Type == AssetType::Script) { hasScripts = true; break; }
        }

        if (!hasScripts) { return; }

        if (m_CompileConfigPath.empty())
        {
            AE_CLIENT_ERROR("Script compiler: compile_config.yaml not found. Rebuild the engine.");
            return;
        }

        fs::path assetsDir { m_ProjectRoot / "Assets" };
        fs::path generatedDir { m_ProjectRoot / ".generated" };
    #ifdef NDEBUG
        fs::path dllPath { generatedDir / "Scripts.dll" };
    #else
        fs::path dllPath { generatedDir / "Scripts_d.dll" };
    #endif


        std::error_code ec;
        fs::create_directories(generatedDir, ec);

        AE_CLIENT_INFO("HeaderTool: scanning scripts...");
        HeaderTool::ScanAndGenerate(assetsDir.string(), generatedDir.string());

        AE_CLIENT_INFO("ScriptCompiler: building Scripts.dll...");
        std::string libDir { (m_ProjectRoot / "Libraries").string() };
        auto result { ScriptCompiler::Compile(assetsDir.string(), generatedDir.string(), generatedDir.string(), m_CompileConfigPath.string(), { libDir }) };

        if (!result.Success)
        {
            AE_CLIENT_ERROR("Script compilation failed:\n{0}", result.Output);
            return;
        }

        AE_CLIENT_INFO("Scripts compiled successfully.");

        if (ScriptLibrary::Get().IsLoaded())
        {
            ScriptSystem::HotReload(*GetWorld());
        }
        else
        {
            ScriptSystem::LoadDLL(dllPath.string());
        }
    }

    Entity AntelopeApp::SpawnModel(UUID assetUUID, glm::vec3 spawnPos)
    {
        const auto& meta { AssetManager::GetMetadata(assetUUID) };
        
        if (!meta.IsValid()) { return {}; }

        if (m_ModelCache.find((uint64_t)assetUUID) == m_ModelCache.end())
        {
            m_ModelCache[(uint64_t)assetUUID] = ModelLoader::Load(meta.FilePath.string(), true);
        }

        std::string name { meta.FilePath.stem().string() };
        Entity root { GetWorld()->SpawnModel(m_ModelCache[(uint64_t)assetUUID], name, assetUUID, &m_AssetBindings) };

        auto& tc { root.GetComponent<TransformComponent>() };
        tc.Translation = spawnPos;
        GetWorld()->MarkTransformDirty(root);

        AE_CLIENT_INFO("Spawned model '{0}' into scene.", name);
        return root;
    }
}