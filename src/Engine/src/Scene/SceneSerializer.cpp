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

        if (entity.HasComponent<DisabledComponent>())
        {
            out << YAML::Key << "Disabled" << YAML::Value << true;
        }

        if (entity.HasComponent<TagComponent>())
        {
            out << YAML::Key << "TagComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Tag" << YAML::Value << entity.GetComponent<TagComponent>().Tag;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<TransformComponent>())
        {
            const auto& tc { entity.GetComponent<TransformComponent>() };
            out << YAML::Key << "TransformComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Translation" << YAML::Value; EmitVec3(out, tc.Translation);
            out << YAML::Key << "Rotation" << YAML::Value; EmitVec3(out, tc.Rotation);
            out << YAML::Key << "Scale" << YAML::Value; EmitVec3(out, tc.Scale);
            out << YAML::EndMap;
        }

        if (entity.HasComponent<MeshComponent>())
        {
            const auto& mesh { entity.GetComponent<MeshComponent>() };
            out << YAML::Key << "MeshComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Offset" << YAML::Value; EmitVec3(out, mesh.Offset);
            out << YAML::Key << "Scale" << YAML::Value; EmitVec3(out, mesh.Scale);
            out << YAML::EndMap;
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

        if (entity.HasComponent<DirectionalLightComponent>())
        {
            const auto& light { entity.GetComponent<DirectionalLightComponent>() };
            out << YAML::Key << "DirectionalLightComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Color" << YAML::Value; EmitVec3(out, light.Color);
            out << YAML::Key << "Intensity" << YAML::Value << light.Intensity;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<PointLightComponent>())
        {
            const auto& light { entity.GetComponent<PointLightComponent>() };
            out << YAML::Key << "PointLightComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Color" << YAML::Value; EmitVec3(out, light.Color);
            out << YAML::Key << "Intensity" << YAML::Value << light.Intensity;
            out << YAML::Key << "Radius" << YAML::Value << light.Radius;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<SpotLightComponent>())
        {
            const auto& light { entity.GetComponent<SpotLightComponent>() };
            out << YAML::Key << "SpotLightComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Color" << YAML::Value; EmitVec3(out, light.Color);
            out << YAML::Key << "Intensity" << YAML::Value << light.Intensity;
            out << YAML::Key << "Radius" << YAML::Value << light.Radius;
            out << YAML::Key << "InnerCutOff" << YAML::Value << light.InnerCutOff;
            out << YAML::Key << "OuterCutOff" << YAML::Value << light.OuterCutOff;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<AmbientComponent>())
        {
            const auto& amb { entity.GetComponent<AmbientComponent>() };
            out << YAML::Key << "AmbientComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "SkyColorDay" << YAML::Value; EmitVec3(out, amb.SkyColorDay);
            out << YAML::Key << "HorizonColorDay" << YAML::Value; EmitVec3(out, amb.HorizonColorDay);
            out << YAML::Key << "GroundColor" << YAML::Value; EmitVec3(out, amb.GroundColor);
            out << YAML::Key << "SkyColorNight" << YAML::Value; EmitVec3(out, amb.SkyColorNight);
            out << YAML::Key << "HorizonColorNight" << YAML::Value; EmitVec3(out, amb.HorizonColorNight);
            out << YAML::Key << "StarIntensity" << YAML::Value << amb.StarIntensity;
            
            if (amb.SunEntity != entt::null)
            {
                out << YAML::Key << "SunEntity" << YAML::Value << (uint64_t)Entity(amb.SunEntity, &world).GetComponent<IDComponent>().ID;
            }

            if (amb.MoonEntity != entt::null)
            {
                out << YAML::Key << "MoonEntity" << YAML::Value << (uint64_t)Entity(amb.MoonEntity, &world).GetComponent<IDComponent>().ID;
            }

            out << YAML::Key << "SunMaxIntensity" << YAML::Value << amb.SunMaxIntensity;
            out << YAML::Key << "MoonMaxIntensity" << YAML::Value << amb.MoonMaxIntensity;
            
            out << YAML::EndMap;
        }

        if (entity.HasComponent<TimeCycleComponent>())
        {
            const auto& time { entity.GetComponent<TimeCycleComponent>() };
            out << YAML::Key << "TimeCycleComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "TimeOfDay" << YAML::Value << time.TimeOfDay;
            out << YAML::Key << "TimeScale" << YAML::Value << time.TimeScale;
            out << YAML::Key << "CurrentDay" << YAML::Value << time.CurrentDay;
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
            auto entityIt { entityMap.find(entityNode["Entity"].as<uint64_t>()) };
            
            if (entityIt == entityMap.end())
            {
                AE_ENGINE_ERROR("SceneSerializer: entity UUID not found during component load, skipping.");
                continue;
            }
            
            Entity entity { entityIt->second };
            
            if (auto parentNode { entityNode["ParentID"] })
            {
                auto it { entityMap.find(parentNode.as<uint64_t>()) };

                if (it != entityMap.end()) { entity.SetParent(it->second); }
            }

            if (entityNode["Disabled"] && entityNode["Disabled"].as<bool>())
            {
                entity.SetActive(false);
            }

            if (auto node { entityNode["TransformComponent"] })
            {
                auto& tc { entity.GetComponent<TransformComponent>() };
                tc.Translation = DecodeVec3(node["Translation"]);
                tc.Rotation = DecodeVec3(node["Rotation"]);
                tc.Scale = DecodeVec3(node["Scale"]);
                world.MarkTransformDirty(entity);
            }

            if (auto node { entityNode["MeshComponent"] })
            {
                auto& mesh { entity.AddComponent<MeshComponent>() };
                if (node["Offset"]) { mesh.Offset = DecodeVec3(node["Offset"]); }
                if (node["Scale"])  { mesh.Scale  = DecodeVec3(node["Scale"]); }
            }

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

            if (auto node { entityNode["DirectionalLightComponent"] })
            {
                auto& light { entity.AddComponent<DirectionalLightComponent>() };
                light.Color = DecodeVec3(node["Color"]);
                light.Intensity = node["Intensity"].as<float>();
            }

            if (auto node { entityNode["PointLightComponent"] })
            {
                auto& light { entity.AddComponent<PointLightComponent>() };
                light.Color = DecodeVec3(node["Color"]);
                light.Intensity = node["Intensity"].as<float>();
                light.Radius = node["Radius"].as<float>();
            }

            if (auto node { entityNode["SpotLightComponent"] })
            {
                auto& light { entity.AddComponent<SpotLightComponent>() };
                light.Color = DecodeVec3(node["Color"]);
                light.Intensity = node["Intensity"].as<float>();
                light.Radius = node["Radius"].as<float>();
                light.InnerCutOff = node["InnerCutOff"].as<float>();
                light.OuterCutOff = node["OuterCutOff"].as<float>();
            }

            if (auto node { entityNode["AmbientComponent"] })
            {
                auto& amb { entity.AddComponent<AmbientComponent>() };
                amb.SkyColorDay = DecodeVec3(node["SkyColorDay"]);
                amb.HorizonColorDay = DecodeVec3(node["HorizonColorDay"]);
                amb.GroundColor = DecodeVec3(node["GroundColor"]);
                amb.SkyColorNight = DecodeVec3(node["SkyColorNight"]);
                amb.HorizonColorNight = DecodeVec3(node["HorizonColorNight"]);
                amb.StarIntensity = node["StarIntensity"].as<float>();

                if (auto sunNode { node["SunEntity"] })
                {
                    uint64_t sunID { sunNode.as<uint64_t>() };
                
                    if (entityMap.find(sunID) != entityMap.end()) { amb.SunEntity = entityMap[sunID]; }
                }

                if (auto moonNode { node["MoonEntity"] })
                {
                    uint64_t moonID { moonNode.as<uint64_t>() };
                    
                    if (entityMap.find(moonID) != entityMap.end()) { amb.MoonEntity = entityMap[moonID]; }
                }
                
                if (node["SunMaxIntensity"]) { amb.SunMaxIntensity = node["SunMaxIntensity"].as<float>(); }
                if (node["MoonMaxIntensity"]) { amb.MoonMaxIntensity = node["MoonMaxIntensity"].as<float>(); }
            }

            if (auto node { entityNode["TimeCycleComponent"] })
            {
                auto& time { entity.HasComponent<TimeCycleComponent>()
                           ? entity.GetComponent<TimeCycleComponent>()
                           : entity.AddComponent<TimeCycleComponent>() };
                time.TimeOfDay = node["TimeOfDay"].as<float>();
                time.TimeScale = node["TimeScale"].as<float>();
                time.CurrentDay = node["CurrentDay"].as<uint32_t>();
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