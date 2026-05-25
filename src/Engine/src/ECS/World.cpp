#include <Engine/ECS/World.hpp>
#include <Engine/ECS/Entity.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/ECS/System/RenderSystem.hpp>
#include <Engine/ECS/System/TransformSystem.hpp>
#include <Engine/ECS/System/PhysicsSystem.hpp>
#include <Engine/ECS/System/AmbientSystem.hpp>
#include <Engine/Physics/PhysicsContext.hpp>
#include <Engine/Core/Application.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Asset/AssetManager.hpp>
#include <Engine/Asset/TextureManager.hpp>
#include <Engine/ECS/System/AnimationSystem.hpp>
#include <Engine/ECS/System/AudioSystem.hpp>
#include <Engine/Audio/AudioContext.hpp>
#include <Engine/ECS/System/ScriptSystem.hpp>
#include <Engine/Renderer/Vulkan/RenderTexture.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>

#include <vector>
#include <unordered_map>


namespace Antelope
{
    World::World()
    {
        m_PhysicsContext = std::make_unique<PhysicsContext>(Application::Get().GetAllocator(), Application::Get().GetJobSystem());
        m_AudioContext = std::make_unique<AudioContext>();

        m_Registry.on_construct<TransformComponent>().connect<[](entt::registry& reg, entt::entity e)
        {
            reg.emplace_or_replace<LocalMatrixComponent>(e);
            reg.emplace_or_replace<WorldMatrixComponent>(e);
        }>();

        m_Registry.on_construct<MeshComponent>().connect<[](entt::registry& reg, entt::entity e)
        {
            reg.emplace_or_replace<NormalMatrixComponent>(e);
        }>();

        m_Registry.on_construct<CameraComponent>().connect<&World::OnCameraConstructed>(this);
        m_Registry.on_destroy<CameraComponent>().connect<&World::OnCameraDestroyed>(this);

        m_Registry.on_construct<AmbientComponent>().connect<[](entt::registry& reg, entt::entity e)
        {
            reg.emplace_or_replace<TimeCycleComponent>(e);
        }>();
    }

    World::~World() 
    {
        if (m_IsSimulating)
        {
            OnSimulationStop();
        }
    }

    Entity World::CreateEntity(const std::string& name, UUID uuid)
    {
        entt::entity handle { m_Registry.create() };
        Entity entity { handle, this };

        entity.AddComponent<IDComponent>().ID = uuid;
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<RelationshipComponent>();
        auto& tag { entity.AddComponent<TagComponent>() };
        tag.Tag = name.empty() ? "Entity" : name;
        
        MarkTransformDirty(entity);
        return entity;
    }

    Entity World::SpawnModel(const ModelData& modelData, const std::string& rootName, UUID modelAssetUUID, std::vector<AssetBinding>* outBindings)
    {
        return SpawnModel(modelData, rootName, modelAssetUUID, Entity{}, outBindings);
    }

