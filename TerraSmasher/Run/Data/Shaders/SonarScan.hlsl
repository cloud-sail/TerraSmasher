#include "Common/ShaderConstants.hlsli"

struct SonarScanResources
{
    uint    cameraConstantsIndex;
    uint    sceneTextureSRV;
    uint    depthTextureSRV;
    uint    outputTextureUAV;

    float3  sonarWorldCenter;
    float   padding0;

    float   innerRadius;
    float   ringThickness;
    float2  padding1;

    float4  sonarColor;

};


//-----------------------------------------------------------------------------------------------
// input [0, 1] -> output  [0, 1]
// 0 = innerRadius, 1 = innerRadius + ringThickness
float GetSonarWaveIntensity(float x)
{    
    float amplitude = x;             
    float frequency = 3.3;           
    float wave = sin(x * frequency * 6.28318530718);  
    
    float result = amplitude * wave;  
    result += 0.4;                    
    
    return saturate(result);         
}

float3 DiminishingAdd(float3 a, float3 b)
{
    return 1.0f - (1.0f - a) * (1.0f - b);
}

//-----------------------------------------------------------------------------------------------
ConstantBuffer<SonarScanResources> renderResources : register(b0);

//-----------------------------------------------------------------------------------------------
// Composite Main
//
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float4> sceneTexture = ResourceDescriptorHeap[renderResources.sceneTextureSRV];
    Texture2D<float> depthTexture = ResourceDescriptorHeap[renderResources.depthTextureSRV];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[renderResources.outputTextureUAV];

    uint2 outputDim;
    outputTexture.GetDimensions(outputDim.x, outputDim.y);

    if (dispatchThreadID.x >= outputDim.x || dispatchThreadID.y >= outputDim.y)
        return;

    float3 sceneColor = sceneTexture[dispatchThreadID.xy].rgb;


    float depth = depthTexture[dispatchThreadID.xy];

    float2 uv = (dispatchThreadID.xy + 0.5) / float2(outputDim);
    float2 ndc = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;

    float4 clipPos = float4(ndc, depth, 1.0);
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];
    float4 worldPos = mul(cameraConstants.clipToWorldTransform, clipPos);
    worldPos /= worldPos.w;

    float distanceToCenter = length(worldPos.xyz - renderResources.sonarWorldCenter);

    float distanceFromInner = distanceToCenter - renderResources.innerRadius;

    float normalizedDistance = distanceFromInner / renderResources.ringThickness;

    float3 finalColor = sceneColor;

    if (normalizedDistance >= 0.0 && normalizedDistance <= 1.0 && renderResources.innerRadius >= 0.0)
    {
        float intensity = GetSonarWaveIntensity(normalizedDistance);

        float3 sonarContribution = renderResources.sonarColor.rgb * intensity;

        finalColor = DiminishingAdd(sceneColor, sonarContribution);
    }

    outputTexture[dispatchThreadID.xy] = float4(finalColor, 1.0);
}
