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

    void Application::Run()
    {
        AE_ENGINE_INFO("Engine Core Loop Started.");

        MeshData diamondMesh;
        
        diamondMesh.positions = {
            {{ 0.0f,  0.6f,  0.0f}}, {{ 0.0f, -0.6f,  0.0f}},
            {{ 0.5f,  0.0f,  0.0f}}, {{-0.5f,  0.0f,  0.0f}},
            {{ 0.0f,  0.0f,  0.5f}}, {{ 0.0f,  0.0f, -0.5f}}
        };
        
        diamondMesh.colors = {
            {{1.0f, 0.0f, 0.0f}}, {{0.0f, 1.0f, 1.0f}},
            {{0.0f, 1.0f, 0.0f}}, {{1.0f, 0.0f, 1.0f}},
            {{0.0f, 0.0f, 1.0f}}, {{1.0f, 1.0f, 0.0f}}
        };

        float n = 0.57735f;
        diamondMesh.normals = {
            {{ n, n,  n}}, {{ n, n, -n}}, {{-n, n, -n}}, {{-n, n,  n}},
            {{ n, -n,  n}}, {{ n, -n, -n}}, {{-n, -n, -n}}, {{-n, -n,  n}}
        };

        diamondMesh.faces = {
            {0, 2, 4, 0}, {0, 5, 2, 1}, {0, 3, 5, 2}, {0, 4, 3, 3},
            {1, 4, 2, 4}, {1, 2, 5, 5}, {1, 5, 3, 6}, {1, 3, 4, 7}
        };

        MeshHandle diamondHandle = m_LowLevelRenderer->UploadMesh(diamondMesh);

        while(m_Running)
        {
            m_Window->OnUpdate();

            if(m_Window->IsResizing() || m_Window->GetWidth() == 0 || m_Window->GetHeight() == 0)
            {
                continue;
            }
            
            if(m_HighLevelRenderer)
            {
                UniformBufferObject camera {};
                camera.view = glm::lookAt(glm::vec3(0.0f, 2.0f, 4.5f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                
                auto extent = m_SwapChain->GetExtent();
                camera.proj = glm::perspective(glm::radians(45.0f), extent.width / (float)extent.height, 0.1f, 10.0f);
                camera.proj[1][1] *= -1;

                m_HighLevelRenderer->BeginScene(camera);

                static auto startTime = std::chrono::high_resolution_clock::now();
                auto currentTime = std::chrono::high_resolution_clock::now();
                float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

                for(int i = -1; i <= 1; i++) 
                {
                    glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(i * 1.5f, 0.0f, 0.0f));
                    float rotationSpeed = (i == 0) ? time : -time;
                    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), rotationSpeed * glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                    glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.25f, 1.0f));
                    m_HighLevelRenderer->Submit(translation * rotation * scale, diamondHandle);
                }

                m_HighLevelRenderer->EndScene();
            }

            if(m_Window->ShouldClose()) 
            {
                m_Running = false;
            }

            if(Input::IsKeyPressed(GLFW_KEY_A))
            {
                AE_ENGINE_TRACE("'A' key is pressed");
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