#pragma once

#include <Engine/Renderer/Graphics/Animation.hpp>
#include <Engine/Asset/AssetTypes.hpp>
#include <Engine/Core/UUID.hpp>
#include <Engine/Renderer/Graphics/Model.hpp>

#include <filesystem>
#include <unordered_map>
#include <vector>


namespace Antelope::Editor
{
    struct ClipDragPayload
    {
        UUID ModelAssetUUID;
        uint32_t ClipIndex;
    };

    class ProjectPanel
    {
        public:
            ProjectPanel() = default;

            void SetAssetsRoot(const std::filesystem::path& assetsRoot);
            void SetModelCache(std::unordered_map<uint64_t, ModelData>* cache);
            void OnUIRender();

        private:
            void DrawFolderTree(const std::filesystem::path& dir);
            void DrawContentView();

        private:
            std::filesystem::path m_AssetsRoot;
            std::filesystem::path m_SelectedDir;
            std::unordered_map<uint64_t, std::vector<AnimationClip>> m_AnimCache;
            std::unordered_map<uint64_t, ModelData>* m_ModelCache { nullptr };
    };

}