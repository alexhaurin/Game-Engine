#version 450

#define PI 3.1415926538

layout(std140, binding = 0) uniform UniformBufferObjectVert {
    mat4 view;
    mat4 proj;
} ubo;

layout( push_constant ) uniform constants {
    mat4 model;
    vec4 materialValues;

    int textureIndex;
    int time;
} push;

// IN
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

// OUT
layout(location = 0) out vec4 outFragColor;
layout(location = 1) out vec2 outFragTexCoord;
layout(location = 2) out vec2 outTexIndexAndMaterialType;
layout(location = 3) out vec3 outFragPos;
layout(location = 4) out vec3 outNormal;
layout(location = 5) out vec4 outMaterialValues;

void main() {
    // wave constraints
    vec2 waveDirection = normalize(vec2(5, 3));

    float waveAmplitude = 1;
    float waveLength = 5;
    float waveNumber = (2 * PI) / waveLength; // calculates length of each sin wave
    float waveSpeed = 1.0 / 500.0;
    float phaseSpeed = sqrt(9.8 / waveNumber); // not arbitrary, related to gravitation pull and waveNumber

    // x, y, and z coords of each vertex at any given time
    float x = cos(dot(waveDirection, inPosition.xz) + push.time / 500.0) * waveDirection.x + inPosition.x;
    float y = sin(dot(waveDirection, inPosition.xz) + push.time / 500.0);
    float z = cos(dot(waveDirection, inPosition.xz) + push.time / 500.0) * waveDirection.y + inPosition.z;

    // float x = 0;
    // float y = waveAmplitude / waveNumber *  sin((inPosition.z + (push.time * phaseSpeed)) * waveNumber) ); // * waveAmplitude;
    // float z = 0;

    // color and position using calculated x, y, z coords
    outFragColor = vec4(0.1, 0.6, 1, 1.0);
    gl_Position = ubo.proj * ubo.view * push.model * vec4((inPosition + vec3(0, 0, 0)), 1.0);
    
    // Out
    outFragTexCoord = inTexCoord;
    outTexIndexAndMaterialType.x = push.textureIndex;
    outMaterialValues = push.materialValues;
    outFragPos = vec3(push.model * vec4(inPosition, 1.0)); // How do this work?

    outNormal  = mat3(push.model) * inNormal;
}