#include <Engine/Core/Allocator.hpp>
#include <Engine/Debug/Log.hpp>

#include <rpmalloc/rpmalloc.h>

#include <stdexcept>


namespace Antelope
{
    Allocator::Allocator()
    {
        rpmalloc_initialize();
        AE_ENGINE_INFO("Allocator initialized.");
    }

    Allocator::~Allocator()
    {
        rpmalloc_finalize();
        AE_ENGINE_TRACE("Allocator shut down.");
    }

    void* Allocator::Allocate(size_t size)
    {
        return rpmalloc(size);
    }

    void* Allocator::AllocateAligned(size_t size, size_t alignment)
    {
        return rpaligned_alloc(alignment, size);
    }

    void* Allocator::AllocateZeroed(size_t count, size_t size)
    {
        return rpcalloc(count, size);
    }

    void* Allocator::Reallocate(void* ptr, size_t size)
    {
        return rprealloc(ptr, size);
    }

    void Allocator::Free(void* ptr)
    {
        rpfree(ptr);
    }

    void Allocator::InitThread()
    {
        rpmalloc_thread_initialize();
    }

    void Allocator::ShutdownThread()
    {
        rpmalloc_thread_finalize(1);
    }

    LinearAllocator::LinearAllocator(size_t capacityBytes)
        : m_Buffer(static_cast<std::byte*>(::malloc(capacityBytes)))
        , m_Capacity(capacityBytes)
        , m_Offset(0)
    {
        AE_ENGINE_INFO("LinearAllocator: reserved {0} KB system block.", capacityBytes / 1024);
    }

    LinearAllocator::~LinearAllocator()
    {
        ::free(m_Buffer);
    }

    void* LinearAllocator::Allocate(size_t size, size_t alignment)
    {
        size_t aligned { (m_Offset + alignment - 1) & ~(alignment - 1) };

        if (aligned + size > m_Capacity)
        {
            AE_ENGINE_CRITICAL("LinearAllocator exhausted! Used: {0} KB, Requested: {1} bytes", m_Offset / 1024, size);
            throw std::runtime_error("LinearAllocator capacity exceeded");
        }

        m_Offset = aligned + size;
        return m_Buffer + aligned;
    }

    FrameAllocator::FrameAllocator(size_t capacityPerFrame, uint32_t framesInFlight)
    {
        for (uint32_t i { 0 }; i < framesInFlight; ++i)
        {
            m_Arenas.push_back(std::make_unique<LinearAllocator>(capacityPerFrame));
        }
    }

    void* FrameAllocator::Allocate(size_t size, size_t alignment)
    {
        return m_Arenas[m_CurrentFrame]->Allocate(size, alignment);
    }
    
    void FrameAllocator::BeginFrame(uint32_t frameIndex)
    {
        m_CurrentFrame = frameIndex;
        m_Arenas[m_CurrentFrame]->Reset();
    }
    
    LinearAllocator& FrameAllocator::GetCurrentArena()
    {
        return *m_Arenas[m_CurrentFrame];
    }

    void* RpmallocResource::do_allocate(size_t size, size_t alignment)
    {
        return rpaligned_alloc(alignment, size);
    }

    void RpmallocResource::do_deallocate(void* ptr, size_t, size_t)
    {
        rpfree(ptr);
    }

    bool RpmallocResource::do_is_equal(const std::pmr::memory_resource& other) const noexcept
    {
        return &other == this;
    }

    std::pmr::memory_resource* GetRpResource()
    {
        static RpmallocResource s_Resource;
        return &s_Resource;
    }
}