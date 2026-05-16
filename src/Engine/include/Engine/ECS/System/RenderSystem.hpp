#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

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
            static void RenderEditor(World& world, std::shared_ptr<Renderer> renderer, const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos);
            static void RenderBlack(std::shared_ptr<Renderer> renderer);
        #endif

            static void RenderRuntime(World& world, std::shared_ptr<Renderer> renderer);
    };
}