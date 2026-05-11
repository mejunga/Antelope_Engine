#include <Engine/Physics/PhysicsContext.hpp>
#include <Engine/Core/Allocator.hpp>
#include <Engine/Core/JobSystem.hpp>
#include <Engine/Debug/Log.hpp>

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Physics/PhysicsSettings.h>

#include <cstdarg>


namespace Antelope
{
    static void TraceImpl(const char* inFMT, ...)
    {
        va_list list;
        va_start(list, inFMT);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), inFMT, list);
        va_end(list);
        AE_ENGINE_TRACE("Jolt: {0}", buffer);
    }

#ifdef JPH_ENABLE_ASSERTS
    static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint32_t inLine)
    {
        AE_ENGINE_CRITICAL("Jolt Assert: {0} : {1} at {2}:{3}", inExpression, (inMessage ? inMessage : ""), inFile, inLine);
        return true;
    }
#endif

    class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
    {
        public:
            BPLayerInterfaceImpl()
            {
                m_ObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
                m_ObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
            }

            virtual uint32_t GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }

            virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
            {
                JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
                return m_ObjectToBroadPhase[inLayer];
            }

        private:
            JPH::BroadPhaseLayer m_ObjectToBroadPhase[Layers::NUM_LAYERS];
    };

    class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
    {
        public:
            virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
            {
                switch (inLayer1)
                {
                    case Layers::NON_MOVING:
                        return inLayer2 == BroadPhaseLayers::MOVING;
                    case Layers::MOVING:
                        return true;
                    default:
                        JPH_ASSERT(false);
                        return false;
                }
            }
    };

    class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
    {
        public:
            virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
            {
                switch (inObject1)
                {
                    case Layers::NON_MOVING:
                        return inObject2 == Layers::MOVING;
                    case Layers::MOVING:
                        return true;
                    default:
                        JPH_ASSERT(false);
                        return false;
                }
            }
    };

    struct PhysicsFilters
    {
        BPLayerInterfaceImpl BPLayerInterface;
        ObjectVsBroadPhaseLayerFilterImpl ObjVsBPLayerFilter;
        ObjectLayerPairFilterImpl ObjVsObjLayerFilter;
    };

    class AntelopeAllocator final : public JPH::TempAllocator
    {
        public:
            explicit AntelopeAllocator(Allocator& allocator) : m_Allocator(allocator) {}

            void* Allocate(JPH::uint inSize) override
            {
                return m_Allocator.AllocateAligned(inSize, 16);
            }

            void Free(void* inAddress, [[maybe_unused]] JPH::uint inSize) override
            {
                m_Allocator.Free(inAddress);
            }

        private:
            Allocator& m_Allocator;
    };

    class AntelopeJobSystem final : public JPH::JobSystemWithBarrier
    {
        public:
            AntelopeJobSystem(Antelope::JobSystem& jobSystem, Allocator& allocator)
                : m_JobSystem(jobSystem)
                , m_Allocator(allocator)
            {
                Init(JPH::cMaxPhysicsBarriers);
            }

            int GetMaxConcurrency() const override
            {
                return static_cast<int>(m_JobSystem.GetThreadCount());
            }

            JobHandle CreateJob(const char* name, JPH::ColorArg color, const JobFunction& fn, JPH::uint32 numDeps) override
            {
                void* mem { m_Allocator.AllocateAligned(sizeof(Job), alignof(Job)) };
                Job* job { ::new(mem) Job(name, color, this, fn, numDeps) };
                JobHandle handle(job);

                if (numDeps == 0) { QueueJob(job); }
                
                return handle;
            }

            void QueueJob(Job* job) override
            {
                job->AddRef();
                m_JobSystem.FireAndForget([job]() {
                    job->Execute();
                    job->Release();
                });
            }

            void QueueJobs(Job** jobs, JPH::uint count) override
            {
                for (JPH::uint i { 0 }; i < count; ++i) { QueueJob(jobs[i]); }
            }

            void FreeJob(Job* job) override
            {
                job->~Job();
                m_Allocator.Free(job);
            }

        private:
            Antelope::JobSystem& m_JobSystem;
            Allocator& m_Allocator;
    };

    PhysicsContext::PhysicsContext(Allocator& allocator, JobSystem& jobSystem)
    {
        JPH::RegisterDefaultAllocator();

        JPH::Trace = TraceImpl;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        m_TempAllocator = std::make_unique<AntelopeAllocator>(allocator);
        m_JobSystem = std::make_unique<AntelopeJobSystem>(jobSystem, allocator);

        uint32_t maxBodies { 10240 };
        uint32_t numBodyMutexes { 0 };
        uint32_t maxBodyPairs { 10240 };
        uint32_t maxContactConstraints { 10240 };

        m_Filters = std::make_unique<PhysicsFilters>();
        m_PhysicsSystem = std::make_unique<JPH::PhysicsSystem>();
        m_PhysicsSystem->Init(
            maxBodies,
            numBodyMutexes,
            maxBodyPairs,
            maxContactConstraints,
            m_Filters->BPLayerInterface,
            m_Filters->ObjVsBPLayerFilter,
            m_Filters->ObjVsObjLayerFilter
        );

        AE_ENGINE_INFO("PhysicsContext created.");
    }

    PhysicsContext::~PhysicsContext()
    {
        JPH::UnregisterTypes();

        if (JPH::Factory::sInstance)
        {
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }

        AE_ENGINE_TRACE("PhysicsContext destroyed.");
    }

    void PhysicsContext::OptimizeBroadPhase()
    {
        if (m_PhysicsSystem) { m_PhysicsSystem->OptimizeBroadPhase(); }
    }
}