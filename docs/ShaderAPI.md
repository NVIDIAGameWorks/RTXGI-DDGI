# RTXGI SDK Shader API

## Dynamic Diffuse Global Illumination (DDGI)

As discussed in the [Integration Guide](Integration.md), the DDGI algorithm is composed of a few steps. The SDK handles the following steps of the DDGI algorithm:

- Probe Irradiance and Distance Updates (also called Blending)
  - Probe Irradiance and Distance Border Updates
- Probe Classification
- Probe Relocation

Due to the design of modern ray tracing APIs and the variability of material (hit) shaders from one application to another, the SDK purposely leaves the ownership and maintenance of ray tracing acceleration structures, shader tables, and ray tracing pipeline state objects to the application. It would be inappropriate for the SDK to own these structures, since this would impose unnecessary limitations on the application and reduce the generality of the SDK's implementation.

As a result, the application is responsible for handling the following steps of the DDGI algorithm:

- Probe ray tracing
- Probe sampling (in probe ray tracing and the screen-space indirect light gather)

To help facilitate these tasks, the SDK provides useful shader defines, structures, and functions that the application should use in ray generation, compute, and/or pixel shader code. To use these shared defines, structures, and functions include [ProbeCommon.hlsl](../rtxgi-sdk/shaders/ddgi/include/ProbeCommon.hlsl), [DDGIRootConstants.hlsl](../rtxgi-sdk/shaders/ddgi/include/DDGIRootConstants.hlsl), and [Irradiance.hlsl](../rtxgi-sdk/shaders/ddgi/Irradiance.hlsl) in your shader where appropriate.

### Common Structures

```C++
DDGIVolumeDescGPU
```
This structure contains the runtime parameters of a `DDGIVolume` that are used by shader functions in GPU code.

```C++
DDGIVolumeDescGPUPacked
```
The compact version of ```DDGIVolumeDescGPU``` that is stored in the constants structured buffer. A packed version of this structure is created on the CPU with ```PackDDGIVolumeDescGPU(...)```, uploaded to the GPU, and unpacked with ```UnpackDDGIVolumeDescGPU(...)```.

### Common Functions

```C++
DDGIVolumeDescGPU UnpackDDGIVolumeDescGPU(DDGIVolumeDescGPUPacked input)
```
Unpacks the compacted ```DDGIVolumeDescGPUPacked``` structure and returns the full sized structure.


```C++
bool IsVolumeMovementScrolling(DDGIVolumeDescGPU volume)
```
Returns true if the provided volume's infinite scrolling movement feature is enabled.

```C++
uint GetDDGIVolumeIndex()
```
Returns the index of the ```DDGIVolume``` to use. The index is set in the root constants / push constants block of the root signature / pipeline layout.

### Probe Ray Tracing

The Test Harness sample application provides [an example ray generation shader](../samples/test-harness/shaders/ddgi/ProbeTraceRGS.hlsl) that demonstrates the process applications should implement to trace probe rays and store the results in probe ray data textures. The helper functions below are provided to make this process easier.

```C++
int3 DDGIGetProbeCoords(int probeIndex, DDGIVolumeDescGPU volume)
```
Computes the 3D grid-space coordinates for the probe at a given probe index in the range [0, numProbes]. Provide these coordinates to ```DDGIGetProbeWorldPosition(...)``` and ```DDGIGetScrollingProbeIndex(...)```.

```C++
int DDGIGetScrollingProbeIndex(int3 probeCoords, DDGIVolumeDescGPU volume)
```
Returns an adjusted probe index that accounts for the volume's infinite scrolling offsets. Provide this index to ```DDGILoadProbeState(...)```.


```C++
float DDGILoadProbeState(int probeIndex, RWTexture2DArray<float4> probeData, DDGIVolumeDescGPU volume)
float DDGILoadProbeState(int probeIndex, Texture2DArray<float4> probeData, DDGIVolumeDescGPU volume)
```
Loads and returns the probe's classification state for a given probe index. The provided probe index should be adjusted for infinite volume scrolling using ```DDGIGetScrollingProbeIndex()``` if the feature is enabled.


