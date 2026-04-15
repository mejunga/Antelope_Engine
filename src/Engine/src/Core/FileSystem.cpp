#include <Engine/Core/FileSystem.hpp>
#include <Engine/Debug/Log.hpp>


namespace Antelope
{
    std::unordered_map<std::string, std::filesystem::path> FileSystem::s_MountPoints;

    void FileSystem::Mount(const std::string& alias, const std::filesystem::path& realPath)
    {
        s_MountPoints[alias] = realPath;
    }

    std::filesystem::path FileSystem::Resolve(const std::string& virtualPath)
    {
        size_t sep { virtualPath.find("://") };
        if (sep == std::string::npos)
        {
            AE_ENGINE_WARN("FileSystem: '{0}' has no mount point prefix.", virtualPath);
            return virtualPath;
        }

        std::string alias { virtualPath.substr(0, sep) };
        std::string relative { virtualPath.substr(sep + 3) };

        auto it { s_MountPoints.find(alias) };
        if (it == s_MountPoints.end())
        {
            AE_ENGINE_ERROR("FileSystem: Mount point '{0}' not registered.", alias);
            return virtualPath;
        }

        return it->second / relative;
    }
}