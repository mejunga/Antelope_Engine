#pragma once

#include <Engine/Renderer/Mesh.hpp>

#include <string>


namespace Antelope
{
    class ModelLoader
    {
        public:
            static MeshData Load(const std::string& filepath);
    };
}