```C++
float3 DDGIGetProbeWorldPosition(int3 probeCoords, DDGIVolumeDescGPU volume)
float3 DDGIGetProbeWorldPosition(int3 probeCoords, DDGIVolumeDescGPU volume, Texture2DArray<float4> probeData)
float3 DDGIGetProbeWorldPosition(int3 probeCoords, DDGIVolumeDescGPU volume, RWTexture2DArray<float4> probeData)
```
Computes the world-space position of a probe from the probe's 3D grid-space coordinates. When probe relocation is enabled, offsets are loaded from the probe data texture array and used to adjust the final world position. Two variants of the function exist to support both read-only and read-write formats of the probe data texture array.

```C++
float3 DDGIGetProbeRayDirection(int rayIndex, DDGIVolumeDescGPU volume)
```
Computes a spherically distributed, normalized ray direction for the given ray index in a set of ray samples. Applies the volume's random probe ray rotation transformation to ```non-fixed``` ray direction samples.

```C++
void DDGIStoreProbeRayMiss(RWTexture2DArray<float4> RayData, uint3 coords, DDGIVolumeDescGPU volume, float3 radiance)
```
Stores the provided radiance and a large hit distance in the volume's ray data texture array at the given coordinates. Formats (compacts) the radiance value to the volume's ray data texture storage format.

```C++
void DDGIStoreProbeRayBackfaceHit(RWTexture2DArray<float4> RayData, uint3 coords, DDGIVolumeDescGPU volume, float hitT)
```
Stores the provided hit distance in the volume's ray data texture array at the given coordinates. Negates and decreases the hit distance by 80% to decrease the probe's influence during blending and mark a backface hit for probe relocation and classification.

```C++
void DDGIStoreProbeRayFrontfaceHit(RWTexture2DArray<float4> RayData, uint3 coords, DDGIVolumeDescGPU volume, float3 radiance, float hitT)
void DDGIStoreProbeRayFrontfaceHit(RWTexture2DArray<float4> RayData, uint3 coords, DDGIVolumeDescGPU volume, float hitT)
```
Stores the provided radiance and/or hit distance in the volume's ray data texture array at the given coordinates. Formats (compacts) the radiance value to the volume's ray data storage format.

### Probe Sampling

The Test Harness provides [example compute](../samples/test-harness/shaders/IndirectCS.hlsl) and [probe ray generation](../samples/test-harness/shaders/ddgi/ProbeTraceRGS.hlsl) shaders that demonstrate the process applications need to implement to gather indirect light from ```DDGIVolumes``` in screen-space or during probe ray tracing.

The structures and functions that facilitate sampling ```DDGIVolume``` probes are provided by the SDK in [Irradiance.hlsl](../rtxgi-sdk/shaders/ddgi/Irradiance.hlsl). Include this file in any shader that needs to sample probes. ```Irradiance.hlsl``` already includes ```ProbeCommon.hlsl```, so there is no need to include that file also.

```C++
struct DDGIVolumeResources
{
    Texture2DArray<float4> probeIrradiance;
    Texture2DArray<float4> probeDistance;
    Texture2DArray<float4> probeData;
    SamplerState bilinearSampler;
};
```
```DDGIVolumeResources``` is a wrapper struct that contains a volume's probe irradiance, distance, and probe data texture arrays, along with a bilinear sampler.

```C++
float3 DDGIGetSurfaceBias(float3 surfaceNormal, float3 cameraDirection, DDGIVolumeDescGPU volume)
```
Computes the ```surfaceBias``` parameter that should be passed to ```DDGIGetVolumeIrradiance(...)```. The ```surfaceNormal``` and ```cameraDirection``` arguments should be normalized.

```C++
float DDGIGetVolumeBlendWeight(float3 worldPosition, DDGIVolumeDescGPU volume)
```
Computes a weight value in the range [0, 1] for a world position and ```DDGIVolume``` pair. All positions inside the given volume recieve a weight of 1. Positions outside the volume receive a weight in [0, 1] that decreases as the position moves away from the volume.

```C++
float3 DDGIGetVolumeIrradiance(float3 worldPosition, float3 surfaceBias, float3 direction, DDGIVolumeDescGPU volume, DDGIVolumeResources resources)
```
Computes irradiance for the given world-position using the given volume, surface bias, sampling direction, and volume resources.
