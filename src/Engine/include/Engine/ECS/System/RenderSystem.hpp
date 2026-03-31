#pragma once

#include <Engine/ECS/World.hpp>
#include <Engine/Renderer/Graphics/Renderer.hpp>
#ifdef ANTELOPE_EDITOR_MODE
#include <Engine/Renderer/Graphics/EditorCamera.hpp>
#endif

#include <memory>


namespace Antelope
{
    class RenderSystem
    {
        public:
        #ifdef ANTELOPE_EDITOR_MODE
            static void RenderEditor(World& world, std::shared_ptr<Renderer> renderer, const EditorCamera& camera);
        #endif
            static void RenderRuntime(World& world, std::shared_ptr<Renderer> renderer);
    };
}