    Entity World::SpawnModel(const ModelData& modelData, const std::string& rootName, UUID modelAssetUUID, Entity parentEntity, std::vector<AssetBinding>* outBindings)
    {
        Entity rootEntity { CreateEntity(rootName) };

        if (parentEntity) { rootEntity.SetParent(parentEntity); }

        if (modelData.BoneCount > 0)
        {
            rootEntity.AddComponent<SkinnedMeshComponent>().ModelAssetUUID = modelAssetUUID;
        }

        glm::vec3 scale, translation, skew;
        glm::quat rotation;
        glm::vec4 perspective;
        glm::decompose(modelData.RootNode.LocalTransform, scale, rotation, translation, skew, perspective);

        auto& rootTransform { rootEntity.GetComponent<TransformComponent>() };
        rootTransform.Translation = translation;
        rootTransform.Rotation = glm::eulerAngles(rotation);
        rootTransform.Scale = scale;
        MarkTransformDirty(rootEntity);

        auto renderer { Application::Get().GetRenderer() };
        auto textureManager { Application::Get().GetTextureManager() };

        std::vector<uint32_t> ssboMaterialIndices;
        ssboMaterialIndices.reserve(modelData.Materials.size());
        std::unordered_map<std::string, std::filesystem::path> texturePaths;

        for (const auto& [uuid, meta] : AssetManager::GetRegistry())
        {
            if (meta.Type != AssetType::Texture2D) { continue; }
            auto [it, inserted] { texturePaths.emplace(meta.FilePath.filename().string(), meta.FilePath) };
            if (!inserted)
            {
                AE_ENGINE_WARN("World: Duplicate texture filename '{0}': '{1}' vs '{2}'. Consider renaming one.",
                    meta.FilePath.filename().string(), it->second.string(), meta.FilePath.string());
            }
        }

        auto loadTex { [&](const std::string& filename, bool isSRGB) -> uint32_t {
            if (filename.empty()) { return 0xFFFFFFFF; }
            auto it { texturePaths.find(filename) };
            if (it == texturePaths.end())
            {
                std::string lower { filename };
                std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });
                for (const auto& [key, path] : texturePaths)
                {
                    std::string keyLower { key };
                    std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), [](unsigned char c){ return std::tolower(c); });
                    if (keyLower == lower) { it = texturePaths.find(key); break; }
                }
                if (it == texturePaths.end()) { return 0xFFFFFFFF; }
            }
            return textureManager->LoadTexture(it->second.string(), isSRGB);
        }};

        for (const auto& modelMat : modelData.Materials)
        {
            PBRMaterialData pbrMat;
            pbrMat.AlbedoFactor = modelMat.AlbedoFactor;
            pbrMat.MetallicRoughnessFactors = modelMat.MetallicRoughnessFactors;

            pbrMat.AlbedoTexIndex = loadTex(modelMat.AlbedoTexPath, true);
            pbrMat.EmissiveTexIndex = loadTex(modelMat.EmissiveTexPath, true);
            pbrMat.NormalTexIndex = loadTex(modelMat.NormalTexPath, false);
            pbrMat.MetRoughAOTexIndex = loadTex(modelMat.MetRoughAOTexPath, false);

            ssboMaterialIndices.push_back(renderer->AddMaterial(pbrMat));
        }

        if (!modelData.RootNode.Children.empty())
        {
            for (const auto& childNode : modelData.RootNode.Children)
            {
                SpawnModelNodeRecursive(childNode, modelData, modelAssetUUID, rootEntity, outBindings, ssboMaterialIndices);
            }

            return rootEntity;
        }

        for (uint32_t i { 0 }; i < modelData.SubMeshes.size(); ++i)
        {
            const auto& subMesh { modelData.SubMeshes[i] };
            Entity part { CreateEntity(subMesh.Name.empty() ? rootName + "_Part" : subMesh.Name) };
            part.SetParent(rootEntity);
            auto& meshComp { part.AddComponent<MeshComponent>() };
            meshComp.Handle = renderer->UploadMesh(subMesh.Data);

            uint32_t matIdx { subMesh.MaterialIndex };
            
            if (matIdx < ssboMaterialIndices.size())
            {
                part.AddComponent<MaterialComponent>().MaterialIndex = ssboMaterialIndices[matIdx];
            }

            if (outBindings)
            {
                UUID entityUUID { m_Registry.get<IDComponent>(part.GetHandle()).ID };
                outBindings->push_back({ entityUUID, modelAssetUUID, "MeshComponent", i });
            }
        }

        return rootEntity;
    }

    void World::DestroyEntity(Entity entity)
    {
        if (m_IsSimulating && m_Registry.all_of<RigidBodyComponent>(entity))
        {
            auto& rb { m_Registry.get<RigidBodyComponent>(entity) };
            
            if (rb.RuntimeBodyID != 0xFFFFFFFF)
            {
                auto& bodyInterface { m_PhysicsContext->GetBodyInterface() };
                JPH::BodyID bodyID(rb.RuntimeBodyID);
                
                bodyInterface.RemoveBody(bodyID);
                bodyInterface.DestroyBody(bodyID);
            }
        }

        m_Registry.destroy(entity);
        MarkHierarchyDirty();
    }

    void World::MarkTransformDirty(Entity entity)
    {
        m_Registry.emplace_or_replace<DirtyTransform>(entity);
    }

    void World::Clear()
    {
        if (m_IsSimulating) { OnSimulationStop(); }
        m_Registry.clear();
        m_HierarchyDirty = true;
        m_PrimaryCamera = entt::null;
        Application::Get().GetRenderer()->ClearMaterials();
    }

    void World::TakeSnapshot()
    {
        m_SimulationSnapshot.clear();

        for (auto [entity, transform] : m_Registry.view<TransformComponent>().each())
        {
            m_SimulationSnapshot.push_back({ entity, transform });
        }

        m_AnimatorSnapshot.clear();
        
        for (auto [entity, anim] : m_Registry.view<AnimatorComponent>().each())
        {
            m_AnimatorSnapshot.push_back({ entity, anim });
        }
    }

    void World::RestoreSnapshot()
    {
        for (auto& [entity, transform] : m_SimulationSnapshot)
        {
            if (m_Registry.valid(entity) && m_Registry.all_of<TransformComponent>(entity))
            {
                m_Registry.get<TransformComponent>(entity) = transform;
                m_Registry.emplace_or_replace<DirtyTransform>(entity);
            }
        }

        m_SimulationSnapshot.clear();

        for (auto& [entity, anim] : m_AnimatorSnapshot)
        {
            if (m_Registry.valid(entity) && m_Registry.all_of<AnimatorComponent>(entity))
            {
                auto& current { m_Registry.get<AnimatorComponent>(entity) };
                ModelData* model { current.Model };
                current = anim;
                current.Model = model;
            }
        }

        m_AnimatorSnapshot.clear();

        TransformSystem::OnUpdate(*this);
    }

    void World::OnSimulationStart()
    {
        if (m_IsSimulating) { return; }

        PhysicsSystem::OnRuntimeStart(*this, *m_PhysicsContext);
        AudioSystem::OnRuntimeStart(*this, *m_AudioContext);
        ScriptSystem::OnRuntimeStart(*this);
        m_PhysicsContext->OptimizeBroadPhase();
        
        m_IsSimulating = true;
        AE_ENGINE_INFO("World Simulation Started.");
    }

    void World::OnSimulationStop()
    {
        if (!m_IsSimulating) { return; }

        ScriptSystem::OnRuntimeStop(*this);
        AudioSystem::OnRuntimeStop(*this, *m_AudioContext);
        PhysicsSystem::OnRuntimeStop(*this, *m_PhysicsContext);
        
        m_IsSimulating = false;
        AE_ENGINE_INFO("World Simulation Stopped.");
    }

    void World::StepSimulation(float timeStep)
    {
        if (m_IsSimulating)
        {
            PhysicsSystem::OnUpdate(*this, *m_PhysicsContext, timeStep);
        }
    }

