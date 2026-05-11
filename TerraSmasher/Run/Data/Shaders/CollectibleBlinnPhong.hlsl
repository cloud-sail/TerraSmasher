#include "Common/Utils.hlsli"
#include "Common/ShaderConstants.hlsli"
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


struct CollectibleRenderResources
{
    uint diffuseTextureIndex;
    uint normalTextureIndex;
    uint specGlossEmitTextureIndex;
    uint sceneDepthTextureIndex;

    uint engineConstantsIndex;
    uint cameraConstantsIndex;
    uint modelConstantsIndex;
    uint lightConstantsIndex;

    float sonarHighlightAlpha;
    float3 padding;
};


ConstantBuffer<CollectibleRenderResources> renderResources : register(b0);

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
// Standard BlinnPhong lighting calculation
float3 ComputeBlinnPhongLighting(v2p_t input, float4 diffuseColor, float specularity, float glossiness, float emissive, float4 diffuseTexel)
{
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];
    ConstantBuffer<LightConstants> lightConstants = ResourceDescriptorHeap[renderResources.lightConstantsIndex];

    Texture2D<float4> normalTexture = ResourceDescriptorHeap[renderResources.normalTextureIndex];

	float4 normalTexel = normalTexture.Sample(s_linearWrap, input.uv);

	float3 pixelNormalTBNSpace = normalize(DecodeRGBToXYZ(normalTexel.rgb));
	float3 surfaceNormalWorldSpace = normalize(input.worldNormal.xyz);
	float3 surfaceTangentWorldSpace = normalize(input.worldTangent.xyz - dot(input.worldTangent.xyz, surfaceNormalWorldSpace) * surfaceNormalWorldSpace);
	float3 surfaceBitangentWorldSpace = cross(surfaceNormalWorldSpace, surfaceTangentWorldSpace);

	float3x3 tbnToWorld = float3x3(surfaceTangentWorldSpace, surfaceBitangentWorldSpace, surfaceNormalWorldSpace);
	float3 pixelNormalWorldSpace = mul(pixelNormalTBNSpace, tbnToWorld);

	float3 N = normalize(pixelNormalWorldSpace);
	float3 V = normalize(cameraConstants.cameraWorldPosition - input.worldPos);
	float specularExponent = RangeMapClamped(glossiness, 0.f, 1.f, 1.f, 32.f);

	float3 totalDiffuseLight = float3(0.f, 0.f, 0.f);
	float3 totalSpecularLight = float3(0.f, 0.f, 0.f);

	// Sunlight
	{
		float sunAmbience = 0.2f;
		float3 L = -lightConstants.sunNormal;
		float lightStrength = lightConstants.sunColor.a * saturate(RangeMap(dot(L, N), -sunAmbience, 1.f, 0.f, 1.f));
		totalDiffuseLight += (lightStrength * lightConstants.sunColor.rgb);
		float3 H = normalize(V + L);
		float NDotH = saturate(dot(N, H));
		float specularStength = glossiness * specularity * lightConstants.sunColor.a * pow(NDotH, specularExponent);
		totalSpecularLight += specularStength * lightConstants.sunColor.rgb;
	}

	// Point & Spot Lights
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
		float NDotH = saturate(dot(N, H));
		float specularStength = glossiness * specularity * lightBrightness * fallOff * penumbra * pow(NDotH, specularExponent);
		totalSpecularLight += specularStength * lightColor;
	}

	float3 emissiveLight = diffuseTexel.rgb * emissive;
	float3 finalRGB = saturate(totalDiffuseLight) * diffuseColor.rgb + totalSpecularLight + emissiveLight;
	return finalRGB;
}

//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];

    Texture2D<float4> diffuseTexture = ResourceDescriptorHeap[renderResources.diffuseTextureIndex];
    Texture2D<float4> sgeTexture = ResourceDescriptorHeap[renderResources.specGlossEmitTextureIndex];

	float4 diffuseTexel = diffuseTexture.Sample(s_linearWrap, input.uv);
	float4 specGlossEmitTexel = sgeTexture.Sample(s_linearWrap, input.uv);

	float4 surfaceColor = input.color;
	ConstantBuffer<ModelConstants> modelConstants = ResourceDescriptorHeap[renderResources.modelConstantsIndex];
	float4 modelColor = modelConstants.modelColor;
	float4 diffuseColor = diffuseTexel * surfaceColor * modelColor;
	clip(diffuseColor.a - 0.01f);

	float specularity	= specGlossEmitTexel.r;
	float glossiness	= specGlossEmitTexel.g;
	float emissive		= specGlossEmitTexel.b;

	// Compare depth with scene depth buffer (using SV_Position pixel coords for Load)
	Texture2D<float> sceneDepthTexture = ResourceDescriptorHeap[renderResources.sceneDepthTextureIndex];
	int2 pixelCoord = int2(input.clipPosition.xy);
	float sceneDepth = sceneDepthTexture.Load(int3(pixelCoord, 0));
	float myDepth = input.clipPosition.z; // current fragment depth in [0,1]

	bool isOccluded = (myDepth > sceneDepth); // depth test: closer = smaller depth value

	float sonarAlpha = renderResources.sonarHighlightAlpha;

	if (!isOccluded)
	{
		// Not occluded: render with standard BlinnPhong lighting
		float3 litColor = ComputeBlinnPhongLighting(input, diffuseColor, specularity, glossiness, emissive, diffuseTexel);
		return float4(litColor, 1.0f);
	}
	else
	{
		// Occluded
		if (sonarAlpha <= 0.0f)
		{
			// No sonar effect: discard this pixel (fully transparent)
			return float4(0.0f, 0.0f, 0.0f, 0.0f);
		}
		else
		{
			// Sonar highlight: render with fresnel magenta glow
			float3 N = normalize(input.worldNormal.xyz);
			float3 V = normalize(cameraConstants.cameraWorldPosition - input.worldPos);
			float NdotV = saturate(dot(N, V));

			// Fresnel: edges (NdotV near 0) are brighter/whiter, center is more magenta
			// float fresnel = pow(1.0f - NdotV, 3.0f);
			float fresnel = 1.0f - NdotV;

			float3 magenta = float3(0.8f, 0.0f, 0.8f);
			float3 white = float3(1.0f, 1.0f, 1.0f);
			float3 sonarColor = lerp(magenta, white, fresnel);

			// Alpha fades with the sonar timer
			float alpha = sonarAlpha * (0.3f + 0.7f * fresnel); // edges more visible
			return float4(sonarColor, alpha);
		}
	}
}
