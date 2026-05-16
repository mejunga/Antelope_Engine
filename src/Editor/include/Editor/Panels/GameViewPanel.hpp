#pragma once

#include <Engine/ECS/World.hpp>


namespace Antelope::Editor
{
    class GameViewPanel
    {
        public:
            void OnUIRender(World* world);
            void DrawPlaybackControls(World* world);
            bool IsPaused() const { return m_Paused; }
            bool IsGameViewActive() const { return m_IsActive; }

        private:
            bool m_Paused { false };
            bool m_IsActive { false };
    };
}