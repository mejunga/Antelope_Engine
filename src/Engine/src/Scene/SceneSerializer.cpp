#include <Engine/Scene/SceneSerializer.hpp>
#include <Engine/Core/FileSystem.hpp>
#include <Engine/ECS/World.hpp>
#include <Engine/ECS/Entity.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/Debug/Log.hpp>

#include <yaml-cpp/yaml.h>
#include <fstream>
#include <filesystem>
#include <unordered_map>


namespace Antelope
{
    static void EmitVec3(YAML::Emitter& out, const glm::vec3& v)
    {
        out << YAML::Flow << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
    }

    static glm::vec3 DecodeVec3(const YAML::Node& node)
    {
        return { node[0].as<float>(), node[1].as<float>(), node[2].as<float>() };
    }

    static void SerializeEntity(YAML::Emitter& out, World& world, Entity entity)
    {
        out << YAML::BeginMap;
        out << YAML::Key << "Entity" << YAML::Value << (uint64_t)entity.GetComponent<IDComponent>().ID;

        auto& rel { entity.GetComponent<RelationshipComponent>() };

        if (rel.Parent != entt::null)
        {
            Entity parent { rel.Parent, &world };
            out << YAML::Key << "ParentID" << YAML::Value << (uint64_t)parent.GetComponent<IDComponent>().ID;
        }

        if (entity.HasComponent<TagComponent>())
        {
            out << YAML::Key << "TagComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Tag" << YAML::Value << entity.GetComponent<TagComponent>().Tag;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<TransformComponent>())
        {
            const auto& t { entity.GetComponent<TransformComponent>() };
            out << YAML::Key << "TransformComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Translation" << YAML::Value; EmitVec3(out, t.Translation);
            out << YAML::Key << "Rotation" << YAML::Value; EmitVec3(out, t.Rotation);
            out << YAML::Key << "Scale" << YAML::Value; EmitVec3(out, t.Scale);
            out << YAML::EndMap;
        }

        if (entity.HasComponent<MeshComponent>())
        {
            out << YAML::Key << "MeshComponent" << YAML::Value << YAML::Null;
        }

        if (entity.HasComponent<CameraComponent>())
        {
            const auto& cam { entity.GetComponent<CameraComponent>() };
            out << YAML::Key << "CameraComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "FOV" << YAML::Value << cam.PerspectiveFOV;
            out << YAML::Key << "Near" << YAML::Value << cam.PerspectiveNear;
            out << YAML::Key << "Far" << YAML::Value << cam.PerspectiveFar;
            out << YAML::Key << "IsPrimary" << YAML::Value << cam.IsPrimary;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<RigidBodyComponent>())
        {
            const auto& rb { entity.GetComponent<RigidBodyComponent>() };
            out << YAML::Key << "RigidBodyComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Type" << YAML::Value << (int)rb.Type;
            out << YAML::Key << "Mass" << YAML::Value << rb.Mass;
            out << YAML::Key << "Friction" << YAML::Value << rb.Friction;
            out << YAML::Key << "Restitution" << YAML::Value << rb.Restitution;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<ColliderComponent>())
        {
            const auto& col { entity.GetComponent<ColliderComponent>() };
            out << YAML::Key << "ColliderComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Type" << YAML::Value << (int)col.Type;
            out << YAML::Key << "Size" << YAML::Value; EmitVec3(out, col.Size);
            out << YAML::Key << "Offset" << YAML::Value; EmitVec3(out, col.Offset);
            out << YAML::EndMap;
        }

        out << YAML::EndMap;

        entt::entity child { rel.FirstChild };

        while (child != entt::null)
        {
            auto& childRel { world.GetRegistry().get<RelationshipComponent>(child) };
            SerializeEntity(out, world, { child, &world });
            child = childRel.NextSibling;
        }
    }

