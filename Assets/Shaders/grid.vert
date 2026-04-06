#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec3 nearPoint;
layout(location = 1) out vec3 farPoint;
layout(location = 2) out mat4 fragView;
layout(location = 6) out mat4 fragProj;
layout(location = 10) out vec3 cameraPos;

vec3 gridPlane[6] = vec3[](vec3( 1,  1, 0), vec3(-1, -1, 0), vec3(-1,  1, 0),
                           vec3(-1, -1, 0), vec3( 1,  1, 0), vec3( 1, -1, 0));

vec3 UnprojectPoint(float x, float y, float z, mat4 view, mat4 proj)
{
    mat4 viewInv = inverse(view);
    mat4 projInv = inverse(proj);
    vec4 unprojectedPoint = viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main()
{
    vec3 p = gridPlane[gl_VertexIndex];
    gl_Position = vec4(p, 1.0); 
    
    fragView = ubo.view;
    fragProj = ubo.proj;
    
    cameraPos = inverse(ubo.view)[3].xyz; 
    
    nearPoint = UnprojectPoint(p.x, p.y, 0.0, ubo.view, ubo.proj);
    farPoint  = UnprojectPoint(p.x, p.y, 1.0, ubo.view, ubo.proj);
}