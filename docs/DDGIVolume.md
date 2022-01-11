# RTXGI DDGIVolume Reference

## Overview

The implementation of the DDGI algorithm revolves around a defined volume of space that supports irradiance queries at arbitrary world-space locations. We refer to this space as a ```DDGIVolume```. The ```DDGIVolume``` can be used in a variety of ways, from placing stationary volumes in fixed positions in a world to attaching a ```DDGIVolume``` to a camera (or player) while scrolling the volume's internal probe grid when movement occurs (see [Volume Movement](#volume-movement) for more on this).

Regardless of use case, the ```DDGIVolume``` executes a set of GPU workloads that implement important parts of the complete DDGI algorithm. The image below illustrates these steps with the GPU timeline scrubber view from [NVIDIA Nsight Graphics](https://developer.nvidia.com/nsight-graphics).

<figure>
<img src="images/ddgivolume-nsight-scrubber.jpg" width="1200px"></img>
<figcaption><b>Figure 1: The DDGI algorithm in Nsight Graphics GPU Timeline Scrubber View</b></figcaption>
</figure>

As discussed in the [Integration Guide](Integration.md#integration-steps), the application is responsible for [tracing rays for ```DDGIVolume``` probes](Integration.md#tracing-probe-rays-for-a-ddgivolume) and [rendering indirect lighting](Integration.md#querying-irradiance-with-a-ddgivolume) using ```DDGIVolumes``` in the scene. The ```DDGIVolume``` handles probe irradiance and distance blending, border updates, probe relocation, and probe classification.

The following sections cover how to use the ```DDGIVolume``` and detail how its functionality is implemented.

## Creating a DDGIVolume

**Step 1:** to create a new ```DDGIVolume```, start by filling out a ```rtxgi::DDGIVolumeDesc``` structure.

- All properties of the descriptor struct are explained in [DDGIVolume.h](../rtxgi-sdk/include/rtxgi/ddgi/DDGIVolume.h#L51-124).
- Example usage is shown in ```GetDDGIVolumeDesc()``` of [DDGI_D3D12.cpp](../samples/test-harness/src/graphics/DDGI_D3D12.cpp) and [DDGI_VK.cpp](../samples/test-harness/src/graphics/DDGI_VK.cpp).

**Probe Irradiance Gamma**

To improve the light-to-dark convergence and the efficiency of texture storage, we use exponential weighting when storing irradiance. The default gamma exponent is 5.f, but it can be modified by changing ```DDGIVolumeDesc::probeIrradianceEncodingGamma```.

This exponent moves the stored irradiance value into a non-linear space that more closely matches human perception, while also allowing for a smaller texture format. If ```probeIrradianceEncodingGamma``` is set to 1.f, then the stored value remains in linear space and the quality of the lighting will decrease. To account for this quality loss when in linear space, ```DDGIVolumeDesc::probeIrradianceFormat``` can be set to ```RTXGI_DDGI_FORMAT_PROBE_IRRADIANCE_R32G32B32A32_FLOAT``` to use a larger texture format.


### Describing Resources

Next, specify information about the resources used by the volume (e.g. textures, shader bytecode/pipeline state objects, descriptors, etc.). This step is more involved since resources are API-specific. That said, there are not major differences in the process for D3D12 and Vulkan.

**Step 2:** fill out the appropriate ```rtxgi::d3d12::DDGIVolumeResources``` or ```rtxgi::vk::DDGIVolumeResources``` struct for the graphics API you are using (shown below).

**D3D12**

```C++
struct DDGIVolumeResources
{
    DDGIVolumeDescriptorHeapDesc descriptorHeapDesc;
    DDGIVolumeBindlessDescriptorDesc descriptorBindlessDesc;
    DDGIVolumeManagedResourcesDesc managed;
    DDGIVolumeUnmanagedResourcesDesc unmanaged;

    ID3D12Resource* constantsBuffer;
    ID3D12Resource* constantsBufferUpload;
    UINT64 constantsBufferSizeInBytes;
};
```

**Vulkan**

```C++
struct DDGIVolumeResources
{
    DDGIVolumeBindlessDescriptorDesc descriptorBindlessDesc;
    DDGIVolumeManagedResourcesDesc managed;
    DDGIVolumeUnmanagedResourcesDesc unmanaged;

    VkBuffer constantsBuffer;
    VkBuffer constantsBufferUpload;
    VkDeviceMemory constantsBufferUploadMemory;
    uint64_t constantsBufferSizeInBytes;
};
```

The ```DDGIVolumeResources``` structs for D3D12 and Vulkan differ in how memory and descriptors are handled (e.g. D3D12's descriptor heap and Vulkan's ```VkDeviceMemory``` object), which reflects the differences in the APIs themselves.

### Resource Management and Shaders

**Step 2a:** the first decision to make as you begin to specify resources with a ```DDGIVolumeResources``` struct is *whether the application or the SDK* will own and manage the lifetime of resources.

The SDK provides two modes for these scenarios:

- **Managed Mode**: the SDK allocates and owns volume resources. Use this mode if you don't need explicit control of the resources and want a simpler setup process.
- **Unmanaged Mode**: the application allocates and owns volume resources. Use this mode if you want to allocate, track, and own volume resources explicitly.

**Step 2b:** the ```DDGIVolumeResources``` structs provide ```DDGIVolumeManagedResourcesDesc``` and ```DDGIVolumeUnmanagedResourcesDesc``` structs to describe the resources associated with their respective modes. Fill out the appropriate descriptor struct for the resource mode you want to use. Be sure to set the ```enabled``` field of that struct to ```true```.

- For a complete view of these resource descriptor structs, see [DDGIVolume_D3D12.h](../rtxgi-sdk/include/rtxgi/ddgi/gfx/DDGIVolume_D3D12.h) and [DDGIVolume_VK.h](../rtxgi-sdk/include/rtxgi/ddgi/gfx/DDGIVolume_VK.h).


**Managed Mode**
  - Provide a pointer to the graphics device (```ID3D12Device```/```VkDevice```).
    - (Vulkan only) Provide a handle to the ``VkPhysicalDevice`` and a ```VkDescriptorPool```.
  - Provide ```ShaderBytecode``` objects that contain the compiled DXIL shader bytecode for the SDK's DDGI shaders.
    - See [Volume Shaders](#volume-shaders) for more information on required shaders and compilation.

**Unmanaged Mode**
  - Create the volume's textures.
  - D3D12
    - Add descriptor heap entries for the volume's texture resources.
    - Create the root signature (if not using bindless) using ```GetDDGIVolumeRootSignatureDesc()```.
    - Create a pipeline state object for each shader.
  - Vulkan
    - Create a shader module for each shader.
    - Create a pipeline for each shader.
  - See [Volume Shaders](#volume-shaders) for more information on required shaders and compilation.

Example usage is shown in ```GetDDGIVolumeResources()``` of [DDGI_D3D12.cpp](../samples/test-harness/src/graphics/DDGI_D3D12.cpp) and [DDGI_VK.cpp](../samples/test-harness/src/graphics/DDGI_VK.cpp).

  - Managed vs. unmanaged code paths are grouped with the ```RTXGI_DDGI_RESOURCE_MANAGEMENT``` preprocessor define.

**Constants**

Regardless of the selected resource management mode, the application is responsible for managing device and upload resources for ```DDGIVolume``` constants data. Constants data for *all volumes* in a scene are expected to be maintained in a single structured buffer that is sized for double (or arbitrary) buffering.

- See ```CreateDDGIVolumeConstantsBuffer(...)``` in [DDGI_D3D12.cpp](../samples/test-harness/src/graphics/DDGI_D3D12.cpp) and [DDGI_VK.cpp](../samples/test-harness/src/graphics/DDGI_VK.cpp) for an example of resource creation for constants data.

- ```DDGIVolumeResources::constantsBufferSizeInBytes``` specifies the size (in bytes) of constants data for all volumes in a scene. This value is **not** multiplied by the number of frames being buffered (e.g. 2 or 3).

- ```rtxgi::UploadDDGIVolumeConstants(...)``` is a helper function that transfers constants data for one or more volumes from the CPU to GPU.

**D3D12 Descriptor Heap**

Similar to constants data, the application is responsible for allocating, managing, and providing information about the descriptor heap to the ```DDGIVolume```. Specifically, the application provides offsets to where on the descriptor heap various volume resource descriptors should be placed. Shown below, the application provides this information with the ```DDGIVolumeDescriptorHeapDesc``` struct (part of ```DDGIVolumeResources```).

```C++
struct DDGIVolumeDescriptorHeapDesc
{
    ID3D12DescriptorHeap*       heap = nullptr;
    uint32_t                    constsOffset;     // Offset to the constants structured buffer SRV
    uint32_t                    uavOffset;        // Offset to the texture UAVs
    uint32_t                    srvOffset;        // Offset to the texture SRVs
};
```
The application can query how many descriptor heap slots to reserve for volume resources, using the ```rtxgi``` namespace helper functions below:

```C++
int GetDDGIVolumeNumRTVDescriptors();
int GetDDGIVolumeNumSRVDescriptors();
int GetDDGIVolumeNumUAVDescriptors();
```

**Textures**

See [Volume Textures](#volume-textures) for more information.

### Create()

**Step 3:** with the ```DDGIVolumeDesc``` and ```DDGIVolumeResources``` structs prepared, the final step to create a new volume is to instantiate a ```DDGIVolume``` instance and call the ```DDGIVolume::Create()``` function. The ```Create()``` function validates the parameters passed via the structs and creates the appropriate resources (if in managed mode).

- ```Create()``` checks for errors and reports them using the ```ERTXGIStatus``` enumeration. See [Common.h](../rtxgi-sdk/include/rtxgi/Common.h) for the complete list of return status codes.

After a successful ```Create()``` call the ```DDGIVolume``` is ready to use. See [Updating a DDGIVolume](#updating-a-ddgivolume), [Volume Movement](#volume-movement), [Probe Relocation](#probe-relocation), and [Probe Classification](#probe-classification) for more information on use.


## Volume Shaders

The ```DDGIVolume``` uses the below shaders for the workloads it executes. To make it possible to directly use any of the shader files in your own codebase (with or without the RTXGI SDK host code), shader functionality is driven by shader compiler defines and all shaders support both traditionally bound and bindless resource access methods.

**Common Shader Defines**

- ```RTXGI_DDGI_RESOURCE_MANAGEMENT``` specifies if the application (0: unmanaged) or the SDK (1: managed) own and manage the volume's resources.
- ```RTXGI_DDGI_BINDLESS_RESOURCES``` specifies resource access mode (0: bound, 1: bindless).
  - **Note:** bindless resources are not compatible with managed mode.
- ```RTXGI_DDGI_SHADER_REFLECTION``` specifies if the application uses shader reflection to discover resources.


To ease the shader configuration process, all shaders support the ```RTXGI_DDGI_USE_SHADER_CONFIG_FILE``` define that allows shader defines to be specified by a configuration file. This is useful when shader define values are the same across all shaders to be compiled.



### [```ProbeBlendingCS.hlsl```](../rtxgi-sdk/shaders/ddgi/ProbeBlendingCS.hlsl)

Compute shader code that updates (blends) either radiance or distance values into an octahedral texture atlas based on information in a volume's Probe Ray Data Texture. Irradiance and distance texels for *all probes of a single volume* are processed in parallel, across two dispatch calls (without serializing barriers). 

- This shader is used by the ```rtxgi::UpdateDDGIVolumeProbes(...)``` function.

**Configuration Defines**

- ```RTXGI_DDGI_PROBE_NUM_TEXELS``` specifies the number of texels in one dimension of a probe, **not including** the 1-texel border.
  - For example, this define is 6 for an 8x8 texel probe.
- ```RTXGI_DDGI_BLEND_RADIANCE``` specifies the blending mode (0: distance, 1: radiance).
- ```RTXGI_DDGI_BLEND_SHARED_MEMORY``` toggles shared memory use. This can substantially improve performance at the cost of higher register and shared memory use (potentially lowering occupancy).
- ```RTXGI_DDGI_BLEND_RAYS_PER_PROBE``` specifies the number of rays traced per probe. Required when shared memory is enabled.

**Debug Defines**

Debug modes are available to help visualize data in the probes. Visualized data is output to the probe irradiance texture atlas. To use these modes, the irradiance atlas texture format must be set to ```RTXGI_DDGI_FORMAT_PROBE_IRRADIANCE_R32G32B32A32_FLOAT```.

- ```RTXGI_DDGI_DEBUG_PROBE_INDEXING``` toggles a visualization mode that outputs probe indices as colors.
- ```RTXGI_DDGI_DEBUG_OCTAHEDRAL_INDEXING``` toggles a visualization mode that outputs colors for the directions computed for the octahedral UV coordinates returned by ```DDGIGetNormalizedOctahedralCoordinates()```.

**Compilation Instructions**

Compile **two** versions of this shader: one for radiance blending and another for distance blending.
  - **Managed Mode:** set the compiled DXIL bytecode of the two shader versions on:
    - ```DDGIVolumeManagedResourcesDesc::probeBlendingIrradianceCS```
    - ```DDGIVolumeManagedResourcesDesc::probeBlendingDistanceCS```
  - **Unmanaged Mode:** create and set the pipeline state objects of the two shader versions on:
    - ```DDGIVolumeUnmanagedResourcesDesc::probeBlendingIrradiance[PSO|Pipeline]```
    - ```DDGIVolumeUnmanagedResourcesDesc::probeBlendingDistance[PSO|Pipeline]```
      - In Vulkan, also create and set shader modules for the two shader versions on:
        - ```DDGIVolumeUnmanagedResourcesDesc::probeBlendingIrradianceModule```
        - ```DDGIVolumeUnmanagedResourcesDesc::probeBlendingDistanceModule```

See ```CompileDDGIVolumeShaders()``` in [DDGI.cpp](../samples/test-harness/src/graphics/DDGI.cpp) for an example.



### [```ProbeBorderUpdateCS.hlsl```](../rtxgi-sdk/shaders/ddgi/ProbeBlendingCS.hlsl)

Compute shader code that updates the 1-texel probe borders of the irradiance and distance octahedral texture atlases. Irradiance and distance atlas rows and columns for all probes of a single volume are processed in parallel, across four dispatch calls (without serializing barriers).

- This shader is used by the ```rtxgi::UpdateDDGIVolumeProbes(...)``` function.

**Configuration Defines**

- ```RTXGI_DDGI_PROBE_NUM_TEXELS``` specifies the number of texels in one dimension of a probe, **not including** the 1-texel border.
  - This should be the same value that is used with ```ProbeBlending.hlsl```.

**Debug Defines**

- ```RTXGI_DDGI_DEBUG_BORDER_COPY_INDEXING``` toggles a visualization mode that outputs the coordinates of the texel to be copied to the border texel.

**Compilation Instructions**

Similar to ```ProbeBlendingCS.hlsl```, compile irradiance and distance versions of each entry point available in the shader file. There are two entry points: ```DDGIProbeBorderRowUpdateCS()``` and ```DDGIProbeBorderColumnUpdateCS()```. This amounts to a total of **four** compiled shaders.

 - **Managed Mode:** set the compiled DXIL bytecode of the four shader versions on:
    - ```DDGIVolumeManagedResourcesDesc::probeBorderRowUpdateIrradianceCS```
    - ```DDGIVolumeManagedResourcesDesc::probeBorderRowUpdateDistanceCS```
    - ```DDGIVolumeManagedResourcesDesc::probeBorderColumnUpdateIrradianceCS```
    - ```DDGIVolumeManagedResourcesDesc::probeBorderColumnUpdateDistanceCS```
 - **Unmanaged Mode:** create and set the pipeline state objects of the four shader versions on:
    - ```DDGIVolumeUnmanagedResourcesDesc::probeBorderRowUpdateIrradiance[PSO|Pipeline]```
    - ```DDGIVolumeUnmanagedResourcesDesc::probeBorderRowUpdateDistance[PSO|Pipeline]```
    - ```DDGIVolumeUnmanagedResourcesDesc::probeBorderColumnUpdateIrradiance[PSO|Pipeline]```
    - ```DDGIVolumeUnmanagedResourcesDesc::probeBorderColumnUpdateDistance[PSO|Pipeline]```
    - In Vulkan, also create and set shader modules for the four shader versions on:
        - ```DDGIVolumeUnmanagedResourcesDesc::probeBorderRowUpdateIrradianceModule```
        - ```DDGIVolumeUnmanagedResourcesDesc::probeBorderRowUpdateDistanceModule```
        - ```DDGIVolumeUnmanagedResourcesDesc::probeBorderRowUpdateIrradianceModule```
        - ```DDGIVolumeUnmanagedResourcesDesc::probeBorderRowUpdateDistanceModule```

See ```CompileDDGIVolumeShaders()``` in [DDGI.cpp](/samples/test-harness/src/graphics/DDGI.cpp) for an example.



### [```ProbeRelocationCS.hlsl```](../rtxgi-sdk/shaders/ddgi/ProbeRelocationCS.hlsl)

Compute shader code that attempts to reposition probes if they are inside of or too close to surrounding geometry. See [Probe Relocation](#probe-relocation) for more information.

- This shader is used by the ```rtxgi::RelocateDDGIVolumeProbes()``` function.

**Compilation Instructions**

This shader file provides two entry points:
 - ```DDGIProbeRelocationCS()``` performs probe relocation, moving probes within their grid voxel.
 - ```DDGIProbeRelocationResetCS()``` resets all probe world-space positions to the center of their grid voxel.

Pass compiled shader bytecode or pipeline state objects to the  `ProbeRelocationBytecode` or `ProbeRelocation[PSO|Pipeline]` structs that map to the entry points in the shader file.

```C++
struct ProbeRelocationBytecode
{
    ShaderBytecode updateCS; // DDGIProbeRelocationCS() entry point
    ShaderBytecode resetCS;  // DDGIProbeRelocationResetCS() entry point
};
```




### [```ProbeClassificationCS.hlsl```](../rtxgi-sdk/shaders/ddgi/ProbeClassificationCS.hlsl)

Compute shader code that classifies probes into various states for performance optimization. See [Probe Classification](#probe-classification).

- This shader is used by the ```rtxgi::ClassifyDDGIVolumeProbes()``` function.

**Compilation Instructions**

Similar to ```ProbeClassification.hlsl```, this shader file provides two entry points:
 - ```DDGIProbeClassificationCS()``` performs probe classification.
 - ```DDGIProbeClassificationResetCS()``` resets all probes to the default state classification.

Pass compiled shader bytecode or pipeline state objects to the  `ProbeClassificationBytecode` or `ProbeClassification[PSO|Pipeline]` structs that map to the entry points in the shader file.

```C++
struct ProbeRelocationBytecode
{
    ShaderBytecode updateCS; // DDGIProbeClassificationCS() entry point
    ShaderBytecode resetCS;  // DDGIProbeClassificationResetCS() entry point
};
```




## Volume Textures

Each ```DDGIVolume``` uses a set of four textures:

 1. Probe Ray Data
 2. Probe Irradiance Atlas
 3. Probe Distance Atlas
 4. Probe Data

### Probe Ray Data

This texture stores ray hit data for all probes in a volume.

 - Texture `rows` represent probes in the volume's probe grid. Row number is the probe's index.
 - Texture `columns` represent rays traced from probes. Column number is the ray index.
 - Each `texel` contains the incoming radiance and distance to the closest surface obtained by ray `column#` for probe `row#`.

<figure>
<img src="images/ddgivolume-textures-raydata.jpg" width=500px></img>
<figcaption><b>Figure 2: The Probe Ray Data texture (zoomed) from the Cornell Box scene</b></figcaption>
</figure>

### Probe Irradiance and Distance Atlases

Irradiance and distance data for all probes of a volume are stored in two texture atlases. Each probe stored in an atlas is an octahedral parameterization of a sphere unwrapped to the unit square as described by [Cigolle et al](http://jcgt.org/published/0003/02/01/).

<figure>
<img src="images/ddgivolume-octahedral-param.jpg" width=750px></img>
<figcaption><b>Figure 3: Octahedral parameterization of a sphere <a href="http://jcgt.org/published/0003/02/01/" target="_blank">[Cigolle et al. 2014]</b></a></figcaption>
</figure>

To support fast hardware bilinear texture sampling of the unwrapped probes, our octahedral textures include a 1-texel border.

- Note that these border texels are *added* to the values specified in the ```DDGIVolumeDesc::probeNumIrradianceTexels``` and ```DDGIVolumeDesc::probeNumDistanceTexels```.

Below are annotated diagrams of the unwrapped octahedral unit square for a single probe. The left and center diagrams highlight the 1-texel border added to a 6x6 texel interior and identify the texels that map to the "front" and "back" hemispheres of the probe. The scheme to populate border texels with the data for bilinear interpolation is shown on the right.

<figure>
<img src="images/ddgivolume-octahedral-border.png" width=1000px></img>
<figcaption><b>Figure 4: Octahedral map interior and border texel layout</b></a></figcaption>
</figure>

Probe octahedral maps are organized in an atlas based on the 3D grid-space coordinates of probes in the ```DDGIVolume```. For example, in a right-handed y-up coordinate system the dimensions of the irradiance texture atlas are:

- Width: ```(probeSpacing.x``` * ```probeSpacing.y```) * (```probeNumIrradianceTexels``` + 2)
- Height: ```probeSpacing.z``` * (```probeNumIrradianceTexels``` + 2)

For convenience, the ```rtxgi``` namespace includes the ```GetDDGIVolumeTextureDimensions()``` function that returns the proper atlas texture dimensions based on the coordinate system.

This scheme always lays out horizontal planes of probes in the atlas texture (i.e. each plane is a "slice" of the vertical axis). A visualization of the irradiance and distance atlas textures is below:

<figure>
<img src="images/ddgivolume-textures-atlases.jpg" width=950px></img>
<figcaption><b>Figure 5: Irradiance (top) and distance (bottom) probe atlases for the Cornell Box scene</b></a></figcaption>
</figure>

#### **Probe Limits**

A consequence of this atlas layout scheme is a limit on the maximum number of probes that can be distributed across a given axis of a volume. This upper limit is a function of the number of texels in a probe and the resource limits of the graphics API and/or hardware. Since distance data uses the most texels per probe, its dimensions determine the upper limit.

The upper limits, as per-axis probe counts, can be written as:
  - <img src="https://render.githubusercontent.com/render/math?math=L_a = floor(T_l/D_t)"></img>
  - <img src="https://render.githubusercontent.com/render/math?math=L_b = floor(L_a/P_c)"></img>
  - <img src="https://render.githubusercontent.com/render/math?math=L_c = floor(L_a/P_b)"></img>

where, <img src="https://render.githubusercontent.com/render/math?math=L_a"></img>, <img src="https://render.githubusercontent.com/render/math?math=L_b"></img>, and <img src="https://render.githubusercontent.com/render/math?math=L_c"></img> are the maximum number of probes across axes <img src="https://render.githubusercontent.com/render/math?math=A"></img>, <img src="https://render.githubusercontent.com/render/math?math=B"></img>, and <img src="https://render.githubusercontent.com/render/math?math=C"></img>. <img src="https://render.githubusercontent.com/render/math?math=T_l"></img> is the graphics API's texel count limit and <img src="https://render.githubusercontent.com/render/math?math=D_t"></img> is the number of texels in one dimension of a probe's distance data. <img src="https://render.githubusercontent.com/render/math?math=P_b"></img> and <img src="https://render.githubusercontent.com/render/math?math=P_c"></img> are the number of probes across axes <img src="https://render.githubusercontent.com/render/math?math=B"></img> and <img src="https://render.githubusercontent.com/render/math?math=C"></img>, respectively.

  - **D3D12:** the maximum texture dimension size is **16384 texels**. See ```D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION``` in [d3d12.h](https://docs.microsoft.com/en-us/windows/win32/direct3d12/constants)
  - **Vulkan:** the specification requires a maximum texture dimension size of at least 4096 texels. Most implementations on [Windows](https://vulkan.gpuinfo.org/displaydevicelimit.php?name=maxImageDimension2D&platform=windows) and [Linux](https://vulkan.gpuinfo.org/displaydevicelimit.php?name=maxImageDimension2D&platform=linux) support **16384** or even **32768** texels. Check [vkGetPhysicalDeviceImageFormatProperties()](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/vkGetPhysicalDeviceImageFormatProperties.html) for the limits of your hardware and platform combination.

### Probe Data

This texture stores world-space offsets and classification states for all probes of a volume. This information is used in the [Probe Relocation](#probe-relocation) and [Probe Classification](#probe-classification) passes that optimize the probe configuration for image quality and performance.

- World-space offsets for probe relocation are stored in the XYZ channels.
  - ```ProbeDataCommon.hlsl``` contains helper functions for reading and writing world-space offset data.
- Probe classification state is stored in the W channel.

The texture's layout is the same as the irradiance and distance texture altases, except each probe uses just *one texel*. Below is a visualization of the probe data texture's world-space offsets (top) and probe states (bottom).

<figure>
<img src="images/ddgivolume-textures-probedata.jpg" width=800px></img>
<figcaption><b>Figure 6: The Probe Data texture (zoomed) from the Crytek Sponza scene</b></figcaption>
</figure>



## Updating a ```DDGIVolume```

Before tracing rays for a volume, call the ```DDGIVolume::Update()``` function to:
  - Compute a new random rotation to apply to the ray directions generated for each probe with ```DDGIGetProbeRayDirection()```.
  - Compute new probe scroll offsets and probe clear flags (if infinite scrolling movement is enabled).

Well distributed random rotations are computed using [James Arvo’s implementation from Graphics Gems 3 (pg 117-120)](http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.53.1357&rep=rep1&type=pdf) in the ```DDGIVolume::ComputeRandomRotation()``` function.

If ```Update()``` is not called, the previous rotation is used and the same data as the previous frame is unnecessarily recomputed. A common update frequency is to update the probes with newly ray traced data every frame; however, this is not the only option. Aternatively, updates may be scheduled at a lower frequency than the frame rate, or even as asynchronous workloads that execute continuously on lower priority background queues - essentially streaming radiance and distance data to ```DDGIVolume``` probes. This functionality is not directly implemented by the SDK, but the separation of functionality in the ```DDGIVolume::Update()``` and ```rtxgi::UpdateDDGIVolumeProbes()``` functions provides the flexibility for this possibility.



## Volume Movement

The simplest use of a ```DDGIVolume``` is to place it in a fixed location of the world that never changes. There are many scenarios where this basic use case is not sufficient though; for example, when the space requiring global illumination *is itself not stationary*. Examples of this include elevators, trains, cars, spaceships, and many more.

To achieve consistent results in moving spaces:
 - Attach the ```DDGIVolume``` to the moving space's reference frame.
 - Translate the volume using ```DDGIVolume::SetOrigin(...)```.
 - Rotate the volume using ```DDGIVolume::SetEulerAngles(...)```.

```DDGIVolume``` translation and rotation functions are also useful for in-editor debugging purposes, to visualize how various probe configurations function in different locations of a scene.

### Infinite Scrolling Movement

If the scene requiring dynamic global illumination exists in an open or large world setting, it may not be possible (or practical) to cover the entire space with one, or even multiple, ```DDGIVolume``` due to memory and/or performance limits. To address this problem and provide a solution with fixed memory and performance characteristics, the ```DDGIVolume``` includes a mode called *Infinite Scrolling Volume (ISV)*.

When enabled, an infinite scrolling volume provides a virtual infinite volume without requiring an unbounded number of probes (or memory to store them).

This is accomplished by:
  - Computing lighting for a subset of the infinite space. This is called the *active* space.
  - Moving - or *scrolling* - the active space through the infinite volume.

Critically, instead of adjusting the position of all probes when the active area is scrolled, *only the planes of probes on the edges of the volume* on the axis of motion are invalidated and must reconverge. *All interior probes remain stationary* and retain valid irradiance and distance values. This behavior is analagous to how tank tread rolls forward: the tread's individual segments remain in fixed locations while in contact with the ground.

<figure>
<img src="images/ddgivolume-movement-isv.gif" width="700px"></img>
<figcaption><b>Figure 7: Infinite Scrolling Volume Movement</b></figcaption>
</figure>

ISVs are also useful when dynamic indirect lighting is desired around the camera view or a player character. Anchor the infinite scrolling volume to the camera view or a player character and use the camera or player's movement to drive the volume's scrolling of the active area.

To use the ```DDGIVolume``` infinite scrolling feature, set ```DDGIVolumeDesc::movementType``` to ```EDDGIVolumeMovementType::Scrolling``` during initialization or call ```DDGIVolume::SetMovementType(EDDGIVolumeMovementType::Scrolling)```.

To adjust the active area of the ISV (i.e. perform scrolling):
  - Call ```DDGIVolume::SetScrollAnchor(...)```.
  - Provide a world-space position that the volume should scroll its probes towards. The volume will scroll planes of probes and attempt to place the active area's origin as close to the scroll anchor as possible.

**Other Notes:**
  - The amount of probe scrolling can be inspected with ```DDGIVolume::GetScrollOffsets()```.
  - Scroll offsets are reset to zero when probes complete an entire cycle.
    - i.e. when (probeScrollOffsets.x % probeCounts.x) == 0
  - Volumes with infinite scrolling movement ignore rotation transforms.
  - Volumes with infinite scrolling movement can be translated with ```DDGIVolume::SetOrigin(...)``` if the space itself moves too.

## Probe Relocation

Any regular grid of sampling points will struggle to robustly handle all content in all lighting situations. The probe grids employed by DDGI are no exception. To mitigate this shortcoming, the ```DDGIVolume``` provides a "relocation" feature that automatically adjusts the world-space position of probes at runtime to avoid common problematic scenarios (see below).

<figure>
<img src="images/ddgivolume-relocation-off.jpg" width=49%></img>
<img src="images/ddgivolume-relocation-on.jpg" width=49%></img>
<figcaption><b>Figure 8: (Left) Probes falling inside wall geometry without probe relocation. (Right) Probes adjusted to a more useful locations with probe relocation enabled.</b></figcaption>
</figure>

To use Probe Relocation:
  - Set ```DDGIVolumeDesc::probeRelocationEnabled``` to ```true``` during initialization or call ```DDGIVolume::SetProbeRelocationEnabled()``` at runtime.
  - Provide the compiled DXIL shader bytecode (or PSOs) for relocation during initialization.
    - See [Volume Shaders](#volume-shaders) for more information.
  - Call ```rtxgi::RelocateDDGIVolumeProbes(...)``` at render-time (at some frequency).

As shown in the above images, a common problem case occurs when probes land inside of geometry (e.g. walls). To move probes from the interior of geometry to a more useful location, probe relocation determines if the probe is inside of geometry and then adjusts its position. This is implemented by using a threshold value for backface probe rays hits. If enough of the probe's total rays are backfaces hits, then the probe is likely inside geometry. The ```DDGIVolumeDesc::probeBackfaceThreshold``` variable controls this ratio. When a probe is determined to be inside of geometry, the maximum adjustment to its position is 45% of the grid cell distance to prevent probes from being relocated outside of their grid voxel.

## Probe Classification

Probe classification aims to improve performance by identifying ```DDGIVolume``` probes that do not contribute to the final lighting result and disabling GPU work for these probes. For example, probes may be stuck inside geometry (even after relocation attempts to move them), exist in spaces without nearby geometry, or be far enough outside the play space that they are never relevant. In these cases, there is no need to spend time tracing and shading rays or updating the irradiance and distance texture atlases for these probes.

A per-probe state is computed on the GPU and maintained in the Probe Data texture. Each probe's *fixed rays* are used to determine if a probe is useful. See [Fixed Probe Rays for Relocation and Classsification](#fixed-probe-rays-for-relocation-and-classification) for more information. A probe's state can be retrieved using the ``DDGILoadProbeState()`` function. See [ProbeTraceRGS.hlsl](../samples/test-harness/shaders/ddgi/ProbeTraceRGS.hlsl) for an example and the [Shader API](ShaderAPI.md) for more information.

Classification is executed in two phases:
  - **Phase 1: Check if the probe is inside geometry**
    - Load the fixed ray distances and count the number of backface hits.
    - If the ratio of backface hits exceeds the threshold, mark the probe as inactive.
      - Classification is complete, Phase 2 is skipped.
  - **Phase 2: Check if the probe has nearby (front facing) geometry**
    - Iterate over the fixed rays (from Phase 1), get the ray direction, and find the intersection of the ray with the probe's relevant voxel planes.
    - Compare the distance stored for the ray with the intersection distance to the closest probe voxel plane.
    - If the ray distance is *less* than the plane intersection distance, mark the probe as active - there is geometry in the probe's voxel.
    - If no ray marks the probe as active, mark the probe as inactive.

<figure>
<img src="images/ddgivolume-classification-01.jpg" width=49%></img>
<img src="images/ddgivolume-classification-02.jpg" width=49%></img>
<figcaption><b>Figure 9: Disabled probes are highlighted with red outlines. Probes inside of geometry or with no surrounding geometry are disabled.</b></figcaption>
</figure>



## Fixed Probe Rays for Relocation and Classification

Since probe relocation and classification can run continuously at arbitrary frequencies, temporal stability is essential to ensure probe irradiance and distance values remain accurate and reliable. To generate stable results, a subset of the rays traced for a probe are designated as *fixed rays*. Fixed rays have the *same* direction(s) every update cycle and are **not** randomly rotated. Fixed rays are *excluded* from probe blending since their regularity would bias the radiance and distance results. Consequently, lighting and shading can be skipped for fixed rays since these rays are never used for irradiance.

The number of fixed rays is specified by the ```RTXGI_DDGI_NUM_FIXED_RAYS``` define, which is 32 by default. Note that all probes trace *at least* ```RTXGI_DDGI_NUM_FIXED_RAYS``` rays every update cycle to have sufficient data for the relocation and classification processes. See [ProbeTraceRGS.hlsl](../samples/test-harness/shaders/ddgi/ProbeTraceRGS.hlsl) for an example.

 Fixed rays are distributed uniformly over the unit sphere using the same spherical fibonnaci method as the rest of the probe rays. A visualization of the default 32 fixed rays is below.

<figure>
<img src="images/ddgivolume-fixedRays.gif"></img>
<figcaption><b>Figure 10: The default fixed rays distribution used in probe relocation and classification.</b></figcaption>
</figure>

## Rules of Thumb

Below are rules of thumb related to ```DDGIVolume``` configuration and how a volume's settings affect the lighting results and content creation.

### Geometry Configuration

- To achieve proper occlusion results from DDGI, **avoid representing walls with zero thickness planes**.
  - Walls should have a "thickness" that is proportional to the probe density of the containing ```DDGIVolume```.
- If walls are too thin relative to the volume's probe density, you will observe **light leaking**.
- The planes that walls consist of should be **single-sided**.
  - The probe relocation and probe classification features track backfaces in order to make decisions.

### Probe Density and Counts

- High probe counts within a volume are usually not necessary (with properly configured wall geometry).
- **We recommend a probe every 2-3 meters for typical use cases.**
- Sparse probe grids can often produce better visual results than dense probe grids, since dense probes grids localize the effect of each probe and can (at times) reveal the structure of the probe grid.
- When in doubt, use the minimum number of probes necessary to get the desired result.

### View Bias

- If light leaking occurs - and zero thickness planes are not used for walls - the volume's ```probeViewBias``` value probably needs to be adjusted.
  - **Reminder:** ```probeViewBias``` is a world-space offset along the camera view ray applied to the shaded surface point to avoid numerical instabilities when determining visibility.
  - Increasing the ```probeViewBias``` value pushes the shaded point away from the surface and further into the probe's voxel, where the variance of the probe's mean distance values is lower.
  - Since ```probeViewBias``` is a world-space value, scene scale matters! As a result, **the SDK's default value likely won't be what your content requires**.


