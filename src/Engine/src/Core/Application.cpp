#include <Engine/Core/Application.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Platform/Input.hpp>
#include <Engine/Platform/Window.hpp>
#include <Engine/Renderer/VulkanContext.hpp>
#include <Engine/Renderer/SwapChain.hpp>
#include <Engine/Renderer/LowLevelRenderer.hpp>
#include <Engine/Renderer/HighLevelRenderer.hpp>
#include <Engine/Renderer/Camera.hpp>

#include <GLFW/glfw3.h>
#include <chrono>

namespace Antelope
{
    Application *Application::s_Instance { nullptr };

    Application::Application() 
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
        m_LowLevelRenderer = std::make_shared<LowLevelRenderer>(m_VulkanContext, m_SwapChain);
        m_HighLevelRenderer = std::make_shared<HighLevelRenderer>(m_LowLevelRenderer);        
        m_TextureManager = std::make_shared<TextureManager>(m_VulkanContext, m_LowLevelRenderer);
    }

    Application::~Application() {}

    void Application::SetupMockData()
    {
        m_WoodTexID = m_TextureManager->LoadTexture("Assets/seamless_wood_texture.png");
        m_StoneTexID = m_TextureManager->LoadTexture("Assets/seamless_stone_texture.png");    
        m_LowLevelRenderer->UpdateTextureDescriptors(m_TextureManager->GetGlobalTextures());

        m_CubeMesh.positions = {
            {{-0.5f, -0.5f,  0.5f}}, {{ 0.5f, -0.5f,  0.5f}}, {{ 0.5f,  0.5f,  0.5f}}, {{-0.5f,  0.5f,  0.5f}},
            {{ 0.5f, -0.5f, -0.5f}}, {{-0.5f, -0.5f, -0.5f}}, {{-0.5f,  0.5f, -0.5f}}, {{ 0.5f,  0.5f, -0.5f}},
            {{-0.5f, -0.5f, -0.5f}}, {{-0.5f, -0.5f,  0.5f}}, {{-0.5f,  0.5f,  0.5f}}, {{-0.5f,  0.5f, -0.5f}},
            {{ 0.5f, -0.5f,  0.5f}}, {{ 0.5f, -0.5f, -0.5f}}, {{ 0.5f,  0.5f, -0.5f}}, {{ 0.5f,  0.5f,  0.5f}},
            {{-0.5f,  0.5f,  0.5f}}, {{ 0.5f,  0.5f,  0.5f}}, {{ 0.5f,  0.5f, -0.5f}}, {{-0.5f,  0.5f, -0.5f}},
            {{-0.5f, -0.5f, -0.5f}}, {{ 0.5f, -0.5f, -0.5f}}, {{ 0.5f, -0.5f,  0.5f}}, {{-0.5f, -0.5f,  0.5f}}
        };

        m_CubeMesh.colors.resize(24, {{1.0f, 1.0f, 1.0f}});

        m_CubeMesh.normals = {
            {{ 0,  0,  1}}, {{ 0,  0,  1}}, {{ 0,  0,  1}}, {{ 0,  0,  1}},
            {{ 0,  0, -1}}, {{ 0,  0, -1}}, {{ 0,  0, -1}}, {{ 0,  0, -1}},
            {{-1,  0,  0}}, {{-1,  0,  0}}, {{-1,  0,  0}}, {{-1,  0,  0}},
            {{ 1,  0,  0}}, {{ 1,  0,  0}}, {{ 1,  0,  0}}, {{ 1,  0,  0}},
            {{ 0,  1,  0}}, {{ 0,  1,  0}}, {{ 0,  1,  0}}, {{ 0,  1,  0}},
            {{ 0, -1,  0}}, {{ 0, -1,  0}}, {{ 0, -1,  0}}, {{ 0, -1,  0}}
        };

        m_CubeMesh.uvs = {
            {{0.0f, 0.0f}}, {{1.0f, 0.0f}}, {{1.0f, 1.0f}}, {{0.0f, 1.0f}},
            {{0.0f, 0.0f}}, {{1.0f, 0.0f}}, {{1.0f, 1.0f}}, {{0.0f, 1.0f}},
            {{0.0f, 0.0f}}, {{1.0f, 0.0f}}, {{1.0f, 1.0f}}, {{0.0f, 1.0f}},
            {{0.0f, 0.0f}}, {{1.0f, 0.0f}}, {{1.0f, 1.0f}}, {{0.0f, 1.0f}},
            {{0.0f, 0.0f}}, {{1.0f, 0.0f}}, {{1.0f, 1.0f}}, {{0.0f, 1.0f}},
            {{0.0f, 0.0f}}, {{1.0f, 0.0f}}, {{1.0f, 1.0f}}, {{0.0f, 1.0f}}
        };

        m_CubeMesh.faces = {
            { 0,  1,  2, 0}, { 2,  3,  0, 0},
            { 4,  5,  6, 4}, { 6,  7,  4, 4},
            { 8,  9, 10, 8}, {10, 11,  8, 8},
            {12, 13, 14,12}, {14, 15, 12,12},
            {16, 17, 18,16}, {18, 19, 16,16},
            {20, 21, 22,20}, {22, 23, 20,20}
        };
    }

    void Application::Run()
    {
        AE_ENGINE_INFO("Engine Core Loop Started.");
        SetupMockData(); 

        auto lastTime = std::chrono::high_resolution_clock::now();

        while (m_Running)
        {
            m_Window->OnUpdate();

            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
            lastTime = currentTime;

            if (m_DebounceTimer > 0.0f) {
                m_DebounceTimer -= deltaTime;
            }

            if (Input::IsKeyPressed(GLFW_KEY_A) && m_DebounceTimer <= 0.0f)
            {
                m_RenderState = (m_RenderState + 1) % 3;
                m_DebounceTimer = DEBOUNCE_DELAY;
                AE_ENGINE_TRACE("A Key Pressed! Switching to State: {0}", m_RenderState);

                if (m_IsCubeUploaded)
                {
                    AE_ENGINE_TRACE("GC: Freeing previous Cube Mesh from GPU...");
                    m_LowLevelRenderer->FreeMesh(m_CubeHandle);
                    m_IsCubeUploaded = false;
                }

                if (m_RenderState == 1)
                {
                    AE_ENGINE_INFO("Uploading Cube Mesh (WOOD) to GPU...");
                    m_CubeHandle = m_LowLevelRenderer->UploadMesh(m_CubeMesh);
                    m_CubeHandle.materialIndex = m_WoodTexID;
                    m_IsCubeUploaded = true;
                }
                else if (m_RenderState == 2)
                {
                    AE_ENGINE_INFO("Uploading Cube Mesh (STONE) to GPU...");
                    m_CubeHandle = m_LowLevelRenderer->UploadMesh(m_CubeMesh);
                    m_CubeHandle.materialIndex = m_StoneTexID;
                    m_IsCubeUploaded = true;
                }
            }

            if (m_Window->IsResizing() || m_Window->GetWidth() == 0 || m_Window->GetHeight() == 0) { continue; }
            
            if (m_HighLevelRenderer)
            {
                UniformBufferObject camera {};
                camera.view = glm::lookAt(glm::vec3(0.0f, 2.25f, 5.0f), glm::vec3(0.0f, 0.75f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                
                auto extent = m_SwapChain->GetExtent();
                camera.proj = glm::perspective(glm::radians(45.0f), extent.width / (float)extent.height, 0.1f, 10.0f);
                camera.proj[1][1] *= -1;

                m_HighLevelRenderer->BeginScene(camera);

                static float time = 0.0f;
                time += deltaTime;

                if (m_RenderState == 1 || m_RenderState == 2)
                {
                    glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
                    glm::mat4 r = glm::rotate(glm::mat4(1.0f), time, glm::vec3(0.5f, 1.0f, 0.2f));
                    m_HighLevelRenderer->Submit(t * r, m_CubeHandle);
                }

                m_HighLevelRenderer->EndScene();
            }

            if (m_Window->ShouldClose()) { m_Running = false; }
        }

        vkDeviceWaitIdle(m_VulkanContext->GetDevice());
        
        if (m_IsCubeUploaded) { m_LowLevelRenderer->FreeMesh(m_CubeHandle); }
        
        AE_ENGINE_INFO("Engine Core Loop Stopped.");
    }

    void Application::OnWindowResize(int width, int height) 
    {
        if (m_SwapChain) { m_SwapChain->SetFramebufferResized(true); }
    }
}