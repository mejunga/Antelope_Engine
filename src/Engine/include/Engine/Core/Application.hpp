#pragma once

#include <Engine/Platform/Window.hpp>
#include <Engine/Renderer/VulkanContext.hpp>

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
                if (m_VulkanContext)
                    m_VulkanContext->SetFramebufferResized(true);
            }

            inline Window& GetWindow() { return *m_Window; }
            inline static Application& Get() { return *s_Instance; }

        private:
            std::unique_ptr<Window> m_Window;
            std::unique_ptr<VulkanContext> m_VulkanContext;
            bool m_Running = true;

            static Application *s_Instance;
    };

    Application* CreateApplication();
}