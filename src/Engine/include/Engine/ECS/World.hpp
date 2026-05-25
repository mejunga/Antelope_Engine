#pragma once

#include <Engine/Renderer/Graphics/Model.hpp>
#include <Engine/Scene/SceneSerializer.hpp>
#include <Engine/Core/UUID.hpp>
#include <Engine/ECS/BaseComponents.hpp>

#include <entt/entt.hpp>

#include <string>
#include <memory>
#include <utility>
#include <vector>


namespace Antelope
{
    class Entity;
    class PhysicsContext;
    class AudioContext;
#ifdef ANTELOPE_EDITOR_MODE
    class EditorCamera;
#endif

    class World
    {
        public:
            World();
            ~World();

            Entity CreateEntity(const std::string& name = "Entity", UUID uuid = UUID());
            void DestroyEntity(Entity entity);
            Entity SpawnModel(const ModelData& modelData, const std::string& rootName, UUID modelAssetUUID, std::vector<AssetBinding>* outBindings = nullptr);
            Entity SpawnModel(const ModelData& modelData, const std::string& rootName, UUID modelAssetUUID, Entity parentEntity, std::vector<AssetBinding>* outBindings = nullptr);
            void MarkTransformDirty(Entity entity);
            void MarkHierarchyDirty() { m_HierarchyDirty = true; }
            void Clear();

            void TakeSnapshot();
            void RestoreSnapshot();
            void OnSimulationStart();
            void OnSimulationStop();
            void StepSimulation(float timeStep);
        #ifdef ANTELOPE_EDITOR_MODE
            void OnUpdateEditor(float timeStep, const EditorCamera& camera);
        #endif
            void OnUpdateRuntime(float timeStep);
            
            inline entt::registry& GetRegistry() { return m_Registry; }
            inline PhysicsContext* GetPhysicsContext() { return m_PhysicsContext.get(); }
            inline AudioContext* GetAudioContext() { return m_AudioContext.get(); }
            inline bool IsSimulating() const { return m_IsSimulating; }
            inline void SetGameViewActive(bool active) { m_GameViewActive = active; }
            inline void SetPaused(bool paused) { m_IsPaused = paused; }
            inline entt::entity GetPrimaryCamera() const { return m_PrimaryCamera; }
            inline void SetPrimaryCamera(entt::entity e) { m_PrimaryCamera = e; }

        private:
            Entity SpawnModelNodeRecursive(const ModelNode& node, const ModelData& modelData, UUID modelAssetUUID, Entity parentEntity, std::vector<AssetBinding>* outBindings, const std::vector<uint32_t>& materialIndices);
            void OnCameraConstructed(entt::registry& reg, entt::entity e);
            void OnCameraDestroyed(entt::registry& reg, entt::entity e);

        private:
            std::unique_ptr<PhysicsContext> m_PhysicsContext;
            std::unique_ptr<AudioContext> m_AudioContext;

            entt::registry m_Registry;
            entt::entity m_PrimaryCamera { entt::null };
            bool m_HierarchyDirty { true };
            bool m_IsSimulating { false };
            bool m_GameViewActive { false };
            bool m_IsPaused { false };
            float m_Accumulator { 0.0f };

            struct TransformSnapshot { entt::entity entity; TransformComponent transform; };
            struct AnimatorSnapshot { entt::entity entity; AnimatorComponent  animator; };

            std::vector<TransformSnapshot> m_SimulationSnapshot;
            std::vector<AnimatorSnapshot> m_AnimatorSnapshot;

            friend class Entity;
    };
}