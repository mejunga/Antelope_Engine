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
        if(s_Instance) 
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
    }

    Application::~Application() {}

    void Application::SetupMockData()
    {
        m_DiamondMesh.positions = {
            {{ 0.0f,  0.6f,  0.0f}}, {{ 0.0f, -0.6f,  0.0f}},
            {{ 0.5f,  0.0f,  0.0f}}, {{-0.5f,  0.0f,  0.0f}},
            {{ 0.0f,  0.0f,  0.5f}}, {{ 0.0f,  0.0f, -0.5f}}
        };
        m_DiamondMesh.colors = {
            {{1.0f, 0.0f, 0.0f}}, {{0.0f, 1.0f, 1.0f}},
            {{0.0f, 1.0f, 0.0f}}, {{1.0f, 0.0f, 1.0f}},
            {{0.0f, 0.0f, 1.0f}}, {{1.0f, 1.0f, 0.0f}}
        };
        float n = 0.57735f;
        m_DiamondMesh.normals = {
            {{ n, n,  n}}, {{ n, n, -n}}, {{-n, n, -n}}, {{-n, n,  n}},
            {{ n, -n,  n}}, {{ n, -n, -n}}, {{-n, -n, -n}}, {{-n, -n,  n}}
        };
        m_DiamondMesh.faces = {
            {0, 2, 4, 0}, {0, 5, 2, 1}, {0, 3, 5, 2}, {0, 4, 3, 3},
            {1, 4, 2, 4}, {1, 2, 5, 5}, {1, 5, 3, 6}, {1, 3, 4, 7}
        };

        m_CubeMesh.positions = {
            {{-0.5f, -0.5f, -0.5f}}, {{ 0.5f, -0.5f, -0.5f}}, {{ 0.5f,  0.5f, -0.5f}}, {{-0.5f,  0.5f, -0.5f}},
            {{-0.5f, -0.5f,  0.5f}}, {{ 0.5f, -0.5f,  0.5f}}, {{ 0.5f,  0.5f,  0.5f}}, {{-0.5f,  0.5f,  0.5f}}
        };
        m_CubeMesh.colors = {
            {{0.8f, 0.2f, 0.2f}}, {{0.2f, 0.8f, 0.2f}}, {{0.2f, 0.2f, 0.8f}}, {{0.8f, 0.8f, 0.2f}},
            {{0.8f, 0.2f, 0.8f}}, {{0.2f, 0.8f, 0.8f}}, {{0.9f, 0.9f, 0.9f}}, {{0.1f, 0.1f, 0.1f}}
        };
        m_CubeMesh.normals = {
            {{0,0,-1}}, {{0,0,1}}, {{-1,0,0}}, {{1,0,0}}, {{0,-1,0}}, {{0,1,0}}
        };
        m_CubeMesh.faces = {
            {0, 1, 2, 0}, {2, 3, 0, 0}, {5, 4, 7, 1}, {7, 6, 5, 1},
            {4, 0, 3, 2}, {3, 7, 4, 2}, {1, 5, 6, 3}, {6, 2, 1, 3},
            {4, 5, 1, 4}, {1, 0, 4, 4}, {3, 2, 6, 5}, {6, 7, 3, 5}
        };

        m_PyramidMesh.positions = {
            {{ 0.0f,  0.5f,  0.0f}},
            {{-0.5f, -0.5f, -0.5f}}, {{ 0.5f, -0.5f, -0.5f}}, 
            {{ 0.5f, -0.5f,  0.5f}}, {{-0.5f, -0.5f,  0.5f}}
        };
        m_PyramidMesh.colors = {
            {{1.0f, 1.0f, 0.0f}}, {{0.5f, 0.0f, 0.5f}}, {{0.5f, 0.0f, 0.5f}}, {{0.5f, 0.0f, 0.5f}}, {{0.5f, 0.0f, 0.5f}}
        };
        m_PyramidMesh.normals = m_DiamondMesh.normals; 
        m_PyramidMesh.faces = {
            {0, 2, 1, 0}, {0, 3, 2, 1}, {0, 4, 3, 2}, {0, 1, 4, 3},
            {1, 2, 3, 4}, {3, 4, 1, 5}
        };
    }

    void Application::Run()
    {
        AE_ENGINE_INFO("Engine Core Loop Started.");
        
        SetupMockData(); 

        auto lastTime = std::chrono::high_resolution_clock::now();

        while(m_Running)
        {
            m_Window->OnUpdate();

            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime).count();
            lastTime = currentTime;

            if (m_DebounceTimer > 0.0f) {
                m_DebounceTimer -= deltaTime;
            }

            if(Input::IsKeyPressed(GLFW_KEY_A) && m_DebounceTimer <= 0.0f)
            {
                m_RenderState = (m_RenderState + 1) % 5;
                m_DebounceTimer = DEBOUNCE_DELAY;
                
                AE_ENGINE_TRACE("A Key Pressed! Switching to State: {0}", m_RenderState);

                if(m_RenderState == 1 && !m_IsDiamondUploaded) {
                    AE_ENGINE_INFO("Uploading Diamond Mesh to GPU in background...");
                    m_DiamondHandle = m_LowLevelRenderer->UploadMesh(m_DiamondMesh);
                    m_IsDiamondUploaded = true;
                }
                else if (m_RenderState == 2 && !m_IsCubeUploaded) {
                    AE_ENGINE_INFO("Uploading Cube Mesh to GPU in background...");
                    m_CubeHandle = m_LowLevelRenderer->UploadMesh(m_CubeMesh);
                    m_IsCubeUploaded = true;
                }
                else if (m_RenderState == 3 && !m_IsPyramidUploaded) {
                    AE_ENGINE_INFO("Uploading Pyramid Mesh to GPU in background...");
                    m_PyramidHandle = m_LowLevelRenderer->UploadMesh(m_PyramidMesh);
                    m_IsPyramidUploaded = true;
                }
            }

            if(m_Window->IsResizing() || m_Window->GetWidth() == 0 || m_Window->GetHeight() == 0)
            {
                continue;
            }
            
            if(m_HighLevelRenderer)
            {
                UniformBufferObject camera {};
                camera.view = glm::lookAt(glm::vec3(0.0f, 2.25f, 5.0f), glm::vec3(0.0f, 0.75f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                
                auto extent = m_SwapChain->GetExtent();
                camera.proj = glm::perspective(glm::radians(45.0f), extent.width / (float)extent.height, 0.1f, 10.0f);
                camera.proj[1][1] *= -1;

                m_HighLevelRenderer->BeginScene(camera);

                static float time = 0.0f;
                time += deltaTime;

                if (m_RenderState == 1 || m_RenderState == 4) 
                {
                    glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
                    glm::mat4 r = glm::rotate(glm::mat4(1.0f), time, glm::vec3(0.0f, 1.0f, 0.0f));
                    m_HighLevelRenderer->Submit(t * r, m_DiamondHandle);
                }

                if (m_RenderState == 2 || m_RenderState == 4) 
                {
                    glm::mat4 tLeft = glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, 0.0f, 0.0f));
                    glm::mat4 tRight = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.0f, 0.0f));
                    glm::mat4 r = glm::rotate(glm::mat4(1.0f), -time, glm::vec3(0.0f, 1.0f, 0.0f));
                    m_HighLevelRenderer->Submit(tLeft * r, m_CubeHandle);
                    m_HighLevelRenderer->Submit(tRight * r, m_CubeHandle);
                }

                if (m_RenderState == 3 || m_RenderState == 4) 
                {
                    glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f));
                    glm::mat4 r = glm::rotate(glm::mat4(1.0f), time * 1.5f, glm::vec3(1.0f, 1.0f, 0.0f));
                    m_HighLevelRenderer->Submit(t * r, m_PyramidHandle);
                }

                m_HighLevelRenderer->EndScene();
            }

            if(m_Window->ShouldClose()) 
            {
                m_Running = false;
            }
        }
        
        AE_ENGINE_INFO("Engine Core Loop Stopped.");
    }

    void Application::OnWindowResize(int width, int height) 
    {
        if(m_SwapChain)
        {
            m_SwapChain->SetFramebufferResized(true);
        }
    }
}