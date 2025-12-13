#include "Common/Utils.hlsli"
#include "Common/ShaderConstants.hlsli"
#include "Common/Resources.hlsli"
#include "Common/Lighting.hlsli"
#include "Common/Math.hlsli"
#include "Common/StaticSampler.hlsli"
#include "Common/TriplanarUtils.hlsli"
#include "Common/ToneMapping.hlsli"

//------------------------------------------------------------------------------------------------
struct vs_input_t
{
    float3 modelPosition : POSITION;
    float2 encodedSmoothNormal : NORMAL;            //[0, 1]
    // float3 modelSmoothNormal : NORMAL;
    uint materialID1 : MATERIAL_PRIMARY;
    uint materialID2 : MATERIAL_SECONDARY;
    float blendFactor : BLEND_FACTOR;        // [0, 1] 0 is mat1 1 is mat2
};

//------------------------------------------------------------------------------------------------
struct v2p_t
{
    float4 clipPosition  : SV_Position;
    float3 worldPos      : WORLD_POSITION;
    float3 worldSmoothNormal   : WORLD_NORMAL;

    nointerpolation uint materialID1 : MATERIAL_ID0;
    nointerpolation uint materialID2 : MATERIAL_ID1;
    float blendFactor : BLEND_FACTOR;
};

//----------------------------------------------------------------------------------------------------

struct GMaterial
{
    uint albedoTextureIndex;
	uint normalTextureIndex;
	uint ormhTextureIndex;      // x-ao y-roughness z-metallic w-height
	uint emissiveTextureIndex;

	float uvScale;
	float lowPolyLevel;
};


struct TerrainRenderResources
{
    uint engineConstantsIndex;
    uint cameraConstantsIndex;
    uint modelConstantsIndex;
    uint lightConstantsIndex;
    uint perFrameConstantsIndex;

    uint materialBufferIndex; // StructuredBuffer<GMaterial> materialBuffer;
};



float3 CalculateNormalForLighting(float3 worldSmoothNormal, float3 worldFaceNormal, float lowPolyLevel)
{
    const float MY_EPSILON = 0.001f;
    
    // early out
    if (lowPolyLevel < MY_EPSILON) return worldSmoothNormal; // wood - very smooth
    if (lowPolyLevel > 1.0 - MY_EPSILON) return worldFaceNormal; // diamond/crystal - very sharp

    float normalSimilarity = dot(worldSmoothNormal, worldFaceNormal);

    // float hardEdgeThreshold = lerp(0.5, 0.8, lowPolyLevel);
    // float softEdgeThreshold = saturate(hardEdgeThreshold + 0.3);

    float hardEdgeThreshold = lerp(0.5, 0.85, lowPolyLevel);
    float transitionWidth = lerp(0.3, 0.1, lowPolyLevel); // high lowPolyLevel transition more sharply
    float softEdgeThreshold = min(hardEdgeThreshold + transitionWidth, 0.95f);
  
    if (normalSimilarity < hardEdgeThreshold)
    {
        return worldFaceNormal;
    }

    if (normalSimilarity > softEdgeThreshold)
    {
        return worldSmoothNormal;
    }

    float blendFactor = smoothstep(hardEdgeThreshold, softEdgeThreshold, normalSimilarity);

    return normalize(lerp(worldFaceNormal, worldSmoothNormal, blendFactor));
}







ConstantBuffer<TerrainRenderResources> renderResources : register(b0);
//----------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];
    ConstantBuffer<ModelConstants> modelConstants = ResourceDescriptorHeap[renderResources.modelConstantsIndex];

    float4 modelPosition = float4(input.modelPosition, 1);
	float4 worldPosition = mul(modelConstants.modelToWorldTransform, modelPosition);
	float4 cameraPosition = mul(cameraConstants.worldToCameraTransform, worldPosition);
	float4 renderPosition = mul(cameraConstants.cameraToRenderTransform, cameraPosition);
	float4 clipPosition = mul(cameraConstants.renderToClipTransform, renderPosition);

    float3 modelSmoothNormal = DecodeNormal(input.encodedSmoothNormal);
    float4 worldSmoothNormal = mul(modelConstants.modelToWorldTransform, float4(modelSmoothNormal, 0.0f));
    // float4 worldSmoothNormal = mul(modelConstants.modelToWorldTransform, float4(input.modelSmoothNormal, 0.0f));

    v2p_t v2p;
    v2p.clipPosition = clipPosition;
    v2p.worldPos     = worldPosition.xyz;
    v2p.worldSmoothNormal  = normalize(worldSmoothNormal.xyz);

    v2p.materialID1   = input.materialID1;
    v2p.materialID2   = input.materialID2;
    v2p.blendFactor = input.blendFactor;
    return v2p;
}

