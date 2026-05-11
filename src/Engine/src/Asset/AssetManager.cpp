#include <Engine/Asset/AssetManager.hpp>
#include <Engine/Debug/Log.hpp>

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <unordered_map>
#include <unordered_set>


namespace Antelope
{
    std::unordered_map<UUID, AssetMetadata> AssetManager::s_AssetRegistry;

    void AssetManager::LoadAssetRegistry(const std::filesystem::path& assetDirectory,
                                         const std::vector<AssetRecord>& lastKnownAssets)
    {
        AE_ENGINE_INFO("AssetManager: Scanning directory '{0}'", assetDirectory.string());

        if (!std::filesystem::exists(assetDirectory))
        {
            AE_ENGINE_ERROR("AssetManager: Directory does not exist!");
            return;
        }

        s_AssetRegistry.clear();

        if (!lastKnownAssets.empty())
        {
            ReconcileOfflineChanges(assetDirectory, lastKnownAssets);
        }

        ProcessDirectory(assetDirectory);

        AE_ENGINE_INFO("AssetManager: Loaded {0} assets into registry.", s_AssetRegistry.size());
    }

    void AssetManager::ReconcileOfflineChanges(const std::filesystem::path& assetDirectory,
                                               const std::vector<AssetRecord>& lastKnownAssets)
    {
        std::error_code ec;

        std::pmr::unordered_map<uint64_t, std::filesystem::path> currentMetaByUUID;
        std::pmr::unordered_map<uint64_t, std::pmr::vector<std::filesystem::path>> orphanedMetaByUUID;

        for (auto& entry : std::filesystem::recursive_directory_iterator(assetDirectory, ec))
        {
            if (!entry.is_regular_file(ec)) { continue; }
            if (entry.path().extension() != ".meta") { continue; }

            AssetMetadata meta;

            if (ReadMetaFile(entry.path(), meta))
            {
                std::filesystem::path assetPath { entry.path() };
                assetPath.replace_extension("");

                if (std::filesystem::exists(assetPath, ec))
                {
                    currentMetaByUUID[(uint64_t)meta.Handle] = assetPath;
                }
                else
                {
                    orphanedMetaByUUID[(uint64_t)meta.Handle].push_back(entry.path());
                }
            }
        }

        std::pmr::vector<std::filesystem::path> noMetaFiles;

        for (auto& entry : std::filesystem::recursive_directory_iterator(assetDirectory, ec))
        {
            if (!entry.is_regular_file(ec)) { continue; }
            if (entry.path().extension() == ".meta") { continue; }
            if (GetAssetTypeFromFileExtension(entry.path().extension()) == AssetType::None) { continue; }
            if (!std::filesystem::exists(entry.path().string() + ".meta", ec))
            {
                noMetaFiles.push_back(entry.path());
            }
        }

        std::pmr::unordered_set<size_t> matched;

        for (const auto& record : lastKnownAssets)
        {
            uint64_t uuid { (uint64_t)record.Handle };

            if (currentMetaByUUID.count(uuid)) { continue; }

            std::filesystem::path lastAbsPath { assetDirectory / record.RelativePath };
            std::string filename { lastAbsPath.filename().string() };
            std::string lastDir { lastAbsPath.parent_path().string() };

            size_t sameDirIdx { SIZE_MAX };
            size_t anyDirIdx { SIZE_MAX };

            for (size_t i { 0 }; i < noMetaFiles.size(); ++i)
            {
                if (matched.count(i)) { continue; }
                if (noMetaFiles[i].filename().string() != filename) { continue; }

                if (anyDirIdx == SIZE_MAX) { anyDirIdx = i; }

                if (noMetaFiles[i].parent_path().string() == lastDir)
                {
                    sameDirIdx = i;
                    break;
                }
            }

            size_t matchIdx { sameDirIdx != SIZE_MAX ? sameDirIdx : anyDirIdx };

            if (matchIdx != SIZE_MAX)
            {
                const auto& matchPath { noMetaFiles[matchIdx] };

                AssetMetadata meta;
                meta.Handle = record.Handle;
                meta.Type = record.Type;
                WriteMetaFile(matchPath.string() + ".meta", meta);

                auto orphanIt { orphanedMetaByUUID.find(uuid) };

                if (orphanIt != orphanedMetaByUUID.end())
                {
                    for (const auto& orphanPath : orphanIt->second)
                    {
                        std::filesystem::remove(orphanPath, ec);
                        if (!ec)
                        {
                            AE_ENGINE_INFO("AssetManager: Removed stale .meta '{0}'", orphanPath.filename().string());
                        }
                    }
                    orphanedMetaByUUID.erase(orphanIt);
                }

                matched.insert(matchIdx);

                AE_ENGINE_INFO("AssetManager: Reconciled '{0}' -> '{1}'",
                    record.RelativePath,
                    std::filesystem::relative(matchPath, assetDirectory, ec).string());
            }
            else
            {
                AE_ENGINE_INFO("AssetManager: Asset deleted since last session: '{0}'", record.RelativePath);
            }
        }

        for (const auto& [uuid, orphanPaths] : orphanedMetaByUUID)
        {
            for (const auto& orphanPath : orphanPaths)
            {
                std::filesystem::remove(orphanPath, ec);
                if (!ec)
                {
                    AE_ENGINE_WARN("AssetManager: Removed stale .meta '{0}'", orphanPath.filename().string());
                }
            }
        }
    }

