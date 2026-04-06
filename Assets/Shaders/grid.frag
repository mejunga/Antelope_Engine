#version 450

layout(location = 0) in vec3 nearPoint;
layout(location = 1) in vec3 farPoint;
layout(location = 2) in mat4 fragView;
layout(location = 6) in mat4 fragProj;
layout(location = 10) in vec3 cameraPos;

layout(location = 0) out vec4 outColor;

vec4 grid(vec3 fragPos3D, float scale, bool drawAxis) 
{
    vec2 coord = fragPos3D.xz * scale;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float minimumz = min(derivative.y, 1.0);
    float minimumx = min(derivative.x, 1.0);
    
    vec4 color = vec4(1.0, 1.0, 1.0, 1.0 - min(line, 1.0));
    
    if(drawAxis) 
    {
        if(fragPos3D.z > -1.0 * minimumz && fragPos3D.z < 1.0 * minimumz)
        {
            color = vec4(1.0, 0.2, 0.2, 1.0);
        }

        if(fragPos3D.x > -1.0 * minimumx && fragPos3D.x < 1.0 * minimumx)
        {
            color = vec4(0.2, 0.2, 1.0, 1.0);
        }
    }
    return color;
}

float computeDepth(vec3 pos) 
{
    vec4 clipSpace = fragProj * fragView * vec4(pos.xyz, 1.0);
    return (clipSpace.z / clipSpace.w);
}

void main() 
{
    float t = -nearPoint.y / (farPoint.y - nearPoint.y);
    if (t < 0.0) { discard; }

    vec3 fragPos3D = nearPoint + t * (farPoint - nearPoint);
    gl_FragDepth = computeDepth(fragPos3D);

    float dist = length(fragPos3D - cameraPos);
    
    float thinFade = 1.0 - smoothstep(10.0, 75.0, dist);
    float thickFade = 1.0 - smoothstep(100.0, 500.0, dist);

    vec4 thinGrid = grid(fragPos3D, 1.0, false);
    vec4 thickGrid = grid(fragPos3D, 0.1, true); 
    
    float thinAlpha = thinGrid.a * 0.1 * thinFade;   
    float thickAlpha = thickGrid.a * 0.2 * thickFade;
    
    vec3 finalColor = mix(thinGrid.rgb, thickGrid.rgb, thickGrid.a);
    
    float finalAlpha = max(thinAlpha, thickAlpha);

    outColor = vec4(finalColor, finalAlpha);
    
    if (outColor.a <= 0.01) { discard; }
}