#pragma once

#include <memory>


namespace Antelope
{
    class EditorCamera;
    class Renderer;
    class World;

    class RenderSystem
    {
        public:
        #ifdef ANTELOPE_EDITOR_MODE
            static void RenderEditor(World& world, std::shared_ptr<Renderer> renderer, const EditorCamera& camera);
        #endif
            static void RenderRuntime(World& world, std::shared_ptr<Renderer> renderer);
    };
}