    void SceneSerializer::Serialize(const std::string& virtualPath, World& world, const std::vector<AssetBinding>& bindings)
    {
        std::filesystem::path path { FileSystem::Resolve(virtualPath) };
        std::filesystem::create_directories(path.parent_path());

        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Scene" << YAML::Value << path.stem().string();

        out << YAML::Key << "AssetBindings" << YAML::Value << YAML::BeginSeq;

        for (const auto& binding : bindings)
        {
            out << YAML::BeginMap;
            out << YAML::Key << "EntityID" << YAML::Value << (uint64_t)binding.EntityID;
            out << YAML::Key << "ComponentType" << YAML::Value << binding.ComponentType;
            out << YAML::Key << "AssetUUID" << YAML::Value << (uint64_t)binding.AssetUUID;
            out << YAML::Key << "MeshIndex" << YAML::Value << binding.MeshIndex;
            out << YAML::EndMap;
        }

        out << YAML::EndSeq;

        out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
        auto& reg { world.GetRegistry() };
        auto view { reg.view<IDComponent>() };

        for (auto e : view)
        {
            if (reg.get<RelationshipComponent>(e).Parent == entt::null)
            {
                SerializeEntity(out, world, { e, &world });
            }
        }

        out << YAML::EndSeq;
        out << YAML::EndMap;

        std::ofstream fout(path);
        fout << out.c_str();
    }

    std::vector<AssetBinding> SceneSerializer::Deserialize(const std::string& virtualPath, World& world)
    {
        std::filesystem::path path { FileSystem::Resolve(virtualPath) };

        if (!std::filesystem::exists(path))
        {
            AE_ENGINE_ERROR("SceneSerializer: File not found: '{0}'", path.string());
            return {};
        }

        YAML::Node data;

        try
        {
            data = YAML::LoadFile(path.string());
        }
        catch (YAML::ParserException& e)
        {
            AE_ENGINE_ERROR("SceneSerializer: Failed to parse '{0}': {1}", path.string(), e.what());
            return {};
        }

        auto entitiesNode { data["Entities"] };

        if (!entitiesNode) { return {}; }

        std::unordered_map<uint64_t, Entity> entityMap;

        for (auto entityNode : entitiesNode)
        {
            uint64_t id { entityNode["Entity"].as<uint64_t>() };
            std::string name { "Entity" };

            if (entityNode["TagComponent"]) { name = entityNode["TagComponent"]["Tag"].as<std::string>(); }
            
            entityMap[id] = world.CreateEntity(name, UUID(id));
        }

        for (auto entityNode : entitiesNode)
        {
            Entity entity { entityMap[entityNode["Entity"].as<uint64_t>()] };

            if (auto parentNode { entityNode["ParentID"] })
            {
                auto it { entityMap.find(parentNode.as<uint64_t>()) };

                if (it != entityMap.end()) { entity.SetParent(it->second); }
            }

            if (auto node { entityNode["TransformComponent"] })
            {
                auto& t { entity.GetComponent<TransformComponent>() };
                t.Translation = DecodeVec3(node["Translation"]);
                t.Rotation = DecodeVec3(node["Rotation"]);
                t.Scale = DecodeVec3(node["Scale"]);
                world.MarkTransformDirty(entity);
            }

            if (entityNode["MeshComponent"]) { entity.AddComponent<MeshComponent>(); }

            if (auto node { entityNode["CameraComponent"] })
            {
                auto& cam { entity.AddComponent<CameraComponent>() };
                cam.PerspectiveFOV = node["FOV"].as<float>();
                cam.PerspectiveNear = node["Near"].as<float>();
                cam.PerspectiveFar = node["Far"].as<float>();
                cam.IsPrimary = node["IsPrimary"].as<bool>();
            }

            if (auto node { entityNode["RigidBodyComponent"] })
            {
                auto& rb { entity.AddComponent<RigidBodyComponent>() };
                rb.Type = (RigidBodyType)node["Type"].as<int>();
                rb.Mass = node["Mass"].as<float>();
                rb.Friction = node["Friction"].as<float>();
                rb.Restitution = node["Restitution"].as<float>();
            }

            if (auto node { entityNode["ColliderComponent"] })
            {
                auto& col { entity.AddComponent<ColliderComponent>() };
                col.Type = (ColliderType)node["Type"].as<int>();
                col.Size = DecodeVec3(node["Size"]);
                col.Offset = DecodeVec3(node["Offset"]);
            }
        }

        std::vector<AssetBinding> bindings;
        
        if (auto bindingsNode { data["AssetBindings"] })
        {
            for (auto node : bindingsNode)
            {
                bindings.push_back({
                    UUID(node["EntityID"].as<uint64_t>()),
                    UUID(node["AssetUUID"].as<uint64_t>()),
                    node["ComponentType"].as<std::string>(),
                    node["MeshIndex"].as<uint32_t>()
                });
            }
        }

        return bindings;
    }
}