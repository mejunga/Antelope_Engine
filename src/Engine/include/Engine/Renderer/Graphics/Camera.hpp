#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>


namespace Antelope
{
    struct UniformBufferObject
    {
        glm::mat4 view;
        glm::mat4 proj;
    };

    class EditorCamera
    {
        public:
            EditorCamera(const glm::vec3& position = glm::vec3(0.0f, 0.0f, 0.0f));

            void OnUpdate(float deltaTime);

            glm::mat4 GetViewMatrix() const;
            glm::mat4 GetProjectionMatrix(float width, float height) const;

            inline glm::vec3 GetPosition() const { return m_Position; }

        private:
            void UpdateCameraVectors();

        private:
            glm::vec3 m_Position;
            glm::vec3 m_Front;
            glm::vec3 m_Up;
            glm::vec3 m_Right;
            glm::vec3 m_WorldUp;

            float m_Yaw;
            float m_Pitch;

            float m_MovementSpeed;
            float m_MouseSensitivity;

            float m_LastMouseX;
            float m_LastMouseY;
            bool m_FirstMouse;
    };
}