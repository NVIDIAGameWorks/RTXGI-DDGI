# RTXGI SDK Shader API

## Dynamic Diffuse Global Illumination (DDGI)

As discussed in the [Integration Guide](Integration.md), the DDGI algorithm is composed of a few steps. The SDK handles the following steps of the DDGI algorithm:

- Probe Irradiance and Distance Updates (also called Blending)
- Probe Irradiance and Distance Border Updates
- Probe Classification
- Probe Relocation

Due to the design of modern ray tracing APIs and the variability of material (hit) shaders from one application to another, the SDK purposely leaves the ownership and maintenance of ray tracing acceleration structures, shader tables, and ray tracing pipeline state objects to the application. It would be inappropriate for the SDK to own these structures, since this would impose unnecessary limitations on the application and reduce the generality of the SDKs implementation.

As a result, the application is responsible for handling the following steps of the DDGI algorithm:

- Probe ray tracing
- Probe sampling (probe ray tracing, screen-space lighting)

To help facilitate these tasks, the SDK provides useful shader defines, structures, and functions that the application should use in ray generation, compute, and/or pixel shader code. To use these shared defines, structures, and functions include [ProbeCommon.hlsl](../rtxgi-sdk/shaders/ddgi/include/ProbeCommon.hlsl) in your shader.

### Common Structures

#### ```DDGIVolumeDescGPU```

This structure contains the runtime parameters of a `DDGIVolume` that are used by most shader functions. A packed version of this structure is uploaded to the GPU and unpacked with ```UnpackDDGIVolumeDescGPU()```.

#### ```DDGIVolumeDescGPUPacked```

The compact version of ```DDGIVolumeDescGPU``` that is stored in a structured buffer.

### Common Functions

#### ```UnpackDDGIVolumeDescGPU(...)```

    DDGIVolumeDescGPU UnpackDDGIVolumeDescGPU(DDGIVolumeDescGPUPacked input)

Unpacks the compacted DDGIVolume descriptor and returns the full sized structure.

#### ```IsVolumeMovementScrolling(...)```

    bool IsVolumeMovementScrolling(DDGIVolumeDescGPU volume)

Returns true if the provided volume's infinite scrolling movement feature is enabled.

### Probe Ray Tracing

The Test Harness sample application provides [an example ray generation shader](../samples/test-harness/shaders/ddgi/ProbeTraceRGS.hlsl) that demonstrates the process applications should implement to trace probe rays and store the results in probe ray data textures. The helper functions below are provided to make this process easier.

#### ```DDGIGetProbeCoords(...)```

    int3 DDGIGetProbeCoords(int probeIndex, DDGIVolumeDescGPU volume)

Computes the 3D grid-space coordinates for the probe at a given probe index in the range [0, numProbes]. Provide these coordinates to ```DDGIGetProbeWorldPosition()``` and ```DDGIGetScrollingProbeIndex()```.

#### ```DDGIGetProbeState(...)```

    float DDGIGetProbeState(
        int probeIndex,
        Texture2D<float4> probeData,
        DDGIVolumeDescGPU volume)

Loads and returns the probe's classification state for a given probe index. The provided probe index should be adjusted for infinite volume scrolling using ```DDGIGetScrollingProbeIndex()``` if the feature is enabled.

#### ```DDGIGetScrollingProbeIndex(...)```

    int DDGIGetScrollingProbeIndex(int3 probeCoords, DDGIVolumeDescGPU volume)

Returns an adjusted probe index that accounts for the volume's infinite scrolling offsets. Provide this probe index to ```DDGIGetProbeState()```.




#### ```DDGIGetProbeWorldPosition(...)```

    float3 DDGIGetProbeWorldPosition(
        int3 probeCoords,
        DDGIVolumeDescGPU volume)

    float3 DDGIGetProbeWorldPosition(
        int3 probeCoords,
        DDGIVolumeDescGPU volume,
        Texture2D<float4> probeData)

    float3 DDGIGetProbeWorldPosition(
        int3 probeCoords,
        DDGIVolumeDescGPU volume,
        RWTexture2D<float4> probeData)

