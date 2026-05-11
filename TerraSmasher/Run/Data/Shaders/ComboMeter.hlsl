#include "Common/ShaderConstants.hlsli"
#include "Common/Resources.hlsli"

// ComboMeter: a square 2D quad rendered in screen space.
//   - Center (r < innerRadius): solid filled disc.
//   - Ring   (innerRadius < r < outerRadius): progress arc, top of quad (+UV.y) is the
//     0/1 boundary, sweeping clockwise.
//   - Outside the outer radius: transparent.
//
// All radii are in UV space, so they cap at 0.5 (the distance from the quad center to its edge).
// The C++ side scales innerRadius / outerRadius together for a "scale shake" pump effect.
//
// Optional fake-3D tilt (FakePerspective-style):
//   yRotDegrees / xRotDegrees apply an inverse-rotation projection so the procedurally-drawn
//   ring/disc appears tilted. The math collapses to identity (sampleUV == centeredUV) when both
//   angles are zero, so the no-tilt path is bit-for-bit unchanged.
//
//   Per-vertex VS computes a ray direction in image-local space:
//       rayDir = R^-1 * (centeredUV.x, centeredUV.y, camD)
//   then scales the .xy by (camD * invRotCol2.z) so that PS can do
//       sampleUV = rayDir.xy / rayDir.z - centerOffset
//   with centerOffset = camD * invRotCol2.xy.
//
//   This is FakePerspective's pipeline minus the quad-expansion factor (we don't expand the quad,
//   so the (camD + 0.5) factor in the original shader would over-scale and clip our ring).

//-----------------------------------------------------------------------------------------------
struct vs_input_t
{
    float3 modelSpacePosition : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct v2p_t
{
    float4 clipSpacePosition : SV_Position;
    float4 color             : COLOR;
    float2 uv                : TEXCOORD0;
    nointerpolation float2 centerOffset : TEXCOORD1;
    float3 rayDirection      : TEXCOORD2;
};

ConstantBuffer<ComboMeterRenderResources> renderResources : register(b0);

//-----------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
    ConstantBuffer<CameraConstants> cameraConstants = ResourceDescriptorHeap[renderResources.cameraConstantsIndex];
    ConstantBuffer<ModelConstants> modelConstants = ResourceDescriptorHeap[renderResources.modelConstantsIndex];

    // ---- Fake-perspective: build inverse rotation columns + camera distance + the math factor.
    const float PI = 3.14159265359f;
    float yRotRad = renderResources.yRotDegrees * PI / 180.0f;
    float xRotRad = renderResources.xRotDegrees * PI / 180.0f;
    float sy = sin(yRotRad);
    float cy = cos(yRotRad);
    float sx = sin(xRotRad);
    float cx = cos(xRotRad);

    float3 invRotCol0 = float3(cy,        0.0f, -sy);
    float3 invRotCol1 = float3(sy * sx,   cx,    cy * sx);
    float3 invRotCol2 = float3(sy * cx,  -sx,    cy * cx); // image normal in world space

    float tanHalfFov     = tan(renderResources.fovDegrees * PI / 360.0f);
    float cameraDistance = 0.5f / tanHalfFov;
    float oneMinusInset  = 1.0f - renderResources.insetAmount;

    // Math factor that pairs with the quad expansion below. At insetAmount=0 (max expansion) this
    // matches FakePerspective's "expandedHalfWidth = camD + 0.5". At insetAmount=1 (no expansion)
    // this collapses to camD, giving sampleUV == centeredUV at no rotation.
    float effectiveHalfWidth = cameraDistance + 0.5f * oneMinusInset;

    // ---- Quad expansion (FakePerspective Step 8). Co-located with effectiveHalfWidth so the two
    // can never drift out of sync.  Each corner is pushed outward along its own (uv - 0.5) by
    // baseSize * tanHalfFov * (1 - insetAmount). Equivalent to growing the quad's half-extent by
    // tanHalfFov * (1 - insetAmount), matching the math factor above.
    float2 centeredUV = input.uv - 0.5f;
    float  expansionRatio = tanHalfFov * oneMinusInset;
    float3 expandedPosition = input.modelSpacePosition;
    expandedPosition.xy += centeredUV * renderResources.baseSize * expansionRatio;

