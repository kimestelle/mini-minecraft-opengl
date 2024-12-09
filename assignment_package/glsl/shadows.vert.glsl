#version 330 core

in vec4 vs_Pos;
in vec4 vs_Nor;
in vec4 vs_UV;

uniform mat4 u_DepthMVP;

void main()
{
    gl_Position = u_DepthMVP * vs_Pos;
}
