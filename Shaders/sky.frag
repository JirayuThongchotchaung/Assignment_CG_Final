#version 330 core

// Flat background colour. Drawn inside the puddle mirror with depth = far plane
// ("set depth to infinity where the stencil passes", Week 13).

uniform vec3 skyColour;

out vec4 FragColour;

void main()
{
    gl_FragDepth = 1.0;
    FragColour = vec4(skyColour, 1.0);
}
