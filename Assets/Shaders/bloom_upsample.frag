#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D srcTexture;
layout(binding = 1) uniform sampler2D currentMipTexture;

layout(push_constant) uniform PushConstants
{
    vec2 texelSize;
    float filterRadius;
} pc;

void main()
{
    float x = pc.texelSize.x;
    float y = pc.texelSize.y;

    vec3 a = texture(srcTexture, vec2(inUV.x - x, inUV.y + y)).rgb;
    vec3 b = texture(srcTexture, vec2(inUV.x,     inUV.y + y)).rgb;
    vec3 c = texture(srcTexture, vec2(inUV.x + x, inUV.y + y)).rgb;
    vec3 d = texture(srcTexture, vec2(inUV.x - x, inUV.y)).rgb;
    vec3 e = texture(srcTexture, vec2(inUV.x,     inUV.y)).rgb;
    vec3 f = texture(srcTexture, vec2(inUV.x + x, inUV.y)).rgb;
    vec3 g = texture(srcTexture, vec2(inUV.x - x, inUV.y - y)).rgb;
    vec3 h = texture(srcTexture, vec2(inUV.x,     inUV.y - y)).rgb;
    vec3 i = texture(srcTexture, vec2(inUV.x + x, inUV.y - y)).rgb;

    vec3 upsample = e * 4.0;
    upsample += (b+d+f+h) * 2.0;
    upsample += (a+c+g+i) * 1.0;
    upsample *= 1.0 / 16.0;

    vec3 currentDetail = texture(currentMipTexture, inUV).rgb;
    
    outColor = vec4(currentDetail + upsample * 0.5, 1.0);
}
