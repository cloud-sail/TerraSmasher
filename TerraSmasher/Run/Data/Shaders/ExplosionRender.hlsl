#include "Common/ShaderConstants.hlsli"

//-----------------------------------------------------------------------------------------------
// Instance data structure (must match C++ ExplosionInstanceData)
struct ExplosionInstanceData
{
    float3 center;
    float  radius;
    float4 color;
    float  intensity;
    float  fresnelPower;
    float  alpha;        // normalized lifetime [0, 1]
    float  padding;
};

//-----------------------------------------------------------------------------------------------
// Bindless resources
struct ExplosionRenderResources
{
    uint explosionBufferIndex;     // StructuredBuffer<ExplosionInstanceData>
    uint cameraConstantsIndex;
};

ConstantBuffer<ExplosionRenderResources> renderResources : register(b0);

//-----------------------------------------------------------------------------------------------
// Vertex input (Vertex_PCU from the shared sphere VB)
struct vs_input_t
{
    float3 localPosition : POSITION;
    float4 color         : COLOR;
    float2 uv            : TEXCOORD;
};

//-----------------------------------------------------------------------------------------------
// Vertex to Pixel
struct v2p_t
{
    float4 clipSpacePosition : SV_Position;
    float3 worldPosition     : WORLD_POSITION;
    float3 worldNormal       : NORMAL;
    float4 color             : COLOR;
    float  intensity         : TEXCOORD0;
    float  fresnelPower      : TEXCOORD1;
    float  alpha             : TEXCOORD2;
};

//-----------------------------------------------------------------------------------------------
// Vertex Shader
v2p_t VertexMain(vs_input_t input, uint instanceID : SV_InstanceID)
{
    StructuredBuffer<ExplosionInstanceData> instanceBuffer = ResourceDescriptorHeap[renderResources.explosionBufferIndex];
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];

    ExplosionInstanceData instance = instanceBuffer[instanceID];

    v2p_t output;

    // Unit sphere vertex position IS the outward normal
    float3 normal = normalize(input.localPosition);

    // Scale by current radius and translate to world center
    float3 worldPos = normal * instance.radius + instance.center;

    // Transform to clip space
    float4 worldPos4 = float4(worldPos, 1.0);
    float4 cameraSpacePos = mul(cameraConstants.worldToCameraTransform, worldPos4);
    float4 renderSpacePos = mul(cameraConstants.cameraToRenderTransform, cameraSpacePos);
    float4 clipSpacePos   = mul(cameraConstants.renderToClipTransform, renderSpacePos);

    output.clipSpacePosition = clipSpacePos;
    output.worldPosition     = worldPos;
    output.worldNormal       = normal;
    output.color             = instance.color;
    output.intensity         = instance.intensity;
    output.fresnelPower      = instance.fresnelPower;
    output.alpha             = instance.alpha;

    return output;
}

//-----------------------------------------------------------------------------------------------
// Pixel Shader - Energy ball with fresnel
float4 PixelMain(v2p_t input) : SV_Target0
{
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];

    float3 N = normalize(input.worldNormal);
    float3 V = normalize(cameraConstants.cameraWorldPosition - input.worldPosition);

    // Fresnel: abs() so both front and back faces have consistent rim lighting
    float NdotV = abs(dot(N, V));
    float fresnel = pow(1.0 - NdotV, input.fresnelPower);

    // Energy ball: soft core glow + strong rim glow
    float coreGlow = 0.15;
    float rimGlow  = 1.0;
    float brightness = coreGlow + rimGlow * fresnel;

    // Fade out over lifetime (1 at birth -> 0 at death)
    float lifeFade = saturate(1.0 - input.alpha);

    brightness *= input.intensity * lifeFade;

    float3 finalColor = input.color.rgb * brightness;
    return float4(finalColor, 1.0);
}
