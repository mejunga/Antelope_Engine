#include "Bear.hpp"

void Bear::OnCreate() {}

void Bear::OnUpdate(float dt)
{
    bool xDown { IsKeyDown(Antelope::Key::X) };

    if (xDown && !m_WasXPressed)
    {
        FireAnimationTrigger("Snort");
        PlayAudio(0);
    }
    
    m_WasXPressed = xDown;
}

void Bear::OnDestroy() {}