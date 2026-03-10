#include <Engine/Renderer/HighLevelRenderer.hpp>
#include <Engine/Renderer/LowLevelRenderer.hpp>

namespace Antelope
{
    HighLevelRenderer::HighLevelRenderer(std::shared_ptr<LowLevelRenderer> lowLevelRenderer)
        : m_LowLevelRenderer(lowLevelRenderer)
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
        m_LowLevelRenderer->DrawFrame(m_CurrentCamera, m_RenderQueue);
    }
}