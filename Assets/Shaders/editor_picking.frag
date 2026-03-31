#version 450

layout(location = 0) in flat uint fragEntityID;
layout(location = 0) out uint outEntityID;

void main() 
{
    outEntityID = fragEntityID;
}