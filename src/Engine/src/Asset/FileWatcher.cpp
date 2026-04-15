#include <Engine/Asset/FileWatcher.hpp>
#include <Engine/Asset/AssetManager.hpp>
#include <Engine/Debug/Log.hpp>

#include <unordered_set>
#include <algorithm>


namespace Antelope
{
    void FileWatcher::BuildFrom(const std::filesystem::path& watchDir, const std::map<UUID, AssetMetadata>& registry)
    {
        m_WatchDir = watchDir;
        m_Records.clear();

        std::error_code ec;

        for (auto& [uuid, meta] : registry)
        {
            if (!std::filesystem::exists(meta.FilePath, ec)) { continue; }

            m_Records[(uint64_t)uuid] = {
                meta.FilePath,
                std::filesystem::last_write_time(meta.FilePath, ec),
                uuid
            };
        }
    }

    void FileWatcher::Poll()
    {
        std::error_code ec;

        struct ScanEntry
        {
            std::filesystem::path Path;
            std::filesystem::file_time_type WriteTime;
            UUID AssetUUID { 0 };
            bool HasMeta { false };
        };

        std::vector<ScanEntry> scanned;

        for (auto& dirEntry : std::filesystem::recursive_directory_iterator(m_WatchDir, ec))
        {
            if (dirEntry.is_directory(ec)) { continue; }

            auto path { dirEntry.path() };

            if (path.extension() == ".meta") { continue; }
            if (AssetManager::GetAssetTypeFromFileExtension(path.extension()) == AssetType::None) { continue; }

            ScanEntry entry;
            entry.Path = path;
            entry.WriteTime = std::filesystem::last_write_time(path, ec);

            AssetMetadata meta;
            if (AssetManager::ReadMetaFile(path.string() + ".meta", meta))
            {
                entry.AssetUUID = meta.Handle;
                entry.HasMeta = true;
            }

            scanned.push_back(entry);
        }

        std::unordered_set<uint64_t> seenUUIDs;
        std::vector<ScanEntry*> unregistered;

        for (auto& entry : scanned)
        {
            if (entry.HasMeta && m_Records.count((uint64_t)entry.AssetUUID))
            {
                seenUUIDs.insert((uint64_t)entry.AssetUUID);

                auto& record { m_Records[(uint64_t)entry.AssetUUID] };

                if (entry.WriteTime != record.LastWriteTime)
                {
                    record.LastWriteTime = entry.WriteTime;
                    m_PendingChanges.push_back({ AssetChangeType::Modified, entry.AssetUUID, entry.Path, {} });
                }
            }
            else
            {
                unregistered.push_back(&entry);
            }
        }

        std::unordered_map<std::string, std::vector<uint64_t>> missingByFilename;

        for (auto& [uuidInt, record] : m_Records)
        {
            if (!seenUUIDs.count(uuidInt))
            {
                missingByFilename[record.Path.filename().string()].push_back(uuidInt);
            }
        }

        for (auto* entry : unregistered)
        {
            std::string filename { entry->Path.filename().string() };
            auto it { missingByFilename.find(filename) };

            if (it != missingByFilename.end() && !it->second.empty())
            {
                std::string entryDir { entry->Path.parent_path().string() };
                uint64_t bestUUID { it->second[0] };

                for (uint64_t candidate : it->second)
                {
                    if (m_Records[candidate].Path.parent_path().string() == entryDir)
                    {
                        bestUUID = candidate;
                        break;
                    }
                }

                auto& candidates { it->second };
                candidates.erase(std::remove(candidates.begin(), candidates.end(), bestUUID), candidates.end());

                UUID uuid { bestUUID };
                auto& record { m_Records[bestUUID] };

                std::filesystem::path oldMeta { record.Path.string() + ".meta" };
                std::filesystem::path newMeta { entry->Path.string() + ".meta" };

                if (!std::filesystem::exists(newMeta, ec))
                {
                    std::filesystem::rename(oldMeta, newMeta, ec);

                    if (ec)
                    {
                        AE_ENGINE_WARN("FileWatcher: Could not move .meta for '{0}': {1}", filename, ec.message());
                    }
                }
                else
                {
                    std::filesystem::remove(oldMeta, ec);
                }

                AssetManager::UpdateFilePath(uuid, entry->Path);

                m_PendingChanges.push_back({ AssetChangeType::Moved, uuid, entry->Path, record.Path });

                record.Path = entry->Path;
                record.LastWriteTime = entry->WriteTime;
                seenUUIDs.insert((uint64_t)uuid);
            }
            else
            {
                UUID newUUID { AssetManager::ImportAsset(entry->Path) };

                if ((uint64_t)newUUID != 0)
                {
                    m_Records[(uint64_t)newUUID] = { entry->Path, entry->WriteTime, newUUID };
                    m_PendingChanges.push_back({ AssetChangeType::Imported, newUUID, entry->Path, {} });
                }
            }
        }

        for (auto& [filename, uuids] : missingByFilename)
        {
            for (uint64_t uuidInt : uuids)
            {
                auto& record { m_Records[uuidInt] };
                m_PendingChanges.push_back({ AssetChangeType::Deleted, UUID(uuidInt), {}, record.Path });
                AssetManager::Remove(UUID(uuidInt));
                m_Records.erase(uuidInt);
            }
        }
    }

    std::vector<AssetChange> FileWatcher::FlushChanges()
    {
        std::vector<AssetChange> changes { std::move(m_PendingChanges) };
        m_PendingChanges.clear();
        return changes;
    }
}
