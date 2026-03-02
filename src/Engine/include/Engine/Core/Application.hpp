#pragma once

namespace Antelope
{
    class Application
    {
        public:
            Application();
            virtual ~Application();

            void Run();

        private:
            bool m_Running = true;
    };

    Application* CreateApplication();
}
