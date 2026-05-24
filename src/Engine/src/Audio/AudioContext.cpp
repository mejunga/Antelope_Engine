#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <Engine/Audio/AudioContext.hpp>
#include <Engine/Debug/Log.hpp>


namespace Antelope
{
    AudioContext::AudioContext()
    {
        m_Engine = new ma_engine();

        if (ma_engine_init(nullptr, m_Engine) != MA_SUCCESS)
        {
            AE_ENGINE_ERROR("AudioContext: Failed to initialize audio engine.");
            delete m_Engine;
            m_Engine = nullptr;
            return;
        }
        
        m_Initialized = true;
        AE_ENGINE_INFO("AudioContext: Audio engine initialized.");
    }

    AudioContext::~AudioContext()
    {
        if (m_Initialized && m_Engine) { ma_engine_uninit(m_Engine); }
        delete m_Engine;
    }

    ma_engine& AudioContext::GetEngine() { return *m_Engine; }
}