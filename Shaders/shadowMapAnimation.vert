#version 450

layout(std140, set = 1, binding = 0) readonly buffer BoneMatrices
{
    mat4 boneMatrices[];
};

layout( push_constant ) uniform constants {
    mat4 model;
    mat4 lightSpaceMat;
    int skinningMatsIndiceStart;
} push;

// IN
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;
layout(location = 4) in vec4 inWeights1;
layout(location = 5) in vec4 inWeights2;
layout(location = 6) in ivec4 inBoneIDs1;
layout(location = 7) in ivec4 inBoneIDs2;

void main()
{
    int matsStart = push.skinningMatsIndiceStart;
    mat4 skinMat =
		inWeights1.x * boneMatrices[matsStart + inBoneIDs1.x] +
		inWeights1.y * boneMatrices[matsStart + inBoneIDs1.y] +
		inWeights1.z * boneMatrices[matsStart + inBoneIDs1.z] +
		inWeights1.w * boneMatrices[matsStart + inBoneIDs1.w] +

		inWeights2.x * boneMatrices[matsStart + inBoneIDs2.x] +
		inWeights2.y * boneMatrices[matsStart + inBoneIDs2.y] +
		inWeights2.z * boneMatrices[matsStart + inBoneIDs2.z] +
		inWeights2.w * boneMatrices[matsStart + inBoneIDs2.w];
    
	// Model matrix already added to the transform directly by ozz (model mats are now in world space
	// instead of local space)
    gl_Position = push.lightSpaceMat * /*push.model **/ skinMat * vec4(inPosition, 1.0);
}