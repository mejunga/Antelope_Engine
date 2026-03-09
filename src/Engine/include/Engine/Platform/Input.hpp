#pragma once


namespace Antelope
{
    class Input
    {
        public:
            Input() = delete;
            
            static bool IsKeyPressed(int keycode);
            static bool IsMouseButtonClicked(int button);
    };
}