#pragma once
#ifdef ANTELOPE_EDITOR_MODE

#include <imgui.h>

namespace Antelope
{
    struct EditorThemeProps
    {
        float WindowRounding { 4.0f };
        float FrameRounding { 4.0f };
        float TabRounding { 4.0f };
        float BorderSize { 1.0f };
        float FrameBorderSize { 0.0f };

        ImVec4 WindowBg;
        ImVec4 ChildBg;
        ImVec4 PopupBg;

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

        ImVec4 FrameBg;
        ImVec4 FrameBgHovered;
        ImVec4 FrameBgActive;

        ImVec4 Button;
        ImVec4 ButtonHovered;
        ImVec4 ButtonActive;

        ImVec4 Border;
        ImVec4 Separator;
        ImVec4 SeparatorHovered;
        ImVec4 SeparatorActive;

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
                theme.FrameRounding = 4.0f;
                theme.TabRounding = 4.0f;
                theme.BorderSize = 1.0f;
                theme.FrameBorderSize = 0.0f;

                theme.WindowBg = ImVec4{ 0.09f, 0.09f, 0.15f, 1.0f };
                theme.ChildBg = ImVec4{ 0.08f, 0.08f, 0.13f, 1.0f };
                theme.PopupBg = ImVec4{ 0.11f, 0.11f, 0.18f, 1.0f };

                theme.Header = ImVec4{ 0.21f, 0.20f, 0.34f, 1.0f };
                theme.HeaderHovered = ImVec4{ 0.33f, 0.25f, 0.55f, 1.0f };
                theme.HeaderActive = ImVec4{ 0.55f, 0.28f, 0.78f, 1.0f };

                theme.Tab = ImVec4{ 0.07f, 0.07f, 0.12f, 1.0f };
                theme.TabHovered = ImVec4{ 0.21f, 0.20f, 0.34f, 1.0f };
                theme.TabActive = ImVec4{ 0.09f, 0.09f, 0.15f, 1.0f };
                theme.TabUnfocused = ImVec4{ 0.07f, 0.07f, 0.12f, 1.0f };
                theme.TabUnfocusedActive = ImVec4{ 0.09f, 0.09f, 0.15f, 1.0f };

                theme.TitleBg = ImVec4{ 0.05f, 0.05f, 0.08f, 1.0f };
                theme.TitleBgActive = ImVec4{ 0.07f, 0.07f, 0.12f, 1.0f };
                theme.TitleBgCollapsed = ImVec4{ 0.05f, 0.05f, 0.08f, 1.0f };

                theme.FrameBg = ImVec4{ 0.11f, 0.11f, 0.18f, 1.0f };
                theme.FrameBgHovered = ImVec4{ 0.21f, 0.20f, 0.34f, 1.0f };
                theme.FrameBgActive = ImVec4{ 0.44f, 0.24f, 0.70f, 1.0f };

                theme.Button = ImVec4{ 0.21f, 0.20f, 0.34f, 1.0f };
                theme.ButtonHovered = ImVec4{ 0.44f, 0.24f, 0.70f, 1.0f };
                theme.ButtonActive = ImVec4{ 0.55f, 0.28f, 0.78f, 1.0f };

                theme.Border = ImVec4{ 0.24f, 0.23f, 0.40f, 1.0f };
                theme.Separator = ImVec4{ 0.24f, 0.23f, 0.40f, 1.0f };
                theme.SeparatorHovered = ImVec4{ 0.44f, 0.24f, 0.70f, 1.0f };
                theme.SeparatorActive = ImVec4{ 0.55f, 0.28f, 0.78f, 1.0f };

                theme.DockingPreview = ImVec4{ 0.66f, 0.37f, 0.95f, 0.30f };
                theme.DockingEmptyBg = ImVec4{ 0.07f, 0.07f, 0.12f, 1.0f };

                theme.Text = ImVec4{ 0.78f, 0.77f, 0.86f, 1.0f };
                theme.TextDisabled = ImVec4{ 0.38f, 0.37f, 0.50f, 1.0f };

                return theme;
            }
    };
}
#endif