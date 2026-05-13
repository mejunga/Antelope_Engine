#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
} ubo;

struct VertexPosition { vec3 pos; };
struct Face { uvec4 data; };
struct ObjectData
{
    mat4  model;
    mat4  normalMatrix;
    uint posOffset;
    uint colorOffset;
    uint normalOffset;
    uint uvOffset;
    uint faceOffset;
    uint materialIndex;
    uint tangentOffset;
    uint jointOffset;
    uint boneOffset;
    uint isAnimated;
    uint entityID;
    uint pad1;
};
struct VertexJointData { ivec4 boneIDs; vec4 weights; };

layout(std430, binding = 1) readonly buffer PosBuffer { VertexPosition vertices[]; } posBuf;
layout(std430, binding = 4) readonly buffer FaceBuffer { Face faces[]; } faceBuf;
layout(std430, binding = 6) readonly buffer ObjectBuffer { ObjectData objects[]; } objBuf;
layout(std430, binding = 12) readonly buffer JointBuffer { VertexJointData joints[]; } jointBuf;
layout(std430, binding = 13) readonly buffer BoneMatrixBuffer { mat4 bones[]; } boneBuf;

void main()
{
    ObjectData obj = objBuf.objects[gl_InstanceIndex];

    uint faceIndex = obj.faceOffset + (gl_VertexIndex / 3);
    uint vertexOffsetInFace = gl_VertexIndex % 3;

    Face f = faceBuf.faces[faceIndex];

    uint vIndex;
    if (vertexOffsetInFace == 0) { vIndex = f.data.x; }
    else if (vertexOffsetInFace == 1) { vIndex = f.data.y; }
    else { vIndex = f.data.z; }

    vec3 position = posBuf.vertices[obj.posOffset + vIndex].pos;

    if (obj.isAnimated == 1)
    {
        VertexJointData joint = jointBuf.joints[obj.jointOffset + vIndex];
        mat4 boneTransform = mat4(0.0);
        bool hasBones = false;
        for (int i = 0; i < 4; ++i)
        {
            if (joint.boneIDs[i] >= 0)
            {
                boneTransform += boneBuf.bones[obj.boneOffset + joint.boneIDs[i]] * joint.weights[i];
                hasBones = true;
            }
        }
        if (hasBones)
        {
            vec4 skinnedPos = boneTransform * vec4(position, 1.0);
            position = skinnedPos.xyz / skinnedPos.w;
        }
    }

    gl_Position = ubo.proj * ubo.view * obj.model * vec4(position, 1.0);
}