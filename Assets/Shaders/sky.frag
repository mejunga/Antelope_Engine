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
    vec4 moonDirection;
    vec4 moonColor;
    mat4 lightSpaceMatrices[2];
    vec4 cascadeSplits;
    vec4 skyColorDayAndStar;
    vec4 horizonColorDay;
    vec4 skyColorNight;
    vec4 horizonColorNight;
    vec4 groundColor;
    uint pointLightCount;
    uint spotLightCount;
    float time;
    float ambientEnabled;
    uint shadowCaster;
    float _pad1;
    float _pad2;
    float _pad3;
} ubo;

float hash31(vec3 p3)
{
    p3 = fract(p3 * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main()
{
    const float PI = 3.14159265359;

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
            float sunDisk = smoothstep(0.028, 0.026, sunAngle);
            float corona = exp(-sunAngle * sunAngle * 60.0) * 0.3;
            float sunGlow = exp(-sunAngle * sunAngle * 12.0) * 0.12;
            float horizonMask = smoothstep(-0.08, 0.12, dir.y);
            finalColor += ubo.sunColor.xyz * (sunDisk * 1.5 + corona + sunGlow) * sunElevation * horizonMask;
        }

        if (ubo.moonDirection.w > 0.5 && ubo.moonColor.a > 0.001)
        {
            float moonElevation = smoothstep(-0.1, 0.2, ubo.moonDirection.y);
            float moonDot = clamp(dot(dir, normalize(ubo.moonDirection.xyz)), -1.0, 1.0);
            float moonAngle = acos(moonDot);
            float moonDisk = smoothstep(0.028, 0.026, moonAngle);
            float moonGlow = exp(-moonAngle * moonAngle * 80.0) * 0.05;
            float nightFactor = 1.0 - smoothstep(0.0, 0.25, ubo.sunDirection.y);
            finalColor += vec3(0.88, 0.95, 1.0) * (moonDisk * 0.8 + moonGlow) * moonElevation * nightFactor;
        }

        finalColor = pow(finalColor, vec3(2.2));
        outColor = vec4(finalColor, 1.0);
        return;
    }

    const float kSunRadius = 0.028;
    const float kMoonRadius = 0.028;
    float sunAngle = (ubo.sunDirection.w  > 0.5) ? acos(clamp(dot(dir, normalize(ubo.sunDirection.xyz)),  -1.0, 1.0)) : 99.0;
    float moonAngle = (ubo.moonDirection.w > 0.5) ? acos(clamp(dot(dir, normalize(ubo.moonDirection.xyz)), -1.0, 1.0)) : 99.0;
    
    float sunDisk = smoothstep(kSunRadius, kSunRadius - 0.002, sunAngle);
    float moonDisk = smoothstep(kMoonRadius, kMoonRadius - 0.002, moonAngle);

    float skyGradient = smoothstep(0.0, 0.6, dir.y);
    vec3 currentZenith = mix(ubo.skyColorNight.xyz, ubo.skyColorDayAndStar.xyz, sunElevation);
    vec3 currentHorizon = mix(ubo.horizonColorNight.xyz, ubo.horizonColorDay.xyz, sunElevation);

    float sunAtHorizon = (1.0 - sunElevation) * smoothstep(-0.1, 0.3, ubo.sunDirection.y);

    if (ubo.sunDirection.w > 0.5 && sunAtHorizon > 0.001)
    {
        vec3 sunDirFlat  = vec3(ubo.sunDirection.x, 0.0, ubo.sunDirection.z);
        float sunFlatLen = length(sunDirFlat);
        vec3 fragDirFlat  = vec3(dir.x, 0.0, dir.z);
        float fragFlatLen = length(fragDirFlat);

        if (sunFlatLen > 0.01 && fragFlatLen > 0.01)
        {
            float sunAlignH = dot(fragDirFlat / fragFlatLen, sunDirFlat / sunFlatLen);
            
            float horizonFade = 1.0 - smoothstep(0.0, 1.5, dir.y);
            float warmness = smoothstep(-0.4, 1.0, sunAlignH) * sunAtHorizon * horizonFade;
            float hotness = smoothstep( 0.7, 1.0, sunAlignH) * sunAtHorizon * horizonFade;
            float coolness = smoothstep( 0.3,-1.0, sunAlignH) * sunAtHorizon * horizonFade;
            
            currentHorizon = mix(currentHorizon, vec3(1.0, 0.25, 0.02), warmness * 1.5);
            currentHorizon = mix(currentHorizon, vec3(1.0, 0.45, 0.05), hotness * 1.2);
            currentHorizon = mix(currentHorizon, vec3(0.35, 0.50, 0.65), coolness * 0.6);
            currentZenith = mix(currentZenith, vec3(0.85, 0.4, 0.2), warmness * 0.5);
        }
    }

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
            sky += starColor * (circle * brightness + spike) * twinkle * nightFactor * ubo.skyColorDayAndStar.w * (1.0 - sunDisk) * (1.0 - moonDisk);
        }
    }

    float moonSunSep = (ubo.sunDirection.w > 0.5 && ubo.moonDirection.w > 0.5)
                        ? acos(clamp(dot(normalize(ubo.sunDirection.xyz), normalize(ubo.moonDirection.xyz)), -1.0, 1.0))
                        : 99.0;
    
    float eclipseFactor = smoothstep(kSunRadius + kMoonRadius, 0.0, moonSunSep);
    
    sky *= (1.0 - eclipseFactor * 0.88);

    float groundGradient = smoothstep(-0.01, 0.01, dir.y);
    vec3 finalColor = mix(ubo.groundColor.xyz, sky, groundGradient);

    if (ubo.sunDirection.w > 0.5 && sunElevation > 0.0)
    {
        float horizonMask = smoothstep(-0.08, 0.12, dir.y);

        if (eclipseFactor > 0.01)
        {
            float moonCoversSun = smoothstep(kMoonRadius, kMoonRadius - 0.002, moonAngle);
            float blockedSunDisk = sunDisk * (1.0 - moonCoversSun * eclipseFactor);
            float coronaDist = moonAngle - kMoonRadius;
            float coronaRing = smoothstep(0.04, 0.0, coronaDist) * smoothstep(-0.001, 0.003, coronaDist);
            float coronaInSun = smoothstep(kSunRadius * 3.5, 0.0, sunAngle);
            float corona = coronaRing * coronaInSun * eclipseFactor;
            vec3  coronaColor = mix(vec3(1.0, 0.55, 0.08), vec3(1.0, 0.07, 0.0), smoothstep(0.035, 0.0, coronaDist));
            float sunGlow = exp(-sunAngle * sunAngle * 12.0) * 0.12 * (1.0 - eclipseFactor * 0.85);
            finalColor += ubo.sunColor.xyz * (blockedSunDisk * 1.5 + sunGlow) * sunElevation * horizonMask;
            finalColor += coronaColor * corona * 5.0 * sunElevation;
        }
        else
        {
            float corona = exp(-sunAngle * sunAngle * 60.0) * 0.3;
            float sunGlow = exp(-sunAngle * sunAngle * 12.0) * 0.12;
            finalColor += ubo.sunColor.xyz * (sunDisk * 1.5 + corona + sunGlow) * sunElevation * horizonMask;
        }
    }

    if (ubo.moonDirection.w > 0.5)
    {
        float moonElevFactor = smoothstep(-0.05, 0.1, ubo.moonDirection.y);
        float nightFade = 1.0 - smoothstep(0.0, 0.35, ubo.sunDirection.y);
        if (eclipseFactor > 0.01)
        {
            float horizonMask = smoothstep(-0.08, 0.12, dir.y);
            finalColor = mix(finalColor, vec3(0.0), moonDisk * eclipseFactor * horizonMask);
        }
        else
        {
            vec3 moonDirN = normalize(ubo.moonDirection.xyz);
            vec3 moonRight = normalize(cross(moonDirN, vec3(0.0, 1.0, 0.0)));
            vec3 moonUp = cross(moonRight, moonDirN);
            vec2 moonLocal = vec2(dot(dir, moonRight), dot(dir, moonUp));
            
            vec2 uv = moonLocal / kMoonRadius;
            float z = sqrt(max(1.0 - dot(uv, uv), 0.0));
            vec3 moonNormal = vec3(uv.x, uv.y, z);
            
            vec3 sunDirN = normalize(ubo.sunDirection.xyz);
            
            vec3 sunLocal = normalize(vec3(dot(sunDirN, moonRight), dot(sunDirN, moonUp), dot(sunDirN, -moonDirN)));
            float NdotL = dot(moonNormal, sunLocal);
            
            float litFactor = smoothstep(-0.05, 0.05, NdotL);
            vec2 moonUV = moonLocal * 300.0;
            float craterNoise = sin(moonUV.x * 0.8) * cos(moonUV.y * 0.8) + sin(moonUV.x * 1.5 + moonUV.y * 1.5) * 0.5;
            float spotMask = smoothstep(0.1, 0.9, sin(moonUV.x * 0.3) * cos(moonUV.y * 0.25));
            craterNoise = smoothstep(-0.5, 1.0, craterNoise) * spotMask;
            vec3 darkColor = vec3(0.005, 0.005, 0.01); 
            
            float bloomMpx = mix(0.4, 1.0, nightFade);
            vec3 litColor = mix(vec3(1.0, 1.0, 0.98), vec3(0.6, 0.6, 0.55), craterNoise * 0.6) * bloomMpx;
            vec3 finalMoonSurf = mix(darkColor * nightFade, litColor, litFactor);
            
            float litFraction = clamp((1.0 - cos(moonSunSep)) * 0.5, 0.0, 1.0); 
            float moonGlow = exp(-moonAngle * moonAngle * 120.0) * 0.12 * moonElevFactor * nightFade * litFraction;
            
            float dayAlpha = mix(0.35, 1.0, nightFade);
            
            finalColor += finalMoonSurf * moonDisk * moonElevFactor * dayAlpha;
            finalColor += vec3(0.7, 0.8, 1.0) * moonGlow;
        }
    }

    finalColor = pow(finalColor, vec3(2.2));
    outColor = vec4(finalColor, 1.0);
}