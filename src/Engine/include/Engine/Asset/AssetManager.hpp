#pragma once

#include <Engine/Asset/AssetTypes.hpp>

#include <unordered_map>
#include <vector>
#include <filesystem>


namespace Antelope
{
    class AssetManager
    {
        public:
            static void LoadAssetRegistry(const std::filesystem::path& assetDirectory,
                                          const std::vector<AssetRecord>& lastKnownAssets = {});
            static const AssetMetadata& GetMetadata(UUID assetID);
            static UUID GetAssetHandleFromFilePath(const std::filesystem::path& filepath);
            static bool ReadMetaFile(const std::filesystem::path& metaPath, AssetMetadata& outMetadata);
            static AssetType GetAssetTypeFromFileExtension(const std::filesystem::path& extension);
            static UUID ImportAsset(const std::filesystem::path& filepath);
            static void UpdateFilePath(UUID uuid, const std::filesystem::path& newPath);
            static void Remove(UUID uuid);

            static const std::unordered_map<UUID, AssetMetadata>& GetRegistry() { return s_AssetRegistry; }

        private:
            static void ProcessDirectory(const std::filesystem::path& directory);
            static void WriteMetaFile(const std::filesystem::path& metaPath, const AssetMetadata& metadata);
            static void ReconcileOfflineChanges(const std::filesystem::path& assetDirectory,
                                                const std::vector<AssetRecord>& lastKnownAssets);

        private:
            static std::unordered_map<UUID, AssetMetadata> s_AssetRegistry;
    };
}