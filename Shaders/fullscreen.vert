#version 330 core

// One triangle that covers the whole screen, generated from gl_VertexID (no vertex buffer):
// vertices (-1,-1), (3,-1), (-1,3). Used by the sky pass (the puddle mirror background).
out vec2 TexCoord;
out vec2 NdcPos;

void main()
{
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    NdcPos = p * 2.0 - 1.0;
    TexCoord = p;
    gl_Position = vec4(NdcPos, 0.0, 1.0);
}
