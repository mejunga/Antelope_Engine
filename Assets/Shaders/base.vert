#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

struct VertexPosition { vec4 pos; };
struct VertexColor    { vec4 color; };
struct VertexNormal   { vec4 normal; };
struct VertexUV       { vec2 uv; vec2 padding; };
struct Face           { uvec4 data; }; 

layout(std140, binding = 1) readonly buffer PosBuffer  { VertexPosition vertices[]; } posBuf;
layout(std140, binding = 2) readonly buffer ColBuffer  { VertexColor colors[]; } colBuf;
layout(std140, binding = 3) readonly buffer NormBuffer { VertexNormal normals[]; } normBuf;
layout(std140, binding = 4) readonly buffer FaceBuffer { Face faces[]; } faceBuf;
layout(std140, binding = 5) readonly buffer UvBuffer   { VertexUV uvs[]; } uvBuf;

struct ObjectData {
    mat4 model;
    uint posOffset;
    uint colorOffset;
    uint normalOffset;
    uint uvOffset;
    uint faceOffset;
    uint materialIndex;
    uint padding1;
    uint padding2;
};

layout(std140, binding = 6) readonly buffer ObjectBuffer { ObjectData objects[]; } objBuf;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out flat uint fragMatID;

void main() 
{
    ObjectData obj = objBuf.objects[gl_InstanceIndex];

    uint faceIndex = obj.faceOffset + (gl_VertexIndex / 3);
    uint vertexOffsetInFace = gl_VertexIndex % 3;

    Face f = faceBuf.faces[faceIndex];
    
    uint vIndex;
    if(vertexOffsetInFace == 0) vIndex = f.data.x;
    else if(vertexOffsetInFace == 1) vIndex = f.data.y;
    else vIndex = f.data.z;

    vec3 position = posBuf.vertices[obj.posOffset + vIndex].pos.xyz;
    vec3 color = colBuf.colors[obj.colorOffset + vIndex].color.xyz;
    vec3 normal = normBuf.normals[obj.normalOffset + vIndex].normal.xyz;
    vec2 uv = uvBuf.uvs[obj.uvOffset + vIndex].uv;

    gl_Position = ubo.proj * ubo.view * obj.model * vec4(position, 1.0);
    
    fragColor = color;
    fragNormal = mat3(obj.model) * normal; 
    fragUV = uv;
    fragMatID = obj.materialIndex;
}