    const AssetMetadata& AssetManager::GetMetadata(UUID assetID)
    {
        static AssetMetadata s_NullMetadata;
        auto it { s_AssetRegistry.find(assetID) };
        if (it != s_AssetRegistry.end()) { return it->second; }
        return s_NullMetadata;
    }

    UUID AssetManager::GetAssetHandleFromFilePath(const std::filesystem::path& filepath)
    {
        for (const auto& [handle, metadata] : s_AssetRegistry)
        {
            if (metadata.FilePath == filepath) { return handle; }
        }

        return 0;
    }

    bool AssetManager::ReadMetaFile(const std::filesystem::path& metaPath, AssetMetadata& outMetadata)
    {
        if (!std::filesystem::exists(metaPath)) { return false; }

        try
        {
            YAML::Node data { YAML::LoadFile(metaPath.string()) };
            auto assetNode { data["Asset"] };
            if (!assetNode) { return false; }

            outMetadata.Handle = assetNode["Handle"].as<uint64_t>();
            outMetadata.Type = (AssetType)assetNode["Type"].as<uint16_t>();
            return true;
        }
        catch (YAML::Exception& e)
        {
            AE_ENGINE_ERROR("AssetManager: Corrupt meta file '{0}': {1}", metaPath.string(), e.what());
            return false;
        }
    }

    AssetType AssetManager::GetAssetTypeFromFileExtension(const std::filesystem::path& extension)
    {
        std::string ext { extension.string() };
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".antelope") { return AssetType::Scene; }
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga") { return AssetType::Texture2D; }
        if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb") { return AssetType::Mesh; }
        return AssetType::None;
    }

    UUID AssetManager::ImportAsset(const std::filesystem::path& filepath)
    {
        std::filesystem::path metaPath { filepath.string() + ".meta" };
        AssetMetadata metadata;

        if (std::filesystem::exists(metaPath))
        {
            if (ReadMetaFile(metaPath, metadata))
            {
                metadata.FilePath = filepath;
                s_AssetRegistry[metadata.Handle] = metadata;
                return metadata.Handle;
            }
        }
        else
        {
            AssetType type { GetAssetTypeFromFileExtension(filepath.extension()) };
            if (type == AssetType::None) { return UUID(0); }

            metadata.Handle = UUID();
            metadata.Type = type;
            metadata.FilePath = filepath;

            WriteMetaFile(metaPath, metadata);
            s_AssetRegistry[metadata.Handle] = metadata;

            AE_ENGINE_TRACE("AssetManager: Imported new asset '{0}' with UUID {1}", filepath.filename().string(), (uint64_t)metadata.Handle);
            return metadata.Handle;
        }

        return UUID(0);
    }

    void AssetManager::UpdateFilePath(UUID uuid, const std::filesystem::path& newPath)
    {
        auto it { s_AssetRegistry.find(uuid) };
        if (it != s_AssetRegistry.end()) { it->second.FilePath = newPath; }
    }

    void AssetManager::Remove(UUID uuid)
    {
        s_AssetRegistry.erase(uuid);
    }

    void AssetManager::ProcessDirectory(const std::filesystem::path& directory)
    {
        std::error_code ec;
        std::pmr::unordered_map<std::string, std::filesystem::path> orphanedMetas;

        for (auto& entry : std::filesystem::recursive_directory_iterator(directory, ec))
        {
            if (!entry.is_regular_file(ec)) { continue; }
            if (entry.path().extension() != ".meta") { continue; }

            std::filesystem::path assetPath { entry.path() };
            assetPath.replace_extension("");

            if (!std::filesystem::exists(assetPath, ec))
            {
                orphanedMetas[assetPath.filename().string()] = entry.path();
            }
        }

        for (auto& directoryEntry : std::filesystem::recursive_directory_iterator(directory, ec))
        {
            if (directoryEntry.is_directory(ec)) { continue; }

            auto assetPath { directoryEntry.path() };

            if (assetPath.extension() == ".meta") { continue; }
            if (GetAssetTypeFromFileExtension(assetPath.extension()) == AssetType::None) { continue; }

            std::filesystem::path metaPath { assetPath.string() + ".meta" };

            if (!std::filesystem::exists(metaPath, ec))
            {
                auto it { orphanedMetas.find(assetPath.filename().string()) };

                if (it != orphanedMetas.end())
                {
                    std::filesystem::rename(it->second, metaPath, ec);
                    if (!ec)
                    {
                        AE_ENGINE_INFO("AssetManager: Recovered .meta for moved asset '{0}'", assetPath.filename().string());
                    }
                    orphanedMetas.erase(it);
                }
            }

            ImportAsset(assetPath);
        }
    }

    void AssetManager::WriteMetaFile(const std::filesystem::path& metaPath, const AssetMetadata& metadata)
    {
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Asset" << YAML::BeginMap;
        out << YAML::Key << "Handle" << YAML::Value << (uint64_t)metadata.Handle;
        out << YAML::Key << "Type" << YAML::Value << (uint16_t)metadata.Type;
        out << YAML::EndMap;
        out << YAML::EndMap;

        std::ofstream fout(metaPath);

        if (!fout.is_open())
        {
            AE_ENGINE_ERROR("AssetManager: Failed to write .meta file '{0}'", metaPath.string());
            return;
        }
        
        fout << out.c_str();
    }
}