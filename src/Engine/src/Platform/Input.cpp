#include <Engine/Platform/Input.hpp>
#include <Engine/Core/Application.hpp>
#include <Engine/Platform/Window.hpp>

#include <GLFW/glfw3.h>


namespace Antelope
{
    bool Input::IsKeyPressed(int keycode)
    {
        auto window { Application::Get().GetWindow().GetNativeWindow() };
        auto state { glfwGetKey(window, keycode) };
        return state == GLFW_PRESS || state == GLFW_REPEAT;
    }

    bool Input::IsMouseButtonClicked(int button)
    {
        auto window { Application::Get().GetWindow().GetNativeWindow() };
        auto state { glfwGetMouseButton(window, button) };
        return state == GLFW_PRESS;
    }

    void Input::GetMousePosition(float& x, float& y)
    {
        auto window { Application::Get().GetWindow().GetNativeWindow() };
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        x = static_cast<float>(xpos);
        y = static_cast<float>(ypos);
    }
}