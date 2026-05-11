#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>

#include <memory>


namespace Antelope
{
    class Allocator;
    class JobSystem;
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
            PhysicsContext(Allocator& allocator, JobSystem& jobSystem);
            ~PhysicsContext();

            void OptimizeBroadPhase();

            inline JPH::PhysicsSystem& GetSystem() { return *m_PhysicsSystem; }
            inline JPH::BodyInterface& GetBodyInterface() { return m_PhysicsSystem->GetBodyInterface(); }
            inline JPH::TempAllocator* GetTempAllocator() { return m_TempAllocator.get(); }
            inline JPH::JobSystem* GetJobSystem() { return m_JobSystem.get(); }

        private:
            std::unique_ptr<JPH::PhysicsSystem> m_PhysicsSystem;
            std::unique_ptr<JPH::TempAllocator> m_TempAllocator;
            std::unique_ptr<JPH::JobSystem> m_JobSystem;
            std::unique_ptr<PhysicsFilters> m_Filters;
    };
}