#pragma once
#include "Math.hlsli"

struct SurfaceData
{
    float3 Albedo;
    float3 Normal; // after normal map

    float  Metallic;
    float  Roughness;
    float3 Emission;
    float  AO;
};

inline SurfaceData MakeDefaultSurfaceData()
{
    SurfaceData data;
    data.Albedo = float3(1,1,1);
    data.Normal = float3(0,0,1);
    data.Metallic = 0.0;
    data.Roughness = 1.0;
    data.Emission = float3(0,0,0);
    data.AO = 1.0;

    return data;
}
//----------------------------------------------------------------------------------------
float3 DiffuseLighting(
    SurfaceData surf,  
    float3 lightDir,    
    float3 lightColor, 
    float  lightAtten, 
    float  ambience
)
{
    float NdotL = max(dot(surf.Normal, lightDir), 0.0);
    float diffuse = lerp(ambience, 1.0, NdotL);
    float3 result = surf.Albedo * lightColor * diffuse * lightAtten;
    return result;
}

//-----------------------------------------------------------------------------------------
// D: Normal Distribution Function using GGX Distribution
float D_GGX(float NdotH, float roughness)
{
    float a2 = roughness * roughness;
    float f = (NdotH * NdotH) * (a2 - 1.0) +1.0;
    return a2 / (kPi * f * f);
}

// G: Geometric attenuation term Self Shadowing
float GGX(float NdotX, float k)
{
    return NdotX / max((NdotX * (1.0 - k) + k), 1e-5); 
}

float G_Smith(float NdotV, float NdotL, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    return GGX(NdotL, k) * GGX(NdotV, k);
}

// F: Fresnel Equation
float3 F_Schlick(float VdotH, float3 f0)
{
    float f = pow(saturate(1.0 - VdotH), 5.0);
    return f0 + (1.0 - f0) * f;
}

//-----------------------------------------------------------------------------------------
// Direct Lighting PBR, Metalic&Roughness
float3 PBRLighting(
    SurfaceData surf,
    float3 lightDir,      // L, normalized, point to light source
    float3 viewDir,       // V, normalized, point to camera
    float3 lightColor,    // LightColor * LightStrength
    float  lightAtten     // Light Attenuation (Point Light/ Spotlight)
)
{
    float3 N = normalize(surf.Normal);
    float3 V = normalize(viewDir);
    float3 L = normalize(lightDir);
    float3 H = normalize(V + L);

    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));
    float VdotH = saturate(dot(V, H));

    float3 F0 = lerp(float3(0.04,0.04,0.04), surf.Albedo, surf.Metallic);

    // Cook-Torrance BRDF
    float D = D_GGX(NdotH, surf.Roughness);
    float G = G_Smith(NdotV, NdotL, surf.Roughness);
    float3 F = F_Schlick(VdotH, F0);

    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= (1.0 - surf.Metallic);

    float3 diffuse = kD * surf.Albedo / kPi;

    float3 numerator    = D * G * F;
    float  denominator  = 4.0 * NdotV * NdotL + 0.0001;
    float3 specular     = numerator / denominator;

    float3 radiance = lightColor * lightAtten;
    float3 lighting = (diffuse + specular) * radiance * NdotL;

    return lighting;
}

/**************************************************************
Notes:
AO only affect indirect diffuse indirectDiffuse *= AO;
Fake ambient
After all calculation: tone mapping & gammar correction
float3 ambient = float3(0.03, 0.03, 0.03) * albedo * ao;
float3 color = ambient + Lo; // + emissive + indirectDiffuse + indirectSpecular
color = color / (color + float3(1.0, 1.0, 1.0)); // Reinhard operator
color = pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2)); // Gamma correction
return float4(color, 1.0); 

gltf 2.0
albedo rgba
metallicRoughness bg
normal(tangent space) rgb
ao r
emmisive rgb
*/

