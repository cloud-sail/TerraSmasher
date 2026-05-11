#pragma once

struct UnlitRenderResources
{
    uint diffuseTextureIndex;
    uint diffuseSamplerIndex;
    
    uint cameraConstantsIndex;
    uint modelConstantsIndex;
};

struct DiffuseRenderResources
{
    uint diffuseTextureIndex;
    uint diffuseSamplerIndex;
    
    uint cameraConstantsIndex;
    uint modelConstantsIndex;
    uint lightConstantsIndex;
};

struct BlinnPhongRenderResources
{
    uint diffuseTextureIndex;
    uint normalTextureIndex;
    uint specGlossEmitTextureIndex;
    
    uint engineConstantsIndex;
    uint cameraConstantsIndex;
    uint modelConstantsIndex;
    uint lightConstantsIndex;
};

struct SkyboxRenderResources
{
    uint cubeMapTextureIndex;

    uint cameraConstantsIndex;
    uint modelConstantsIndex;
};

struct ComboMeterRenderResources
{
    uint cameraConstantsIndex;
    uint modelConstantsIndex;

    float progress;     // 0..1 fill amount of the ring
    float innerRadius;  // 0..0.5 in UV space (in the un-expanded image)
    float outerRadius;  // 0..0.5 in UV space

    // Fake-3D tilt (FakePerspective-style). 0 means no tilt; identity output when insetAmount == 1.
    float yRotDegrees;
    float xRotDegrees;
    float fovDegrees;   // virtual camera FOV

    // 0 = max expansion (no clip on tilt), 1 = no expansion. VS expands the quad itself, so
    // C++ does NOT recompute the expansion -- just reports baseSize.
    float insetAmount;

    // Side length (pixels) of the un-expanded quad. VS uses this with insetAmount + fov to
    // expand the 4 corners outward, matching FakePerspective Step 8.
    float baseSize;

    float pad1;
    float pad2;
};