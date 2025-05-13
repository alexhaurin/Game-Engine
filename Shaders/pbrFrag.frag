#version 450

// --- IN --- //
layout(location = 0) in vec4 inFragColor;
layout(location = 1) in vec2 inFragTexCoord;
layout(location = 2) in vec2 inTexIndexAndMaterialType;
layout(location = 3) in vec3 inFragPos;
layout(location = 4) in vec3 inNormal;
layout(location = 5) in vec4 inMaterialValues;

// --- OUT--- //
layout(location = 0) out vec4 outFragColor;

// --- DATA--- //
// Defined in Engine/Graphics/Objects/Light.h
int LIGHT_ID_POINT = 0;
int LIGHT_ID_DIRECTIONAL = 1;
//int LIGHT_ID_SPHERE = 2;

struct LightData
{
    int type;

    float str;
    float radius;

    float inCutOff;
    float outCutOff;
    float PADDING1;
    float PADDING2;
    float PADDING3;

    vec3  pos;
    vec3  dir;
    vec3 col;
};

layout(std140, binding = 1) uniform UniformBufferObjectLights
{
    mat4 lightSpaceMat;
    vec3 viewPos;
    vec3 viewDir;

    uint lightCount;

    LightData lights[99];
} ubo;

layout(binding = 2) uniform sampler2D texSampler[6];

layout(binding = 3) uniform sampler2D shadowMap;

// --- FUNCTIONS --- //
const float PI = 3.14159265359;

