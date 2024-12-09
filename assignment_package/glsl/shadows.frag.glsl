#version 330 core

out vec4 fs_Depth;

in vec4 fs_UV;

void main()
{
    if (fs_UV.z == 2.0 || fs_UV.z == 1.0) { // lava and water
        fs_Depth = vec4(1.0, 1.0, 1.0, 1.0);
    }
    fs_Depth = vec4(gl_FragCoord.z, gl_FragCoord.z, gl_FragCoord.z, 1.0);
}