#ifdef ANTELOPE_EDITOR_MODE
    void World::OnUpdateEditor(float timeStep, const EditorCamera& camera)
    {
        if (m_HierarchyDirty)
        {
            TransformSystem::SortHierarchy(*this);
            m_HierarchyDirty = false;
        }

        float simDelta { (m_IsSimulating && !m_IsPaused) ? timeStep : 0.0f };
        AmbientSystem::OnUpdate(*this, simDelta);
        AnimationSystem::OnUpdate(*this, simDelta);

        if (m_IsSimulating && !m_IsPaused) { ScriptSystem::OnUpdate(*this, timeStep); }
        
        TransformSystem::OnUpdate(*this);

        auto renderer { Application::Get().GetRenderer() };

        if (m_GameViewActive)
        {
            if (m_PrimaryCamera != entt::null && m_Registry.all_of<WorldMatrixComponent, CameraComponent>(m_PrimaryCamera))
            {
                auto& worldMat { m_Registry.get<WorldMatrixComponent>(m_PrimaryCamera) };
                auto& cam { m_Registry.get<CameraComponent>(m_PrimaryCamera) };
                auto extent { renderer->GetFinalLDRTexture()->GetExtent() };
                cam.CalculateProjection(static_cast<float>(extent.width) / static_cast<float>(extent.height));
                glm::mat4 view { glm::inverse(worldMat.Matrix) };
                glm::vec3 pos { worldMat.Matrix[3] };
                RenderSystem::RenderEditor(*this, renderer, view, cam.Projection, pos);
            }
            else
            {
                RenderSystem::RenderBlack(renderer);
            }
        }
        else
        {
            RenderSystem::RenderEditor(*this, renderer, camera);
        }
    }
