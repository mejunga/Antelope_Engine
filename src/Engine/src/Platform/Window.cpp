#include <Engine/Platform/Window.hpp>
#include <Engine/Debug/Log.hpp>
#include <GLFW/glfw3.h>

namespace Antelope
{
    static bool s_GLFWInitialized { false };

    Window::Window(const WindowProps& props)
    {
        Init(props);
    }

    Window::~Window()
    {
        Shutdown();
    }

    void Window::Init(const WindowProps& props)
    {
        m_Data.Title = props.Title;
        m_Data.Height = props.Height;
        m_Data.Width = props.Width;

        AE_ENGINE_INFO("Creating window: {0} ({1}x{2})", props.Title, props.Width, props.Height);

        if(!s_GLFWInitialized) 
        {
            int succes { glfwInit() };

            if(!succes)
            {
                AE_ENGINE_ERROR("Could not initialize GLFW!");
                return;
            }
            
            s_GLFWInitialized = true;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        m_Window = glfwCreateWindow((int)m_Data.Width, (int)m_Data.Height, m_Data.Title.c_str(), nullptr, nullptr);

        if(!m_Window)
        {
            AE_ENGINE_ERROR("Failed to create GLFW window");
            return;
        }

        glfwSetWindowUserPointer(m_Window, &m_Data);
    }

    void Window::Shutdown()
    {
        if(m_Window) 
        {
            glfwDestroyWindow(m_Window);
            AE_ENGINE_INFO("Window destroyed");
        }
    }

    void Window::OnUpdate()
    {
        glfwPollEvents();
    }

    bool Window::ShouldClose() const
    {
        return glfwWindowShouldClose(m_Window);
    }
}