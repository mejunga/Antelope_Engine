#pragma once

#include <string>
#include <cstdint>


struct GLFWwindow;

namespace Antelope
{
    struct WindowProps
    {
        std::string Title;
        uint32_t Width;
        uint32_t Height;

        WindowProps(const std::string& title = "Antelope Editor",
                    uint32_t width = 1280,
                    uint32_t height = 720)
            : Title(title), Width(width), Height(height) {}
    };

    class Window
    {
        public:
            Window(const WindowProps& props = WindowProps());
            ~Window();

            Window(const Window&) = delete;
            Window& operator=(const Window&) = delete;

            void OnUpdate();
            bool ShouldClose() const;
            
            inline uint32_t GetWidth() const { return m_Data.Width; }
            inline uint32_t GetHeight() const { return m_Data.Height; }
            inline GLFWwindow* GetNativeWindow() const { return m_Window; }

        private:
            void Shutdown();
            void Init(const WindowProps& props);

        private:
            GLFWwindow* m_Window;

            struct WindowData
            {
                std::string Title;
                uint32_t Width;
                uint32_t Height;
            };

            WindowData m_Data;  
    };
}