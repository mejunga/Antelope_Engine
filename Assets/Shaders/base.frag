#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in flat uint fragMatID;
layout(location = 4) in vec3 fragWorldPos;
layout(location = 5) in vec3 fragTangent;

layout(location = 0) out vec4 outColor;

struct PointLight
{
    vec4 positionAndRadius;
    vec4 colorAndIntensity;
};

struct SpotLight
{
    vec4 positionAndRadius;
    vec4 directionAndCutOff;
    vec4 colorAndIntensity;
    vec4 outerCutOffAndPad;
};

layout(binding = 0) uniform GlobalUBO
{
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    vec4 sunDirection;
    vec4 sunColor;
    mat4 lightSpaceMatrix;
    vec4 skyColorDayAndStar; 
    vec4 horizonColorDay;    
    vec4 skyColorNight;      
    vec4 horizonColorNight;  
    vec4 groundColor;        
    uint pointLightCount;
    uint spotLightCount;
    float time;
    float ambientEnabled;
    PointLight pointLights[32];
    SpotLight spotLights[32];
} ubo;

struct MaterialData
{
    vec4 AlbedoFactor;
    vec4 MetRoughFactors;
    uint AlbedoTex;
    uint NormalTex;
    uint MetRoughAOTex;
    uint EmissiveTex;
};

layout(binding = 7) uniform sampler2D globalTextures[];

layout(std430, binding = 8) readonly buffer MaterialBuffer
{
    MaterialData materials[];
};

layout(binding = 10) uniform sampler2DShadow shadowMap;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / max(denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float denom = NdotV * (1.0 - k) + k;
    return NdotV / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 CalcRadiance(vec3 L, vec3 V, vec3 N, vec3 radiance, vec3 albedo, float metallic, float roughness, vec3 F0)
{
    vec3 H = normalize(V + L);
    float NDF = DistributionGGX(N, H, roughness);   
    float G = GeometrySmith(N, V, L, roughness);      
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);       

    float actualNdotL = max(dot(N, L), 0.0);

    vec3 numerator = NDF * G * F; 
    float denominator = 4.0 * max(dot(N, V), 0.0) * actualNdotL + 0.0001;
    vec3 specular = numerator / denominator;
    
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;   

    float wrap = 0.3;
    float diffuseNdotL = max((dot(N, L) + wrap) / (1.0 + wrap), 0.0);

    vec3 diffuseTerm = (kD * albedo / PI) * diffuseNdotL;
    vec3 specularTerm = specular * actualNdotL; 

    return (diffuseTerm + specularTerm) * radiance;
}

vec3 ACESFilm(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

float InterleavedGradientNoise(vec2 position_screen)
{
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(position_screen, magic.xy)));
}

float ShadowPCF(vec3 worldPos, vec3 normal)
{
    vec3 L = normalize(ubo.sunDirection.xyz);
    float cosTheta = clamp(dot(normal, L), 0.0, 1.0);

    float normalBias = mix(0.04, 0.01, cosTheta);
    vec3 biasedPos = worldPos + normal * normalBias;

    vec4 sc = ubo.lightSpaceMatrix * vec4(biasedPos, 1.0);
    sc.xyz /= sc.w;
    sc.xy = sc.xy * 0.5 + 0.5;

    if (sc.z > 1.0 || sc.z < 0.0) { return 1.0; }

    vec2 poissonDisk[16] = vec2[](
        vec2( -0.94201624, -0.39906216 ), vec2( 0.94558609, -0.76890725 ),
        vec2( -0.094184101, -0.92938870 ), vec2( 0.34495938, 0.29387760 ),
        vec2( -0.91588581, 0.45771432 ),  vec2( -0.81544232, -0.87912464 ),
        vec2( -0.38277543, 0.27676845 ),  vec2( 0.97484398, 0.75648379 ),
        vec2( 0.44323325, -0.97511554 ),  vec2( 0.53742981, -0.47373420 ),
        vec2( -0.26496911, -0.41893023 ), vec2( 0.79197514, 0.19090188 ),
        vec2( -0.24188840, 0.99706507 ),  vec2( -0.81409955, 0.91437590 ),
        vec2( 0.19984126, 0.78641367 ),   vec2( 0.14383161, -0.14100467 )
    );

    float noise = InterleavedGradientNoise(gl_FragCoord.xy);
    float angle = noise * 6.2831853;
    float s = sin(angle);
    float c = cos(angle);
    mat2 rot = mat2(c, -s, s, c);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(4096.0); 
    float spread = 2.5; 

    for (int i = 0; i < 16; i++)
    {
        vec2 offset = rot * poissonDisk[i];
        
        shadow += texture(shadowMap, vec3(sc.xy + offset * texelSize * spread, sc.z));
    }

    return shadow / 16.0;
}

