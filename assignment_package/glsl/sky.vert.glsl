#version 150

in vec4 sky_Pos;
out vec4 fs_Pos;
uniform mat4 u_ViewProjInv;

void main() {
    //transform sky position to world space
    fs_Pos = u_ViewProjInv * sky_Pos;
    gl_Position = sky_Pos;
}
