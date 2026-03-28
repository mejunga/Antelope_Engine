#pragma once

#include <Engine/Renderer/Graphics/Model.hpp>
#include <Engine/ECS/Entity.hpp>
#include <Engine/ECS/BaseComponents.hpp>
#ifdef ANTELOPE_EDITOR_MODE
#include <Engine/Renderer/Graphics/EditorCamera.hpp>
#endif

#include <memory>


namespace Antelope
{
    class VulkanContext;
    class SwapChain;
    class Renderer;
    class HighLevelRenderer;
    class Window;
    class TextureManager;
    class World;
    
    class Application
    {
        public:
            Application();
            virtual ~Application();

            Application(const Application&) = delete;
            Application& operator=(const Application&) = delete;

            void Run();
            void OnWindowResize(int width, int height);

            inline Window& GetWindow() const { return *m_Window; }
            inline static Application& Get() { return *s_Instance; }
            inline std::shared_ptr<VulkanContext> GetVulkanContext() const { return m_VulkanContext; }
            inline std::shared_ptr<SwapChain> GetSwapChain() const { return m_SwapChain; }
            inline std::shared_ptr<Renderer> GetRenderer() const { return m_Renderer; }

        private:
            void SetupMockData();

        private:
            std::unique_ptr<Window> m_Window;
            std::shared_ptr<VulkanContext> m_VulkanContext;
            std::shared_ptr<SwapChain> m_SwapChain;
            std::shared_ptr<Renderer> m_Renderer;
            std::shared_ptr<TextureManager> m_TextureManager;
            std::shared_ptr<World> m_World;

            ModelData m_BearMesh;       
            uint32_t m_BearTexID;
            
            ModelData m_GorillaMesh;
            uint32_t m_GorillaTexID;

            bool m_Running = true;
            int m_RenderState = 0;
            float m_DebounceTimer = 0.0f;
            const float DEBOUNCE_DELAY = 0.2f;
        #ifdef ANTELOPE_EDITOR_MODE
            EditorCamera m_EditorCamera;
        #endif
            std::vector<Entity> m_ActiveEntities;

            static Application *s_Instance;
    };

    Application* CreateApplication();
}