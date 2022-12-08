# RTXGI SDK Change Log

## 1.3.5

### SDK
- **Improvements**
  - Adds the new **Probe Variability** feature to the ```DDGIVolume```
    - This is an optional feature that tracks the [coefficient of variation](https://en.wikipedia.org/wiki/Coefficient_of_variation) of a ```DDGIVolume```
    - This can be used to estimate of how converged the probes of the volume are. When the coefficient settles around a small value, it is likely the probes contain representative irradiance values and ray tracing and probe updates can be disabled until an event occurs that invalidates the light field
    - See [Probe Variability](docs/DDGIVolume.md#probe-variability) in the documentation for more details
  - Adds changes to ```DDGIVolume``` D3D12 resource transitions based on feedback from GitHub Issue #68 (thanks!)
    - ```UpdateDDGIVolumes()``` can now be safely used on direct *and* compute command lists
    - Irradiance, Distance, and Probe Data resources are now expected to be in the ```D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE``` state by default
    - These resources can be transitioned to the required states for each workload using the new ```DDGIVolume::TransitionResources(...)``` function where appropriate (also see ```EDDGIExecutionStage```)

### Test Harness
- **Improvements**
  - Adds support for the SDK's new [Probe Variability](docs/DDGIVolume.md#probe-variability) feature, including buffer visualization, UI toggles, and checks to disable/enable probe traces based on volume variability
  - Adds support for Shader Execution Reordering in DDGI probe ray tracing and the reference Path Tracer (D3D12 only). Requires an RTX 4000 series (Ada) GPU.
  - Adds NVAPI as a new dependency (Test Harness only)
  - Improves acceleration structure organization
    - Reorganizes how BLAS are created from GLTF2 Mesh and MeshPrimitives
      - MeshPrimitives are now geometries of the same BLAS (instead of individual BLAS)
      - This prevents bad traversal characteristics when MeshPrimitives create substantially overlapping BLAS and increases trace performance up to 2x
    - Adds the GeometryData and MeshOffsets indirection buffers for looking up MeshPrimitive information
      - Updates RGS and Hit Shaders to look up MeshPrimitive information using DXR 1.1 GeometryIndex() and the new indirection buffers
  - Updates Closest Hit shaders to conform with the [GLTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#metallic-roughness-material) for how albedo values sampled from texture should be combined with ```baseColorFactor```. Fixes GitHub Issue #67.
  - Updates scene cache serialization/deserialization
    - Stores new information and now stores a scene cache file for .glb scenes too
- **Bug Fixes**
  - Updates DXC binaries to v1.7.2207 (on Windows) to fix a shader compilation issue
  - Fixes issues with ```DDGIVolume``` name strings not being handled properly
  - Fixes D3D12 resource state problems caught by the debug layer
  - Fixes a handful of other minor issues


## 1.3.0

### SDK
- **Dynamic Library**
  - Adds support for building and using the SDK as a dynamic library (`*.dll`, `*.so`)
  - Removes the use of std in SDK headers
- **DDGIVolume Probe Counts** 
  - Changes all GPU-side texture resources to ```Texture2DArrays``` (D3D12 and Vulkan)
  - Increases the *theoretical* maximum number of probes per volume to ~33.5 million (128x2048x128)
  - Increases the *practical* maximum probes per volume increases to 2,097,152 probes (with the default texel format settings)
    - This is a 2x increase over the previous maximum of 1024x32x32 (1,048,576 probes) and is limited by API restrictions: a single D3D12/Vulkan resource cannot be larger than 4GB
    - More importantly, this change enables a more flexible and (at times) less wasteful memory layout since monolithic 2D texture atlases are no longer used for ```DDGIVolume``` probe resources
- **Bindless Resources**
  - Adds support for [Shader Model 6.6 Dynamic Resources](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_DynamicResources.html) (aka Descriptor Heap bindless) in D3D12
  - Reworks how bindless resource indices are specified for both Resource Array and Descriptor Heap style bindless implementations (D3D12 and Vulkan)
- **Probe Border Updates, Probe Blending, and GPU Utilization**
  - Moves probe border update work into the probe blending passes
    - *This removes the need for separate border update compute passes*, simplifying SDK use while also providing a small performance benefit
  - Adjusts probe blending thread group sizes to increase GPU utilization and enable **probe ray counts in factors of 64** to be stored in shared memory (no more ```unusual``` ray counts like 144, 288, etc).
- **Vulkan Push Constants**
  - Updates the Vulkan push constants implementation to be more flexible and customizable so you can tailor them to your application's setup
- **Other Improvements and Fixes**
  - Represents probe counts with 10 bits (instead of 8) in ```DDGIVolumeDescGPUPacked```
  - Adds ```GetRayDispatchDimensions()``` to make probe ray tracing dispatches easier to setup
  - Adds ```GetGPUMemoryUsedInBytes()``` to make ```DDGIVolume``` memory tracking easier
  - Adds ```ValidatePackedData()``` to ensure correct functionality of the ```PackDDGIVolumeDescGPU()``` and ```UnpackDDGIVolumeDescGPU()``` functions, executes in Debug configurations as part of ```UploadDDGIVolumeConstants()```.
  - Moves shader define validation code to separate files to make the main SDK shader code more friendly and readable
  - Adds fix for ```EulerAnglesToRotationMatrixYXZ()``` not producing correct ```DDGIVolume``` rotations across all coordinate systems
  - Adds ```ConvertEulerAngles()``` convenience function to convert euler angles to alternate coordinate systems

### Test Harness

- **Improvements**
  - Adds support for RTXGI SDK ```v1.3.0``` features
  - Reorganizes the (D3D12) global root signature / (Vulkan) pipeline layout to bindlessly index texture array resources
  - Eliminates the visualization-specific descriptor heap - all descriptors are now part of the global descriptor heap
  - Uses the D3D12 Agility SDK v1.606.3 to access Shader Model 6.6+ features
  - Reworks shader compilation code to use latest DXC interfaces, explicitly links DXC
  - Adds support for storing ```DDGIVolume``` texture array data to disk (for debug / regression)
  - Sets the `COORDINATE_SYSTEM` define through CMake, based on the SDK's `RTXGI_COORDINATE_SYSTEM` define. The Test Harness and RTXGI SDK can no longer have mismatching coordinate systems.
  - Scene graph node transforms are now converted to the selected coordinate system
  - Test Harness converts all config file positions, directions, and rotations to the selected coordinate system
    - All scene config files updated to specify right hand, y-up positions, directions, and rotations.
  - Scene Cache
    - Coordinate system of the scene geometry is now stored in the cache file
    - Cache files with mismatching coordinate system are rejected and the cache file is rebuilt
    - Cache files are built for binary (.glb) GLTF files too now
- **Bug Fixes**
  - Adds several fixes for issues with Vulkan timestamps
  - Adds fix to properly compute bounding boxes, taking mesh instance transforms into account for the chosen coordinate system
  - Adds fix so probe visualization spheres are not inside out in left handed coordinate systems



## 1.2.13

### SDK
Bug Fixes:
- Fixes a bit packing issue in `DDGIVolumeDescGPU.h` 
  - This bug causes the irradiance texture format to be improperly packed and unpacked. This problem may also affect feature bits, depending on which ones are set.

### Test Harness
Bug Fixes:
- Fixes Vulkan validation layer errors related to the ray query extension not being enabled

## 1.2.12a

### SDK
- No changes

### Test Harness
Features and Improvements:
- Uses stb (stb_image_write) for image saving and screenshot functionality (included with the tinygltf dependency)
- Removes zlib-ng and libpng dependencies
- Updates Windows DXC (packman) dependencies to version 1.6.2112 which includes ```dxil.dll```
- Now using std::filesystem on Windows and Linux
- CMake
  - Adds option to select in CMake whether to use the DXC & DXIL binaries from Packman or an installed Win10 SDK  (Windows only)
  - Properly sets the default startup project for RTXGI.sln (it really works now!)
  - Adds arguments to automatically set the available config ini files for the SmartCmdArgs extension
  - Removes DirectXTK ```ScreenGrab[.h|.cpp]``` files

Bug Fixes:
- Fixes VK validation layer error related to the back buffer not being marked as a possible copy source
- Fixes D3D12 and VK swapchain creation failures (causing a crash) when alt+tabbing or minimizing the application when in fullscreen mode

## 1.2.12

### SDK

Features and Improvements:
- Adds support to DDGI probes for 16-bit / channel irradiance

### Test Harness

Features and Improvements:
- Improvements to the benchmark mode
- Swaps to [zlib-ng](https://github.com/zlib-ng/zlib-ng) since it is more CMake/Git friendly
- Other improvements for CMake
- Adds SDK version number to the debug UI

Bug Fixes:
- Fixes compilation regressions on Linux

## 1.2.11

### SDK

Features and Improvements:
- Improved flexibility for specifying thresholds for probe ray backface hits
  - The backface hit threshold used in ```ProbeBlendingCS``` is now adjustable
  - Thresholds for random rays (blending) and fixed rays (relocation/classification) can now be specified separately
  - **Note:** this change modifies the ```DDGIVolumeDescGPU``` struct
- Improved lighting responsiveness after a probe is scrolled (and cleared)
  - Drops hysteresis to zero for the first update cycle after a probe is scrolled and cleared
  - **Note:** hysteresis is now set to zero for all probe texels where the previous irradiance is zero in all color channels
- Renames ```DDGIResetScrolledPlane()``` to ```DDGIClearScrolledPlane()``` to better reflect its purpose
- Adds option to use shared memory during probe scroll clear testing
  - When enabled, scroll clear tests will only run for the first thread of a blending thread group
  - ```RTXGI_DDGI_BLEND_SCROLL_SHARED_MEMORY``` is now a required define for ```ProbeBlendingCS.hlsl```
  - Can be a performance win on some hardware

Bug Fixes:
- Fixes issue where some probes were not being cleared properly during scrolling events ([Issue #62](https://github.com/NVIDIAGameWorks/RTXGI/issues/62))
  - **Note:** this change modifies the ```DDGIVolumeDescGPU``` struct

### Test Harness

Features and Improvements:
- Improves support for loading GLTF files that contain geometry primitives without assigned materials ([Issue #61](https://github.com/NVIDIAGameWorks/RTXGI/issues/61))
- Updates UI to reflect the separate random and fixed probe ray threshold options

## 1.2.07

### SDK

Features and Improvements:
- Shader improvements for probe data reads/writes
  - The probe data texture now uses the same layout as the irradiance and distance textures
  - This change makes probe data visualizations easier to understand
- Adds a RWTexture2D variant of ```DDGILoadProbeState()```
- Uses the ```DDGILoadProbeState()``` convenience function where appropriate
- Moved the ```data``` member of the ```DDGIVolumeDescGPU``` struct inside the ```GetPackedData()``` function so the struct size is the same on the CPU and GPU
- Updated stale code comments (non-functional)
- Bumped SDK revision number and version string

Bug Fixes:
- ```DDGIGetVolumeBlendWeight```: fixed regression related to volume edge fade not respecting rotations

### Test Harness

Features and Improvements:
- ```ProbeTraceRGS.hlsl```: no longer performs lighting for fixed rays when probe relocation is enabled (performance optimization)
- Adds a performance benchmark mode that collects CPU and GPU performance data and outputs the results to ```*.csv``` files
  - Press the ```F2``` key to run a benchmark
- Adds the ability to store intermediate textures (back buffer, GBuffer, RTAO, and DDGIVolume textures) to disk
  - Uses the zlib and libpng libraries for cross-platform compatibility
  - Press the ```F1``` key to save the current image data to disk
- ```VolumeTexturesCS.hlsl```: minor updates to the layout order of visualized textures
- Config file and documentation updates

## 1.2.04

### SDK

Features and Improvements:
- Bumped SDK revision number and version string
- Updated copyright dates

Bug Fixes:
- Fixed regression related to distance values sampled from probe distance textures

### Test Harness

Features and Improvements:
- Relative paths defined in configuration ```.ini``` files are now interpreted as relative to the ```.ini``` file, instead of the Visual Studio working directory
  - This should be easier to use than the previous behavior
- CMake: the ```nvidia.jpg``` copy operations now use ```CMAKE_BINARY_DIR```
- Added version number and version string assert checks during DDGI initialization
- Updated documentation with DDGIVolume Rules of Thumb
- Updated copyright dates

Bug Fixes:
- Fixed regression related to distance values sampled from probe distance textures

## 1.2.0

### SDK

Features and Improvements:
  * Substantial code and API refactor
  * Added support for Vulkan
  * Added support for Linux (x86 and ARM)
  * Added support for bindless resource access models
  * Infinite Scrolling Volumes (ISV) feature is now available (no longer beta)
  * Improved multi-volume support, including GPU workload batching
  * Improved shaders with define driven feature and resource declarations
    * Shaders can now be used directly in any codebase that supports D3D12 or Vulkan, regardless of resource binding model
  * Improved Probe Classification accuracy and increased performance gains when classification is enabled
  * Moved ```DDGIVolume``` constants to structured buffer(s)
  * Reduced the size of ```DDGIVolume``` constants by 2x
  * Merged per-probe relocation and classification data into a single texture resource
    * Removed the R8 UINT texture resource used with classification
  * Substantial CMake and build system improvements
    * Removed unnecessary third party dependencies
    * Refactored and simplified the defines system
  * Refreshed and improved documentation and converted to Markdown

Bug Fixes:
  * Infinite scrolling volumes now properly clear irradiance and distance data for probes that are scrolled
  * Probe classification now accurately tests each probe ray's hit distance against the probe voxel's bounding planes
  * Fixed probe rays no longer perform unnecessary shading and lighting
  * Fixed regression related to distance blending and sampling

### Test Harness

Features and Improvements:
  * Substantial code redesign and overhaul
  * Added support for Vulkan
  * Added support for Linux (x86 and ARM)
  * Added support for a bindless resource access in all shaders
  * Added support for compressing textures to BC7 format on the CPU or GPU (using DirectXTex)
  * Added support for creating texture mipmaps (using DirectXTex)
  * Added support for using BC7 compressed textures on the GPU
  * Added support for using mipmapped textures on the GPU
  * Added support for an arbitrary number of lights
  * Added hot shader reloading
  * Added primary ray texture level of detail with ray differentials
  * Added a binary scene cache system for fast runtime scene loading
  * Added cross-platform windowing and input with GLFW
  * Added CPU and GPU timestamp instrumentation
  * Moved RTXGI DDGI indirect lighting pass to compute
  * All GPU resources moved off of the upload heap to the device (default heap)
  * Substantial CMake and build system improvements
    * Third party dependencies now included as proper Git Submodules
    * DXC binaries now delivered through packman
    * Refactored and simplified defines system
  * Added new "Tunnel" and "Cornell Boxes" test scenes
  * Refreshed and improved the configuration file system
  * Refreshed and improved UI

Bug Fixes:
  * Fixed vsync and fullscreen crashing issues


## 1.1.31

Features:
  * Adds the ability to rotate DDGIVolumes.

Bug Fixes:
  * Fixes a control flow issue in the probe classification compute shader that stops back face hits from being treated as front faces hits.
  * Fixes a compilation issue on non-windows platforms.
  * Fixes an issue with perfect diffuse reflectors. Adds a BRDF absorption factor since perfect diffuse reflectors don't exist in the real world!
  * Fixes a color shift issue. Swaps packed irradiance textures to R10G10B10 format. Using 10-bits for all channels prevents numerical precision differences from causing discoloration artifacts in some cases.
  * Fixes a numerical precision issue preventing radiance from reaching black when using 10-bit texture formats (in some cases).
  * Fixes an issue where inactive probes were contributing to indirect lighting.

## 1.1.23

Bug Fixes:
  * Swaps packed irradiance textures to R10G10B10 format. Using 10-bits for all channels prevents numerical precision issues from causing discoloration artifacts in some cases.
  * Adds a fix for numerical precision issues preventing radiance from reaching black when using 10-bit texture formats in some cases.
  * Adds a fix that prevents inactive probes from contributing to indirect lighting.
  * [Test Harness] Adds a fix that includes a BRDF absorption factor. Perfect diffuse reflectors don't exist in the real world!
  * [Test Harness] Minor change to normal transformation for rigor. No functional change.
  * [Test Harness] Other minor fixes.

## 1.1.22

Features:
  * [Test Harness] Improves probe visualization.

Bug Fixes:
  * [Test Harness] Fixes sRGB conversion issues in probe visualization
  * [Test Harness] Adds irradiance boost in probe visualization
  * [Test Harness] Removes duplicate application of tonemapping in probe visualization
  * [Test Harness] Removes dead and duplicate code

## 1.1.20

Features:
  * Improvements to DDGI math
  * Adds define to swap the probe distance texture format
  * Makes FP16 the default probe distance format. The blending math change makes FP16 viable
  * Adds and uses 2PI static constant
  * Bumps SDK revision number

Test Harness:
  * Updates probe irradiance and distance reconstruction to account for math changes in the SDK.
  * Updates random hemisphere sampling code. Adds cosine distribution version.
  * Updates the Path Tracer to use new random sampling and throughput computation.
  * Improves ProbeTraceRGS and PrimaryTraceRGS logic.
  * Adds SkyIntensity term. Sky (miss) lighting is no longer hardcoded in the Miss shader. Exposes this value to Config files.
  * Exposes AOPower setting to Config files.
  * ```BaseColor``` renamed ```Albedo``` to match PBRT terminology.
  * Adds Furnace Test scene.
  * Updates probe configuration in Sponza.
  * Updates Two Rooms ground material to have less of a red hue. With the sampling improvements, the red gets very intense with many bounces.
  
Bug Fixes:
  * Fixes probe blending and irradiance reconstruction to properly account for summing the cosine weights (was previously off by a factor of 2)
  * Adds a 9% boost to the final irradiance estimate due to energy loss from precision is
  * [Test Harness] Fixes broken shading normals in CHS.

## 1.1.0

Features:
  * Support for multiple coordinate systems improved.

Test Harness:
  * Asset loading refactor. Moves to GLTF2 interchange format.
  * Other substantial infrastructure improvements.
  * Path tracer improvements.

## 1.0.0
Initial release