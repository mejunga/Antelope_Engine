#include <Engine/Core/TestEngine.hpp>

namespace Engine {
    void PrintTest() {
        glm::vec2 vector(20.0f, 10.0f);
        
        spdlog::info("Engine linking test: SUCCESS");
        spdlog::info("GLM linking test: x={}, y={}", vector.x, vector.y);
    }
}