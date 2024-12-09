#version 150

in vec4 fs_Pos;
out vec4 out_Col;

uniform mat4 u_ViewProjInv;    // inverse projection matrix for screen-space to world-space conversion
uniform vec3 u_CameraPos;      // camera position
uniform float u_Time;          // time parameter for animation
uniform vec2 u_Resolution;     // screen resolution (passed from cpu)

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;

// colors from example
const vec3 sunset[5] = vec3[](vec3(255, 229, 119) / 255.0,
                               vec3(254, 192, 81) / 255.0,
                               vec3(255, 137, 103) / 255.0,
                               vec3(253, 96, 81) / 255.0,
                               vec3(57, 32, 51) / 255.0);

const vec3 dusk[5] = vec3[](vec3(144, 96, 144) / 255.0,
                            vec3(96, 72, 120) / 255.0,
                            vec3(72, 48, 120) / 255.0,
                            vec3(48, 24, 96) / 255.0,
                            vec3(0, 24, 72) / 255.0);

const vec3 sunColor = vec3(255, 255, 190) / 255.0;
const vec3 cloudColor = sunset[3];

// convert spherical coords to uv coords in sky dome
vec2 sphereToUV(vec3 p) {
    float phi = atan(p.z, p.x);
    if (phi < 0) {
        phi += TWO_PI;
    }
    float theta = acos(p.y);
    return vec2(1 - phi / TWO_PI, 1 - theta / PI);
}

// gradient interpolation between colors at set height intervals
vec3 interpolateSkyColor(float time, vec3 rayDir) {
    if (time < 0.3 && time > 0.25) {
        return mix(sunset[0], sunset[1], smoothstep(0.0, 1.0, rayDir.y));
    } else if (time < 0.4) {
        return mix(sunset[1], sunset[2], smoothstep(0.0, 1.0, rayDir.y));
    } else if (time < 0.6) {
        return mix(sunset[2], sunset[3], smoothstep(0.0, 1.0, rayDir.y));
    } else if (time < 0.85) {
        return mix(sunset[3], sunset[4], smoothstep(0.0, 1.0, rayDir.y));
    } else {
        return mix(sunset[4], dusk[0], smoothstep(0.0, 1.0, rayDir.y));
    }
}

// sun effect that expands as it nears the horizon
vec3 sunEffect(vec3 rayDir, vec3 sunDir, vec3 sunBaseColor) {
    float sunSize = 0.15;
    float outerGlowSize = 0.3;

    // calculate angle between ray direction and sun direction
    float angle = acos(dot(rayDir, sunDir));
    // increase sun size as it nears the horizon
    float sizeFactor = smoothstep(PI * 0.25, PI * 0.5, angle);
    sunSize = mix(0.15, 0.3, sizeFactor); // increase size from 0.15 to 0.3

    // intensity + sun corona
    float sunIntensity = exp(-pow(length(rayDir - sunDir) / sunSize, 2.0));
    float glowIntensity = exp(-pow(length(rayDir - sunDir) / outerGlowSize, 2.0)) * sizeFactor;
    vec3 outerGlowColor = mix(sunBaseColor, vec3(1.0, 0.8, 0.6), 0.3); // slightly yellowish glow

    return sunIntensity * sunBaseColor + glowIntensity * outerGlowColor;
}

// compute ray direction from screen position
vec3 rayDirection(vec4 screenPos) {
    vec4 worldPos = u_ViewProjInv * screenPos;  // convert to world space
    worldPos /= worldPos.w;  // convert from homogeneous coordinates
    return normalize(worldPos.xyz - u_CameraPos); // direction vector from camera to screen position
}

// random function from homework
vec2 random2(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)),
              dot(p, vec2(269.5, 183.3)));
    return fract(sin(p) * 43758.5453);
}

// inspo from reddit channel and https://www.shadertoy.com/view/3dSXWt
float starField(vec3 rayDir) {
    vec3 noisePos = rayDir * 300.0;
    // grid coords to generate stars at
    vec3 grid = floor(noisePos);
    vec2 grid2D = vec2(grid.x, grid.z);
    vec2 randomOffsets = random2(grid2D);
    float brightness = step(0.999, randomOffsets.x) * randomOffsets.y;
    return brightness * smoothstep(0.0, -0.1, rayDir.y);
}

void main() {
    vec3 rayDir = rayDirection(vec4(fs_Pos.xy, 1.0, 1.0));

    // wraparound time for animation
    float dayTime = mod(u_Time / 800.0, 1.0);

    // sun angle based on time
    float sunAngle = dayTime * TWO_PI + PI;
    vec3 sunDir = normalize(vec3(cos(sunAngle), sin(sunAngle), 0.0));

    // sky color based on time
    vec3 skyColor = interpolateSkyColor(dayTime, rayDir);

    // sun color
    vec3 currentSunColor = sunColor;
    // sun's effect on the scene
    vec3 sun = sunEffect(rayDir, sunDir, currentSunColor);

    // stars appear after 85% of the day cycle
    float stars = step(0.85, dayTime) * starField(rayDir);
    vec3 starColor = vec3(stars);

    // combine sky + sun + stars
    vec3 finalColor = skyColor + sun + starColor;

    out_Col = vec4(finalColor, 1.0);
}
