#version 450

layout(location = 0) in vec3 viewDir;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform GlobalUBO
{
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    vec4 sunDirection;
    vec4 sunColor;
    vec4 skyColorDayAndStar;
    vec4 horizonColorDay;
    vec4 skyColorNight;
    vec4 horizonColorNight;
    vec4 groundColor;
    uint pointLightCount;
    uint spotLightCount;
    float time;
    float ambientEnabled;
} ubo;

float hash31(vec3 p3)
{
    p3 = fract(p3 * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main()
{
    vec3 dir = normalize(viewDir);

    float sunElevation = smoothstep(-0.2, 0.3, ubo.sunDirection.y);
    if (ubo.sunDirection.w < 0.5) { sunElevation = 0.0; }

    if (ubo.ambientEnabled < 0.5)
    {
        float skyGradient = smoothstep(0.0, 0.6, dir.y);
        vec3 sky = mix(vec3(0.75, 0.80, 0.85), vec3(0.25, 0.40, 0.60), skyGradient);
        float groundGradient = smoothstep(-0.01, 0.01, dir.y);
        vec3 finalColor = mix(vec3(0.22, 0.22, 0.22), sky, groundGradient);

        if (ubo.sunDirection.w > 0.5 && sunElevation > 0.0)
        {
            float sunAngle = acos(clamp(dot(dir, normalize(ubo.sunDirection.xyz)), -1.0, 1.0));
            float sunDisk = smoothstep(0.025, 0.008, sunAngle);
            float corona = exp(-sunAngle * sunAngle * 60.0) * 0.3;
            float sunGlow = exp(-sunAngle * sunAngle * 12.0) * 0.12;
            float horizonMask = smoothstep(-0.08, 0.12, dir.y);
            finalColor += ubo.sunColor.xyz * (sunDisk * 1.5 + corona + sunGlow) * sunElevation * horizonMask;
        }

        outColor = vec4(finalColor, 1.0);
        return;
    }

    float skyGradient = smoothstep(0.0, 0.6, dir.y);

    vec3 currentZenith = mix(ubo.skyColorNight.xyz, ubo.skyColorDayAndStar.xyz, sunElevation);
    vec3 currentHorizon = mix(ubo.horizonColorNight.xyz, ubo.horizonColorDay.xyz, sunElevation);
    vec3 sky = mix(currentHorizon, currentZenith, skyGradient);

    if (dir.y > 0.0)
    {
        vec3 cellID = floor(dir * 280.0);
        vec3 cellFract = fract(dir * 280.0) - 0.5;
        float cellNoise = hash31(cellID);

        if (cellNoise > 0.987)
        {
            float brightness = (cellNoise - 0.987) / 0.013;
            float starDist = length(cellFract);
            float circle = smoothstep(0.35, 0.0, starDist);

            float phase = hash31(cellID * 1.7) * 6.283;
            float twinkle = 0.65 + 0.35 * sin(ubo.time * (1.5 + hash31(cellID * 2.3) * 3.0) + phase);

            vec3 starColor = mix(vec3(0.75, 0.85, 1.0), vec3(1.0, 0.92, 0.78), hash31(cellID * 0.9));

            float spike = 0.0;
            if (cellNoise > 0.996)
            {
                spike = max(
                    smoothstep(0.45, 0.0, abs(cellFract.x)) * smoothstep(0.07, 0.0, abs(cellFract.y)),
                    smoothstep(0.45, 0.0, abs(cellFract.y)) * smoothstep(0.07, 0.0, abs(cellFract.x))
                ) * 0.6 * brightness;
            }

            float nightFactor = 1.0 - smoothstep(-0.1, 0.25, ubo.sunDirection.y);
            sky += starColor * (circle * brightness + spike) * twinkle * nightFactor * ubo.skyColorDayAndStar.w;
        }
    }

    float groundGradient = smoothstep(-0.01, 0.01, dir.y);
    vec3 finalColor = mix(ubo.groundColor.xyz, sky, groundGradient);

    if (ubo.sunDirection.w > 0.5 && sunElevation > 0.0)
    {
        float sunDot = clamp(dot(dir, normalize(ubo.sunDirection.xyz)), -1.0, 1.0);
        float sunAngle = acos(sunDot);

        float sunDisk = smoothstep(0.025, 0.008, sunAngle);
        float corona = exp(-sunAngle * sunAngle * 60.0) * 0.3;
        float sunGlow = exp(-sunAngle * sunAngle * 12.0) * 0.12;

        float horizonMask = smoothstep(-0.08, 0.12, dir.y);
        finalColor += ubo.sunColor.xyz * (sunDisk * 1.5 + corona + sunGlow) * sunElevation * horizonMask;
    }

    outColor = vec4(finalColor, 1.0);
}
