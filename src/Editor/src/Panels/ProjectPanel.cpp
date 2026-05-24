#include <Editor/Panels/ProjectPanel.hpp>

#include <Engine/Asset/AssetManager.hpp>
#include <Engine/Asset/ModelLoader.hpp>

#include <imgui.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>


namespace Antelope::Editor
{
    void ProjectPanel::SetAssetsRoot(const std::filesystem::path& assetsRoot)
    {
        m_AssetsRoot  = assetsRoot;
        m_SelectedDir = assetsRoot;
    }

    void ProjectPanel::SetModelCache(std::unordered_map<uint64_t, ModelData>* cache)
    {
        m_ModelCache = cache;
    }

    void ProjectPanel::OnUIRender()
    {
        ImGui::Begin("Project");

        ImGui::BeginChild("##FolderTree", ImVec2(180.0f, 0.0f), true);

        if (!m_AssetsRoot.empty()) { DrawFolderTree(m_AssetsRoot); }
        
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##ContentView", ImVec2(0.0f, 0.0f), false);
        DrawContentView();
        ImGui::EndChild();

        ImGui::End();
    }

    void ProjectPanel::DrawFolderTree(const std::filesystem::path& dir)
    {
        std::error_code ec;
        bool hasSubdirs { false };

        for (auto& entry : std::filesystem::directory_iterator(dir, ec))
        {
            if (entry.is_directory()) { hasSubdirs = true; break; }
        }

        ImGuiTreeNodeFlags flags { ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth };
        
        if (!hasSubdirs) { flags |= ImGuiTreeNodeFlags_Leaf; }
        if (dir == m_SelectedDir) { flags |= ImGuiTreeNodeFlags_Selected; }
        if (dir == m_AssetsRoot) { flags |= ImGuiTreeNodeFlags_DefaultOpen; }

        bool open { ImGui::TreeNodeEx(dir.string().c_str(), flags, "%s", dir.filename().string().c_str()) };
        
        if (ImGui::IsItemClicked()) { m_SelectedDir = dir; }

        if (open)
        {
            std::vector<std::filesystem::path> subdirs;
            
            for (auto& entry : std::filesystem::directory_iterator(dir, ec))
            {
                if (entry.is_directory()) { subdirs.push_back(entry.path()); }
            }

            std::sort(subdirs.begin(), subdirs.end());
            for (auto& sub : subdirs) { DrawFolderTree(sub); }
            ImGui::TreePop();
        }
    }

    void ProjectPanel::DrawContentView()
    {
        if (m_SelectedDir.empty() || !std::filesystem::exists(m_SelectedDir)) { return; }

        std::error_code ec;
        std::vector<std::filesystem::path> dirs, files;

        for (auto& entry : std::filesystem::directory_iterator(m_SelectedDir, ec))
        {
            if (entry.is_directory()) { dirs.push_back(entry.path()); }
            else if (entry.path().extension() != ".meta") { files.push_back(entry.path()); }
        }

        std::sort(dirs.begin(), dirs.end());
        std::sort(files.begin(), files.end());

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.75f, 0.3f, 1.0f));

        for (auto& d : dirs)
        {
            if (ImGui::Selectable(("[D] " + d.filename().string()).c_str()))
            {
                m_SelectedDir = d;
            }
        }

        ImGui::PopStyleColor();
        
        if (!dirs.empty() && !files.empty()) { ImGui::Separator(); }

        const auto& registry { AssetManager::GetRegistry() };

        for (auto& filePath : files)
        {
            UUID uuid { AssetManager::GetAssetHandleFromFilePath(filePath) };
            AssetType type { AssetType::None };

            if (uuid != UUID(0))
            {
                auto it { registry.find(uuid) };
                if (it != registry.end()) { type = it->second.Type; }
            }

            std::string filename { filePath.filename().string() };
            const bool isMesh { type == AssetType::Mesh };
            const bool isAudio { type == AssetType::AudioClip };

            if (isMesh && uuid != UUID(0) && m_AnimCache.find((uint64_t)uuid) == m_AnimCache.end())
            {
                const auto& meta { AssetManager::GetMetadata(uuid) };
                if (meta.IsValid())
                {
                    if (m_ModelCache && m_ModelCache->count((uint64_t)uuid))
                    {
                        m_AnimCache[(uint64_t)uuid] = (*m_ModelCache)[(uint64_t)uuid].Animations;
                    }
                    else
                    {
                        ModelData data { ModelLoader::Load(meta.FilePath.string(), true) };
                        m_AnimCache[(uint64_t)uuid] = data.Animations;
                        if (m_ModelCache) { (*m_ModelCache)[(uint64_t)uuid] = std::move(data); }
                    }
                }
                else
                {
                    m_AnimCache[(uint64_t)uuid] = {};
                }
            }

            const bool hasAnims { isMesh && !m_AnimCache[(uint64_t)uuid].empty() };
            const char* badge { "" };

            switch (type)
            {
                case AssetType::Mesh: badge = "[M] "; break;
                case AssetType::Texture2D: badge = "[T] "; break;
                case AssetType::Scene: badge = "[S] "; break;
                case AssetType::Material: badge = "[Mat] "; break;
                case AssetType::AudioClip: badge = "[A] "; break;
                default: break;
            }

            ImGuiTreeNodeFlags flags { ImGuiTreeNodeFlags_SpanAvailWidth };
            if (!hasAnims) { flags |= ImGuiTreeNodeFlags_Leaf; }

            bool open { ImGui::TreeNodeEx(filePath.string().c_str(), flags, "%s%s", badge, filename.c_str()) };

            if (isMesh && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::SetDragDropPayload("ASSET_MESH", &uuid, sizeof(UUID));
                ImGui::Text("%s", filename.c_str());
                ImGui::EndDragDropSource();
            }

            if (isAudio && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::SetDragDropPayload("ASSET_AUDIO", &uuid, sizeof(UUID));
                ImGui::Text("%s", filename.c_str());
                ImGui::EndDragDropSource();
            }

            if (open)
            {
                auto& clips { m_AnimCache[(uint64_t)uuid] };

                for (uint32_t i { 0 }; i < static_cast<uint32_t>(clips.size()); ++i)
                {
                    ImGui::PushID(static_cast<int>(i));

                    float dur { clips[i].TicksPerSecond > 0.0f ? clips[i].Duration / clips[i].TicksPerSecond : 0.0f };
                    ImGui::TreeNodeEx("##clip", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth,
                        "  [Anim] %s  (%.1fs)", clips[i].Name.c_str(), dur);

                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                    {
                        ClipDragPayload payload { uuid, i };
                        ImGui::SetDragDropPayload("ANIM_CLIP", &payload, sizeof(ClipDragPayload));
                        ImGui::Text("%s", clips[i].Name.c_str());
                        ImGui::EndDragDropSource();
                    }

                    ImGui::TreePop();
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
        }
    }
}