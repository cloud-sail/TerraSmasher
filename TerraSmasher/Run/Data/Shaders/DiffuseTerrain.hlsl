#include "Common/Utils.hlsli"
#include "Common/ShaderConstants.hlsli"
#include "Common/Resources.hlsli"
#include "Common/Lighting.hlsli"

//------------------------------------------------------------------------------------------------
// Only For Test, opaque white model

//------------------------------------------------------------------------------------------------
struct vs_input_t
{
    float3 modelPosition : POSITION;
    float3 modelNormal   : NORMAL;
    uint2  matdens      : MATDENS; // x: materialID, y: density
};

//------------------------------------------------------------------------------------------------
struct v2p_t
{
    float4 clipPosition  : SV_Position;
    float3 worldPos      : WORLD_POSITION;
    float3 worldNormal   : WORLD_NORMAL;
    nointerpolation uint materialID : MATERIALID;
    nointerpolation uint density    : DENSITY;
};

//----------------------------------------------------------------------------------------------------

struct DiffuseTerrainRenderResources
{
    uint engineConstantsIndex;
    uint cameraConstantsIndex;
    uint modelConstantsIndex;
    uint lightConstantsIndex;
};


ConstantBuffer<DiffuseTerrainRenderResources> renderResources : register(b0);


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

    float4 worldNormal = mul(modelConstants.modelToWorldTransform, float4(input.modelNormal, 0.0f));

    v2p_t v2p;
    v2p.clipPosition = clipPosition;
    v2p.worldPos     = worldPosition.xyz;
    v2p.worldNormal  = normalize(worldNormal.xyz);
    v2p.materialID   = input.matdens.x;
    v2p.density      = input.matdens.y;
    return v2p;
}

float4 PixelMain(v2p_t input) : SV_Target0
{
    ConstantBuffer<EngineConstants> engineConstants = ResourceDescriptorHeap[renderResources.engineConstantsIndex];
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];
    ConstantBuffer<ModelConstants> modelConstants = ResourceDescriptorHeap[renderResources.modelConstantsIndex];
    ConstantBuffer<LightConstants> lightConstants = ResourceDescriptorHeap[renderResources.lightConstantsIndex];

    float4 diffuseColor = float4(1.f, 1.f, 1.f, 1.f);

    float3 normalWorldSpace = normalize(input.worldNormal.xyz);
    if (engineConstants.debugInt == 1 || engineConstants.debugInt == 3)
    {
        // Low Poly
        normalWorldSpace = -normalize(cross(ddx(input.worldPos), ddy(input.worldPos)));
    }

    SurfaceData surf = MakeDefaultSurfaceData();
    surf.Albedo = diffuseColor.rgb;
    surf.Normal = normalWorldSpace;

    float3 totalLight = float3(0.f, 0.f, 0.f); // Result

	CALC_TOTAL_DIFFUSE_LIGHT(totalLight, surf, input.worldPos);

    float3 finalRGB = saturate(totalLight);

    float4 finalColor = float4(finalRGB, diffuseColor.a);
    if (engineConstants.debugInt == 2 || engineConstants.debugInt == 3)
    {
        finalColor.rgb = EncodeXYZToRGB(normalWorldSpace);
    }

	clip(finalColor.a - 0.01f);
	return finalColor;
}