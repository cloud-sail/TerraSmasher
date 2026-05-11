#include "Common/Utils.hlsli"
#include "Common/ShaderConstants.hlsli"
#include "Common/Resources.hlsli"
#include "Common/Lighting.hlsli"
#include "Common/Math.hlsli"
#include "Common/StaticSampler.hlsli"
#include "Common/TriplanarUtils.hlsli"
#include "Common/ToneMapping.hlsli"

//-----------------------------------------------------------------------------------------------
// Mirrors C++ GMaterial layout
struct GMaterial
{
    uint  albedoTextureIndex;
    uint  normalTextureIndex;
    uint  ormhTextureIndex;       // x:AO  y:Roughness  z:Metallic  w:Height
    uint  emissiveTextureIndex;

    float uvScale;
    float lowPolyLevel;
};

//-----------------------------------------------------------------------------------------------
// Mirrors C++ DebrisParticleGPU (64 bytes). All fields are immutable after spawn;
// VS computes the current transform analytically from `age`.
struct DebrisParticle
{
    float3 initPos;       float initialScale;
    float3 velocity;      float lifetime;
    float3 initEuler;     float pad0;
    float3 angularVel;    float pad1;
};

//-----------------------------------------------------------------------------------------------
struct DebrisRenderResources
{
    uint debrisVertexBufferIndex;  // StructuredBuffer<float4>  (xyz = local pos, w = 0)
    uint particleBufferIndex;      // StructuredBuffer<DebrisParticle>

    uint cameraConstantsIndex;
    uint lightConstantsIndex;
    uint materialBufferIndex;      // StructuredBuffer<GMaterial>

    uint  materialID;
    float age;                     // emitter age in seconds
    float pad0;
};

ConstantBuffer<DebrisRenderResources> renderResources : register(b0);

//-----------------------------------------------------------------------------------------------
// XYZ euler -> 3x3 rotation matrix (R = Rz * Ry * Rx). Convention is arbitrary as long as it's
// consistent; debris just needs to tumble visibly.
float3x3 BuildEulerXYZ(float3 e)
{
    float cx = cos(e.x); float sx = sin(e.x);
    float cy = cos(e.y); float sy = sin(e.y);
    float cz = cos(e.z); float sz = sin(e.z);

    return float3x3(
        float3(cz*cy,  cz*sy*sx - sz*cx,  cz*sy*cx + sz*sx),
        float3(sz*cy,  sz*sy*sx + cz*cx,  sz*sy*cx - cz*sx),
        float3(-sy,    cy*sx,             cy*cx)
    );
}

//-----------------------------------------------------------------------------------------------
struct v2p_t
{
    float4 clipPos                            : SV_Position;
    float3 worldPos                           : WORLD_POSITION;
    float3 localPos                           : LOCAL_POSITION;
    nointerpolation float3 rotMatRow0         : ROT_ROW0;
    nointerpolation float3 rotMatRow1         : ROT_ROW1;
    nointerpolation float3 rotMatRow2         : ROT_ROW2;
};

//-----------------------------------------------------------------------------------------------
v2p_t VertexMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    StructuredBuffer<float4>          debrisVerts = ResourceDescriptorHeap[renderResources.debrisVertexBufferIndex];
    StructuredBuffer<DebrisParticle>  particles   = ResourceDescriptorHeap[renderResources.particleBufferIndex];
    ConstantBuffer<CameraConstants>   cam         = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];

    DebrisParticle p = particles[instanceID];
    float3 localPos  = debrisVerts[vertexID].xyz;

    float age        = renderResources.age;
    float lifeAlpha  = saturate(age / max(p.lifetime, 1e-4));
    float currentScale = lerp(p.initialScale, 0.0, lifeAlpha);

    float3 currentEuler = p.initEuler + p.angularVel * age;
    float3x3 R = BuildEulerXYZ(currentEuler);

    // worldPos = R * (localPos * scale) + (initPos + velocity * age)
    float3 rotatedScaled = mul(R, localPos) * currentScale;
    float3 worldPos      = rotatedScaled + (p.initPos + p.velocity * age);

    float4 wp4 = float4(worldPos, 1.0);
    float4 cp  = mul(cam.worldToCameraTransform,  wp4);
    float4 rp  = mul(cam.cameraToRenderTransform, cp);
    float4 clipPos = mul(cam.renderToClipTransform, rp);

    v2p_t o;
    o.clipPos     = clipPos;
    o.worldPos    = worldPos;
    o.localPos    = localPos;
    o.rotMatRow0  = R[0];
    o.rotMatRow1  = R[1];
    o.rotMatRow2  = R[2];
    return o;
}

//-----------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];
    ConstantBuffer<LightConstants>  lightConstants  = ResourceDescriptorHeap[renderResources.lightConstantsIndex];
    StructuredBuffer<GMaterial>     materialBuffer  = ResourceDescriptorHeap[renderResources.materialBufferIndex];

    GMaterial mat = materialBuffer[renderResources.materialID];

    SamplerState samp = s_linearWrap;
    const float blendSharpness = 10.f;

    // Object-space face normal: stays constant on each face, so the sampled triplanar
    // texture stays glued to the chunk as it tumbles.
    float3 localFaceNormal = normalize(cross(ddy(input.localPos), ddx(input.localPos)));

    // Reconstruct rotation from interpolant rows (nointerpolation -> exact).
    float3x3 R = float3x3(input.rotMatRow0, input.rotMatRow1, input.rotMatRow2);
    float3 worldFaceNormal = normalize(mul(R, localFaceNormal));

    // Triplanar sampling in OBJECT space: UVs from localPos, weights from localFaceNormal.
    float4 albedoTexel = SampleTriplanar(input.localPos, localFaceNormal, mat.uvScale, blendSharpness,
                                         ResourceDescriptorHeap[mat.albedoTextureIndex],   samp);
    float4 ormh       = SampleTriplanar(input.localPos, localFaceNormal, mat.uvScale, blendSharpness,
                                         ResourceDescriptorHeap[mat.ormhTextureIndex],     samp);
    float3 emissive   = SampleTriplanar(input.localPos, localFaceNormal, mat.uvScale, blendSharpness,
                                         ResourceDescriptorHeap[mat.emissiveTextureIndex], samp).rgb;

    // Normal map: sample in object space, then rotate the perturbed normal into world space.
    float3 localPixelNormal = SampleTriplanarNormal(input.localPos, localFaceNormal, mat.uvScale, blendSharpness,
                                                    ResourceDescriptorHeap[mat.normalTextureIndex], samp);
    float3 worldPixelNormal = normalize(mul(R, localPixelNormal));

    SurfaceData surf = MakeDefaultSurfaceData();
    surf.Albedo    = albedoTexel.rgb;
    surf.Normal    = worldPixelNormal;
    surf.Metallic  = ormh.z;
    surf.Roughness = ormh.y;

    float3 directLighting = float3(0, 0, 0);
    CALC_TOTAL_PBR_LIGHT(directLighting, surf, input.worldPos);

    float3 ambient = float3(0.02, 0.02, 0.02) * albedoTexel.rgb * ormh.x;
    float3 color   = ambient + directLighting + emissive;

    color = ACESFilm(color);
    color = pow(color, 1.0 / 2.2);
    return float4(color, 1.0);
}
