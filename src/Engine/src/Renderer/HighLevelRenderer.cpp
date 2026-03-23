#include <Engine/Renderer/HighLevelRenderer.hpp>
#include <Engine/Renderer/Renderer.hpp>


namespace Antelope
{
    HighLevelRenderer::HighLevelRenderer(std::shared_ptr<Renderer> lowLevelRenderer)
        : m_Renderer(lowLevelRenderer)
    {
        m_RenderQueue.reserve(10000);
    }

    void HighLevelRenderer::BeginScene(const UniformBufferObject& cameraData)
    {
        m_CurrentCamera = cameraData;
        m_RenderQueue.clear();
    }

    void HighLevelRenderer::Submit(const glm::mat4& transform, const MeshHandle& mesh)
    {
        m_RenderQueue.push_back({transform, mesh});
    }

    void HighLevelRenderer::EndScene()
    {
        m_Renderer->DrawFrame(m_CurrentCamera, m_RenderQueue);
    }
}