/**
    SurfaceData surf = MakeDefaultSurfaceData();
    surf.Albedo = diffuseColor.rgb;
    surf.Normal = normalize(input.worldNormal.xyz);

	float3 totalLight = float3(0.f, 0.f, 0.f); // Result

	//-----------------------------------------------------------------------------------------------------------
	// Sunlight
	//-----------------------------------------------------------------------------------------------------------
	{
		float sunAmbience = 0.2f;
        float3 sunDir = -lightConstants.sunNormal;
        float3 sunColor = lightConstants.sunColor.rgb;
        float  sunAtten = lightConstants.sunColor.a;

		totalLight += DiffuseLighting(surf, sunDir, sunColor, sunAtten, sunAmbience);
	}

    //-----------------------------------------------------------------------------------------------------------
	// Point & Spot Lights
	//-----------------------------------------------------------------------------------------------------------
	for (int lightIndex = 0; lightIndex < lightConstants.numLights; ++lightIndex)
	{
		float3 lightPos 		= lightConstants.lightArray[lightIndex].worldPosition;
		float3 lightColor 		= lightConstants.lightArray[lightIndex].color.rgb;
		float  ambience			= lightConstants.lightArray[lightIndex].ambience;
		float  lightBrightness	= lightConstants.lightArray[lightIndex].color.a;
		float3 spotForward		= lightConstants.lightArray[lightIndex].spotForward;
		float  innerRadius		= lightConstants.lightArray[lightIndex].innerRadius;
		float  outerRadius		= lightConstants.lightArray[lightIndex].outerRadius;
		float  innerPenumbraDot	= lightConstants.lightArray[lightIndex].innerDotThreshold;
		float  outerPenumbraDot	= lightConstants.lightArray[lightIndex].outerDotThreshold;

        float dist = length(lightPos - input.worldPos);
        float3 L = normalize(lightPos - input.worldPos);

        float fallOff = saturate(RangeMap(dist, innerRadius, outerRadius, 1.f, 0.f));
        fallOff = SmoothStep3(fallOff);

        float penumbra = saturate(RangeMap(dot(-L, spotForward), innerPenumbraDot, outerPenumbraDot, 1.f, 0.f));
        penumbra = SmoothStep3(penumbra);

        float lightAtten = fallOff * penumbra * lightBrightness;

        totalLight += DiffuseLighting(surf, L, lightColor, lightAtten, ambience);
	}

    float3 finalRGB = saturate(totalLight);

    float4 finalColor = float4(finalRGB, diffuseColor.a);

	clip(finalColor.a - 0.01f);
	return finalColor;

*/




/**
SurfaceData surf = MakeDefaultSurfaceData(); 
//...Fill surf with real data

float3 totalLight = float3(0,0,0);

// Sunlight
{
    float3 sunDir = -lightConstants.sunNormal;
    float3 viewDir = normalize(cameraConstants.cameraPosition.xyz - input.worldPos);
    float3 sunColor = lightConstants.sunColor.rgb;
    float  sunAtten = lightConstants.sunColor.a;
    totalLight += PBRLighting(surf, sunDir, viewDir, sunColor, sunAtten);
}

// Point & Spot Lights
for (int lightIndex = 0; lightIndex < lightConstants.numLights; ++lightIndex)
{
    float3 lightPos     = lightConstants.lightArray[lightIndex].worldPosition;
    float3 lightColor   = lightConstants.lightArray[lightIndex].color.rgb;
    float  lightStrength= lightConstants.lightArray[lightIndex].color.a;
    float3 spotForward  = lightConstants.lightArray[lightIndex].spotForward;
    float  innerRadius  = lightConstants.lightArray[lightIndex].innerRadius;
    float  outerRadius  = lightConstants.lightArray[lightIndex].outerRadius;
    float  innerPenumbraDot = lightConstants.lightArray[lightIndex].innerDotThreshold;
    float  outerPenumbraDot = lightConstants.lightArray[lightIndex].outerDotThreshold;

    float3 L = normalize(lightPos - input.worldPos);
    float dist = length(lightPos - input.worldPos);

    float fallOff = saturate(RangeMap(dist, innerRadius, outerRadius, 1.f, 0.f));
    fallOff = SmoothStep3(fallOff);

    float penumbra = saturate(RangeMap(dot(-L, spotForward), innerPenumbraDot, outerPenumbraDot, 1.f, 0.f));
    penumbra = SmoothStep3(penumbra);

    float atten = fallOff * penumbra * lightStrength;

    float3 viewDir = normalize(cameraConstants.cameraPosition.xyz - input.worldPos);
    totalLight += PBRLighting(surf, L, viewDir, lightColor, atten);
}

float3 finalRGB = saturate(totalLight);
float4 finalColor = float4(finalRGB, surf.Opacity);
clip(finalColor.a - 0.01f);
return finalColor;
*/



// Use it carefully. it is hard to debug macro, if have problems, use the sample code above

