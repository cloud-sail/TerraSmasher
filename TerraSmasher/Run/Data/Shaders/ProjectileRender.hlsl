#include "Common/ShaderConstants.hlsli"

//-----------------------------------------------------------------------------------------------
// Instance data structure (must match C++ struct)
struct ProjectileInstanceData
{
    float3 startPos;
    float  intensity;
    float3 endPos;
    float  thickness;
    float4 color;
};

//-----------------------------------------------------------------------------------------------
// Resources
struct ProjectileRenderResources
{
    uint projectileBufferIndex;    // StructuredBuffer<ProjectileInstanceData>
    uint cameraConstantsIndex;
};

ConstantBuffer<ProjectileRenderResources> renderResources : register(b0);

//-----------------------------------------------------------------------------------------------
// Vertex to Pixel
struct v2p_t
{
    float4 clipSpacePosition : SV_Position;
    float2 uv                : TEXCOORD0;
    float4 color             : COLOR;
    float  intensity         : TEXCOORD1;
};

//-----------------------------------------------------------------------------------------------
// Static lookup table for quad UVs (no vertex buffer needed!)
static const float2 QUAD_UVS[4] = {
    float2(0.0, 0.0),  // 0: Bottom-left (tail left)
    float2(1.0, 0.0),  // 1: Bottom-right (tail right)
    float2(1.0, 1.0),  // 2: Top-right (head right)
    float2(0.0, 1.0)   // 3: Top-left (head left)
};

//-----------------------------------------------------------------------------------------------
// Vertex Shader - Generate billboard quads dynamically
v2p_t VertexMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    StructuredBuffer<ProjectileInstanceData> instanceBuffer = ResourceDescriptorHeap[renderResources.projectileBufferIndex];
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];

    // Get instance data
    ProjectileInstanceData instance = instanceBuffer[instanceID];

    v2p_t output;

    // Calculate beam direction and length
    float3 beamDir = instance.endPos - instance.startPos;
    float beamLength = length(beamDir);

    // Handle degenerate case (very short beam or first frame)
    if (beamLength < 0.01f)
    {
        beamDir = float3(0, 0, 1);          // Default forward
        beamLength = instance.thickness;    // Square Shape
    }
    else
    {
        beamDir = beamDir / beamLength;  // Normalize
    }

    // Build billboard coordinate system (facing camera)
    float3 cameraPosition = cameraConstants.cameraWorldPosition;
    float3 toCamera = normalize(cameraPosition - instance.startPos);

    float3 right = normalize(cross(beamDir, toCamera));

    // Handle degenerate case (beam pointing at camera)
    float alignment = abs(dot(beamDir, toCamera));
    if (alignment > 0.999f)
    {
        float3 fallback = abs(beamDir.y) < 0.999f ? float3(0, 1, 0) : float3(1, 0, 0);
        right = normalize(cross(beamDir, fallback));
    }

    // Map vertexID (0-5) to quad vertices (0,1,2,0,2,3)
    static const uint QUAD_INDICES[6] = { 0, 1, 2, 0, 2, 3 };
    uint quadVertexID = QUAD_INDICES[vertexID];

    // Get UV from lookup table
    float2 uv = QUAD_UVS[quadVertexID];

    // Generate billboard position
    float3 worldPos;

    // UV.y determines position along beam (0 = tail, 1 = head)
    worldPos = lerp(instance.startPos, instance.endPos, uv.y);

    // UV.x determines horizontal offset (0 = left, 1 = right)
    float horizontalOffset = (uv.x - 0.5f) * 2.0f * instance.thickness;
    worldPos += right * horizontalOffset;

    // Transform to clip space
    float4 worldPos4 = float4(worldPos, 1.0);
    float4 cameraSpacePosition = mul(cameraConstants.worldToCameraTransform, worldPos4);
    float4 renderSpacePosition = mul(cameraConstants.cameraToRenderTransform, cameraSpacePosition);
    float4 clipSpacePosition = mul(cameraConstants.renderToClipTransform, renderSpacePosition);

    // Output
    output.clipSpacePosition = clipSpacePosition;
    output.uv = uv;
    output.color = instance.color;
    output.intensity = instance.intensity;

    return output;
}


float sdRoundedBox(float2 p, float2 b, float r)
{
    float2 d = abs(p) - b + r;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - r;
}

float sdBox(float2 p, float2 b)
{
    float2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float sdUnevenCapsule(float2 p, float r1, float r2, float h)
{
    p.x = abs(p.x);
    float b = (r1 - r2) / h;
    float a = sqrt(1.0 - b * b);
    float k = dot(p, float2(-b, a));
    if (k < 0.0) return length(p) - r1;
    if (k > a * h) return length(p - float2(0.0, h)) - r2;
    return dot(p, float2(a, b)) - r1;
}

//-----------------------------------------------------------------------------------------------
// Pixel Shader - Glowing tracer effect
float4 PixelMain(v2p_t input) : SV_Target0
{
    // Convert UV to centered coordinates (-0.5 to 0.5)
    float2 p = input.uv - 0.5;

    float r1 = 0.3f;
    float r2 = 0.075f;
    float radius = 0.1f;
    float h = 1.0 - 2.0 * radius - r1 - r2;

    float2 capsuleP = float2(p.x, -p.y + 0.5 - r1 - radius);

    float dist = sdUnevenCapsule(capsuleP, r1, r2, h);

    // dist <= 0 same color
    // radius > dist > 0 fading
    // dist > radius no color

    float brightness = 1.0 - smoothstep(0.0, radius, dist);

    float headToTail = input.uv.y;
    float tailFade = lerp(0.2, 1.0, headToTail);

    // Combine
    brightness *= tailFade * input.intensity;
    float3 finalColor = input.color.rgb * brightness;

    return float4(finalColor, 1.0f);
}

/*
    // Convert UV to centered coordinates (-0.5 to 0.5)
    float2 p = input.uv - 0.5;
    
    float2 boxSize = float2(0.3, 0.3);
    float radius = 0.2;

    float dist = sdBox(p, boxSize);

    // dist <= 0 same color
    // radius > dist > 0 fading
    // dist > radius no color

    float brightness = 1.0 - smoothstep(0.0, radius, dist);

    float headToTail = input.uv.y;
    float tailFade = lerp(0.2, 1.0, headToTail);

    // Combine
    brightness *= tailFade * input.intensity;
    float3 finalColor = input.color.rgb * brightness;

    return float4(finalColor, 1.0f);


*/
