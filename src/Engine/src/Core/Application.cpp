#include <Engine/Core/Application.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Platform/Input.hpp>
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

        m_VulkanContext = std::make_unique<VulkanContext>();
        m_VulkanContext->Init(m_Window->GetNativeWindow());
    }

    Application::~Application() {}

    void Application::Run()
    {
        while(m_Running)
        {
            m_Window->OnUpdate();

            if(m_Window->ShouldClose()) 
            {
                m_Running = false;
            }

            if(Input::IsKeyPressed(GLFW_KEY_A))
            {
                AE_ENGINE_TRACE("'A' key is pressed");
            }
        }
    }
}