float rand(vec2 co) { return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453); }

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    // Calculates how much the surface reflects at zero incidence (looking directly at it)
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
} 
float DistributionGGX(vec3 N, vec3 H, float roughness, float prime) {
    float a      = roughness*prime;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

float compute_shadow_factor()
{
    float out_shadowFactor = 0.0;

    // Convert light space position to NDC
    vec4 a = ubo.lightSpaceMat * vec4(inFragPos, 1.0);
    vec3 light_space_ndc = a.xyz /= a.w;

    // Bias to avoid shadow acne
    float bias = 0.0005;

    // Early exit for frags outside of the shadow map    
    if (abs(light_space_ndc.x) > 1.0 + bias || abs(light_space_ndc.y) > 1.0 + bias || abs(light_space_ndc.z) > 1.0 + bias)
    {
        out_shadowFactor = 1;
        return out_shadowFactor;
    }

    vec2 shadow_map_coord = light_space_ndc.xy * 0.5 + 0.5;
    float currentDepth = light_space_ndc.z;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    for(int x = -2; x <= 2; ++x)
    {
        for(int y = -2; y <= 2; ++y)
        {
            float pcfDepth = texture(shadowMap, shadow_map_coord.xy.xy + vec2(x, y) * texelSize).r; 
            out_shadowFactor += currentDepth - bias > pcfDepth ? 0.0 : 1.0;        
        }    
    }
    out_shadowFactor /= 16.0;

    return out_shadowFactor;
}

bool IsInLight(const vec3 in_fragPos)
{
    // Convert light space position to NDC
    const float bias = 0.005;
    vec4 lightSpacePos = ubo.lightSpaceMat * vec4(in_fragPos, 1.0);

    // Early exit for frags outside of the shadow map
    if (abs(lightSpacePos.x) > 1.0 + bias || abs(lightSpacePos.y) > 1.0 + bias || abs(lightSpacePos.z) > 1.0 + bias)
    {
        return true;
    }
    
    // Translate from NDC to shadow map space (Vulkan's Z is already in [0..1])
    vec2 shadowMapCoord = lightSpacePos.xy * 0.5 + 0.5;
    float currentDepth = lightSpacePos.z;
    return (currentDepth - bias) <= texture(shadowMap, shadowMapCoord).r;
}

// Mie scaterring approximated with Henyey-Greenstein phase function.
float ComputeScattering(float lightDotView)
{
    const float G_SCATTERING = 0.5;
    float result = 1.0f - G_SCATTERING * G_SCATTERING;
    result /= (4.0f * PI * pow(1.0f + G_SCATTERING * G_SCATTERING - (2.0f * G_SCATTERING) * lightDotView, 1.5f));
    return result;
}

// --- MAIN --- //

void main()
{
    // Texture color / normal map
    int texIndex = int(inTexIndexAndMaterialType.x);

    vec3 NORMAL = normalize(inNormal); // texture(texSampler[2], inFragTexCoord).rgb; //normalize(inNormal);
    if (texIndex < 0) {
        outFragColor = inFragColor;
    }
    else {
    	outFragColor = texture(texSampler[texIndex], inFragTexCoord) * inFragColor;
    }

    vec3 viewPos = ubo.viewPos;
    vec3 viewDir = ubo.viewDir;
    vec3 viewToFrag = normalize(viewPos - inFragPos);

    // Set material variables
    vec3 albedo     = outFragColor.rgb;
    float roughness = inMaterialValues.y; //texture(texSampler[3], inFragTexCoord).x; //inMaterialValues.y; //;
    float metallic  = inMaterialValues.x; //texture(texSampler[4], inFragTexCoord).x; //inMaterialValues.x; //;
    float ao        = inMaterialValues.z; // texture(texSampler[5], inFragTexCoord).x; //inMaterialValues.z; // 0.5f;
    float ambient   = inMaterialValues.w;

    // Calculate
    vec3 Lo = vec3(0.0f);
    for (int i = 0; i < ubo.lightCount; i++)
    {
        // Setup light values
        vec3 lightCol = ubo.lights[i].col;
        vec3 lightPos = ubo.lights[i].pos;
        vec3 lightDir = -normalize(ubo.lights[i].dir);
        int lightType = ubo.lights[i].type;
        float lightRadius = ubo.lights[i].radius;

        vec3 viewToLight = lightPos - viewPos;
	  
        // Get lightToFrag based on the type of light (point, area, etc)
        vec3 fragToLight = normalize(lightPos - inFragPos);
        if (lightType == LIGHT_ID_DIRECTIONAL) { fragToLight = -lightDir; }

        vec3 reflection = normalize(reflect(viewToFrag, NORMAL));

        vec3 halfway = normalize(fragToLight + viewToFrag);
        float distance    = length((lightPos - inFragPos));
        float attenuation = (1.0f / (distance * distance)) * ubo.lights[i].str;

        // Directional light
        // First light drawn is shadow mapped one
        if (lightType == LIGHT_ID_DIRECTIONAL) { attenuation = 1.0f; }

        vec3 radiance = lightCol * attenuation;

        // F, D, G
        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        vec3 F = FresnelSchlick(max(dot(halfway, viewToFrag), 0.0), F0); 

        float prime = roughness;
        //if (lightType == 1) { prime = clamp(lightRadius/(distance*2.0) + roughness, 0.0, 1.0); } // Sphere light shit?
        float NDF = DistributionGGX(NORMAL, halfway, roughness, prime);     
        float G   = GeometrySmith(NORMAL, viewToFrag, fragToLight, roughness);

        // Cook-Torrance BRDF
        vec3 kS = F; // Light that gets reflected
        vec3 kD = vec3(1.0) - kS; // Light that gets refracted
        kD *= 1.0 - metallic; // Metallic surfaces dont refracte light

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(NORMAL, viewToFrag), 0.0) * max(dot(NORMAL, fragToLight), 0.0) + 0.0001;
        vec3 specular     = numerator / denominator;
  
        float NdotL = max(dot(NORMAL, fragToLight), 0.0);       
        vec3 totalLight = (kD * albedo / PI + specular) * radiance * NdotL * ubo.lights[i].str;

        if (lightType == LIGHT_ID_DIRECTIONAL) { totalLight *= max(compute_shadow_factor(), 0.2); }

        Lo += totalLight;
    }

    vec3 a = vec3(ambient) * albedo * ao;

    outFragColor.rgb = Lo + a;

    //// God rays
    //{
    //    mat3 ditherPattern =
    //    {
    //        { 0.0f, 0.5f, 0.125f},
    //        { 0.75f, 0.22f, 0.875f},
    //        { 0.1875f, 0.6875f, 0.0625f},
    //    };
//
    //    // Light data (using first directional light)
    //    vec3 lightColor      = ubo.lights[0].col.rgb;
    //    float lightStrength  = ubo.lights[0].str;
    //    vec3  lightDirection = normalize(ubo.lights[0].dir);
    //    vec3  lightPosition  = ubo.lights[0].dir; // Light direction acting as position here
//
    //    vec3 startPos     = ubo.viewPos;
    //    vec3 rayVector    = inFragPos - startPos;
    //    float rayLength   = length(rayVector);
    //    vec3 rayDirection = rayVector / rayLength;
//
    //    float stepCount = 20;
    //    float stepLength = rayLength / stepCount;
    //    vec3 step = rayDirection * stepLength;
//
    //    // Final output is total amount of fog
    //    vec3 totalFog = vec3(0);
//
    //    // Offset the start position
    //    vec3 currentPos = startPos; // Current position of the ray march (out of camera)
    //    for (int i = 0; i < stepCount; i++)
    //    {
    //        if (IsInLight(currentPos))
    //        {
    //            float distance = length((inFragPos - lightDirection));
    //            float attenuation = 1; //1.0f/distance; //1.0f / (distance * distance);
    //            float lightDotView = dot(rayDirection, -lightDirection);
//
    //            totalFog += attenuation * ComputeScattering(lightDotView).xxx;
    //        };
    //        currentPos += step;
    //    }
    //    totalFog /= stepCount;
//
    //    outFragColor.rgb += (totalFog) * lightColor * lightStrength * 3.0;
    //}
    

    // Gamma correction/Reinhard tone mapping
    const float gamma = 2.2;
    vec3 mapped = outFragColor.rgb / (outFragColor.rgb + vec3(1.0));
    mapped = pow(mapped, vec3(1.0 / gamma));

    outFragColor.rgb = mapped;

    //if (ambient >= 1.0) { outFragColor.rgb = vec3(ambient); }
}