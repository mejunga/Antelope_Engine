#version 450

layout(location = 0) in vec3 nearPoint;
layout(location = 1) in vec3 farPoint;
layout(location = 2) in mat4 fragView;
layout(location = 6) in mat4 fragProj;
layout(location = 10) in vec3 cameraPos;

layout(location = 0) out vec4 outColor;

vec4 grid(vec3 fragPos3D, float scale, float lineThickness) 
{
    vec2 coord = fragPos3D.xz * scale;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    
    float alpha = 1.0 - min(line / lineThickness, 1.0);
    return vec4(1.0, 1.0, 1.0, alpha);
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
    float thickFade = 1.0 - smoothstep(25.0, 350.0, dist);

    float thinThick  = mix(0.5,  0.05, smoothstep(3.0,  40.0,  dist));
float thickThick = mix(1.5,  0.3,  smoothstep(5.0,  100.0, dist));

    vec4 thinGrid = grid(fragPos3D, 1.0, thinThick);
    vec4 thickGrid = grid(fragPos3D, 0.1, thickThick);

    float thinAlpha = thinGrid.a  * 0.1 * thinFade;
    float thickAlpha = thickGrid.a * 0.2 * thickFade;
    
    vec3 finalColor = mix(thinGrid.rgb, thickGrid.rgb, thickGrid.a);
    float finalAlpha = max(thinAlpha, thickAlpha);

    vec2 worldDeriv = fwidth(fragPos3D.xz);
    float axisThickness = 1.0;
    
    if(abs(fragPos3D.z) < worldDeriv.y * axisThickness) 
    {
        finalColor = vec4(1.0, 0.2, 0.2, 1.0).rgb;
        finalAlpha = max(finalAlpha, 0.5 * thickFade); 
    }
    
    if(abs(fragPos3D.x) < worldDeriv.x * axisThickness) 
    {
        finalColor = vec4(0.2, 0.2, 1.0, 1.0).rgb;
        finalAlpha = max(finalAlpha, 0.5 * thickFade);
    }

    outColor = vec4(finalColor, finalAlpha);
    
    if (outColor.a <= 0.01) { discard; }
}