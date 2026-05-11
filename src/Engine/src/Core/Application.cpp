#include <Engine/Core/Application.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Platform/Window.hpp>
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Renderer/Vulkan/SwapChain.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/Asset/TextureManager.hpp>
#include <Engine/ECS/World.hpp>
#include <Engine/Core/Allocator.hpp>
#include <Engine/Core/JobSystem.hpp>
#ifdef ANTELOPE_EDITOR_MODE
#include <Engine/Renderer/UI/UIContext.hpp>
#endif

#include <chrono>
#include <memory_resource>


namespace Antelope
{
    Application *Application::s_Instance { nullptr };

    Application::Application(uint32_t framesInFlight)
    {
        if (s_Instance) { AE_ENGINE_ERROR("Application already exists!"); return; }

        s_Instance = this;

        m_Allocator = PlaceNew<Allocator>(m_SystemArena);
        std::pmr::set_default_resource(GetRpResource());
        m_JobSystem = PlaceNew<JobSystem>(m_SystemArena);
        m_FrameAllocator = PlaceNew<FrameAllocator>(m_SystemArena, 32 * 1024 * 1024, framesInFlight);
        m_Window = PlaceNew<Window>(m_SystemArena);
        m_VulkanContext = ArenaShared(PlaceNew<VulkanContext>(m_SystemArena, m_Window->GetNativeWindow()));
        m_SwapChain = ArenaShared(PlaceNew<SwapChain>(m_SystemArena, m_VulkanContext));
        m_Renderer = ArenaShared(PlaceNew<Renderer>(m_SystemArena, m_VulkanContext, m_SwapChain, framesInFlight));
        m_TextureManager = ArenaShared(PlaceNew<TextureManager>(m_SystemArena, m_VulkanContext, m_Renderer));
        m_World = ArenaShared(PlaceNew<World>(m_SystemArena));
    #ifdef ANTELOPE_EDITOR_MODE
        m_UIContext = ArenaShared(PlaceNew<UIContext>(m_SystemArena, m_VulkanContext, m_SwapChain, m_Renderer, *m_Window));
        m_Renderer->SetUIContext(m_UIContext);
    #endif
        AE_ENGINE_INFO("System arena used: {0} / {1} KB", m_SystemArena.Used() / 1024, m_SystemArena.Capacity() / 1024);
    }

    Application::~Application()
    {
        m_UIContext.reset();
        m_World.reset();
        m_TextureManager.reset();
        m_Renderer.reset();
        m_SwapChain.reset();
        m_VulkanContext.reset();

        if (m_Window) { m_Window->~Window(); }
        if (m_JobSystem) { m_JobSystem->~JobSystem(); }
        if (m_Allocator) { m_Allocator->~Allocator(); }
    }

    void Application::Run()
    {
        AE_ENGINE_INFO("Engine Core Loop Started.");        
        OnInit(); 
        auto lastTime { std::chrono::high_resolution_clock::now() };

        while (m_Running)
        {
            if (m_FrameAllocator && m_Renderer)
            {
                m_FrameAllocator->BeginFrame(m_Renderer->GetCurrentFrame());
            }

            m_Window->OnUpdate();
            if (m_Window->IsResizing() || m_Window->GetWidth() == 0 || m_Window->GetHeight() == 0) { continue; }

            auto currentTime { std::chrono::high_resolution_clock::now() };
            float timeStep { std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count() };
            lastTime = currentTime;

        #ifdef ANTELOPE_EDITOR_MODE
            if (m_UIContext)
            {
                m_UIContext->BeginFrame();
                OnUIRender();
                m_UIContext->EndFrame();
            }
            
            m_FileWatcher.Poll();
        #endif
            
            OnUpdate(timeStep);

        #ifdef ANTELOPE_EDITOR_MODE
            if (m_UIContext)
            {
                m_UIContext->RenderViewports(); 
            }
        #endif

            if (m_Window->ShouldClose()) { m_Running = false; }
        }

        vkDeviceWaitIdle(m_VulkanContext->GetDevice());
        OnShutdown();        
        AE_ENGINE_INFO("Engine Core Loop Stopped.");
    }

    void Application::OnWindowResize(int width, int height) 
    {
        if (m_SwapChain) { m_SwapChain->SetFramebufferResized(true); }
    }
}