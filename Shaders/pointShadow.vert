#version 330 core
layout (location = 0) in vec3 pos;

out vec3 WorldPos;

uniform mat4 model;
uniform mat4 view;         // light looking down one cube face
uniform mat4 projection;   // 90 degree perspective, aspect 1

void main()
{
    WorldPos = vec3(model * vec4(pos, 1.0));
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
