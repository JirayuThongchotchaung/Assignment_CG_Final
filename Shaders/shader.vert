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
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpace;     // sun projection * sun view (shadow map)
uniform vec2 uvScale;        // texture tiling

void main()
{
    FragPos = vec3(model * vec4(pos, 1.0));
    gl_Position = projection * view * vec4(FragPos, 1.0);
    FragPosLightSpace = lightSpace * vec4(FragPos, 1.0);
    vCol = vec4(aColour, 1.0f);
    TexCoord = aTexCoord * uvScale;
    // normal matrix keeps normals correct under non-uniform scaling
    Normal = mat3(transpose(inverse(model))) * aNormal;
}
