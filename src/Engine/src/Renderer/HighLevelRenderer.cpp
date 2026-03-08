#include <Engine/Renderer/HighLevelRenderer.hpp>
#include <Engine/Debug/Log.hpp>

namespace Antelope
{
    HighLevelRenderer::HighLevelRenderer(std::shared_ptr<LowLevelRenderer> lowLevelRenderer) 
        : m_LowLevelRenderer(lowLevelRenderer)
    {
        AE_ENGINE_INFO("HighLevelRenderer initialized.");
    }

    HighLevelRenderer::~HighLevelRenderer()
    {
        AE_ENGINE_TRACE("HighLevelRenderer destroyed.");
    }
}