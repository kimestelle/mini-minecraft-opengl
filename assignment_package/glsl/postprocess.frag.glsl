#version 330 core

in vec4 fs_UV;

out vec4 out_Col;

uniform sampler2D u_Texture;

void main()
{
    vec3 color_noA = texture(u_Texture, fs_UV.xy).rgb;
    out_Col = vec4(color_noA, 1.0);
}
