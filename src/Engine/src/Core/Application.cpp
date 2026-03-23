#include <Engine/Core/Application.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Platform/Input.hpp>
#include <Engine/Platform/Window.hpp>
#include <Engine/Renderer/Vulkan/VulkanContext.hpp>
#include <Engine/Renderer/Vulkan/SwapChain.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>
#include <Engine/Renderer/Graphics/Camera.hpp>
#include <Engine/AssetManager/ModelLoader.hpp>

#include <GLFW/glfw3.h>
#include <chrono>

namespace Antelope
{
    Application *Application::s_Instance { nullptr };

    Application::Application() : m_EditorCamera(glm::vec3(0.0f, 2.0f, 20.0f))
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
        m_Renderer = std::make_shared<Renderer>(m_VulkanContext, m_SwapChain);
        m_TextureManager = std::make_shared<TextureManager>(m_VulkanContext, m_Renderer);

        m_World = std::make_shared<World>();
    }

    Application::~Application() {}

    void Application::SetupMockData()
    {
        m_BearTexID = m_TextureManager->LoadTexture("Assets/Bear.png");
        m_BearMesh = ModelLoader::Load("Assets/Bear_DEMO.fbx");

        m_GorillaTexID = m_TextureManager->LoadTexture("Assets/gorilla.png");
        m_GorillaMesh = ModelLoader::Load("Assets/Gorilla_hd.fbx");

        m_Renderer->UpdateTextureDescriptors(m_TextureManager->GetGlobalTextures());
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

            if (m_DebounceTimer > 0.0f)
            {
                m_DebounceTimer -= deltaTime;
            }

            if (Input::IsKeyPressed(GLFW_KEY_SPACE) && m_DebounceTimer <= 0.0f)
            {
                m_RenderState = (m_RenderState + 1) % 3;
                m_DebounceTimer = DEBOUNCE_DELAY;
                AE_ENGINE_TRACE("Space Key Pressed! Switching to State: {0}", m_RenderState);

                if (m_ActiveEntity)
                {
                    auto& meshComponent = m_ActiveEntity.GetComponent<MeshComponent>();
                    m_Renderer->FreeMesh(meshComponent.Handle);
                    m_World->DestroyEntity(m_ActiveEntity);
                    m_ActiveEntity = {};
                }

                if (m_RenderState == 1) 
                {
                    AE_ENGINE_INFO("Creating Bear Entity...");
                    MeshHandle handle = m_Renderer->UploadMesh(m_BearMesh);
                    handle.materialIndex = m_BearTexID;

                    m_ActiveEntity = m_World->CreateEntity("Bear");
                    m_ActiveEntity.AddComponent<MeshComponent>(handle);
                    
                    auto& transform = m_ActiveEntity.GetComponent<TransformComponent>();
                    transform.Scale = glm::vec3(0.005f);
                }
                else if (m_RenderState == 2)
                {
                    AE_ENGINE_INFO("Creating Gorilla Entity...");
                    MeshHandle handle = m_Renderer->UploadMesh(m_GorillaMesh);
                    handle.materialIndex = m_GorillaTexID;

                    m_ActiveEntity = m_World->CreateEntity("Gorilla");
                    m_ActiveEntity.AddComponent<MeshComponent>(handle);
                    
                    auto& transform = m_ActiveEntity.GetComponent<TransformComponent>();
                    transform.Translation = glm::vec3(0.0f, 0.0f, 0.0f);
                    transform.Scale = glm::vec3(7.5f);
                }
            }

            if (m_Window->IsResizing() || m_Window->GetWidth() == 0 || m_Window->GetHeight() == 0) { continue; }

            m_EditorCamera.OnUpdate(deltaTime);
            m_World->OnUpdateEditor(deltaTime, m_EditorCamera);

            if (m_Window->ShouldClose()) { m_Running = false; }
        }

        vkDeviceWaitIdle(m_VulkanContext->GetDevice());
        
        if (m_ActiveEntity) 
        { 
            auto& meshComponent = m_ActiveEntity.GetComponent<MeshComponent>();
            m_Renderer->FreeMesh(meshComponent.Handle); 
        }
        
        AE_ENGINE_INFO("Engine Core Loop Stopped.");
    }

    void Application::OnWindowResize(int width, int height) 
    {
        if (m_SwapChain) { m_SwapChain->SetFramebufferResized(true); }
    }
}