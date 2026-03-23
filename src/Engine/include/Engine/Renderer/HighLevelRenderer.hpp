#pragma once

#include <Engine/Renderer/Mesh.hpp>
#include <Engine/Renderer/Camera.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <memory>
#include <vector>


namespace Antelope
{    
    class Renderer;
    
    class HighLevelRenderer
    {
        public:
            HighLevelRenderer(std::shared_ptr<Renderer> renderer);
            ~HighLevelRenderer() = default;

            void BeginScene(const UniformBufferObject& cameraData);
            void Submit(const glm::mat4& transform, const MeshHandle& mesh);
            void EndScene();

        private:
            std::shared_ptr<Renderer> m_Renderer;

            UniformBufferObject m_CurrentCamera;
            
            std::vector<RenderCommand> m_RenderQueue; 
    };
}