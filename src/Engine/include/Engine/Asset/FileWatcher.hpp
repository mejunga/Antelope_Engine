#pragma once
#ifdef ANTELOPE_EDITOR_MODE

#include <Engine/Asset/AssetTypes.hpp>

#include <windows.h>
#include <filesystem>
#include <vector>
#include <unordered_map>


namespace Antelope
{
    enum class AssetChangeType { Modified, Moved, Deleted, Imported };

    struct AssetChange
    {
        AssetChangeType Type;
        UUID AssetUUID;
        std::filesystem::path NewPath;
        std::filesystem::path OldPath;
    };

    class FileWatcher
    {
        public:
            ~FileWatcher();
            void BuildFrom(const std::filesystem::path& watchDir, const std::unordered_map<UUID, AssetMetadata>& registry);
            void Poll();
            std::vector<AssetChange> FlushChanges();

        private:
            struct FileRecord
            {
                std::filesystem::path Path;
                std::filesystem::file_time_type LastWriteTime;
                UUID AssetUUID;
            };

            std::filesystem::path m_WatchDir;
            std::unordered_map<uint64_t, FileRecord> m_Records;
            std::vector<AssetChange> m_PendingChanges;
            HANDLE m_ChangeHandle { INVALID_HANDLE_VALUE };
    };
}
#endif