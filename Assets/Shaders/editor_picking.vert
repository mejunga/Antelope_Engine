#version 450
#extension GL_EXT_nonuniform_qualifier : require

struct VertexPosition { vec3 pos; };
layout(std430, binding = 1) readonly buffer PosBuffer { VertexPosition positions[]; } posBuf;

struct Face { uint v0, v1, v2, normalIndex; };
layout(std430, binding = 4) readonly buffer FaceBuffer { Face faces[]; } faceBuf;

struct ObjectData
{
    mat4 model;
    mat4 normalMatrix;
    uint posOffset; uint colorOffset; uint normalOffset; uint uvOffset;
    uint faceOffset; uint materialIndex;
    uint tangentOffset;
    uint entityID;
};

layout(push_constant) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
} ubo;

layout(std430, binding = 6) readonly buffer ObjectBuffer { ObjectData objects[]; } objectData;

layout(location = 0) out flat uint fragEntityID;

void main()
{
    ObjectData obj = objectData.objects[gl_InstanceIndex];
    uint faceIndex = obj.faceOffset + (gl_VertexIndex / 3);
    uint vertSlot  = gl_VertexIndex % 3;
    Face face = faceBuf.faces[faceIndex];
    uint posIndex = (vertSlot == 0) ? face.v0 : (vertSlot == 1) ? face.v1 : face.v2;

    vec3 pos = posBuf.positions[obj.posOffset + posIndex].pos;
    gl_Position = ubo.proj * ubo.view * obj.model * vec4(pos, 1.0);
    fragEntityID = obj.entityID;
}