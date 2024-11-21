#version 330 core

in vec2 fs_UV;

out vec3 color;

uniform sampler2D u_Texture;

void main()
{
    color = texture(u_Texture, fs_UV).rgb;
}
