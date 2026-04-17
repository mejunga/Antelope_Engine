#include <Engine/Core/Application.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Platform/Window.hpp>
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Renderer/Vulkan/SwapChain.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/Asset/TextureManager.hpp>
#include <Engine/ECS/World.hpp>
#ifdef ANTELOPE_EDITOR_MODE
#include <Engine/Renderer/UI/UIContext.hpp>
#endif

#include <chrono>


namespace Antelope
{
    Application *Application::s_Instance { nullptr };

    Application::Application(uint32_t framesInFlight) 
    {
        if (s_Instance) 
        { 
            AE_ENGINE_ERROR("Application already exists!");
            return; 
        }

        s_Instance = this;
        m_Window = std::make_unique<Window>();
        m_VulkanContext = std::make_shared<VulkanContext>(m_Window->GetNativeWindow());
        m_SwapChain = std::make_shared<SwapChain>(m_VulkanContext);
        m_Renderer = std::make_shared<Renderer>(m_VulkanContext, m_SwapChain, framesInFlight);
        m_TextureManager = std::make_shared<TextureManager>(m_VulkanContext, m_Renderer);
        m_World = std::make_shared<World>();
    #ifdef ANTELOPE_EDITOR_MODE
        m_UIContext = std::make_shared<UIContext>(m_VulkanContext, m_SwapChain, m_Renderer, *m_Window);
        m_Renderer->SetUIContext(m_UIContext);
    #endif
    }

    Application::~Application() {}

    void Application::Run()
    {
        AE_ENGINE_INFO("Engine Core Loop Started.");        
        OnInit(); 
        auto lastTime { std::chrono::high_resolution_clock::now() };

        while (m_Running)
        {
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