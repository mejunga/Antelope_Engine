#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D sceneTexture;
layout(binding = 1) uniform sampler2D bloomTexture;
layout(binding = 2) uniform sampler3D colorGradingLUT;
layout(binding = 3) uniform sampler2D flareTexture;

layout(push_constant) uniform PushConstants
{
    float exposure;
    float time;
} pc;

float Hash(vec2 p)
{
    vec3 p3  = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

vec3 PBRNeutralTonemap(vec3 color)
{
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;
    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;
    
    float peak = max(color.r, max(color.g, color.b));
    
    if (peak < startCompression) { return color + offset; }
    
    float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / peak;
    
    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return mix(color, newPeak * vec3(1.0), g) + offset;
}

void main() 
{
    vec3 color = texture(sceneTexture, inUV).rgb;
    vec3 bloom = texture(bloomTexture, inUV).rgb;
    vec3 flare = texture(flareTexture, inUV).rgb;

    color += bloom * 0.05;
    color += flare * 0.2;
    color = color * pc.exposure;

    color = PBRNeutralTonemap(color);

    vec3 lutCoord = color * (31.0 / 32.0) + (0.5 / 32.0);
    color = texture(colorGradingLUT, lutCoord).rgb;

    vec2 d = abs(inUV - 0.5) * 1.5;
    float dist = length(d);
    float vignette = 1.0 - smoothstep(0.8, 1.5, dist);
    color *= vignette;

    color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));

    float t = mod(pc.time, 1000.0);
    float noiseR = Hash(gl_FragCoord.xy + vec2(t, t * 1.61803399));
    float noiseG = Hash(gl_FragCoord.xy + vec2(t * 1.61803399 + 31.7, t + 11.3));
    float noiseB = Hash(gl_FragCoord.xy + vec2(t * 2.61803399 + 74.3, t * 0.61803399 + 45.7));

    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color += (vec3(noiseR, noiseG, noiseB) - 0.5) * mix(0.008, 0.02, lum);

    outColor = vec4(color, 1.0);
}