Computes the world-space position of a probe from the probe's 3D grid-space coordinates. When probe relocation is enabled, offsets are loaded from the provided probe data texture and used to adjust the final world position. Two variants of the function exist to support both read-only and read-write formats of the probe data texture.

#### ```DDGIGetProbeRayDirection(...)```

    float3 DDGIGetProbeRayDirection(int rayIndex, DDGIVolumeDescGPU volume)

Computes a spherically distributed, normalized ray direction for the given ray index in a set of ray samples. Applies the volume's random probe ray rotation transformation to "non-fixed" ray direction samples.

#### ```DDGIStoreProbeRayMiss(...)```

    void DDGIStoreProbeRayMiss(
        RWTexture2D<float4> RayData,
        uint2 coords,
        DDGIVolumeDescGPU volume,
        float3 radiance)

Stores the provided radiance and a large hit distance in the volume's ray data texture at the given coordinates. Formats (compacts) the radiance value to the volume's ray data texture storage format.

#### ```DDGIStoreProbeRayBackfaceHit(...)```

    void DDGIStoreProbeRayBackfaceHit(
        RWTexture2D<float4> RayData,
        uint2 coords,
        DDGIVolumeDescGPU volume,
        float hitT)

Stores the provided hit distance in the volume's ray data texture at the given coordinates. Negates and decreases the hit distance by 80% to decrease the probe's influence during blending and mark a backface hit for probe relocation and classification.

#### ```DDGIStoreProbeRayFrontfaceHit(...)```

    DDGIStoreProbeRayFrontfaceHit(
        RWTexture2D<float4> RayData,
        uint2 coords,
        DDGIVolumeDescGPU volume,
        float3 radiance,
        float hitT)

    DDGIStoreProbeRayFrontfaceHit(
        RWTexture2D<float4> RayData,
        uint2 coords,
        DDGIVolumeDescGPU volume,
        float hitT)

Stores the provided radiance and hit distance in the volume's ray data texture at the given coordinates. Formats (compacts) the radiance value to the volume's ray data texture storage format.

### Probe Sampling

The Test Harness sample application provides [an example compute shader](../samples/test-harness/shaders/IndirectCS.hlsl) that demonstrates the process applications should implement to compute indirect lighting in screen-space from DDGIVolumes. The helper functions below are provided to make this process easier.

The structures and functions that facilitate sampling DDGI probes are provided by the SDK in [Irradiance.hlsl](../rtxgi-sdk/shaders/ddgi/Irradiance.hlsl). Include this file in any shader that needs to sample probes. `Irradiance.hlsl` already includes `ProbeCommon.hlsl`, so there is no need to include that file too.

#### ```DDGIVolumeResources```

    struct DDGIVolumeResources
    {
        Texture2D<float4> probeIrradiance;
        Texture2D<float4> probeDistance;
        Texture2D<float4> probeData;
        SamplerState bilinearSampler;
    };

Wrapper structure that contains a volume's probe irradiance and distance texture atlases, probe data texture, and a bilinear sampler.

#### ```DDGIGetSurfaceBias(...)```

    float3 DDGIGetSurfaceBias(
        float3 surfaceNormal,
        float3 cameraDirection,
        DDGIVolumeDescGPU volume)

Computes the ```surfaceBias``` parameter that should be passed to ```DDGIGetVolumeIrradiance()```. The ```surfaceNormal``` and ```cameraDirection``` arguments are expected to be normalized.

#### ```DDGIGetVolumeBlendWeight(...)```

    float DDGIGetVolumeBlendWeight(
        float3 worldPosition,
        DDGIVolumeDescGPU volume)

Computes a weight value in the range [0, 1] for a world position and DDGIVolume pair. All positions inside the given volume recieve a weight of 1. Positions outside the volume receive a weight in [0, 1] that decreases as the position moves away from the volume.

#### ```DDGIGetVolumeIrradiance(...)```

    float3 DDGIGetVolumeIrradiance(
        float3 worldPosition,
        float3 surfaceBias,
        float3 direction,
        DDGIVolumeDescGPU volume,
        DDGIVolumeResources resources)

Computes irradiance for the given world-position using the given volume, surface bias, sampling direction, and volume resources.
