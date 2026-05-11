#include <Engine/Core/JobSystem.hpp>
#include <Engine/Core/Allocator.hpp>
#include <Engine/Debug/Log.hpp>

#include <enkiTS/TaskScheduler.h>
#include <rpmalloc/rpmalloc.h>

#include <vector>
#include <mutex>
#include <algorithm>


namespace Antelope
{
    struct JobSystem::Impl
    {
        enki::TaskScheduler scheduler;
        std::vector<std::shared_ptr<enki::TaskSet>> pendingTasks;
        std::mutex tasksMutex;
    };

    JobSystem::JobSystem(uint32_t numThreads)
        : m_Impl(std::make_unique<Impl>())
    {
        enki::TaskSchedulerConfig config;

        if (numThreads > 0)
        {
            config.numTaskThreadsToCreate = numThreads - 1;
        }

        config.profilerCallbacks.threadStart = [](uint32_t) { Allocator::InitThread(); };
        config.profilerCallbacks.threadStop = [](uint32_t) { Allocator::ShutdownThread(); };
        config.customAllocator.alloc = [](size_t align, size_t size, void*, const char*, int) -> void*
        {
            return align ? rpaligned_alloc(align, size) : rpmalloc(size);
        };
        config.customAllocator.free = [](void* ptr, size_t, void*, const char*, int)
        {
            rpfree(ptr);
        };

        m_Impl->scheduler.Initialize(config);
        AE_ENGINE_INFO("JobSystem initialized with {0} threads.", m_Impl->scheduler.GetNumTaskThreads());
    }

    JobSystem::~JobSystem()
    {
        m_Impl->scheduler.WaitforAllAndShutdown();
        AE_ENGINE_TRACE("JobSystem shut down.");
    }

    JobHandle JobSystem::Submit(const char* name, std::function<void()> fn)
    {
        (void)name;
        Cleanup();

        auto promise { std::make_shared<std::promise<void>>() };
        JobHandle future { promise->get_future().share() };

        auto task { std::make_shared<enki::TaskSet>(
        [fn = std::move(fn), p = std::move(promise)](enki::TaskSetPartition, uint32_t) mutable
        {
            fn();
            p->set_value();
        })};

        m_Impl->scheduler.AddTaskSetToPipe(task.get());

        {
            std::lock_guard lock { m_Impl->tasksMutex };
            m_Impl->pendingTasks.push_back(task);
        }

        return future;
    }

    void JobSystem::FireAndForget(std::function<void()> fn)
    {
        Cleanup();

        auto task { std::make_shared<enki::TaskSet>(
        [fn = std::move(fn)](enki::TaskSetPartition, uint32_t) mutable
        {
            fn();
        })};

        m_Impl->scheduler.AddTaskSetToPipe(task.get());

        {
            std::lock_guard lock { m_Impl->tasksMutex };
            m_Impl->pendingTasks.push_back(task);
        }
    }

    void JobSystem::WaitAll()
    {
        m_Impl->scheduler.WaitforAll();
        Cleanup();
    }

    uint32_t JobSystem::GetThreadCount() const
    {
        return m_Impl->scheduler.GetNumTaskThreads();
    }

    void JobSystem::Cleanup()
    {
        std::lock_guard lock { m_Impl->tasksMutex };
        m_Impl->pendingTasks.erase(
            std::remove_if(
                m_Impl->pendingTasks.begin(),
                m_Impl->pendingTasks.end(),
                [](const auto& t) { return t->GetIsComplete(); }
            ),
            m_Impl->pendingTasks.end()
        );
    }
}