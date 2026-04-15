#pragma once

#include <Engine/Core/UUID.hpp>

#include <string>
#include <vector>


namespace Antelope
{
    class World;

    struct AssetBinding
    {
        UUID EntityID;
        UUID AssetUUID;
        std::string ComponentType;
        uint32_t MeshIndex { 0 };
    };

    class SceneSerializer
    {
        public:
            static void Serialize(const std::string& virtualPath, World& world, const std::vector<AssetBinding>& bindings);
            static std::vector<AssetBinding> Deserialize(const std::string& virtualPath, World& world);
    };
}