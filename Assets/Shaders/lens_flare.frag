#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D brightTexture;

layout(push_constant) uniform PushConstants
{
    vec2  sunUV;
    float sunIntensity;
    float pad0;
    vec2  moonUV;
    float moonIntensity;
    float pad1;
} pc;

float hash(float t)
{
    return fract(sin(t * 127.1) * 43758.5453);
}

float noise2(vec2 p)
{
    float a = hash(p.x + p.y * 57.0);
    float b = hash(p.x + 1.0 + p.y * 57.0);
    float c = hash(p.x + (p.y + 1.0) * 57.0);
    float d = hash(p.x + 1.0 + (p.y + 1.0) * 57.0);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

vec3 lensflare(vec2 uv, vec2 pos)
{
    vec2 main = uv - pos;
    vec2 uvd = uv * length(uv);

    float ang = atan(main.x, main.y);
    float dist = pow(length(main), 0.1);

    float d = length(main);
    float f0 = exp(-d * d * 50.0) * 0.6;
    float coronaMask = smoothstep(0.15, 0.02, d) * smoothstep(0.0, 0.02, d);
    f0 += coronaMask * (noise2(vec2(ang * 10.0 + pos.x * 5.0, dist * 20.0)) * 0.3 + 0.7) * 0.22;

    float f2 = max(1.0 / (1.0 + 32.0 * pow(length(uvd + 0.800 * pos), 2.0)), 0.0) * 0.25;
    float f22 = max(1.0 / (1.0 + 32.0 * pow(length(uvd + 0.850 * pos), 2.0)), 0.0) * 0.23;
    float f23 = max(1.0 / (1.0 + 32.0 * pow(length(uvd + 0.900 * pos), 2.0)), 0.0) * 0.21;

    vec2 uvx = mix(uv, uvd, -0.5);
    float f4 = max(0.01 - pow(length(uvx + 0.40 * pos), 2.4), 0.0) * 6.0;
    float f42 = max(0.01 - pow(length(uvx + 0.45 * pos), 2.4), 0.0) * 5.0;
    float f43 = max(0.01 - pow(length(uvx + 0.50 * pos), 2.4), 0.0) * 3.0;

    uvx = mix(uv, uvd, -0.4);
    float f5 = max(0.01 - pow(length(uvx + 0.20 * pos), 5.5), 0.0) * 2.0;
    float f52 = max(0.01 - pow(length(uvx + 0.40 * pos), 5.5), 0.0) * 2.0;
    float f53 = max(0.01 - pow(length(uvx + 0.60 * pos), 5.5), 0.0) * 2.0;

    uvx = mix(uv, uvd, -0.5);
    float f6 = max(0.01 - pow(length(uvx - 0.300 * pos), 1.6), 0.0) * 6.0;
    float f62 = max(0.01 - pow(length(uvx - 0.325 * pos), 1.6), 0.0) * 3.0;
    float f63 = max(0.01 - pow(length(uvx - 0.350 * pos), 1.6), 0.0) * 5.0;

    vec3 c = vec3(0.0);
    c.r += f2  + f4  + f5  + f6;
    c.g += f22 + f42 + f52 + f62;
    c.b += f23 + f43 + f53 + f63;    
    c = c * 1.6 - vec3(length(uvd) * 0.05);
    c += vec3(f0);
    return c;
}

float sampleVisibility(vec2 screenUV)
{
    float lum = 0.0;
    const float r = 0.03;
    const int N = 12;
    const float GOLDEN_ANGLE = 2.39996;

    for (int i = 0; i < N; ++i)
    {
        float theta = float(i) * GOLDEN_ANGLE;
        float ri = sqrt(float(i) + 0.5) / sqrt(float(N));
        vec2 off = vec2(cos(theta), sin(theta)) * ri * r;
        vec2 uv = clamp(screenUV + off, vec2(0.001), vec2(0.999));
        lum += dot(texture(brightTexture, uv).rgb, vec3(0.333));
    }
    
    return clamp(lum / float(N) * 5.0, 0.0, 1.0);
}


void main()
{
    vec2 res = vec2(textureSize(brightTexture, 0));
    float aspect = res.x / res.y;

    vec2 uv = inUV - 0.5;
    uv.x *= aspect;

    vec3 color = vec3(0.0);

    if (pc.sunUV.x > -1.5)
    {
        vec2 sunPos = pc.sunUV - 0.5;
        sunPos.x *= aspect;
        float vis = sampleVisibility(pc.sunUV);
        color += vec3(1.4, 1.2, 1.0) * lensflare(uv, sunPos) * vis * clamp(pc.sunIntensity, 0.0, 1.0);
    }

    if (pc.moonUV.x > -1.5)
    {
        vec2 moonPos = pc.moonUV - 0.5;
        moonPos.x *= aspect;
        float vis = (any(lessThan(pc.moonUV, vec2(0.01))) || any(greaterThan(pc.moonUV, vec2(0.99)))) ? 0.0 : 1.0;
        color += vec3(0.8, 0.9, 1.1) * lensflare(uv, moonPos) * vis * clamp(pc.moonIntensity * 0.3, 0.0, 0.3);
    }

    float w = color.r + color.g + color.b;
    color = mix(color, vec3(w) * 0.5, w * 0.1);

    outColor = vec4(color, 1.0);
}