void main() 
{
    MaterialData mat = materials[fragMatID];

    vec4 albedoData = mat.AlbedoFactor;

    if (mat.AlbedoTex != 0xFFFFFFFF)
    {
        vec4 texColor = texture(globalTextures[nonuniformEXT(mat.AlbedoTex)], fragUV);
        albedoData.rgb *= texColor.rgb;
        albedoData.a *= texColor.a; 
    }

    vec3 albedo = albedoData.rgb;

    float metallic = mat.MetRoughFactors.x;
    float roughness = mat.MetRoughFactors.y;
    float ao = mat.MetRoughFactors.z;

    if (mat.MetRoughAOTex != 0xFFFFFFFF)
    {
        vec4 mraoSample = texture(globalTextures[nonuniformEXT(mat.MetRoughAOTex)], fragUV);
        ao = mraoSample.r;
        roughness *= mraoSample.g;
        metallic *= mraoSample.b;
    }

    vec3 N = normalize(fragNormal);
    vec3 geoNormal = N;

    if (mat.NormalTex != 0xFFFFFFFF)
    {
        vec3 T = normalize(fragTangent);
        T = normalize(T - dot(T, N) * N);
        vec3 B = cross(N, T);
        mat3 TBN = mat3(T, B, N);
        vec3 normalSample = texture(globalTextures[nonuniformEXT(mat.NormalTex)], fragUV).rgb;
        N = normalize(TBN * (normalSample * 2.0 - 1.0));
    }
    
    vec3 V = normalize(ubo.cameraPos.xyz - fragWorldPos);
    
    if (albedoData.a < 0.5) { discard; }

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    if (ubo.sunDirection.w > 0.5)
    {
        vec3 L = normalize(ubo.sunDirection.xyz);
        vec3 radiance = ubo.sunColor.rgb * ubo.sunColor.a;
        float shadow = ShadowPCF(fragWorldPos, geoNormal);
        Lo += CalcRadiance(L, V, N, radiance, albedo, metallic, roughness, F0) * shadow;
    }

    for(int i = 0; i < ubo.pointLightCount; i++)
    {
        vec3 lightPos = ubo.pointLights[i].positionAndRadius.xyz;
        float radius = ubo.pointLights[i].positionAndRadius.w;
        
        vec3 L = normalize(lightPos - fragWorldPos);
        float distance = length(lightPos - fragWorldPos);
        
        float attenuation = pow(clamp(1.0 - pow(distance / radius, 4.0), 0.0, 1.0), 2.0) / (distance * distance + 1.0);
        vec3 radiance = ubo.pointLights[i].colorAndIntensity.rgb * ubo.pointLights[i].colorAndIntensity.a * attenuation;
        
        Lo += CalcRadiance(L, V, N, radiance, albedo, metallic, roughness, F0);
    }

    for(int i = 0; i < ubo.spotLightCount; i++)
    {
        vec3 lightPos = ubo.spotLights[i].positionAndRadius.xyz;
        float radius = ubo.spotLights[i].positionAndRadius.w;
        vec3 spotDir = normalize(ubo.spotLights[i].directionAndCutOff.xyz);
        
        vec3 L = normalize(lightPos - fragWorldPos);
        float distance = length(lightPos - fragWorldPos);
        
        float attenuation = pow(clamp(1.0 - pow(distance / radius, 4.0), 0.0, 1.0), 2.0) / (distance * distance + 1.0);
        
        float theta = dot(L, -spotDir); 
        float epsilon = ubo.spotLights[i].directionAndCutOff.w - ubo.spotLights[i].outerCutOffAndPad.x;
        float intensity = clamp((theta - ubo.spotLights[i].outerCutOffAndPad.x) / epsilon, 0.0, 1.0);
        
        vec3 radiance = ubo.spotLights[i].colorAndIntensity.rgb * ubo.spotLights[i].colorAndIntensity.a * attenuation * intensity;
        
        Lo += CalcRadiance(L, V, N, radiance, albedo, metallic, roughness, F0);
    }

    vec3 nightSky = vec3(0.04, 0.05, 0.10);
    vec3 nightGround = vec3(0.02, 0.015, 0.012);

    vec3 skyBase = vec3(0.30, 0.35, 0.50);
    vec3 groundBase = vec3(0.14, 0.11, 0.09);

    vec3 ambient = vec3(0.0);

    if (ubo.ambientEnabled < 0.5)
    {
        vec3 skyColor = vec3(0.30, 0.35, 0.50);
        vec3 groundColor = vec3(0.14, 0.11, 0.09);
        float upFactor = N.y * 0.5 + 0.5;
        ambient = mix(groundColor, skyColor, upFactor) * albedo * ao;
        ambient += albedo * 0.07;
    }
    else
    {
        float sunElevation = (ubo.sunDirection.w > 0.5) ? smoothstep(-0.2, 0.4, ubo.sunDirection.y) : 0.0;
        
        vec3 skyColor = mix(nightSky, skyBase, sunElevation);
        vec3 groundColor = mix(nightGround, groundBase, sunElevation);
        
        float upFactor = N.y * 0.5 + 0.5;
        ambient = mix(groundColor, skyColor, upFactor) * albedo * ao;
        ambient += albedo * 0.07;
    }
    
    vec3 color = ambient + Lo;

    vec3 emissive = vec3(0.0);

    if (mat.EmissiveTex != 0xFFFFFFFF)
    {
        emissive = texture(globalTextures[nonuniformEXT(mat.EmissiveTex)], fragUV).rgb * mat.MetRoughFactors.w;
    }

    color += emissive;

    color = ACESFilm(color);
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, albedoData.a);
}