float4 PixelMain(v2p_t input) : SV_Target0
{
    // Smooth Normal for texturing(triplanar sampling), face normal for lighting (low poly)

    ConstantBuffer<EngineConstants> engineConstants = ResourceDescriptorHeap[renderResources.engineConstantsIndex];
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];
    ConstantBuffer<ModelConstants> modelConstants = ResourceDescriptorHeap[renderResources.modelConstantsIndex];
    ConstantBuffer<LightConstants> lightConstants = ResourceDescriptorHeap[renderResources.lightConstantsIndex];
    StructuredBuffer<GMaterial> materialBuffer = ResourceDescriptorHeap[renderResources.materialBufferIndex];

    uint materialIndex1 = input.materialID1;
    uint materialIndex2 = input.materialID2;

    float materialWeight1 = 1.f - input.blendFactor;
    float materialWeight2 = input.blendFactor;

    SamplerState samp = s_linearWrap; // Fixed sampler state
    const float blendSharpness = 10.f; // Fixed blendSharpness

    float3 worldSmoothNormal = normalize(input.worldSmoothNormal);
    float3 worldFaceNormal = normalize(cross(ddy(input.worldPos), ddx(input.worldPos)));



    if (materialIndex1 == materialIndex2 || materialWeight2 < 0.002f)
    {
        // no need to blend
        GMaterial mat = materialBuffer[materialIndex1];

        float3 geometricNormal = CalculateNormalForLighting(worldSmoothNormal, worldFaceNormal, mat.lowPolyLevel);

        float4 albedoTexel = SampleTriplanar(input.worldPos, worldSmoothNormal, mat.uvScale, blendSharpness,
                                            ResourceDescriptorHeap[mat.albedoTextureIndex], samp);
        float4 ormh = SampleTriplanar(input.worldPos, worldSmoothNormal, mat.uvScale, blendSharpness,
                                            ResourceDescriptorHeap[mat.ormhTextureIndex], samp);
        float3 emissive = SampleTriplanar(input.worldPos, worldSmoothNormal, mat.uvScale, blendSharpness,
                                            ResourceDescriptorHeap[mat.emissiveTextureIndex], samp).rgb;

        float3 pixelNormalWorldSpace = SampleTriplanarNormalWithBase(input.worldPos, worldSmoothNormal, geometricNormal, mat.uvScale, blendSharpness,
                                                             ResourceDescriptorHeap[mat.normalTextureIndex], samp);

        // float3 pixelNormalWorldSpace = SampleTriplanarNormal(input.worldPos, worldSmoothNormal, mat.uvScale, blendSharpness,
        //                                                      ResourceDescriptorHeap[mat.normalTextureIndex], samp);

        float4 modelColor = modelConstants.modelColor;
        float4 diffuseColor = albedoTexel * modelColor;
        clip(diffuseColor.a < 0.01f);

        SurfaceData surf = MakeDefaultSurfaceData();
        surf.Albedo    = diffuseColor.rgb;
        surf.Normal    = pixelNormalWorldSpace;
        surf.Metallic  = ormh.z;
        surf.Roughness = ormh.y;

        float3 directLighting = float3(0.f, 0.f, 0.f);
        CALC_TOTAL_PBR_LIGHT(directLighting, surf, input.worldPos);

        float3 ambient = float3(0.02, 0.02, 0.02) * diffuseColor.rgb * ormh.x;
        float3 color = ambient + directLighting + emissive;
        color = ACESFilm(color);
        color = pow(color, 1.0/2.2);

        return float4(color, 1.0);
    }
    else
    {
        GMaterial mat1 = materialBuffer[materialIndex1];
        GMaterial mat2 = materialBuffer[materialIndex2];

        float4 albedoTexel1 = SampleTriplanar(input.worldPos, worldSmoothNormal, mat1.uvScale, blendSharpness,
                                            ResourceDescriptorHeap[mat1.albedoTextureIndex], samp);
        float4 ormh1 = SampleTriplanar(input.worldPos, worldSmoothNormal, mat1.uvScale, blendSharpness,
                                    ResourceDescriptorHeap[mat1.ormhTextureIndex], samp);
        float3 emissive1 = SampleTriplanar(input.worldPos, worldSmoothNormal, mat1.uvScale, blendSharpness,
                                        ResourceDescriptorHeap[mat1.emissiveTextureIndex], samp).rgb;
        float3 normalWorld1 = SampleTriplanarNormalWithBase(input.worldPos, worldSmoothNormal, 
                                                            CalculateNormalForLighting(worldSmoothNormal, worldFaceNormal, mat1.lowPolyLevel),
                                                            mat1.uvScale, blendSharpness, 
                                                            ResourceDescriptorHeap[mat1.normalTextureIndex], samp);
        // float3 normalWorld1 = SampleTriplanarNormal(input.worldPos, worldSmoothNormal, 
        //                                             mat1.uvScale, blendSharpness, 
        //                                             ResourceDescriptorHeap[mat1.normalTextureIndex], samp);

        float4 albedoTexel2 = SampleTriplanar(input.worldPos, worldSmoothNormal, mat2.uvScale, blendSharpness,
                                            ResourceDescriptorHeap[mat2.albedoTextureIndex], samp);
        float4 ormh2 = SampleTriplanar(input.worldPos, worldSmoothNormal, mat2.uvScale, blendSharpness,
                                    ResourceDescriptorHeap[mat2.ormhTextureIndex], samp);
        float3 emissive2 = SampleTriplanar(input.worldPos, worldSmoothNormal, mat2.uvScale, blendSharpness,
                                        ResourceDescriptorHeap[mat2.emissiveTextureIndex], samp).rgb;
        float3 normalWorld2 = SampleTriplanarNormalWithBase(input.worldPos, worldSmoothNormal, 
                                                            CalculateNormalForLighting(worldSmoothNormal, worldFaceNormal, mat2.lowPolyLevel),
                                                            mat2.uvScale, blendSharpness, 
                                                            ResourceDescriptorHeap[mat2.normalTextureIndex], samp);

        // float3 normalWorld2 = SampleTriplanarNormal(input.worldPos, worldSmoothNormal, 
        //                                             mat2.uvScale, blendSharpness, 
        //                                             ResourceDescriptorHeap[mat2.normalTextureIndex], samp);


        float h1 = ormh1.w;
        float h2 = ormh2.w;

        float a1 = materialWeight1;
        float a2 = materialWeight2;

        // height blend parameter, can adjust the softness of the boundary
        float depth = 0.2;

        float m1 = h1 + a1;
        float m2 = h2 + a2;
        float ma = max(m1, m2) - depth;

        float b1 = max(m1 - ma, 0);
        float b2 = max(m2 - ma, 0);

        float sumB = b1 + b2 + 1e-5; // prevent divide by 0

        // Blend
        float3 blendAlbedo = (albedoTexel1.rgb * b1 + albedoTexel2.rgb * b2) / sumB;
        float  blendAO = (ormh1.x * b1 + ormh2.x * b2) / sumB;
        float  blendRoughness = (ormh1.y * b1 + ormh2.y * b2) / sumB;
        float  blendMetallic = (ormh1.z * b1 + ormh2.z * b2) / sumB;
        float3 blendEmissive = (emissive1 * b1 + emissive2 * b2) / sumB;

        float3 blendNormal = normalize(normalWorld1 * b1 + normalWorld2 * b2);

        // Blended color
        float4 modelColor = modelConstants.modelColor;
        float4 diffuseColor = float4(blendAlbedo, 1.0) * modelColor;
        clip(diffuseColor.a < 0.01f);

        SurfaceData surf = MakeDefaultSurfaceData();
        surf.Albedo    = diffuseColor.rgb;
        surf.Normal    = blendNormal;
        surf.Metallic  = blendMetallic;
        surf.Roughness = blendRoughness;

        float3 directLighting = float3(0.f, 0.f, 0.f);
        CALC_TOTAL_PBR_LIGHT(directLighting, surf, input.worldPos);

        float3 ambient = float3(0.02, 0.02, 0.02) * diffuseColor.rgb * blendAO;
        float3 color = ambient + directLighting + blendEmissive;
        color = ACESFilm(color);
        color = pow(color, 1.0/2.2);

        return float4(color, 1.0);
    }


}
