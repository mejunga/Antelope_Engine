#include <Engine/Scene/SceneSerializer.hpp>
#include <Engine/Core/FileSystem.hpp>
#include <Engine/ECS/World.hpp>
#include <Engine/ECS/Entity.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Scripting/ComponentRegistry.hpp>

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
            out << YAML::Key << "Prio" << YAML::Value << cam.prio;
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

        if (entity.HasComponent<MeshColliderComponent>())
        {
            out << YAML::Key << "MeshColliderComponent" << YAML::Value << true;
        }

        if (entity.HasComponent<AudioPlayerComponent>())
        {
            const auto& player { entity.GetComponent<AudioPlayerComponent>() };
            out << YAML::Key << "AudioPlayerComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Clips" << YAML::Value << YAML::BeginSeq;
            for (const auto& clip : player.Clips)
            {
                out << YAML::BeginMap;
                out << YAML::Key << "AudioAssetUUID" << YAML::Value << (uint64_t)clip.AudioAssetUUID;
                out << YAML::Key << "Volume" << YAML::Value << clip.Volume;
                out << YAML::Key << "Loop" << YAML::Value << clip.Loop;
                out << YAML::Key << "PlayOnStart" << YAML::Value << clip.PlayOnStart;
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;
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

        if (entity.HasComponent<SkinnedMeshComponent>())
        {
            const auto& smc { entity.GetComponent<SkinnedMeshComponent>() };
            out << YAML::Key << "SkinnedMeshComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "ModelAssetUUID" << YAML::Value << (uint64_t)smc.ModelAssetUUID;
            out << YAML::EndMap;
        }

        if (entity.HasComponent<AnimatorComponent>())
        {
            const auto& anim { entity.GetComponent<AnimatorComponent>() };
            const auto& ctrl { anim.Controller };
            out << YAML::Key << "AnimatorComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "Speed" << YAML::Value << anim.Speed;
            out << YAML::Key << "DefaultState" << YAML::Value << ctrl.DefaultStateIndex;

            out << YAML::Key << "Clips" << YAML::Value << YAML::BeginSeq;
            for (const auto& clip : ctrl.Clips) { out << clip.Name; }
            out << YAML::EndSeq;

            out << YAML::Key << "States" << YAML::Value << YAML::BeginSeq;
            for (const auto& state : ctrl.States)
            {
                out << YAML::BeginMap;
                out << YAML::Key << "Name" << YAML::Value << state.Name;
                out << YAML::Key << "ClipIndex" << YAML::Value << state.ClipIndex;
                out << YAML::Key << "Speed" << YAML::Value << state.Speed;
                out << YAML::Key << "Loop" << YAML::Value << state.Loop;
                out << YAML::Key << "EditorPos" << YAML::Value << YAML::Flow << YAML::BeginSeq << state.EditorPos.x << state.EditorPos.y << YAML::EndSeq;
                out << YAML::Key << "Events" << YAML::Value << YAML::BeginSeq;
                for (const auto& evt : state.Events)
                {
                    out << YAML::BeginMap;
                    out << YAML::Key << "Name" << YAML::Value << evt.Name;
                    out << YAML::Key << "Time" << YAML::Value << evt.NormalizedTime;
                    out << YAML::EndMap;
                }
                out << YAML::EndSeq;
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;

            out << YAML::Key << "Transitions" << YAML::Value << YAML::BeginSeq;
            for (const auto& t : ctrl.Transitions)
            {
                out << YAML::BeginMap;
                out << YAML::Key << "FromState" << YAML::Value << t.FromState;
                out << YAML::Key << "ToState" << YAML::Value << t.ToState;
                out << YAML::Key << "BlendDuration" << YAML::Value << t.BlendDuration;
                out << YAML::Key << "HasExitTime" << YAML::Value << t.HasExitTime;
                out << YAML::Key << "ExitTime" << YAML::Value << t.ExitTime;
                out << YAML::Key << "Conditions" << YAML::Value << YAML::BeginSeq;
                for (const auto& cond : t.Conditions)
                {
                    out << YAML::BeginMap;
                    out << YAML::Key << "Param" << YAML::Value << cond.ParameterIndex;
                    out << YAML::Key << "Op" << YAML::Value << (int)cond.Operation;
                    out << YAML::Key << "Threshold" << YAML::Value << cond.Threshold;
                    out << YAML::EndMap;
                }
                out << YAML::EndSeq;
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;

            out << YAML::Key << "Parameters" << YAML::Value << YAML::BeginSeq;
            for (const auto& param : ctrl.Parameters)
            {
                out << YAML::BeginMap;
                out << YAML::Key << "Name" << YAML::Value << param.Name;
                out << YAML::Key << "Type" << YAML::Value << (int)param.ParamType;
                out << YAML::Key << "FloatValue" << YAML::Value << param.FloatValue;
                out << YAML::Key << "IntValue" << YAML::Value << param.IntValue;
                out << YAML::Key << "BoolValue" << YAML::Value << param.BoolValue;
                out << YAML::EndMap;
            }
            out << YAML::EndSeq;

            out << YAML::EndMap;
        }

        if (entity.HasComponent<ScriptComponent>())
        {
            const auto& sc { entity.GetComponent<ScriptComponent>() };
            out << YAML::Key << "ScriptComponent" << YAML::Value << YAML::BeginMap;
            out << YAML::Key << "ClassName" << YAML::Value << sc.ScriptClassName;
            out << YAML::EndMap;
        }

        ComponentRegistry::SerializeAll(out, world.GetRegistry(), entity.GetHandle());

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

        std::pmr::unordered_map<uint64_t, Entity> entityMap;

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
                cam.prio = node["Prio"].as<uint32_t>();
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

            if (entityNode["MeshColliderComponent"])
            {
                entity.AddComponent<MeshColliderComponent>();
            }

            if (auto node { entityNode["AudioPlayerComponent"] })
            {
                auto& player { entity.AddComponent<AudioPlayerComponent>() };
                if (auto clips { node["Clips"] })
                {
                    for (auto clipNode : clips)
                    {
                        AudioClip clip;
                        
                        if (clipNode["AudioAssetUUID"])
                        {
                            clip.AudioAssetUUID = UUID(clipNode["AudioAssetUUID"].as<uint64_t>());
                        }
                        
                        clip.Volume = clipNode["Volume"].as<float>();
                        clip.Loop = clipNode["Loop"].as<bool>();
                        clip.PlayOnStart = clipNode["PlayOnStart"].as<bool>();
                        player.Clips.push_back(std::move(clip));
                    }
                }
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

            if (auto node { entityNode["SkinnedMeshComponent"] })
            {
                auto& smc { entity.AddComponent<SkinnedMeshComponent>() };
                smc.ModelAssetUUID = UUID(node["ModelAssetUUID"].as<uint64_t>());
            }

            if (auto node { entityNode["AnimatorComponent"] })
            {
                auto& anim { entity.HasComponent<AnimatorComponent>()
                        ? entity.GetComponent<AnimatorComponent>()
                        : entity.AddComponent<AnimatorComponent>() };
                auto& ctrl { anim.Controller };

                anim.Speed = node["Speed"].as<float>();
                ctrl.DefaultStateIndex = node["DefaultState"].as<uint32_t>();

                if (auto clipsNode { node["Clips"] })
                {
                    ctrl.Clips.clear();
                    for (auto cn : clipsNode)
                    {
                        AnimationClip clip;
                        clip.Name = cn.as<std::string>();
                        ctrl.Clips.push_back(std::move(clip));
                    }
                }

                if (auto statesNode { node["States"] })
                {
                    ctrl.States.clear();
                    for (auto sn : statesNode)
                    {
                        AnimationStateNode state;
                        state.Name = sn["Name"].as<std::string>();
                        state.ClipIndex = sn["ClipIndex"].as<uint32_t>();
                        state.Speed = sn["Speed"].as<float>();
                        state.Loop = sn["Loop"].as<bool>();

                        if (sn["EditorPos"]) { state.EditorPos = { sn["EditorPos"][0].as<float>(), sn["EditorPos"][1].as<float>() }; }
                        
                        if (auto evtsNode { sn["Events"] })
                        {
                            for (auto en : evtsNode)
                            {
                                AnimationEvent evt;
                                evt.Name = en["Name"].as<std::string>();
                                evt.NormalizedTime = en["Time"].as<float>();
                                state.Events.push_back(evt);
                            }
                        }
                        
                        ctrl.States.push_back(std::move(state));
                    }
                }

                if (auto transNode { node["Transitions"] })
                {
                    ctrl.Transitions.clear();
                    for (auto tn : transNode)
                    {
                        AnimationTransition t;
                        t.FromState = tn["FromState"].as<uint32_t>();
                        t.ToState = tn["ToState"].as<uint32_t>();
                        t.BlendDuration = tn["BlendDuration"].as<float>();
                        t.HasExitTime = tn["HasExitTime"].as<bool>();
                        t.ExitTime = tn["ExitTime"].as<float>();
                        if (auto condsNode { tn["Conditions"] })
                        {
                            for (auto cn : condsNode)
                            {
                                TransitionCondition cond;
                                cond.ParameterIndex = cn["Param"].as<uint32_t>();
                                cond.Operation = (TransitionCondition::Op)cn["Op"].as<int>();
                                cond.Threshold = cn["Threshold"].as<float>();
                                t.Conditions.push_back(cond);
                            }
                        }
                        ctrl.Transitions.push_back(std::move(t));
                    }
                }

                if (auto paramsNode { node["Parameters"] })
                {
                    ctrl.Parameters.clear();
                    for (auto pn : paramsNode)
                    {
                        AnimatorParameter param;
                        param.Name = pn["Name"].as<std::string>();
                        param.ParamType = (AnimatorParameter::Type)pn["Type"].as<int>();
                        param.FloatValue = pn["FloatValue"].as<float>();
                        param.IntValue = pn["IntValue"].as<int>();
                        param.BoolValue = pn["BoolValue"].as<bool>();
                        ctrl.Parameters.push_back(std::move(param));
                    }
                }
            }
            
            if (auto node { entityNode["ScriptComponent"] })
            {
                auto& sc { entity.AddComponent<ScriptComponent>() };
                sc.ScriptClassName = node["ClassName"].as<std::string>();
            }

            ComponentRegistry::DeserializeAll(entityNode, world.GetRegistry(), entity.GetHandle());
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