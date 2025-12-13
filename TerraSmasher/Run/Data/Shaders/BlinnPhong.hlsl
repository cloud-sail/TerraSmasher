#include "Common/Utils.hlsli"
#include "Common/ShaderConstants.hlsli"
#include "Common/Resources.hlsli"
#include "Common/StaticSampler.hlsli"
#include "Common/Math.hlsli"

//------------------------------------------------------------------------------------------------
struct vs_input_t
{
	float3 modelPosition : POSITION;
	float4 color : COLOR;
	float2 uv : TEXCOORD;
	float3 modelTangent : TANGENT;
	float3 modelBitangent : BITANGENT;
	float3 modelNormal : NORMAL;
};

//------------------------------------------------------------------------------------------------
struct v2p_t
{
	float4 clipPosition		: SV_Position;
	float4 color			: COLOR;
	float2 uv				: TEXCOORD;
	float3 worldPos			: WORLD_POSITION;
	float4 worldTangent		: WORLD_TANGENT;
	float4 worldBitangent	: WORLD_BITANGENT;
	float4 worldNormal		: WORLD_NORMAL;
	float4 modelTangent		: MODEL_TANGENT;
	float4 modelBitangent	: MODEL_BITANGENT;
	float4 modelNormal		: MODEL_NORMAL;
};

ConstantBuffer<BlinnPhongRenderResources> renderResources : register(b0);

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

	float4 worldTangent = mul(modelConstants.modelToWorldTransform, float4(input.modelTangent, 0.0f));
	float4 worldBitangent = mul(modelConstants.modelToWorldTransform, float4(input.modelBitangent, 0.0f));
	float4 worldNormal = mul(modelConstants.modelToWorldTransform, float4(input.modelNormal, 0.0f));

	v2p_t v2p;
	v2p.clipPosition = clipPosition;
	v2p.color = input.color;
	v2p.uv = input.uv;
	v2p.worldPos = worldPosition.xyz;
	v2p.worldTangent = worldTangent;
	v2p.worldBitangent = worldBitangent;
	v2p.worldNormal = worldNormal;
	v2p.modelTangent = float4(input.modelTangent, 0.0f);
	v2p.modelBitangent = float4(input.modelBitangent, 0.0f);
	v2p.modelNormal = float4(input.modelNormal, 0.0f);
	return v2p;
}

//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    ConstantBuffer<EngineConstants> engineConstants = ResourceDescriptorHeap[renderResources.engineConstantsIndex];
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];
    ConstantBuffer<ModelConstants> modelConstants = ResourceDescriptorHeap[renderResources.modelConstantsIndex];
    ConstantBuffer<LightConstants> lightConstants = ResourceDescriptorHeap[renderResources.lightConstantsIndex];

    Texture2D<float4> diffuseTexture = ResourceDescriptorHeap[renderResources.diffuseTextureIndex];
    Texture2D<float4> normalTexture = ResourceDescriptorHeap[renderResources.normalTextureIndex];
    Texture2D<float4> sgeTexture = ResourceDescriptorHeap[renderResources.specGlossEmitTextureIndex];

	float2 uvCoords = input.uv;

	float4 diffuseTexel         = diffuseTexture.Sample(s_linearWrap, uvCoords);
	float4 normalTexel	        = normalTexture.Sample(s_linearWrap, uvCoords);
	float4 specGlossEmitTexel	= sgeTexture.Sample(s_linearWrap, uvCoords);

	float4 surfaceColor = input.color;
	float4 modelColor = modelConstants.modelColor;

	float4 diffuseColor = diffuseTexel * surfaceColor * modelColor;
	clip(diffuseColor.a < 0.01f);

	float specularity	= specGlossEmitTexel.r;
	float glossiness	= specGlossEmitTexel.g;
	float emissive		= specGlossEmitTexel.b;

	// float3 cameraWorldPosition = GetCameraWorldPosition(c_worldToCameraTransform);
	float3 cameraWorldPosition = cameraConstants.cameraWorldPosition;

	// Calculate worldNormal
	float3 pixelNormalTBNSpace = normalize(DecodeRGBToXYZ(normalTexel.rgb));

	//float3 surfaceNormalWorldSpace = normalize(input.worldNormal.xyz);
	//float3 surfaceTangentWorldSpace = normalize(input.worldTangent.xyz);
	//float3 surfaceBitangentWorldSpace = normalize(input.worldBitangent.xyz);

	float3 surfaceNormalWorldSpace = normalize(input.worldNormal.xyz);
	float3 surfaceTangentWorldSpace = normalize(input.worldTangent.xyz - dot(input.worldTangent.xyz, surfaceNormalWorldSpace) * surfaceNormalWorldSpace);
	float3 surfaceBitangentWorldSpace = cross(surfaceNormalWorldSpace, surfaceTangentWorldSpace); // reset the handness?

	float3x3 tbnToWorld = float3x3(surfaceTangentWorldSpace, surfaceBitangentWorldSpace, surfaceNormalWorldSpace);
	
	float3 pixelNormalWorldSpace = mul(pixelNormalTBNSpace, tbnToWorld);

	float3 N = normalize(pixelNormalWorldSpace);
	float3 V = normalize(cameraWorldPosition - input.worldPos);
	float specularExponent = RangeMapClamped(glossiness, 0.f, 1.f, 1.f, 32.f);

	float3 totalDiffuseLight = float3(0.f, 0.f, 0.f);
	float3 totalSpecularLight = float3(0.f, 0.f, 0.f);

	//-----------------------------------------------------------------------------------------------------------
	// Sunlight
	//-----------------------------------------------------------------------------------------------------------
	{
		float sunAmbience = 0.2f;
		float3 L = -lightConstants.sunNormal;

		// diffuse lighting with progressive ambience
		float lightStrength = lightConstants.sunColor.a * saturate(RangeMap(dot(L, N), -sunAmbience, 1.f, 0.f, 1.f));
		totalDiffuseLight += (lightStrength * lightConstants.sunColor.rgb);

		// specular lighting
		float3 H = normalize(V + L);
		float NDotH = saturate(dot(N, H)); // SpecularDot
		float specularStength = glossiness * specularity * lightConstants.sunColor.a * pow(NDotH, specularExponent);
		totalSpecularLight += specularStength * lightConstants.sunColor.rgb;
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

		float lightStrength = fallOff * penumbra * lightBrightness * saturate(RangeMap(dot(L, N), -ambience, 1.f, 0.f, 1.f));
		totalDiffuseLight += lightStrength * lightColor;

		float3 H = normalize(V + L);
		float NDotH = saturate(dot(N, H)); // SpecularDot
		float specularStength = glossiness * specularity * lightBrightness * fallOff * penumbra * pow(NDotH, specularExponent);
		totalSpecularLight += specularStength * lightColor;
	}

	//-----------------------------------------------------------------------------------------------------------
	// Emissive (glow)
	//-----------------------------------------------------------------------------------------------------------
	float3 emissiveLight = diffuseTexel.rgb * emissive;

	//-----------------------------------------------------------------------------------------------------------
	// Final lighting composite
	//-----------------------------------------------------------------------------------------------------------
	float3 finalRGB = saturate(totalDiffuseLight) * diffuseColor.rgb + totalSpecularLight + emissiveLight;

	float4 finalColor = float4(finalRGB, diffuseColor.a);
	
	return finalColor;
}
