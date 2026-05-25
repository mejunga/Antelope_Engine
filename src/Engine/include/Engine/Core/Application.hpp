#pragma once

#ifndef ANTELOPE_API
    #ifdef ANTELOPE_BUILD_DLL
        #define ANTELOPE_API __declspec(dllexport)
    #else
        #define ANTELOPE_API __declspec(dllimport)
    #endif
#endif

#include <Engine/Core/Allocator.hpp>
#ifdef ANTELOPE_EDITOR_MODE
#include <Engine/Asset/FileWatcher.hpp>
#endif


#include <memory>
#include <string>


namespace Antelope
{
    class VulkanContext;
    class SwapChain;
    class Renderer;
    class Window;
    class TextureManager;
    class World;
    class JobSystem;
#ifdef ANTELOPE_EDITOR_MODE
    class UIContext;
#endif
    
    class Application
    {
        public:
            Application(uint32_t framesInFlight = 2);
            virtual ~Application();

            Application(const Application&) = delete;
            Application& operator=(const Application&) = delete;

            void Run();
            void OnWindowResize(int width, int height);

            virtual void OnInit() {}
            virtual void OnUpdate(float timeStep) {}
        #ifdef ANTELOPE_EDITOR_MODE
            virtual void OnUIRender() {}
        #endif
            virtual void OnShutdown() {}

            inline static Application& Get() { return *s_Instance; }
            inline Allocator& GetAllocator() { return *m_Allocator; }
            inline FrameAllocator& GetFrameAllocator() { return *m_FrameAllocator; }
            inline JobSystem& GetJobSystem() { return *m_JobSystem; }
            inline Window& GetWindow() const { return *m_Window; }
            inline std::shared_ptr<VulkanContext> GetVulkanContext() const { return m_VulkanContext; }
            inline std::shared_ptr<SwapChain> GetSwapChain() const { return m_SwapChain; }
            inline std::shared_ptr<Renderer> GetRenderer() const { return m_Renderer; }
            inline std::shared_ptr<TextureManager> GetTextureManager() const { return m_TextureManager; }
            inline std::shared_ptr<World> GetWorld() const { return m_World; }
        #ifdef ANTELOPE_EDITOR_MODE
            inline std::shared_ptr<UIContext> GetUIContext() const { return m_UIContext; }
            inline FileWatcher& GetFileWatcher() { return m_FileWatcher; }
        #endif

        protected:
            LinearAllocator m_SystemArena { 4 * 1024 * 1024 };
            
            Allocator* m_Allocator { nullptr };
            FrameAllocator* m_FrameAllocator { nullptr };
            JobSystem* m_JobSystem { nullptr };
            Window* m_Window { nullptr };
            std::shared_ptr<VulkanContext> m_VulkanContext;
            std::shared_ptr<SwapChain> m_SwapChain;
            std::shared_ptr<Renderer> m_Renderer;
            std::shared_ptr<TextureManager> m_TextureManager;
            std::shared_ptr<World> m_World;
        #ifdef ANTELOPE_EDITOR_MODE
            std::shared_ptr<UIContext> m_UIContext;
        #endif

            bool m_Running { true };

        private:
            static ANTELOPE_API Application *s_Instance;
        #ifdef ANTELOPE_EDITOR_MODE
            FileWatcher m_FileWatcher;
        #endif
    };

    Application* CreateApplication(int argc, char** argv);
}