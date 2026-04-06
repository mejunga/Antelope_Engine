#version 450

layout(location = 0) in vec3 viewDir;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 dir = normalize(viewDir);

    vec3 zenithColor  = vec3(0.25, 0.40, 0.60);
    vec3 horizonColor = vec3(0.75, 0.80, 0.85);
    vec3 groundColor  = vec3(0.22, 0.22, 0.22);

    float skyGradient = smoothstep(0.0, 0.6, dir.y);
    vec3 sky = mix(horizonColor, zenithColor, skyGradient);

    float groundGradient = smoothstep(-0.05, 0.0, dir.y);
    vec3 finalColor = mix(groundColor, sky, groundGradient);

    outColor = vec4(finalColor, 1.0);
}