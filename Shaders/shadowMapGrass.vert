#version 450

layout(std140, set = 0, binding = 0) uniform UniformBufferObjectVert {
    mat4 view;
    mat4 proj;
} ubo;

struct GrassInstanceData
{
    vec4 pos;
    float rotation;
    float height;
    float p1;
    float p2;
};

layout(std140, set = 1, binding = 0) buffer SSBOOut {
   GrassInstanceData grassInstances[];
};

layout(std140, set = 1, binding = 1) uniform constants {
    vec3 viewPos;
    vec3 viewDir;
    vec2 windPos;
    vec2 windDir;
    float time;
} uboGrass;

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

// The grass model's height is normalized, so the control points
// are also represented around 1
const vec2 CP_1 = vec2(0, 0);
const vec2 CP_2 = vec2(0, 1.0);
const vec2 CP_3 = vec2(0.5, 1.5);
const vec2 CP_4 = vec2(1.0, 1.0);

const vec2 CP_D_1 = CP_2 - CP_1;
const vec2 CP_D_2 = CP_3 - CP_2;
const vec2 CP_D_3 = CP_4 - CP_3;

// The derivative of a nth degree bezier is an (n - 1)th
// degree bezier (with some changes to the control points)
vec2 CubicBezierDerivative(float in_t)
{
    float t2 = in_t * in_t;
    float mt = 1 - in_t;
    float mt2 = mt * mt;

    vec2 out_derivative;
    out_derivative.x = (CP_D_1.x * mt2) + (2 * CP_D_2.x * mt * in_t) + (CP_D_3.x * t2);
    out_derivative.y = (CP_D_1.y * mt2) + (2 * CP_D_2.y * mt * in_t) + (CP_D_3.y * t2);
    
    return out_derivative;
}

vec2 CubicBezierNormal(float in_t)
{
    vec2 derivative = CubicBezierDerivative(in_t);
    return normalize(vec2(-derivative.y, derivative.x));
}

vec2 CubicBezier(float in_t)
{
    float t2 = in_t * in_t;
    float t3 = t2 * in_t;
    float mt = 1 - in_t;
    float mt2 = mt * mt;
    float mt3 = mt2 * mt;

    vec2 out_point;
    out_point.x = (CP_1.x * mt3)  +  (3 * CP_2.x * mt2 * in_t)  +  (3 * CP_3.x * mt * t2)  +  (CP_4.x * t3);
    out_point.y = (CP_1.y * mt3)  +  (3 * CP_2.y * mt2 * in_t)  +  (3 * CP_3.y * mt * t2)  +  (CP_4.y * t3);

    return out_point;
}

void main() 
{
    // Grass height is normalized so we can just plug in vert.y into the
    // bezier functions and for the color
    GrassInstanceData grassData = grassInstances[gl_InstanceIndex];

    // Apply bezier curve
    vec3 newPos = inPosition;
    vec2 bezier = CubicBezier(newPos.y);
    vec2 bezierNormal = CubicBezierNormal(newPos.y);
    newPos.x += bezier.x;
    newPos.y += bezier.y;

    // Rotate
    float rot = grassData.rotation;
    newPos.x = newPos.x*cos(rot) - newPos.x*sin(rot);
    newPos.z = newPos.z*cos(rot) + newPos.z*sin(rot);

    newPos *= 5;

    gl_Position = ubo.proj * ubo.view * (vec4(newPos.xyz, 1) + grassData.pos);
    
    // Out
    outFragTexCoord = vec2(0, 0); // inTexCoord;
    outFragColor = vec4(inPosition.y/5.0, 0.8, 0.1, 1);

    outTexIndexAndMaterialType.x = -1; // push.textureIndex;
    outMaterialValues = vec4(0.05, 0.2, 0.7, 0.005); //push.materialValues;

    outFragPos = newPos.xyz + grassData.pos.xyz; // How do this work? // * modelMat

    vec3 normal = vec3(bezierNormal.x, bezierNormal.y, bezierNormal.x);
    normal.x = normal.x*cos(rot) - normal.x*sin(rot);
    //normal.z = normal.z*cos(rot) + normal.z*sin(rot);
    outNormal = normal;
}