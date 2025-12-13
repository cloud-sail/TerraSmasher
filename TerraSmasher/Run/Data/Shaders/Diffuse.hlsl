#include "Common/Utils.hlsli"
#include "Common/ShaderConstants.hlsli"
#include "Common/Resources.hlsli"
#include "Common/Math.hlsli"
#include "Common/Lighting.hlsli"

//------------------------------------------------------------------------------------------------
struct vs_input_t
{
	float3 modelPosition : POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
};

//------------------------------------------------------------------------------------------------
struct v2p_t
{
	float4 clipPosition : SV_Position;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
    float3 worldPos	: WORLD_POSITION;
};


ConstantBuffer<DiffuseRenderResources> renderResources : register(b0);

//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];
    ConstantBuffer<ModelConstants> modelConstants = ResourceDescriptorHeap[renderResources.modelConstantsIndex];

	float4 modelPosition = float4(input.modelPosition, 1);
	float4 worldPosition = mul(modelConstants.modelToWorldTransform, modelPosition);
	float4 cameraPosition = mul(cameraConstants.worldToCameraTransform, worldPosition);
	float4 renderPosition = mul(cameraConstants.cameraToRenderTransform, cameraPosition);
	float4 clipPosition = mul(cameraConstants.renderToClipTransform, renderPosition);

	v2p_t v2p;
	v2p.clipPosition = clipPosition;
	v2p.color = input.color;
	v2p.uv = input.uv;
    v2p.worldPos = worldPosition.xyz;
	return v2p;
}

//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];
    ConstantBuffer<ModelConstants> modelConstants = ResourceDescriptorHeap[renderResources.modelConstantsIndex];
    ConstantBuffer<LightConstants> lightConstants = ResourceDescriptorHeap[renderResources.lightConstantsIndex];


    Texture2D<float4> diffuseTexture = ResourceDescriptorHeap[renderResources.diffuseTextureIndex];
	SamplerState diffuseSampler = SamplerDescriptorHeap[renderResources.diffuseSamplerIndex];
    float4 diffuseTexel = diffuseTexture.Sample(diffuseSampler, input.uv);
	float4 surfaceColor = input.color;
	float4 modelColor = modelConstants.modelColor;
    float4 diffuseColor = diffuseTexel * surfaceColor * modelColor;

    float3 normalWorldSpace = -normalize(cross(ddx(input.worldPos), ddy(input.worldPos)));

    SurfaceData surf = MakeDefaultSurfaceData();
    surf.Albedo = diffuseColor.rgb;
    surf.Normal = normalWorldSpace;

	float3 totalLight = float3(0.f, 0.f, 0.f); // Result

	CALC_TOTAL_DIFFUSE_LIGHT(totalLight, surf, input.worldPos);
	// //-----------------------------------------------------------------------------------------------------------
	// // Sunlight
	// //-----------------------------------------------------------------------------------------------------------
	// {
	// 	float sunAmbience = 0.2f;
    //     float3 sunDir = -lightConstants.sunNormal;
    //     float3 sunColor = lightConstants.sunColor.rgb;
    //     float  sunAtten = lightConstants.sunColor.a;

	// 	totalLight += DiffuseLighting(surf, sunDir, sunColor, sunAtten, sunAmbience);
	// }

    // //-----------------------------------------------------------------------------------------------------------
	// // Point & Spot Lights
	// //-----------------------------------------------------------------------------------------------------------
	// for (int lightIndex = 0; lightIndex < lightConstants.numLights; ++lightIndex)
	// {
	// 	float3 lightPos 		= lightConstants.lightArray[lightIndex].worldPosition;
	// 	float3 lightColor 		= lightConstants.lightArray[lightIndex].color.rgb;
	// 	float  ambience			= lightConstants.lightArray[lightIndex].ambience;
	// 	float  lightBrightness	= lightConstants.lightArray[lightIndex].color.a;
	// 	float3 spotForward		= lightConstants.lightArray[lightIndex].spotForward;
	// 	float  innerRadius		= lightConstants.lightArray[lightIndex].innerRadius;
	// 	float  outerRadius		= lightConstants.lightArray[lightIndex].outerRadius;
	// 	float  innerPenumbraDot	= lightConstants.lightArray[lightIndex].innerDotThreshold;
	// 	float  outerPenumbraDot	= lightConstants.lightArray[lightIndex].outerDotThreshold;

    //     float dist = length(lightPos - input.worldPos);
    //     float3 L = normalize(lightPos - input.worldPos);

    //     float fallOff = saturate(RangeMap(dist, innerRadius, outerRadius, 1.f, 0.f));
    //     fallOff = SmoothStep3(fallOff);

    //     float penumbra = saturate(RangeMap(dot(-L, spotForward), innerPenumbraDot, outerPenumbraDot, 1.f, 0.f));
    //     penumbra = SmoothStep3(penumbra);

    //     float lightAtten = fallOff * penumbra * lightBrightness;

    //     totalLight += DiffuseLighting(surf, L, lightColor, lightAtten, ambience);
	// }

    float3 finalRGB = saturate(totalLight);

    float4 finalColor = float4(finalRGB, diffuseColor.a);

	clip(finalColor.a - 0.01f);
	return finalColor;
}
