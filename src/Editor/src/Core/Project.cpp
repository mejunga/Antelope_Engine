#include <Editor/Core/Project.hpp>

#include <Engine/Debug/Log.hpp>

#include <yaml-cpp/yaml.h>
#include <fstream>


namespace Antelope::Editor
{
    std::filesystem::path Project::FindProjectFile(const std::filesystem::path& projectRoot)
    {
        std::error_code ec;

        for (auto& entry : std::filesystem::directory_iterator(projectRoot, ec))
        {
            if (entry.path().extension() == ".antelopeproject") { return entry.path(); }
        }

        return {};
    }

    bool Project::Load(const std::filesystem::path& projectFilePath, ProjectState& outState)
    {
        if (!std::filesystem::exists(projectFilePath)) { return false; }

        try
        {
            YAML::Node data { YAML::LoadFile(projectFilePath.string()) };

            outState.ProjectName = data["ProjectName"].as<std::string>("Unnamed Project");
            outState.LastScene = data["LastScene"].as<std::string>("");

            if (auto cam { data["Camera"] })
            {
                auto pos { cam["Position"] };
                outState.CameraPosition = { pos[0].as<float>(), pos[1].as<float>(), pos[2].as<float>() };
                outState.CameraYaw = cam["Yaw"].as<float>(-90.0f);
                outState.CameraPitch = cam["Pitch"].as<float>(0.0f);
            }

            outState.LastKnownAssets.clear();

            if (auto assets { data["Assets"] })
            {
                for (auto node : assets)
                {
                    AssetRecord record;
                    record.Handle = UUID(node["UUID"].as<uint64_t>());
                    record.RelativePath = node["Path"].as<std::string>();
                    record.Type = static_cast<AssetType>(node["Type"].as<uint16_t>());
                    record.LastModified = node["LastModified"].as<int64_t>();
                    outState.LastKnownAssets.push_back(record);
                }
            }
        }
        catch (YAML::Exception& e)
        {
            AE_CLIENT_ERROR("Project: Failed to load '{0}': {1}", projectFilePath.string(), e.what());
            return false;
        }

        return true;
    }

    void Project::Save(const std::filesystem::path& projectFilePath, const ProjectState& state)
    {
        std::filesystem::create_directories(projectFilePath.parent_path());

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "ProjectName" << YAML::Value << state.ProjectName;
        out << YAML::Key << "LastScene" << YAML::Value << state.LastScene;
        out << YAML::Key << "Camera" << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "Position" << YAML::Value
            << YAML::Flow << YAML::BeginSeq
            << state.CameraPosition.x << state.CameraPosition.y << state.CameraPosition.z
            << YAML::EndSeq;
        out << YAML::Key << "Yaw" << YAML::Value << state.CameraYaw;
        out << YAML::Key << "Pitch" << YAML::Value << state.CameraPitch;
        out << YAML::EndMap;

        out << YAML::Key << "Assets" << YAML::Value << YAML::BeginSeq;

        for (const auto& record : state.LastKnownAssets)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "UUID" << YAML::Value << (uint64_t)record.Handle;
            out << YAML::Key << "Path" << YAML::Value << record.RelativePath;
            out << YAML::Key << "Type" << YAML::Value << (uint16_t)record.Type;
            out << YAML::Key << "LastModified" << YAML::Value << record.LastModified;
            out << YAML::EndMap;
        }

        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::ofstream fout(projectFilePath);
        fout << out.c_str();
    }
}