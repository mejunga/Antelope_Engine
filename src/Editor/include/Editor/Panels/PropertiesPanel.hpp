#pragma once

#include <Engine/ECS/Entity.hpp>


namespace Antelope::Editor
{
    class PropertiesPanel
    {
        public:
            PropertiesPanel() = default;
            void OnUIRender(Entity selectedEntity);
    };
}