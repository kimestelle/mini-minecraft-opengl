#version 330 core

out float fs_Depth;

void main()
{
    fs_Depth = gl_FragCoord.z;
}
