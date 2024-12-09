#version 150
// ^ Change this to version 130 if you have compatibility issues

// This is a fragment shader. If you've opened this file first, please
// open and read lambert.vert.glsl before reading on.
// Unlike the vertex shader, the fragment shader actually does compute
// the shading of geometry. For every pixel in your program's output
// screen, the fragment shader is run for every bit of geometry that
// particular pixel overlaps. By implicitly interpolating the position
// data passed into the fragment shader by the vertex shader, the fragment shader
// can compute what color to apply to its pixel based on things like vertex
// position, light position, and vertex color.

uniform vec4 u_Color = vec4(0.0, 1.0, 0.0, 1.0); // The color with which to render this instance of geometry.
uniform sampler2D u_Texture;
uniform float u_Time;
uniform sampler2D u_ShadowMap;

// These are the interpolated values out of the rasterizer, so you can't know
// their specific values without knowing the vertices that contributed to them
in vec4 fs_Pos;
in vec4 fs_Nor;
in vec4 fs_LightVec;
in vec4 fs_UV;
in vec4 fs_ShadowPos;

out vec4 out_Col; // This is the final output color that you will see on your
// screen for the pixel that is currently being processed.

void main()
{
    vec2 uv = fs_UV.xy;
    // Material base color (before shading)
    // vec4 diffuseColor = fs_Col;
    if (fs_UV.z == 2.0) { // lava
        uv.x += mod(u_Time * 0.0007, 1.0 / 16.0);
    } else if (fs_UV.z == 1.0) { // water
        uv.x += mod(u_Time * 0.001, 1.0 / 16.0);
    }

    float cosTheta = clamp(dot(fs_Nor, fs_LightVec), 0.0, 1.0);
    vec4 texColor = texture(u_Texture, uv);
    float visibility = texture( u_ShadowMap, fs_ShadowPos.xy).z < fs_ShadowPos.z-clamp(0.005*tan(acos(cosTheta)), 0, 0.01) ? 0.5 : 1.0;

    // Calculate the diffuse term for Lambert shading
    float diffuseTerm = dot(normalize(fs_Nor), normalize(fs_LightVec));
    // Avoid negative lighting values
    diffuseTerm = clamp(diffuseTerm, 0, 1);

    float ambientTerm = 0.25;
    float lightIntensity = (diffuseTerm * visibility) + ambientTerm;
    // float lightIntensity = visibility;
    //Add a small float value to the color multiplier
    //to simulate ambient lighting. This ensures that faces that are not
    //lit by our point light are not completely black.

    // Compute final shaded color
    out_Col = vec4(texColor.rgb * lightIntensity, texColor.a);
    // out_Col = vec4(vec3(visibility), 1);
    // out_Col = vec4(texture(u_ShadowMap, fs_ShadowPos.xy).xyz, 1);
}
