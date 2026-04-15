#pragma once

#include <Engine/Asset/AssetTypes.hpp>

#include <glm/glm.hpp>
#include <filesystem>
#include <string>
#include <vector>


namespace Antelope::Editor
{
    struct ProjectState
    {
        std::string ProjectName;
        std::string LastScene;
        glm::vec3 CameraPosition { 0.0f, 2.0f, 20.0f };
        float CameraYaw { -90.0f };
        float CameraPitch { 0.0f };
        std::vector<AssetRecord> LastKnownAssets;
    };

    class Project
    {
        public:
            static std::filesystem::path FindProjectFile(const std::filesystem::path& projectRoot);
            static bool Load(const std::filesystem::path& projectFilePath, ProjectState& outState);
            static void Save(const std::filesystem::path& projectFilePath, const ProjectState& state);
    };
}