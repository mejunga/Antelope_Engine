#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D srcTexture;

layout(push_constant) uniform PushConstants
{
    vec2 texelSize;
    float threshold;
    float knee;
} pc;

vec3 prefilter(vec3 c)
{
    float brightness = max(c.r, max(c.g, c.b));
    float rq = clamp(brightness - pc.threshold + pc.knee, 0.0, pc.knee * 2.0);
    rq = (rq * rq) / (4.0 * pc.knee + 0.0001);
    float factor = max(rq, brightness - pc.threshold) / max(brightness, 0.0001);
    return c * factor;
}

void main()
{
    vec2 uv = inUV;
    float x = pc.texelSize.x;
    float y = pc.texelSize.y;

    vec3 a = texture(srcTexture, vec2(uv.x - 2*x, uv.y + 2*y)).rgb;
    vec3 b = texture(srcTexture, vec2(uv.x, uv.y + 2*y)).rgb;
    vec3 c = texture(srcTexture, vec2(uv.x + 2*x, uv.y + 2*y)).rgb;
    vec3 d = texture(srcTexture, vec2(uv.x - 2*x, uv.y)).rgb;
    vec3 e = texture(srcTexture, vec2(uv.x, uv.y)).rgb;
    vec3 f = texture(srcTexture, vec2(uv.x + 2*x, uv.y)).rgb;
    vec3 g = texture(srcTexture, vec2(uv.x - 2*x, uv.y - 2*y)).rgb;
    vec3 h = texture(srcTexture, vec2(uv.x, uv.y - 2*y)).rgb;
    vec3 i = texture(srcTexture, vec2(uv.x + 2*x, uv.y - 2*y)).rgb;
    vec3 j = texture(srcTexture, vec2(uv.x - x, uv.y + y)).rgb;
    vec3 k = texture(srcTexture, vec2(uv.x + x, uv.y + y)).rgb;
    vec3 l = texture(srcTexture, vec2(uv.x - x, uv.y - y)).rgb;
    vec3 m = texture(srcTexture, vec2(uv.x + x, uv.y - y)).rgb;

    vec3 downsample = e * 0.125;
    downsample += (a+c+g+i) * 0.03125;
    downsample += (b+d+f+h) * 0.0625;
    downsample += (j+k+l+m) * 0.125;

    if (pc.threshold > 0.0)
    {
        downsample = prefilter(downsample);
    }

    outColor = vec4(downsample, 1.0);
}
