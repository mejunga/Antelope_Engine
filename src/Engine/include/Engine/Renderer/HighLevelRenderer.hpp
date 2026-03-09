#pragma once

#include "LowLevelRenderer.hpp"
#include <memory>


namespace Antelope
{
    class HighLevelRenderer
    {
        public:
            HighLevelRenderer(std::shared_ptr<LowLevelRenderer> lowLevelRenderer);
            ~HighLevelRenderer();

            // void BeginScene();
            // void Submit(Entity entity);
            // void EndScene();

        private:
            std::shared_ptr<LowLevelRenderer> m_LowLevelRenderer;
    };
}