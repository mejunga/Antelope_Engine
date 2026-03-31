#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;
layout(location = 3) in flat uint fragMatID;

layout(location = 0) out vec4 outColor;

layout(binding = 7) uniform sampler2D globalTextures[];

void main() 
{
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(vec3(1.0, 1.0, 0.5));
    float diff = max(dot(norm, lightDir), 0.2); 
    vec4 texColor = texture(globalTextures[nonuniformEXT(fragMatID)], fragUV);

    vec3 linearColor = fragColor * texColor.rgb * diff;
    vec3 gammaCorrected = pow(linearColor, vec3(1.0 / 2.2));

    outColor = vec4(gammaCorrected, texColor.a);
}