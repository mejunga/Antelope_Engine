#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

void main() {
    float ambientStrength = 0.15;
    vec3 ambient = ambientStrength * vec3(1.0, 1.0, 1.0);
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(vec3(2.0, -2.0, 1.0)); 
    
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);
    vec3 result = (ambient + diffuse) * fragColor;
    
    outColor = vec4(result, 1.0);
}