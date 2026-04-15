#pragma once

#include <Engine/Core/UUID.hpp>

#include <filesystem>
#include <string>


namespace Antelope
{
    enum class AssetType : uint16_t
    {
        None = 0,
        Scene,
        Texture2D,
        Mesh,
        Material
    };

    struct AssetRecord
    {
        UUID Handle { 0 };
        std::string RelativePath;
        AssetType Type { AssetType::None };
        int64_t LastModified { 0 };
    };

    struct AssetMetadata
    {
        UUID Handle;
        AssetType Type;
        std::filesystem::path FilePath;
        bool IsDataLoaded = false;
        
        bool IsValid() const { return Handle != 0 && !FilePath.empty(); }
    };
}