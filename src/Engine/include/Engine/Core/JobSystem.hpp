#pragma once

#include <functional>
#include <future>
#include <memory>
#include <cstdint>

namespace Antelope
{
    using JobHandle = std::shared_future<void>;

    class JobSystem
    {
        public:
            explicit JobSystem(uint32_t numThreads = 0);
            ~JobSystem();

            JobSystem(const JobSystem&) = delete;
            JobSystem& operator=(const JobSystem&) = delete;

            JobHandle Submit(const char* name, std::function<void()> fn);
            void FireAndForget(std::function<void()> fn);
            void WaitAll();
            uint32_t GetThreadCount() const;

        private:
            void Cleanup();

            struct Impl;
            std::unique_ptr<Impl> m_Impl;
    };
}