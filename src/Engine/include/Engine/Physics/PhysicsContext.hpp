#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

#include <memory>


namespace Antelope
{
    struct PhysicsFilters;
    
    namespace Layers
    {
        static constexpr JPH::ObjectLayer NON_MOVING { 0 };
        static constexpr JPH::ObjectLayer MOVING { 1 };
        static constexpr JPH::ObjectLayer NUM_LAYERS { 2 };
    };

    namespace BroadPhaseLayers
    {
        static constexpr JPH::BroadPhaseLayer NON_MOVING { 0 };
        static constexpr JPH::BroadPhaseLayer MOVING { 1 };
        static constexpr uint32_t NUM_LAYERS { 2 };
    };

    class PhysicsContext
    {
        public:
            PhysicsContext();
            ~PhysicsContext();

            void OptimizeBroadPhase();

            inline JPH::PhysicsSystem& GetSystem() { return *m_PhysicsSystem; }
            inline JPH::BodyInterface& GetBodyInterface() { return m_PhysicsSystem->GetBodyInterface(); }
            inline JPH::TempAllocatorImpl* GetTempAllocator() { return m_TempAllocator.get(); }
            inline JPH::JobSystemThreadPool* GetJobSystem() { return m_JobSystem.get(); }

        private:
            std::unique_ptr<JPH::PhysicsSystem> m_PhysicsSystem;
            std::unique_ptr<JPH::TempAllocatorImpl> m_TempAllocator;
            std::unique_ptr<JPH::JobSystemThreadPool> m_JobSystem;
            std::unique_ptr<PhysicsFilters> m_Filters;
    };
}