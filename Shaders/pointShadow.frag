#version 330 core

in vec3 WorldPos;

uniform vec3 lightPos;
uniform float farPlane;

// Omnidirectional shadow map (Week 13): store the linear distance to the light (0..1)
// instead of the perspective depth, so all six faces use the same scale
void main()
{
    gl_FragDepth = length(WorldPos - lightPos) / farPlane;
}
