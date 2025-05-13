#pragma once

#define INDEX_TYPE uint32_t
#define SHADER_PATH_VERT "Shaders/mainVert"
#define SHADER_PATH_FRAG "Shaders/pbrFrag"
#define SHADER_PATH_VERT_ANIMATION "Shaders/animationVert"
#define SHADER_PATH_FRAG_ANIMATION "Shaders/pbrFrag"
#define SHADER_PATH_VERT_WATER "Shaders/waterVert"
#define SHADER_PATH_FRAG_WATER "Shaders/pbrFrag"
#define SHADER_PATH_VERT_GRASS "Shaders/grassVert"
#define SHADER_PATH_FRAG_GRASS "Shaders/pbrFrag"
#define SHADER_PATH_VERT_POST "Shaders/postVert"
#define SHADER_PATH_FRAG_POST "Shaders/postFrag"

#define SWAPCHAIN_IMAGE_COUNT 3

#define CULL_MODE VK_CULL_MODE_BACK_BIT
#define MAX_MSAA_SAMPLE_COUNT VK_SAMPLE_COUNT_4_BIT
#define ENABLE_SAMPLE_SHADING VK_FALSE

#define CLEAR_COLOR 166/255.0f, 189/255.0f, 220/255.0f, 1.0f

//#define LIGHT_POS 0.0f, 0.0f, 1.0f
//#define LIGHT_COUNT 1

#define MAX_LIGHT_COUNT 99

#define MAX_BONE_INFLUENCE 8 // Must also update shaders/vertex attribute descriptions if changing this
#define MAX_BONE_COUNT 500

#define MTL_BASE_DIR "Assets/Models"