#define CALC_TOTAL_DIFFUSE_LIGHT(totalLight, surf, worldPos)                     \
    {                                                                            \
        totalLight = float3(0.f, 0.f, 0.f);                                      \
        /* Sunlight */                                                           \
        {                                                                        \
            float sunAmbience = 0.2f;                                            \
            float3 sunDir = -lightConstants.sunNormal;                           \
            float3 sunColor = lightConstants.sunColor.rgb;                       \
            float  sunAtten = lightConstants.sunColor.a;                         \
            totalLight += DiffuseLighting(surf, sunDir, sunColor, sunAtten, sunAmbience); \
        }                                                                        \
        /* Point & Spot Lights */                                                \
        for (int lightIndex = 0; lightIndex < lightConstants.numLights; ++lightIndex) \
        {                                                                        \
            float3 lightPos        = lightConstants.lightArray[lightIndex].worldPosition; \
            float3 lightColor      = lightConstants.lightArray[lightIndex].color.rgb;    \
            float  ambience        = lightConstants.lightArray[lightIndex].ambience;     \
            float  lightBrightness = lightConstants.lightArray[lightIndex].color.a;      \
            float3 spotForward     = lightConstants.lightArray[lightIndex].spotForward;  \
            float  innerRadius     = lightConstants.lightArray[lightIndex].innerRadius;  \
            float  outerRadius     = lightConstants.lightArray[lightIndex].outerRadius;  \
            float  innerPenumbraDot= lightConstants.lightArray[lightIndex].innerDotThreshold; \
            float  outerPenumbraDot= lightConstants.lightArray[lightIndex].outerDotThreshold; \
            float dist = length(lightPos - worldPos);                      \
            float3 L = normalize(lightPos - worldPos);                     \
            float fallOff = saturate(RangeMap(dist, innerRadius, outerRadius, 1.f, 0.f)); \
            fallOff = SmoothStep3(fallOff);                                      \
            float penumbra = saturate(RangeMap(dot(-L, spotForward), innerPenumbraDot, outerPenumbraDot, 1.f, 0.f)); \
            penumbra = SmoothStep3(penumbra);                                    \
            float lightAtten = fallOff * penumbra * lightBrightness;             \
            totalLight += DiffuseLighting(surf, L, lightColor, lightAtten, ambience); \
        }                                                                        \
    }


#define CALC_TOTAL_PBR_LIGHT(totalLight, surf, worldPos)                                \
    {                                                                                   \
        totalLight = float3(0,0,0);                                                     \
        float3 viewDir = normalize(cameraConstants.cameraWorldPosition.xyz - worldPos); \
        /* Sunlight */                                                          \
        {                                                                       \
            float3 sunDir = -lightConstants.sunNormal;                          \
            float3 sunColor = lightConstants.sunColor.rgb;                      \
            float  sunAtten = lightConstants.sunColor.a;                        \
            totalLight += PBRLighting(surf, sunDir, viewDir, sunColor, sunAtten);\
        }                                                                       \
        /* Point & Spot Lights */                                               \
        for (int lightIndex = 0; lightIndex < lightConstants.numLights; ++lightIndex) \
        {                                                                       \
            float3 lightPos     = lightConstants.lightArray[lightIndex].worldPosition; \
            float3 lightColor   = lightConstants.lightArray[lightIndex].color.rgb;    \
            float  lightStrength= lightConstants.lightArray[lightIndex].color.a;      \
            float3 spotForward  = lightConstants.lightArray[lightIndex].spotForward;  \
            float  innerRadius  = lightConstants.lightArray[lightIndex].innerRadius;  \
            float  outerRadius  = lightConstants.lightArray[lightIndex].outerRadius;  \
            float  innerPenumbraDot = lightConstants.lightArray[lightIndex].innerDotThreshold; \
            float  outerPenumbraDot = lightConstants.lightArray[lightIndex].outerDotThreshold; \
            float3 L = normalize(lightPos - worldPos);                    \
            float dist = length(lightPos - worldPos);                     \
            float fallOff = saturate(RangeMap(dist, innerRadius, outerRadius, 1.f, 0.f)); \
            fallOff = SmoothStep3(fallOff);                                     \
            float penumbra = saturate(RangeMap(dot(-L, spotForward), innerPenumbraDot, outerPenumbraDot, 1.f, 0.f)); \
            penumbra = SmoothStep3(penumbra);                                   \
            float atten = fallOff * penumbra * lightStrength;                   \
            totalLight += PBRLighting(surf, L, viewDir, lightColor, atten);     \
        }                                                                       \
    }

