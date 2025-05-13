#version 450

#define HALF_PI 1.5707963268
#define PI      3.1415926536
#define TWO_PI  6.2831853072

layout(std140, set = 0, binding = 0) uniform UniformBufferObjectVert
{
    mat4 view;
    mat4 proj;
} ubo;

struct Plane
{
    vec4 vector;
};

struct Frustum
{
    Plane top;
    Plane bottom;
    Plane right;
    Plane left;
    Plane far;
    Plane near;
};

struct GrassInstanceData
{
    vec4 posXYZhashW; /// position and randomness from 0 - 1
    vec4 color;
    vec4 windForce;
    float rotation;
    float heightOffset;
    int shouldDraw;
    float pad2;
};

layout(std140, set = 1, binding = 0) readonly buffer SSBOIn
{
    uint instanceCount;
    GrassInstanceData grassInstances[];
};

layout(std140, set = 1, binding = 1) readonly buffer SSBOInLow
{
    uint instanceCountLow;
    GrassInstanceData grassInstancesLow[];
};

layout(std140, set = 1, binding = 2) readonly buffer SSBOInFlower
{
    uint instanceCountFlowers;
    GrassInstanceData grassInstancesFlowers[];
};

layout(std140, set = 1, binding = 3) uniform constants {
    Frustum frustum;

    vec4 viewPos;
    vec4 viewDir;
    vec4 windSimCenter;
    vec4 pad;

    float r;
    float time;
} uboGrass;

// Grass::PushConstant
layout( push_constant ) uniform pushConstants
{
  int grassType;
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

//dt 400, v 0.002, ws 4

// The grass model's height is normalized, so the control points
// are also represented around 1
vec2 CP1 = vec2(0.5, 0.0);
vec2 CP2 = vec2(0.5, 0.5);
vec2 CP3 = vec2(0.5, 1.0);

const float GRASS_BLADE_HEIGHT = 1.5;
const float GRASS_BLADE_WIDTH  = 1.0;

// -------------------- Bezier Function Helpers -------------------- //
// y - the height point from 0 - 1 to sample
vec2 Bezier(const float y)
{
    const float y2 = y * y;
    const float mt = 1 - y;
    const float mt2 = mt * mt;

    return vec2(
        (CP1.x * mt2) + (CP2.x * 2 * mt * y) + (CP3.x * y2),
        (CP1.y * mt2) + (CP2.y * 2 * mt * y) + (CP3.y * y2)
    );
}

// The derivative of a nth degree bezier is an (n - 1)th
// degree bezier (with some changes to the control points)
// This is the same as the tangent at point y
vec2 BezierDerivative(float y)
{
    const float y2 = y * y;
    const float mt = 1 - y;

    return vec2(
        (CP1.x * (2 * y - 2)) + (2 * CP3.x - 4 * CP2.x) * y + 2 * CP2.x,
        (CP1.y * (2 * y - 2)) + (2 * CP3.y - 4 * CP2.y) * y + 2 * CP2.y
    );
}

vec2 BezierNormal(float y)
{
    vec2 tangent = BezierDerivative(y);
    // TODO: check if this conversion works or should be negated
    return normalize(vec2(-tangent.y, tangent.x)); // TODO: can remove normalize()?
}
// ---------------------------------------------------------------- //

vec3 RotateAroundY(vec3 v, const float theta)
{
    const float cosTheta = cos(theta);
    const float sinTheta = sin(theta);

    const float xNew = v.x * cosTheta + v.z * sinTheta;
    const float yNew = v.y;  // The y-coordinate remains unchanged
    const float zNew = -v.x * sinTheta + v.z * cosTheta;

    return vec3(xNew, yNew, zNew);
}


void main() 
{
    // Retrive the per blade grass data
    GrassInstanceData grassData;
    if      (push.grassType == 0) { grassData = grassInstances[gl_InstanceIndex];        }
    else if (push.grassType == 1) { grassData = grassInstancesLow[gl_InstanceIndex];     }
    else if (push.grassType == 2) { grassData = grassInstancesFlowers[gl_InstanceIndex]; }

    // Constants
    const float random = grassData.posXYZhashW.w;
    vec3 windDir = grassData.windForce.xyz;
    const float windStrength = grassData.windForce.w;
    const float rot = grassData.rotation;
    
    // Rotate the unchanged grass model first
    vec3 newPos = inPosition;
    newPos = RotateAroundY(newPos, rot);

    // Move the control points based on the wind and the grass's randomness hash
    CP3.y -= (windStrength) / 2.0;
    CP3.x += (windStrength) / 2.0;
    CP2.x += (windStrength) / 3.0;
    //CP2.x += sin((random * TWO_PI) + (inPosition.y * TWO_PI) + (uboGrass.time / 100)) / 40;
    //CP3.y += (random - 0.5) / 2;
    //CP2.y -= (windStrength) / 8.0;
    //CP2.x += (windStrength) / 8.0;
    
    // Bend in the direction of the wind
    const vec2 bezier = Bezier(newPos.y);
    vec2 bezierNormal = BezierNormal(newPos.y);

    newPos.z += windDir.x * (bezier.x - 0.5) * 2; // - 0.5 because thats where the control points are centered
    newPos.x += windDir.z * (bezier.x - 0.5) * 2;
    newPos.y = bezier.y;

    newPos.x *= grassData.heightOffset * GRASS_BLADE_WIDTH;
    newPos.y *= grassData.heightOffset * GRASS_BLADE_HEIGHT;


    
    // Output data to the fragment shader
    gl_Position = ubo.proj * ubo.view * (vec4(newPos.xyz, 1) + vec4(grassData.posXYZhashW.xyz, 0));
    outFragTexCoord = vec2(0, 0); // inTexCoord;

    outTexIndexAndMaterialType.x = -1; // push.textureIndex;
    outMaterialValues = vec4(0.1, 0.2, 0.4, 0.0); //push.materialValues; metal rough ao minimum

    outFragColor = grassData.color;// * inColor;
    if (inPosition.y < 0.4) { outFragColor.xyz /= 7 * (1 - inPosition.y); } // TODO
    outFragPos = newPos + grassData.posXYZhashW.xyz;

    vec3 normal = normalize(vec3(bezierNormal.x, bezierNormal.y, 0));
    normal = RotateAroundY(normal, random / 2 - 0.25); //atan(windDir.z, windDir.x) - HALF_PI);
    outNormal = normalize(vec3(normal.x, normal.y, normal.z)); // TODO: need normalize?
    //outFragColor = vec4(outNormal, 1);
}