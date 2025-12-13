#pragma once

static const uint INVALID_INDEX = uint(-1); // uint32_t(-1) 4294967295

// Please ensure the engine will set default texture to this index
// if the index is invalid use the index below


// static const uint DEFAULT_DIFFUSE_TEXTURE_INDEX = 0; 
// static const uint DEFAULT_NORMAL_TEXTURE_INDEX = 1; 

// uint GetSafeIndex(uint index, uint defaultIndex)
// {
//     return (index != INVALID_INDEX) ? index : defaultIndex;
// }

/**
uint safeIndex = GetSafeIndex(normalTextureIndex, DEFAULT_NORMAL_TEXTURE_INDEX);
Texture2D<float4> normalTexture = ResourceDescriptorHeap[safeIndex];

or Engine Must provide a valid index, you can query the default index and set the index correctly

if sampler index is invalid, use static sampler
*/

float4 GetSafeAlbedo(uint texIndex, SamplerState samp, float2 uvCoords)
{
    if (texIndex == INVALID_INDEX)
    {
        return float4(1.f, 1.f, 1.f, 1.f);
    }
    Texture2D<float4> tex = ResourceDescriptorHeap[texIndex];
    return tex.Sample(samp, uvCoords);
}

float3 GetSafeNormal(uint texIndex, SamplerState samp, float2 uvCoords)
{
    if (texIndex == INVALID_INDEX)
    {
        return float3(0.f, 0.f, 1.f);
    }
    Texture2D<float4> tex = ResourceDescriptorHeap[texIndex];
    float4 normalTexel = tex.Sample(samp, uvCoords);

    return ((normalTexel.rgb * 2.0) - 1.0);
}

float2 GetSafeMetallicRoughness(uint texIndex, SamplerState samp, float2 uvCoords)
{
    if (texIndex == INVALID_INDEX)
    {
        return float2(0.f, 0.5f);
    }
    Texture2D<float4> tex = ResourceDescriptorHeap[texIndex];
    float4 texel = tex.Sample(samp, uvCoords);

    return texel.bg;
}


float GetSafeOcclusion(uint texIndex, SamplerState samp, float2 uvCoords)
{
    if (texIndex == INVALID_INDEX)
    {
        return 1.f;
    }
    Texture2D<float4> tex = ResourceDescriptorHeap[texIndex];
    float4 texel = tex.Sample(samp, uvCoords);

    return texel.r;
}

float3 GetSafeEmissive(uint texIndex, SamplerState samp, float2 uvCoords)
{
    if (texIndex == INVALID_INDEX)
    {
        return float3(0.f, 0.f, 0.f);
    }
    Texture2D<float4> tex = ResourceDescriptorHeap[texIndex];
    float4 texel = tex.Sample(samp, uvCoords);

    return texel.rgb;
}

float GetSafeHeight(uint texIndex, SamplerState samp, float2 uvCoords)
{
    if (texIndex == INVALID_INDEX)
    {
        return 0.5f;
    }
    Texture2D<float4> tex = ResourceDescriptorHeap[texIndex];
    float4 texel = tex.Sample(samp, uvCoords);

    return texel.a;
}

// AO Roughness Metallic Height
float4 GetSafeORMH(uint texIndex, SamplerState samp, float2 uvCoords)
{
    if (texIndex == INVALID_INDEX)
    {
        return float4(1.f, 0.5f, 0.f, 0.5f);
    }
    Texture2D<float4> tex = ResourceDescriptorHeap[texIndex];
    float4 texel = tex.Sample(samp, uvCoords);

    return texel;
}