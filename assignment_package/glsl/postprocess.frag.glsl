#version 330 core

in vec4 fs_UV;

out vec4 color;

uniform sampler2D u_Texture;

void main()
{
    vec3 color_noA = texture(u_Texture, fs_UV.xy).rgb;
    color = vec4(color_noA, 1.0);
}
