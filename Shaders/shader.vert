#version 330 core

#if defined(DEPTH_PASS)

layout (location = 0) in vec3 pos;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    gl_Position = projection * view * model * vec4(pos, 1.0);
}

#elif defined(POINT_SHADOW_PASS)

layout (location = 0) in vec3 pos;

out vec3 WorldPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    WorldPos = vec3(model * vec4(pos, 1.0));
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}

#elif defined(SKY_PASS)

out vec2 TexCoord;
out vec2 NdcPos;

void main()
{
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    NdcPos = p * 2.0 - 1.0;
    TexCoord = p;
    gl_Position = vec4(NdcPos, 0.0, 1.0);
}

#else

layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aColour;

out vec4 vCol;
out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;
out vec4 FragPosLightSpace;

uniform mat4 model;
uniform mat3 normalMatrix;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpace;
uniform vec2 uvScale;

uniform mat4 reflection;
uniform vec4 clipPlane;

void main()
{
    FragPos = vec3(model * vec4(pos, 1.0));
    gl_Position = projection * view * reflection * vec4(FragPos, 1.0);
    gl_ClipDistance[0] = dot(vec4(FragPos, 1.0), clipPlane);
    FragPosLightSpace = lightSpace * vec4(FragPos, 1.0);
    vCol = vec4(aColour, 1.0f);
    TexCoord = aTexCoord * uvScale;
    Normal = normalMatrix * aNormal;
}

#endif
