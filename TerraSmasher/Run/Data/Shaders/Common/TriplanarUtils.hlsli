#pragma once
#include "Math.hlsli"

float3 GetTriplanarWeights(float3 normal, float sharpness)
{
    float3 blend = pow(abs(normal), sharpness); // default sharpness = 1
    float sum = blend.x + blend.y + blend.z + 0.00001f; // prevent divide by zero
    return blend / sum;
}

float4 SampleTriplanar(
    float3 worldPos, float3 worldNormal, 
    float uvScale, float sharpness, 
    Texture2D tex, SamplerState samp)
{
    float3 weights = GetTriplanarWeights(worldNormal, sharpness);
    
    // SD Engine Rules
    // different to shapes in MathUnitTests
    float2 uvX = worldPos.yz / uvScale;
    float2 uvY = worldPos.xz / uvScale;
    float2 uvZ = worldPos.xy / uvScale;

    float3 axisSign = sign(worldNormal);

    uvX.x *= axisSign.x;
    uvY.x *= -axisSign.y;
    uvZ.x *= axisSign.z;

    // Sample
    float4 colorX = tex.Sample(samp, uvX);
    float4 colorY = tex.Sample(samp, uvY);
    float4 colorZ = tex.Sample(samp, uvZ);

    float4 result = colorX * weights.x +
                    colorY * weights.y +
                    colorZ * weights.z;
    return result;
}

float3 SampleTriplanarNormal(
    float3 worldPos, float3 worldNormal, 
    float uvScale, float sharpness, 
    Texture2D tex, SamplerState samp)
{
    float3 weights = GetTriplanarWeights(worldNormal, sharpness);

    // SD Engine Rules
    float2 uvX = worldPos.yz / uvScale;
    float2 uvY = worldPos.xz / uvScale;
    float2 uvZ = worldPos.xy / uvScale;

    float3 axisSign = sign(worldNormal);

    uvX.x *= axisSign.x;
    uvY.x *= -axisSign.y;
    uvZ.x *= axisSign.z;

    // Sample (tangent space normal)
    float3 tnormalX = DecodeRGBToXYZ(tex.Sample(samp, uvX).xyz);
    float3 tnormalY = DecodeRGBToXYZ(tex.Sample(samp, uvY).xyz);
    float3 tnormalZ = DecodeRGBToXYZ(tex.Sample(samp, uvZ).xyz);

    tnormalX.x *= axisSign.x;
    tnormalY.x *= -axisSign.y;
    tnormalZ.x *= axisSign.z;

    // Tangent space normal is a small turbulance on the world normal
    // UDN blend
    // tnormalX = float3(tnormalX.xy + worldNormal.yz, worldNormal.x);
    // tnormalY = float3(tnormalY.xy + worldNormal.xz, worldNormal.y);
    // tnormalZ = float3(tnormalZ.xy + worldNormal.xy, worldNormal.z);

    //  Whiteout blend
    tnormalX = float3(tnormalX.xy + worldNormal.yz, abs(tnormalX.z) * worldNormal.x);
    tnormalY = float3(tnormalY.xy + worldNormal.xz, abs(tnormalY.z) * worldNormal.y);
    tnormalZ = float3(tnormalZ.xy + worldNormal.xy, abs(tnormalZ.z) * worldNormal.z);
    
    float3 result = normalize(
        tnormalX.zxy * weights.x +
        tnormalY.xzy * weights.y +
        tnormalZ.xyz * weights.z
    );

    return result;
}

float3 SampleTriplanarNormalWithBase(
    float3 worldPos, float3 worldNormal, float3 baseNormal, // sample by world normal but perturb the base normal
    float uvScale, float sharpness, 
    Texture2D tex, SamplerState samp)
{
    float3 weights = GetTriplanarWeights(worldNormal, sharpness);

    // SD Engine Rules
    float2 uvX = worldPos.yz / uvScale;
    float2 uvY = worldPos.xz / uvScale;
    float2 uvZ = worldPos.xy / uvScale;

    float3 axisSign = sign(worldNormal);

    uvX.x *= axisSign.x;
    uvY.x *= -axisSign.y;
    uvZ.x *= axisSign.z;

    // Sample (tangent space normal)
    float3 tnormalX = DecodeRGBToXYZ(tex.Sample(samp, uvX).xyz);
    float3 tnormalY = DecodeRGBToXYZ(tex.Sample(samp, uvY).xyz);
    float3 tnormalZ = DecodeRGBToXYZ(tex.Sample(samp, uvZ).xyz);

    tnormalX.x *= axisSign.x;
    tnormalY.x *= -axisSign.y;
    tnormalZ.x *= axisSign.z;

    // Tangent space normal is a small turbulance on the world normal
    // UDN blend
    // tnormalX = float3(tnormalX.xy + worldNormal.yz, worldNormal.x);
    // tnormalY = float3(tnormalY.xy + worldNormal.xz, worldNormal.y);
    // tnormalZ = float3(tnormalZ.xy + worldNormal.xy, worldNormal.z);

    //  Whiteout blend
    tnormalX = float3(tnormalX.xy + baseNormal.yz, abs(tnormalX.z) * baseNormal.x);
    tnormalY = float3(tnormalY.xy + baseNormal.xz, abs(tnormalY.z) * baseNormal.y);
    tnormalZ = float3(tnormalZ.xy + baseNormal.xy, abs(tnormalZ.z) * baseNormal.z);
    
    float3 result = normalize(
        tnormalX.zxy * weights.x +
        tnormalY.xzy * weights.y +
        tnormalZ.xyz * weights.z
    );

    return result;
}
//-----------------------------------------------------------------------------------------------------
// OLD CODES
// float2 MirrorUV(float2 uv, float coord)
// {
//     return (coord < 0.0f) ? float2(-uv.x, uv.y) : uv;
// }

// float4 TriplanarAlbedo(
//     float3 worldPos, float3 worldNormal, 
//     float uvScale, float sharpness, 
//     Texture2D tex, SamplerState samp)
// {
//     float3 weights = GetTriplanarWeights(worldNormal, sharpness);

//     // not sure the uv direction is correct, need more test
//     // assume looking from + to - direction

//     // It is unity coords
//     // float2 uvX = worldPos.zy / uvScale;
//     // float2 uvY = worldPos.xz / uvScale;
//     // float2 uvZ = worldPos.xy / uvScale;

//     // It is SD Engine
//     float2 uvX = worldPos.yz / uvScale;
//     float2 uvY = float2(-worldPos.x, worldPos.z) / uvScale;
//     float2 uvZ = worldPos.xy / uvScale;

//     // fix 
//     uvX = MirrorUV(uvX, worldNormal.x);
//     uvY = MirrorUV(uvY, worldNormal.y);
//     uvZ = MirrorUV(uvZ, worldNormal.z);

//     float4 colorX = tex.Sample(samp, uvX);
//     float4 colorY = tex.Sample(samp, uvY);
//     float4 colorZ = tex.Sample(samp, uvZ);

//     float4 result = colorX * weights.x +
//                     colorY * weights.y +
//                     colorZ * weights.z;
//     return result;
// }