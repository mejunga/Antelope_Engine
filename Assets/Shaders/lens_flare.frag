#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D brightTexture;

layout(push_constant) uniform PushConstants
{
    int ghosts;
    float ghostDispersal;
    float haloWidth;
    float distortion;
} pc;

vec3 textureDistorted(sampler2D tex, vec2 uv, vec2 direction, vec3 distortion)
{
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return vec3(0.0);
    
    return vec3(
        texture(tex, uv + direction * distortion.r).r,
        texture(tex, uv + direction * distortion.g).g,
        texture(tex, uv + direction * distortion.b).b
    );
}

void main()
{
    vec2 texcoord = -inUV + vec2(1.0); 
    vec2 texelSize = 1.0 / vec2(textureSize(brightTexture, 0));
    
    vec2 ghostVec = (vec2(0.5) - texcoord) * pc.ghostDispersal;
    float ghostLen = length(ghostVec);
    
    vec2 direction = ghostLen > 0.0001 ? ghostVec / ghostLen : vec2(0.0);
    
    vec3 result = vec3(0.0);
    vec3 distortion = vec3(-texelSize.x * pc.distortion, 0.0, texelSize.x * pc.distortion);
    
    for (int i = 0; i < pc.ghosts; ++i)
    {
        vec2 offset = texcoord + ghostVec * float(i); 
        
        float distToCenter = length(vec2(0.5) - offset);
        float weight = max(0.0, 1.0 - (distToCenter * 1.5)); 
        
        weight *= pow(0.8, float(i)); 
        
        result += textureDistorted(brightTexture, offset, direction, distortion) * weight;
    }
    
    vec2 haloVec = direction * pc.haloWidth;
    vec2 haloPos = texcoord + haloVec;
    
    float haloDist = length(vec2(0.5) - haloPos);
    float haloWeight = max(0.0, 1.0 - (haloDist * 1.5)); 
    
    float haloFade = smoothstep(0.0, 0.1, ghostLen);
    
    result += textureDistorted(brightTexture, haloPos, direction, distortion) * haloWeight * haloFade;
    
    outColor = vec4(result * 0.5, 1.0);
}
