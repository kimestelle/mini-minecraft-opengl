#version 330 core

out vec4 fs_Depth;

void main()
{
    fs_Depth = vec4(gl_FragCoord.z, gl_FragCoord.z, gl_FragCoord.z, 1.0);
}