    // Standard MVP transform on the expanded position.
    float4 modelSpacePosition  = float4(expandedPosition, 1);
    float4 worldSpacePosition  = mul(modelConstants.modelToWorldTransform, modelSpacePosition);
    float4 cameraSpacePosition = mul(cameraConstants.worldToCameraTransform, worldSpacePosition);
    float4 renderSpacePosition = mul(cameraConstants.cameraToRenderTransform, cameraSpacePosition);
    float4 clipSpacePosition   = mul(cameraConstants.renderToClipTransform, renderSpacePosition);

    // ---- Per-vertex ray direction in image-local space (PS perspective-divides this).
    float3 rayDirection = invRotCol0 * centeredUV.x
                        + invRotCol1 * centeredUV.y
                        + invRotCol2 * cameraDistance;

    rayDirection.xy *= effectiveHalfWidth * invRotCol2.z;

    // Image-center offset on screen (same for all 4 quad vertices, hence nointerpolation).
    float2 centerOffset = effectiveHalfWidth * invRotCol2.xy;

    v2p_t v2p;
    v2p.clipSpacePosition = clipSpacePosition;
    v2p.color = input.color;
    v2p.uv = input.uv;
    v2p.centerOffset = centerOffset;
    v2p.rayDirection = rayDirection;
    return v2p;
}

//-----------------------------------------------------------------------------------------------
// Hardcoded color palette. Tweak here, no rebuild of C++ needed.
static const float4 COLOR_INNER_DISC   = float4(0.05f, 0.06f, 0.10f, 0.85f);  // dark fill behind the rank number
static const float4 COLOR_RING_BG      = float4(0.12f, 0.14f, 0.20f, 0.85f);  // unfilled ring background
static const float4 COLOR_RING_FILL    = float4(1.00f, 0.55f, 0.10f, 1.00f);  // filled progress arc
static const float4 COLOR_RING_OUTLINE = float4(0.00f, 0.00f, 0.00f, 0.95f);  // thin outer outline

static const float OUTLINE_THICKNESS = 0.012f;   // in UV units
static const float EDGE_SOFTNESS     = 0.006f;   // anti-alias falloff half-width per edge

float4 PixelMain(v2p_t input) : SV_Target0
{
    // Backface-cull: tilt past 90 degrees flips the meter away from the camera.
    if (input.rayDirection.z <= 0.0f) discard;

    // Perspective divide -> sampleUV in centered UV space ([-0.5, 0.5] when no rotation).
    // With non-zero tilt this still lives in roughly the same range but warped.
    float2 sampleUV = (input.rayDirection.xy / input.rayDirection.z) - input.centerOffset;

    // Polar coordinates measured from quad center in the (un-rotated) image space.
    float2 dir = sampleUV;
    float r = length(dir);

    float innerR     = renderResources.innerRadius;
    float outerR     = renderResources.outerRadius;
    float totalOuter = outerR + OUTLINE_THICKNESS;
    float progress   = saturate(renderResources.progress);

    // Cheap fillrate cull: skip pixels well past the outer outline.
    if (r > totalOuter + EDGE_SOFTNESS) clip(-1);

    // Angle for the ring fill (top of image = 0, sweep clockwise).
    // atan2(x, y) gives angle measured clockwise from +y axis, in [-PI, PI].
    const float PI = 3.14159265359f;
    float theta = atan2(dir.x, dir.y);
    if (theta < 0.f) theta += 2.f * PI;
    float fraction = theta / (2.f * PI);

    float fillAA   = saturate((progress - fraction) / EDGE_SOFTNESS);
    float4 ringColor = lerp(COLOR_RING_BG, COLOR_RING_FILL, fillAA);

    // Three concentric "presence" masks via smoothstep, each centered on its boundary radius
    // with EDGE_SOFTNESS falloff on either side. 1 fully inside, 0 fully outside.
    float discPresence    = smoothstep(innerR     + EDGE_SOFTNESS, innerR     - EDGE_SOFTNESS, r);
    float ringPresence    = smoothstep(outerR     + EDGE_SOFTNESS, outerR     - EDGE_SOFTNESS, r);
    float outlinePresence = smoothstep(totalOuter + EDGE_SOFTNESS, totalOuter - EDGE_SOFTNESS, r);

    // Composite from outermost layer to innermost. Each layer's lerp weight is its presence,
    // so transitions between concentric regions are smoothed by the underlying presence values.
    float4 col = float4(0, 0, 0, 0);
    col = lerp(col, COLOR_RING_OUTLINE, outlinePresence);
    col = lerp(col, ringColor,          ringPresence);
    col = lerp(col, COLOR_INNER_DISC,   discPresence);

    return col;
}
