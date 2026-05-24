#pragma once

struct ma_engine;

namespace Antelope
{
    class AudioContext
    {
        public:
            AudioContext();
            ~AudioContext();

            ma_engine& GetEngine();
            bool IsInitialized() const { return m_Initialized; }

        private:
            ma_engine* m_Engine { nullptr };
            bool m_Initialized { false };
    };
}