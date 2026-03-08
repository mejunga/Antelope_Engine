#pragma once

#include <Engine/Platform/Window.hpp>
#include <Engine/Renderer/VulkanContext.hpp>
#include <Engine/Renderer/SwapChain.hpp>
#include <Engine/Renderer/LowLevelRenderer.hpp>
#include <Engine/Renderer/HighLevelRenderer.hpp>

#include <memory>


namespace Antelope
{
    class Application
    {
        public:
            Application();
            virtual ~Application();

            Application(const Application&) = delete;
            Application& operator=(const Application&) = delete;

            void Run();
            void OnWindowResize(int width, int height) 
            {
                if(m_SwapChain)
                {
                    m_SwapChain->SetFramebufferResized(true);
                }
            }

            inline Window& GetWindow() { return *m_Window; }
            inline static Application& Get() { return *s_Instance; }
            inline std::shared_ptr<VulkanContext> GetVulkanContext() const { return m_VulkanContext; }
            inline std::shared_ptr<SwapChain> GetSwapChain() const { return m_SwapChain; }
            inline std::shared_ptr<LowLevelRenderer> GetLowLevelRenderer() const { return m_LowLevelRenderer; }

        private:
            std::unique_ptr<Window> m_Window;
            std::shared_ptr<VulkanContext> m_VulkanContext;
            std::shared_ptr<SwapChain> m_SwapChain;
            std::shared_ptr<LowLevelRenderer> m_LowLevelRenderer;
            std::shared_ptr<HighLevelRenderer> m_HighLevelRenderer;
            
            bool m_Running = true;

            static Application *s_Instance;
    };

    Application* CreateApplication();
}