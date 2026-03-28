#ifdef ANTELOPE_EDITOR_MODE
#include <Engine/Renderer/Graphics/EditorCamera.hpp>
#include <Engine/Platform/Input.hpp>
#include <Engine/Core/Application.hpp>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

namespace Antelope
{
    EditorCamera::EditorCamera(const glm::vec3& position)
        : m_Position{position}, m_Yaw{-90.0f}, m_Pitch{0.0f}, 
          m_MovementSpeed{10.0f}, m_MouseSensitivity{0.1f}, m_FirstMouse{true}
    {
        m_WorldUp = glm::vec3(0.0f, 1.0f, 0.0f);
        m_LastMouseX = 0.0f;
        m_LastMouseY = 0.0f;
        UpdateCameraVectors();
    }

    void EditorCamera::OnUpdate(float deltaTime)
    {
        float velocity { m_MovementSpeed * deltaTime };
        
        if (Input::IsMouseButtonClicked(GLFW_MOUSE_BUTTON_RIGHT))
        {
            if (Input::IsKeyPressed(GLFW_KEY_W)) { m_Position += m_Front * velocity; }
            if (Input::IsKeyPressed(GLFW_KEY_S)) { m_Position -= m_Front * velocity; }
            if (Input::IsKeyPressed(GLFW_KEY_A)) { m_Position -= m_Right * velocity; }
            if (Input::IsKeyPressed(GLFW_KEY_D)) { m_Position += m_Right * velocity; }
            if (Input::IsKeyPressed(GLFW_KEY_E)) { m_Position += m_Up * velocity; }
            if (Input::IsKeyPressed(GLFW_KEY_Q)) { m_Position -= m_Up * velocity; }

            float mouseX { 0.0f }, mouseY { 0.0f };
            Input::GetMousePosition(mouseX, mouseY);

            if (m_FirstMouse)
            {
                m_LastMouseX = mouseX;
                m_LastMouseY = mouseY;
                m_FirstMouse = false;
            }

            float xoffset { mouseX - m_LastMouseX };
            float yoffset { m_LastMouseY - mouseY }; 

            m_LastMouseX = mouseX;
            m_LastMouseY = mouseY;

            xoffset *= m_MouseSensitivity;
            yoffset *= m_MouseSensitivity;

            m_Yaw   += xoffset;
            m_Pitch += yoffset;

            if (m_Pitch > 89.0f) { m_Pitch = 89.0f; }
            if (m_Pitch < -89.0f) { m_Pitch = -89.0f; }

            UpdateCameraVectors();
        }
        else
        {
            m_FirstMouse = true; 
        }
    }

    void EditorCamera::UpdateCameraVectors()
    {
        glm::vec3 front 
        {
            cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch)),
            sin(glm::radians(m_Pitch)),
            sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch))
        };

        m_Front = glm::normalize(front);
        m_Right = glm::normalize(glm::cross(m_Front, m_WorldUp));
        m_Up    = glm::normalize(glm::cross(m_Right, m_Front));
    }

    glm::mat4 EditorCamera::GetViewMatrix() const
    {
        return glm::lookAt(m_Position, m_Position + m_Front, m_Up);
    }

    glm::mat4 EditorCamera::GetProjectionMatrix(float width, float height) const
    {
        glm::mat4 proj { glm::perspective(glm::radians(45.0f), width / height, 0.1f, 1000.0f) };
        proj[1][1] *= -1;
        return proj;
    }
}
#endif