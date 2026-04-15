#pragma once

#include <Engine/Renderer/Graphics/Model.hpp>

#include <string>


namespace Antelope
{
    class ModelLoader
    {
        public:
            static ModelData Load(const std::string& filepath, bool preserveSkeleton = false);
    };
}