#pragma once
#ifdef ANTELOPE_EDITOR_MODE

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
            inline bool IsActive() const { return m_IsActive; }
            void SetActive(bool active) { m_IsActive = active; }

        private:
            void UpdateCameraVectors();

        private:
            glm::vec3 m_Position { 0.0f, 0.0f, 0.0f };
            glm::vec3 m_Front { 0.0f, 0.0f, -1.0f };
            glm::vec3 m_Up { 0.0f, 1.0f, 0.0f };
            glm::vec3 m_Right { 1.0f, 0.0f, 0.0f };
            glm::vec3 m_WorldUp { 0.0f, 1.0f, 0.0f };

            float m_Yaw { -90.0f };
            float m_Pitch { 0.0f };

            float m_MovementSpeed { 10.0f };
            float m_MouseSensitivity { 0.1f };

            float m_LastMouseX { 0.0f };
            float m_LastMouseY { 0.0f };
            bool m_FirstMouse { true };
            bool m_IsActive { false };
    };
}
#endif