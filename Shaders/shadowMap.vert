#version 450

layout( push_constant ) uniform constants {
    mat4 model;
    mat4 lightSpaceMatrix;
} push;

// IN
layout(location = 0) in vec3 inPosition;

void main()
{
    gl_Position = push.lightSpaceMatrix * push.model * vec4(inPosition, 1.0);
}