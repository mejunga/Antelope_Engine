#pragma once

#include <cstddef>
#include <memory>
#include <memory_resource>
#include <span>
#include <vector>
#include <utility>
#include <new>

namespace Antelope
{
    class Allocator
    {
        public:
            Allocator();
            ~Allocator();

            Allocator(const Allocator&) = delete;
            Allocator& operator=(const Allocator&) = delete;

            void* Allocate(size_t size);
            void* AllocateAligned(size_t size, size_t alignment);
            void* AllocateZeroed(size_t count, size_t size);
            void* Reallocate(void* ptr, size_t size);
            void Free(void* ptr);

            static void InitThread();
            static void ShutdownThread();
    };

    class LinearAllocator
    {
        public:
            explicit LinearAllocator(size_t capacityBytes);
            ~LinearAllocator();

            LinearAllocator(const LinearAllocator&) = delete;
            LinearAllocator& operator=(const LinearAllocator&) = delete;

            void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));

            size_t Used() const { return m_Offset; }
            size_t Capacity() const { return m_Capacity; }
            void Reset() { m_Offset = 0; }

        private:
            std::byte* m_Buffer { nullptr };
            
            size_t m_Capacity { 0 };
            size_t m_Offset { 0 };
    };

    class FrameAllocator
    {
        public:
            explicit FrameAllocator(size_t capacityPerFrame, uint32_t framesInFlight);
            ~FrameAllocator() = default;

            FrameAllocator(const FrameAllocator&) = delete;
            FrameAllocator& operator=(const FrameAllocator&) = delete;

            void* Allocate(size_t size, size_t alignment = alignof(std::max_align_t));
            void BeginFrame(uint32_t frameIndex);
            LinearAllocator& GetCurrentArena();

            template<typename T, typename... Args>
            T* PlaceNew(Args&&... args)
            {
                void* mem { Allocate(sizeof(T), alignof(T)) };
                return ::new (mem) T(std::forward<Args>(args)...);
            }

            template<typename T>
            std::span<T> AllocateArray(size_t count)
            {
                T* mem { static_cast<T*>(Allocate(sizeof(T) * count, alignof(T))) };
                return std::span<T>(mem, count);
            }

        private:
            std::vector<std::unique_ptr<LinearAllocator>> m_Arenas;
            uint32_t m_CurrentFrame { 0 };
    };

    extern "C" void* rpmalloc(size_t size);
    extern "C" void rpmalloc_free(void* ptr);

    template <typename T>
    class PoolAllocator
    {
        public:
            PoolAllocator(size_t elementsPerBlock = 1024);
            ~PoolAllocator();

            template <typename... Args>
            T* Allocate(Args&&... args);

            void Free(T* ptr);

        private:
            void AllocateBlock();

            size_t m_ElementsPerBlock;
            void* m_FreeList { nullptr };
            std::vector<void*> m_Blocks;
    };

    class RpmallocResource : public std::pmr::memory_resource
    {
        void* do_allocate(size_t size, size_t alignment) override;
        void do_deallocate(void* ptr, size_t, size_t) override;
        bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override;
    };

    std::pmr::memory_resource* GetRpResource();

    template<typename T, typename... Args>
    T* PlaceNew(LinearAllocator& arena, Args&&... args)
    {
        void* mem { arena.Allocate(sizeof(T), alignof(T)) };
        return ::new (mem) T(std::forward<Args>(args)...);
    }

    template<typename T>
    std::shared_ptr<T> ArenaShared(T* ptr)
    {
        return std::shared_ptr<T>(ptr, [](T* p) { p->~T(); });
    }

    template <typename T>
    inline PoolAllocator<T>::PoolAllocator(size_t elementsPerBlock)
        : m_ElementsPerBlock { elementsPerBlock } 
    {}

    template <typename T>
    inline PoolAllocator<T>::~PoolAllocator() 
    {
        for (void* block : m_Blocks) 
        { 
            rpmalloc_free(block); 
        }
    }

    template <typename T>
    template <typename... Args>
    inline T* PoolAllocator<T>::Allocate(Args&&... args) 
    {
        if (!m_FreeList) { AllocateBlock(); }
        
        void* ptr { m_FreeList };
        m_FreeList = *static_cast<void**>(m_FreeList);
        
        return new (ptr) T(std::forward<Args>(args)...);
    }

    template <typename T>
    inline void PoolAllocator<T>::Free(T* ptr) 
    {
        if (!ptr) { return; }
        
        ptr->~T();
        
        *static_cast<void**>(ptr) = m_FreeList;
        m_FreeList = ptr;
    }

    template <typename T>
    inline void PoolAllocator<T>::AllocateBlock() 
    {
        size_t elementSize { sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T) };
        void* block { rpmalloc(elementSize * m_ElementsPerBlock) };
        m_Blocks.push_back(block);

        char* ptr { static_cast<char*>(block) };

        for (size_t i { 0 }; i < m_ElementsPerBlock - 1; ++i) 
        {
            *reinterpret_cast<void**>(ptr + i * elementSize) = ptr + (i + 1) * elementSize;
        }

        *reinterpret_cast<void**>(ptr + (m_ElementsPerBlock - 1) * elementSize) = nullptr;
        
        m_FreeList = block;
    }
}
