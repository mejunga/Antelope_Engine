#include <Engine/ECS/World.hpp>
#include <Engine/ECS/Entity.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/ECS/System/RenderSystem.hpp>
#include <Engine/ECS/System/TransformSystem.hpp>
#include <Engine/ECS/System/PhysicsSystem.hpp>
#include <Engine/Physics/PhysicsContext.hpp>
#include <Engine/Core/Application.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Asset/AssetManager.hpp>
#include <Engine/Asset/TextureManager.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>

#include <vector>


namespace Antelope
{
    World::World()
    {
        m_PhysicsContext = std::make_unique<PhysicsContext>();

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

        for (const auto& modelMat : modelData.Materials)
        {
            PBRMaterialData pbrMat;
            pbrMat.AlbedoFactor = modelMat.AlbedoFactor;
            pbrMat.MetallicRoughnessFactors = modelMat.MetallicRoughnessFactors;

            auto loadTex { [&](const std::string& filename) -> uint32_t {
                if (filename.empty()) return 0xFFFFFFFF;
                for (const auto& [uuid, meta] : AssetManager::GetRegistry()) {
                    if (meta.Type == AssetType::Texture2D && meta.FilePath.filename().string() == filename) {
                        return textureManager->LoadTexture(meta.FilePath.string());
                    }
                }
                return 0xFFFFFFFF;
            }};

            pbrMat.AlbedoTexIndex = loadTex(modelMat.AlbedoTexPath);
            pbrMat.NormalTexIndex = loadTex(modelMat.NormalTexPath);
            pbrMat.MetRoughAOTexIndex = loadTex(modelMat.MetRoughAOTexPath);
            pbrMat.EmissiveTexIndex = loadTex(modelMat.EmissiveTexPath);

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

    void World::OnSimulationStart()
    {
        if (m_IsSimulating) { return; }

        PhysicsSystem::OnRuntimeStart(*this, *m_PhysicsContext);
        m_PhysicsContext->OptimizeBroadPhase();
        
        m_IsSimulating = true;
        AE_ENGINE_INFO("World Simulation Started.");
    }

    void World::OnSimulationStop()
    {
        if (!m_IsSimulating) { return; }

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

        TransformSystem::OnUpdate(*this);
        
        auto renderer { Application::Get().GetRenderer() };
        RenderSystem::RenderEditor(*this, renderer, camera);
    }
#endif

    void World::OnUpdateRuntime(float timeStep)
    {        
        if (!m_IsSimulating) { OnSimulationStart(); }

        StepSimulation(timeStep);

        if (m_HierarchyDirty)
        {
            TransformSystem::SortHierarchy(*this);
            m_HierarchyDirty = false;
        }

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
        if (reg.get<CameraComponent>(e).IsPrimary) { m_PrimaryCamera = e; }
    }

    void World::OnCameraDestroyed(entt::registry& reg, entt::entity e)
    {
        if (m_PrimaryCamera == e) { m_PrimaryCamera = entt::null; }
    }
}