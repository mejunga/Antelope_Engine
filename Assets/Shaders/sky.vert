#version 450

layout(binding = 0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec3 viewDir;

vec3 quad[6] = vec3[](
    vec3( 1,  1, 1), vec3(-1, -1, 1), vec3(-1,  1, 1),
    vec3(-1, -1, 1), vec3( 1,  1, 1), vec3( 1, -1, 1)
);

void main()
{
    vec3 p = quad[gl_VertexIndex];
    
    gl_Position = vec4(p.x, p.y, 1.0, 1.0);

    mat4 invProj = inverse(ubo.proj);
    mat4 invView = inverse(ubo.view);

    vec4 viewSpace = invProj * vec4(p.x, p.y, 1.0, 1.0);
    viewDir = (invView * vec4(viewSpace.x, viewSpace.y, viewSpace.z, 0.0)).xyz;
}