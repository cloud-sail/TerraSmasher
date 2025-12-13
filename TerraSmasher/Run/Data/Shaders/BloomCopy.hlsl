  //-----------------------------------------------------------------------------------------------
  // BloomCopy.hlsl
  // Pixel-level copy: EmissiveRT -> Mip0
  // - Auto format transform
  // - Apply threshold filtering
  //

struct BloomCopyResources
{
    uint inputTextureSRV;
    uint outputTextureUAV;

    float bloomThreshold;
};

float3 ApplyThreshold_Hard(float3 color, float threshold)
{
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    
    float brightness = max(0.0, luminance - threshold);
    
    return color * (brightness / (luminance + 0.0001));
}

float3 ApplyThreshold_Soft(float3 color, float threshold)
{
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    float knee = threshold * 0.5;
    
    float brightness = 0.0;
    if (luminance < threshold - knee)
    {
        brightness = 0.0;
    }
    else if (luminance < threshold + knee)
    {
        float x = luminance - threshold + knee;
        brightness = x * x / (4.0 * knee);
    }
    else
    {
        brightness = luminance - threshold;
    }
    
    return color * (brightness / max(luminance, 0.0001));
}

float3 ApplyThreshold_Soft_UE(float3 color, float threshold)
{
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    
    float knee = threshold * 0.5;
    float soft = luminance - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.0001);
    
    float contribution = max(soft, luminance - threshold);
    
    return color * (contribution / (luminance + 0.0001));
}

//-----------------------------------------------------------------------------------------------
ConstantBuffer<BloomCopyResources> renderResources : register(b0);

//-----------------------------------------------------------------------------------------------
[numthreads(8, 8, 1)]
void ComputeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    Texture2D<float4> inputTexture = ResourceDescriptorHeap[renderResources.inputTextureSRV];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[renderResources.outputTextureUAV];

    uint2 outputDim;
    outputTexture.GetDimensions(outputDim.x, outputDim.y);

    if (dispatchThreadID.x >= outputDim.x || dispatchThreadID.y >= outputDim.y)
    {
        return;
    }

    float3 color = inputTexture[dispatchThreadID.xy].rgb;

    color = ApplyThreshold_Soft_UE(color, renderResources.bloomThreshold);

    outputTexture[dispatchThreadID.xy] = float4(color, 1.0);
}
