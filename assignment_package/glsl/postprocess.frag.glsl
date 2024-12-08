#version 330 core

in vec4 fs_UV;

out vec4 out_Col;

uniform sampler2D u_Texture;
uniform float u_Time;
uniform int u_PostEffect;

void main()
{
    vec3 color_noA = texture(u_Texture, fs_UV.xy).rgb;

    if (u_PostEffect == 1) {
        // water, tint blue
        color_noA = color_noA * 0.8 + vec3(0.0, 0.0, 1.0) * 0.2;
    } else if (u_PostEffect == 2) {
        // lava, tint orange
        color_noA = color_noA * 0.8 + vec3(0.66, 0.33, 0.0) * 0.2;
    }
    out_Col = vec4(color_noA, 1.0);
}
