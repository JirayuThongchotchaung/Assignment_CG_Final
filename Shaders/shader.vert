#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aColour;   // material colour (Kd from .mtl)

out vec4 vCol;
out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;
out vec4 FragPosLightSpace;

uniform mat4 model;
uniform mat3 normalMatrix;   // transpose(inverse(model)), computed once per object on the CPU
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpace;     // sun projection * sun view (shadow map)
uniform vec2 uvScale;        // texture tiling

// Stencil mirror (Week 13): the reflected scene is drawn with reflection = mirror about the
// puddle plane. Lighting still uses the real (unreflected) world position, so shadows and
// lights stay correct; only the projected position is mirrored. Identity for normal passes.
uniform mat4 reflection;
uniform vec4 clipPlane;      // keep only geometry above the mirror (used with GL_CLIP_DISTANCE0)

void main()
{
    FragPos = vec3(model * vec4(pos, 1.0));
    gl_Position = projection * view * reflection * vec4(FragPos, 1.0);
    gl_ClipDistance[0] = dot(vec4(FragPos, 1.0), clipPlane);
    FragPosLightSpace = lightSpace * vec4(FragPos, 1.0);
    vCol = vec4(aColour, 1.0f);
    TexCoord = aTexCoord * uvScale;
    // the normal matrix keeps normals perpendicular under non-uniform scaling
    Normal = normalMatrix * aNormal;
}
