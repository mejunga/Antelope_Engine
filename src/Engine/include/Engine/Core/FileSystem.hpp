#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>


namespace Antelope
{
    class FileSystem
    {
        public:
            static void Mount(const std::string& alias, const std::filesystem::path& realPath);
            static std::filesystem::path Resolve(const std::string& virtualPath);

        private:
            static std::unordered_map<std::string, std::filesystem::path> s_MountPoints;
    };
}