#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
} ubo;

struct VertexPosition { vec4 pos; };
struct Face { uvec4 data; };

struct ObjectData
{
    mat4 model;
    mat4 normalMatrix;
    uint posOffset;
    uint colorOffset;
    uint normalOffset;
    uint uvOffset;
    uint faceOffset;
    uint materialIndex;
    uint padding1;
    uint padding2;
};

layout(std430, binding = 1) readonly buffer PosBuffer { VertexPosition vertices[]; } posBuf;
layout(std430, binding = 4) readonly buffer FaceBuffer { Face faces[]; } faceBuf;
layout(std430, binding = 6) readonly buffer ObjectBuffer { ObjectData objects[]; } objBuf;

void main()
{
    ObjectData obj = objBuf.objects[gl_InstanceIndex];

    uint faceIndex = obj.faceOffset + (gl_VertexIndex / 3);
    uint vertexOffsetInFace = gl_VertexIndex % 3;

    Face f = faceBuf.faces[faceIndex];

    uint vIndex;
    if (vertexOffsetInFace == 0) vIndex = f.data.x;
    else if (vertexOffsetInFace == 1) vIndex = f.data.y;
    else vIndex = f.data.z;

    vec3 position = posBuf.vertices[obj.posOffset + vIndex].pos.xyz;

    gl_Position = ubo.proj * ubo.view * obj.model * vec4(position, 1.0);
}