#version 450

layout(binding = 0) uniform UniformBufferObject 
{
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(std140, binding = 1) readonly buffer PosBuffer    { vec4 positions[]; };
layout(std140, binding = 2) readonly buffer ColorBuffer  { vec4 colors[]; };
layout(std140, binding = 3) readonly buffer NormalBuffer { vec4 normals[]; };

struct Face 
{
    uint v0, v1, v2;
    uint normalIndex;
};

layout(std140, binding = 4) readonly buffer FaceBuffer   { Face faces[]; };

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragPos;

void main() 
{
    uint faceID = gl_VertexIndex / 3;
    uint vertexInFace = gl_VertexIndex % 3;

    Face myFace = faces[faceID];

    uint posIndex;
    if (vertexInFace == 0) posIndex = myFace.v0;
    else if (vertexInFace == 1) posIndex = myFace.v1;
    else posIndex = myFace.v2;

    vec3 inPosition = positions[posIndex].xyz;
    vec3 inColor = colors[posIndex].xyz;
    vec3 inNormal = normals[myFace.normalIndex].xyz;

    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;
    
    fragColor = inColor;
    fragPos = worldPos.xyz;
    fragNormal = mat3(ubo.model) * inNormal; 
}