#version 450

// IN
layout(set = 0, binding = 0) uniform sampler2D inImageSampler;

// OUT
layout(location = 0) out vec4 outFragColor;

void main()
{
    outFragColor = texture(inImageSampler, gl_FragCoord.xy);
}