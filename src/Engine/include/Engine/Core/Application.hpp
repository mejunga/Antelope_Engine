#pragma once

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

            inline Window& GetWindow() const { return *m_Window; }
            inline static Application& Get() { return *s_Instance; }
            inline std::shared_ptr<VulkanContext> GetVulkanContext() const { return m_VulkanContext; }
            inline std::shared_ptr<SwapChain> GetSwapChain() const { return m_SwapChain; }
            inline std::shared_ptr<Renderer> GetRenderer() const { return m_Renderer; }
            inline std::shared_ptr<TextureManager> GetTextureManager() const { return m_TextureManager; }
            inline std::shared_ptr<World> GetWorld() const { return m_World; }
        #ifdef ANTELOPE_EDITOR_MODE
            inline std::shared_ptr<UIContext> GetUIContext() const { return m_UIContext; }
        #endif

        protected:
            std::unique_ptr<Window> m_Window;
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
            static Application *s_Instance;
    };

    Application* CreateApplication();
}