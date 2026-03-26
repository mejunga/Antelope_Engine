#include <Engine/Platform/Window.hpp>
#include <Engine/Debug/Log.hpp>
#include <Engine/Core/Application.hpp>

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

    void Window::OnUpdate()
    {
        glfwPollEvents();

        if (m_Data.IsResizing && (glfwGetTime() - m_Data.LastResizeTime) > 0.15)
        {
            m_Data.IsResizing = false;
            AE_ENGINE_INFO("Window resized to: {0}x{1}", m_Data.Width, m_Data.Height);
            
            Application::Get().OnWindowResize(m_Data.Width, m_Data.Height); 
        }
    }

    bool Window::ShouldClose() const
    {
        return glfwWindowShouldClose(m_Window);
    }

    void Window::Shutdown()
    {
        if (m_Window) 
        {
            glfwDestroyWindow(m_Window);
            AE_ENGINE_TRACE("Window destroyed");
        }

        glfwTerminate();
        s_GLFWInitialized = false;
    }

    void Window::Init(const WindowProps& props)
    {
        m_Data.Title = props.Title;
        m_Data.Height = props.Height;
        m_Data.Width = props.Width;

        AE_ENGINE_INFO("Creating window: {0} ({1}x{2})", props.Title, props.Width, props.Height);

        if (!s_GLFWInitialized) 
        {
            if (glfwInit() == GLFW_FALSE)
            {
                AE_ENGINE_ERROR("Could not initialize GLFW!");
                return;
            }
            
            AE_ENGINE_TRACE("GLFW Initialized successfully.");
            s_GLFWInitialized = true;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        m_Window = glfwCreateWindow((int)m_Data.Width, (int)m_Data.Height, m_Data.Title.c_str(), nullptr, nullptr);

        if (!m_Window)
        {
            AE_ENGINE_ERROR("Failed to create GLFW window");
            return;
        }

        glfwSetWindowUserPointer(m_Window, &m_Data);

        glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
        {
            auto data { reinterpret_cast<WindowData*>(glfwGetWindowUserPointer(window)) };
            
            data->Width = width;
            data->Height = height;
            data->LastResizeTime = glfwGetTime();
            data->IsResizing = true;
        });
    }
}