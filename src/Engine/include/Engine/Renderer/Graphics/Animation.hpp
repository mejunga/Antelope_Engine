#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <string>

namespace Antelope
{
    struct KeyPosition { glm::vec3 position; float timeStamp; };
    struct KeyRotation { glm::quat orientation; float timeStamp; };
    struct KeyScale { glm::vec3 scale; float timeStamp; };

    struct BoneAnimationNode
    {
        std::string NodeName;
        std::vector<KeyPosition> Positions;
        std::vector<KeyRotation> Rotations;
        std::vector<KeyScale> Scales;
    };

    struct AnimationClip
    {
        std::string Name;
        float Duration { 0.0f };
        float TicksPerSecond { 0.0f };
        std::vector<BoneAnimationNode> Channels;
    };
}