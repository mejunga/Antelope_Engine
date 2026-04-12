#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D maskTex;

layout(push_constant) uniform PushConstants
{
    vec4 outlineColor;
} push;

void main()
{
    vec2 texOffset = 2.0 / textureSize(maskTex, 0);
    float center = texture(maskTex, inUV).a;

    if (center == 0.0)
    {
        float n = texture(maskTex, inUV + vec2(0.0,  texOffset.y)).a;
        float s = texture(maskTex, inUV - vec2(0.0,  texOffset.y)).a;
        float e = texture(maskTex, inUV + vec2(texOffset.x,  0.0)).a;
        float w = texture(maskTex, inUV - vec2(texOffset.x,  0.0)).a;

        if (n > 0.0 || s > 0.0 || e > 0.0 || w > 0.0)
        {
            outColor = push.outlineColor;
            return;
        }
    }

    discard;
}