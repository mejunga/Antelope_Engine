#include <Engine/ECS/System/TransformSystem.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#include <Engine/ECS/World.hpp>
#include <Engine/Core/Application.hpp>
#include <Engine/Core/JobSystem.hpp>

#include <vector>


namespace Antelope
{
    namespace
    {
        struct LocalEntry { TransformComponent* transform; LocalMatrixComponent* local; };
        struct NormalEntry { WorldMatrixComponent* world; NormalMatrixComponent* normal; };
    }

    void TransformSystem::SortHierarchy(World& world)
    {
        auto& registry { world.GetRegistry() };

        registry.sort<TransformComponent>([&registry](const entt::entity lhs, const entt::entity rhs)
        {
            return registry.get<RelationshipComponent>(lhs).Depth < registry.get<RelationshipComponent>(rhs).Depth;
        });

        registry.sort<LocalMatrixComponent, TransformComponent>();
        registry.sort<WorldMatrixComponent, TransformComponent>();
    }

    void TransformSystem::OnUpdate(World& world)
    {
        auto& registry   { world.GetRegistry() };
        auto& jobSystem  { Application::Get().GetJobSystem() };
        auto& frameAlloc { Application::Get().GetFrameAllocator() };

        {
            auto dirtyView { registry.view<TransformComponent, LocalMatrixComponent, DirtyTransform>() };

            const size_t maxLocal { dirtyView.size_hint() };
            auto localEntries { frameAlloc.AllocateArray<LocalEntry>(maxLocal) };
            uint32_t count { 0 };
            for (auto [e, t, l] : dirtyView.each()) { localEntries[count++] = { &t, &l }; }

            constexpr uint32_t k_BatchSize { 64 };

            if (count <= k_BatchSize)
            {
                for (uint32_t i { 0 }; i < count; ++i) { localEntries[i].local->Rebuild(*localEntries[i].transform); }
            }
            else
            {
                const uint32_t numBatches { (count + k_BatchSize - 1) / k_BatchSize };
                std::vector<JobHandle> handles;
                handles.reserve(numBatches);

                for (uint32_t b { 0 }; b < numBatches; ++b)
                {
                    uint32_t begin { b * k_BatchSize };
                    uint32_t end   { count < (begin + k_BatchSize) ? count : (begin + k_BatchSize) };

                    handles.push_back(jobSystem.Submit("LocalMatrix", [begin, end, localEntries]()
                    {
                        for (uint32_t i { begin }; i < end; ++i)
                        {
                            localEntries[i].local->Rebuild(*localEntries[i].transform);
                        }
                    }));
                }

                for (auto& h : handles) { h.wait(); }
            }
        }

        {
            auto hierarchyView { registry.view<LocalMatrixComponent, WorldMatrixComponent, RelationshipComponent>() };
            hierarchyView.use<LocalMatrixComponent>();

            for (auto [entity, local, worldMat, rel] : hierarchyView.each())
            {
                bool selfDirty { registry.all_of<DirtyTransform>(entity) };
                bool parentDirty { rel.Parent != entt::null && registry.all_of<DirtyTransform>(rel.Parent) };

                if (rel.Parent == entt::null)
                {
                    if (selfDirty) { worldMat.Matrix = local.Matrix; }
                }
                else
                {
                    if (selfDirty || parentDirty)
                    {
                        const auto& parentWorld { registry.get<WorldMatrixComponent>(rel.Parent) };
                        worldMat.Matrix = parentWorld.Matrix * local.Matrix;
                        registry.emplace_or_replace<DirtyTransform>(entity);
                    }
                }
            }
        }

        {
            auto normalView { registry.view<WorldMatrixComponent, NormalMatrixComponent, DirtyTransform>() };

            const size_t maxNormal { normalView.size_hint() };
            auto normalEntries { frameAlloc.AllocateArray<NormalEntry>(maxNormal) };
            uint32_t count { 0 };
            for (auto [e, wm, nm] : normalView.each()) { normalEntries[count++] = { &wm, &nm }; }

            constexpr uint32_t k_BatchSize { 64 };

            if (count <= k_BatchSize)
            {
                for (uint32_t i { 0 }; i < count; ++i)
                {
                    normalEntries[i].normal->Matrix = glm::transpose(glm::inverse(glm::mat3(normalEntries[i].world->Matrix)));
                }
            }
            else
            {
                const uint32_t numBatches { (count + k_BatchSize - 1) / k_BatchSize };
                std::vector<JobHandle> handles;
                handles.reserve(numBatches);

                for (uint32_t b { 0 }; b < numBatches; ++b)
                {
                    uint32_t begin { b * k_BatchSize };
                    uint32_t end { count < (begin + k_BatchSize) ? count : (begin + k_BatchSize) };

                    handles.push_back(jobSystem.Submit("NormalMatrix", [begin, end, normalEntries]()
                    {
                        for (uint32_t i { begin }; i < end; ++i)
                        {
                            normalEntries[i].normal->Matrix = glm::transpose(glm::inverse(glm::mat3(normalEntries[i].world->Matrix)));
                        }
                    }));
                }

                for (auto& h : handles) { h.wait(); }
            }
        }

        registry.clear<DirtyTransform>();
    }
}