#endif

    void World::OnUpdateRuntime(float timeStep)
    {
        if (!m_IsSimulating) { OnSimulationStart(); }

        constexpr float k_FixedTimestep { 1.0f / 60.0f };
        constexpr float k_MaxFrameTime { 0.25f };

        m_Accumulator += timeStep < k_MaxFrameTime ? timeStep : k_MaxFrameTime;
        while (m_Accumulator >= k_FixedTimestep)
        {
            StepSimulation(k_FixedTimestep);
            m_Accumulator -= k_FixedTimestep;
        }

        if (m_HierarchyDirty)
        {
            TransformSystem::SortHierarchy(*this);
            m_HierarchyDirty = false;
        }

        AnimationSystem::OnUpdate(*this, timeStep);
        ScriptSystem::OnUpdate(*this, timeStep);
        AmbientSystem::OnUpdate(*this, timeStep);
        TransformSystem::OnUpdate(*this);

        auto renderer { Application::Get().GetRenderer() };
        RenderSystem::RenderRuntime(*this, renderer);
    }

    Entity World::SpawnModelNodeRecursive(const ModelNode& node, const ModelData& modelData, UUID modelAssetUUID, Entity parentEntity, std::vector<AssetBinding>* outBindings, const std::vector<uint32_t>& materialIndices)
    {
        Entity entity { CreateEntity(node.Name) };

        if (parentEntity) { entity.SetParent(parentEntity); }

        auto& transform { entity.GetComponent<TransformComponent>() };

        glm::vec3 scale, translation, skew;
        glm::quat rotation;
        glm::vec4 perspective;
        glm::decompose(node.LocalTransform, scale, rotation, translation, skew, perspective);

        transform.Translation = translation;
        transform.Rotation = glm::eulerAngles(rotation);
        transform.Scale = scale;
        entity.GetComponent<LocalMatrixComponent>().Matrix = node.LocalTransform;
        MarkTransformDirty(entity);

        if (!node.MeshIndices.empty())
        {
            uint32_t firstMeshIndex { node.MeshIndices[0] };
            const auto& subMesh { modelData.SubMeshes[firstMeshIndex] };
            auto& meshComp { entity.AddComponent<MeshComponent>() };
            auto renderer { Application::Get().GetRenderer() };
            meshComp.Handle = renderer->UploadMesh(subMesh.Data);

            uint32_t matIdx { subMesh.MaterialIndex };
            if (matIdx < materialIndices.size())
            {
                entity.AddComponent<MaterialComponent>().MaterialIndex = materialIndices[matIdx];
            }

            if (outBindings)
            {
                UUID entityUUID { m_Registry.get<IDComponent>(entity.GetHandle()).ID };
                outBindings->push_back({ entityUUID, modelAssetUUID, "MeshComponent", firstMeshIndex });
            }
        }

        for (const auto& childNode : node.Children)
        {
            SpawnModelNodeRecursive(childNode, modelData, modelAssetUUID, entity, outBindings, materialIndices);
        }

        return entity;
    }

    void World::OnCameraConstructed(entt::registry& reg, entt::entity e)
    {
        auto& cam { reg.get<CameraComponent>(e) };

        if (m_PrimaryCamera == entt::null || !reg.all_of<CameraComponent>(m_PrimaryCamera) || cam.prio >= reg.get<CameraComponent>(m_PrimaryCamera).prio)
        {
            m_PrimaryCamera = e;
        }
    }

    void World::OnCameraDestroyed(entt::registry& reg, entt::entity e)
    {
        if (m_PrimaryCamera != e) { return; }

        m_PrimaryCamera = entt::null;
        uint32_t bestPrio { 0 };
        bool first { true };

        for (auto camEntity : reg.view<CameraComponent>())
        {
            if (camEntity == e) { continue; }

            auto& cam { reg.get<CameraComponent>(camEntity) };

            if (first || cam.prio > bestPrio)
            {
                m_PrimaryCamera = camEntity;
                bestPrio = cam.prio;
                first = false;
            }
        }
    }
}