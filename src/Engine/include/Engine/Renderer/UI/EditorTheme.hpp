#pragma once
#ifdef ANTELOPE_EDITOR_MODE

#include <imgui.h>

namespace Antelope
{
    struct EditorThemeProps
    {
        float WindowRounding { 4.0f };
        float FrameRounding { 3.0f };
        float TabRounding { 3.0f };
        float BorderSize { 0.0f };

        ImVec4 WindowBg;
        ImVec4 Header;
        ImVec4 HeaderHovered;
        ImVec4 HeaderActive;
        
        ImVec4 Tab;
        ImVec4 TabHovered;
        ImVec4 TabActive;
        ImVec4 TabUnfocused;
        ImVec4 TabUnfocusedActive;
        
        ImVec4 TitleBg;
        ImVec4 TitleBgActive;
        ImVec4 TitleBgCollapsed;

        ImVec4 DockingPreview;
        ImVec4 DockingEmptyBg;

        ImVec4 Text;
        ImVec4 TextDisabled;
    };

    class ThemeProvider
    {
        public:
            static EditorThemeProps GetDarkTheme()
            {
                EditorThemeProps theme {};
                theme.WindowRounding = 4.0f;
                theme.FrameRounding = 3.0f;
                theme.TabRounding = 3.0f;
                theme.BorderSize = 0.0f;

                theme.WindowBg = ImVec4{ 0.15f, 0.15f, 0.15f, 1.0f };
                theme.Header = ImVec4{ 0.22f, 0.22f, 0.22f, 1.0f };
                theme.HeaderHovered = ImVec4{ 0.28f, 0.28f, 0.28f, 1.0f };
                theme.HeaderActive = ImVec4{ 0.20f, 0.20f, 0.20f, 1.0f };
                
                theme.Tab = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
                theme.TabHovered = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
                theme.TabActive = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
                theme.TabUnfocused = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
                theme.TabUnfocusedActive = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };

                theme.TitleBg = ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f };
                theme.TitleBgActive = ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f };
                theme.TitleBgCollapsed = ImVec4{ 0.08f, 0.0805f, 0.081f, 1.0f };

                theme.DockingPreview = ImVec4{ 0.2f, 0.5f, 0.8f, 0.3f }; 
                theme.DockingEmptyBg = ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f };

                theme.Text = ImVec4{ 0.9f, 0.9f, 0.9f, 1.0f };
                theme.TextDisabled = ImVec4{ 0.5f, 0.5f, 0.5f, 1.0f };

                return theme;
            }
    };
}
#endif