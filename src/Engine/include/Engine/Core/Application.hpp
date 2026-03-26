#pragma once

#include <Engine/Renderer/Graphics/Mesh.hpp>
#include <Engine/Renderer/Graphics/Camera.hpp>
#include <Engine/ECS/Entity.hpp>
#include <Engine/ECS/BaseComponents.hpp>

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

            Entity m_ActiveBearEntity; 
            MeshData m_BearMesh;       
            uint32_t m_BearTexID;
            Entity m_ActiveEntity;
            MeshData m_GorillaMesh;
            uint32_t m_GorillaTexID;
            bool m_Running = true;
            int m_RenderState = 0;
            float m_DebounceTimer = 0.0f;
            const float DEBOUNCE_DELAY = 0.2f;
            EditorCamera m_EditorCamera;

            static Application *s_Instance;
    };

    Application* CreateApplication();
}