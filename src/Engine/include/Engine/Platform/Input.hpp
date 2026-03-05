#pragma once

namespace Antelope
{
    class Input
    {
        public:
            static bool IsKeyPressed(int keycode);
            static bool IsMouseButtonClicked(int button);
    };
}