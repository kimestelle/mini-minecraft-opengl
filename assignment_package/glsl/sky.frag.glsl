#version 150

in vec4 fs_Pos;
out vec4 out_Col;

uniform mat4 u_ViewProjInv;
uniform vec3 u_CameraPos;
uniform float u_Time;

const float PI = 3.14159265359;
const float TWO_PI = 6.28318530718;

const vec3 dawnSky[3] = vec3[](vec3(0.1, 0.08, 0.15), vec3(0.5, 0.4, 0.45), vec3(0.8, 0.7, 0.6));
const vec3 morningSky[3] = vec3[](vec3(0.6, 0.7, 0.8), vec3(0.75, 0.8, 0.85), vec3(0.8, 0.85, 0.9));
const vec3 noonSky[3] = vec3[](vec3(0.6, 0.8, 1.0), vec3(0.7, 0.9, 1.0), vec3(0.75, 0.95, 1.0));
const vec3 sunsetSky[3] = vec3[](vec3(1.0, 0.5, 0.3), vec3(0.8, 0.4, 0.6), vec3(0.4, 0.2, 0.5));
const vec3 nightSky[3] = vec3[](vec3(0.02, 0.02, 0.1), vec3(0.05, 0.05, 0.2), vec3(0.1, 0.1, 0.3));

const vec3 dawnSun = vec3(1.0, 0.6, 0.4);
const vec3 noonSun = vec3(1.0, 1.0, 0.9);
const vec3 sunsetSun = vec3(1.0, 0.4, 0.3);
const vec3 nightSun = vec3(0.0, 0.0, 0.0);

vec3 rayDirection(vec4 screenPos) {
    //convert screen position to world position with inverted matrix
    vec4 worldPos = u_ViewProjInv * screenPos;
    //convert 4d vector into 3d space
    worldPos /= worldPos.w;
    //get direction by normalizing vector between world space location and camera position
    return normalize(worldPos.xyz - u_CameraPos);
}

//functions to interpolate gradients using smoothstep
vec3 gradient3(vec3 rayDir, const vec3 gradient[3]) {
    float t = clamp(rayDir.y, 0.0, 1.0);
    float stepSize = 1.0 / 2.0;
    float smoothT = smoothstep(0.0, stepSize, t);
    if (t < stepSize) {
        return mix(gradient[0], gradient[1], smoothT);
    } else if (t < 2.0 * stepSize) {
        return mix(gradient[1], gradient[2], smoothT);
    }
    return gradient[2];
}

//choose gradient based on time
vec3 interpolateSkyColor(float time, vec3 rayDir) {
    if (time < 0.3 && time > 0.25) {
        return gradient3(rayDir, dawnSky);
    } else if (time < 0.4) {
        return gradient3(rayDir, morningSky);
    } else if (time < 0.8) {
        return gradient3(rayDir, noonSky);
    } else if (time < 0.85) {
        return gradient3(rayDir, sunsetSky);
    } else {
        return gradient3(rayDir, nightSky);
    }
}

vec3 sunColor(float time) {
    if (time < 0.2) {
        return mix(nightSun, dawnSun, time / 0.2);
    } else if (time < 0.5) {
        return mix(dawnSun, noonSun, (time - 0.2) / 0.3);
    } else if (time < 0.8) {
        return mix(noonSun, sunsetSun, (time - 0.5) / 0.3);
    } else {
        return mix(sunsetSun, nightSun, (time - 0.8) / 0.2);
    }
}

//sun and sun corona
vec3 sunEffect(vec3 rayDir, vec3 sunDir, vec3 sunBaseColor) {
    float sunSize = 0.15;
    float outerGlowSize = 0.3;
    //when ray direction is aligned to sun direction, player is looking at sun
    float sunIntensity = exp(-pow(length(rayDir - sunDir) / sunSize, 2.0));
    float glowIntensity = exp(-pow(length(rayDir - sunDir) / outerGlowSize, 2.0));
    vec3 outerGlowColor = mix(sunBaseColor, vec3(1.0, 0.8, 0.6), 0.3);
    return sunIntensity * sunBaseColor + glowIntensity * outerGlowColor;
}

//random function from homework
vec2 random2(vec2 p) {
    p = vec2(dot(p, vec2(127.1, 311.7)),
              dot(p, vec2(269.5, 183.3)));
    return fract(sin(p) * 43758.5453);
}

//inspo from reddit channel and https://www.shadertoy.com/view/3dSXWt
float starField(vec3 rayDir) {
    vec3 noisePos = rayDir * 300.0;
    //grid coords to generate stars at
    vec3 grid = floor(noisePos);
    vec2 grid2D = vec2(grid.x, grid.z);
    vec2 randomOffsets = random2(grid2D);
    float brightness = step(0.999, randomOffsets.x) * randomOffsets.y;
    return brightness * smoothstep(0.0, -0.1, rayDir.y);
}

void main() {
    vec3 rayDir = rayDirection(vec4(fs_Pos.xy, 1.0, 1.0));
    //adjust to change speed
    float dayTime = mod(u_Time / 4800.0, 1.0);
    float sunAngle = dayTime * TWO_PI + PI;
    vec3 sunDir = normalize(vec3(cos(sunAngle), sin(sunAngle), 0.0));
    vec3 sky = interpolateSkyColor(dayTime, rayDir);
    vec3 currentSunColor = sunColor(dayTime);
    vec3 sun = sunEffect(rayDir, sunDir, currentSunColor);
    float stars = step(0.85, dayTime) * starField(rayDir);
    vec3 starColor = vec3(stars);
    vec3 finalColor = sky + sun + starColor;
    out_Col = vec4(finalColor, 1.0);
}
