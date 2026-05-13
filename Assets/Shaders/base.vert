#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
} ubo;

struct VertexPosition { vec4 pos; };
struct VertexColor { vec4 color; };
struct VertexNormal { vec4 normal; };
struct VertexUV { vec2 uv; };
struct VertexTangent { vec4 tangent; };
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
    uint tangentOffset;
    uint jointOffset;
    uint boneOffset;
    uint isAnimated;
    uint entityID;
    uint pad1;
};
struct VertexJointData { ivec4 boneIDs; vec4 weights; };

layout(std430, binding = 1) readonly buffer PosBuffer { VertexPosition vertices[]; } posBuf;
layout(std430, binding = 2) readonly buffer ColBuffer { VertexColor colors[]; } colBuf;
layout(std430, binding = 3) readonly buffer NormBuffer { VertexNormal normals[]; } normBuf;
layout(std430, binding = 4) readonly buffer FaceBuffer { Face faces[]; } faceBuf;
layout(std430, binding = 5) readonly buffer UvBuffer { VertexUV uvs[]; } uvBuf;
layout(std430, binding = 6) readonly buffer ObjectBuffer { ObjectData objects[]; } objBuf;
layout(std430, binding = 9) readonly buffer TangentBuffer { VertexTangent tangents[]; } tangentBuf;
layout(std430, binding = 12) readonly buffer JointBuffer { VertexJointData joints[]; } jointBuf;
layout(std430, binding = 13) readonly buffer BoneMatrixBuffer { mat4 bones[]; } boneBuf;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out flat uint fragMatID;
layout(location = 4) out vec3 fragWorldPos;
layout(location = 5) out vec3 fragTangent;

void main() 
{
    ObjectData obj = objBuf.objects[gl_InstanceIndex];
    uint faceIndex = obj.faceOffset + (gl_VertexIndex / 3);
    uint vertexOffsetInFace = gl_VertexIndex % 3;
    Face f = faceBuf.faces[faceIndex];
    
    uint vIndex;
    
    if(vertexOffsetInFace == 0) { vIndex = f.data.x; }
    else if(vertexOffsetInFace == 1) { vIndex = f.data.y; }
    else { vIndex = f.data.z; }

    vec3 position = posBuf.vertices[obj.posOffset + vIndex].pos.xyz;
    vec3 color = colBuf.colors[obj.colorOffset + vIndex].color.xyz;
    vec3 normal = normBuf.normals[obj.normalOffset + vIndex].normal.xyz;
    vec2 uv = uvBuf.uvs[obj.uvOffset + vIndex].uv;
    vec3 tangent = tangentBuf.tangents[obj.tangentOffset + vIndex].tangent.xyz;

    if (obj.isAnimated == 1)
    {
        VertexJointData joint = jointBuf.joints[obj.jointOffset + vIndex];
        
        mat4 boneTransform = mat4(0.0);
        bool hasBones = false;
        for(int i = 0; i < 4; ++i)
        {
            if(joint.boneIDs[i] >= 0)
            {
                boneTransform += boneBuf.bones[obj.boneOffset + joint.boneIDs[i]] * joint.weights[i];
                hasBones = true;
            }
        }
        
        if(hasBones)
        {
            vec4 localPosition = boneTransform * vec4(position, 1.0);
            position = localPosition.xyz / localPosition.w;
            
            vec4 localNormal = boneTransform * vec4(normal, 0.0);
            normal = normalize(localNormal.xyz);
        }
    }

    gl_Position = ubo.proj * ubo.view * obj.model * vec4(position, 1.0);
    
    fragColor = color;
    fragNormal = mat3(obj.normalMatrix) * normal;
    fragUV = uv;
    fragTangent = mat3(obj.normalMatrix) * tangent;
    fragMatID = obj.materialIndex;
    fragWorldPos = vec3(obj.model * vec4(position, 1.0));
}