#pragma once

#include <Engine/Renderer/Mesh.hpp>
#include <Engine/Renderer/TextureManager.hpp>

#include <memory>


namespace Antelope
{
    class VulkanContext;
    class SwapChain;
    class LowLevelRenderer;
    class HighLevelRenderer;
    class Window;
    
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
            inline std::shared_ptr<LowLevelRenderer> GetLowLevelRenderer() const { return m_LowLevelRenderer; }

        private:
            void SetupMockData();

        private:
            std::unique_ptr<Window> m_Window;
            std::shared_ptr<VulkanContext> m_VulkanContext;
            std::shared_ptr<SwapChain> m_SwapChain;
            std::shared_ptr<LowLevelRenderer> m_LowLevelRenderer;
            std::shared_ptr<HighLevelRenderer> m_HighLevelRenderer;
            std::shared_ptr<TextureManager> m_TextureManager;

            uint32_t m_WoodTexID = 0;
            uint32_t m_StoneTexID = 0;
            bool m_Running = true;
            int m_RenderState = 0;
            float m_DebounceTimer = 0.0f;
            const float DEBOUNCE_DELAY = 0.2f;
            
            MeshData m_DiamondMesh;
            MeshData m_CubeMesh;
            MeshData m_PyramidMesh;

            bool m_IsDiamondUploaded = false;
            bool m_IsCubeUploaded = false;
            bool m_IsPyramidUploaded = false;

            MeshHandle m_DiamondHandle {};
            MeshHandle m_CubeHandle {};
            MeshHandle m_PyramidHandle {};

            static Application *s_Instance;
    };

    Application* CreateApplication();
}