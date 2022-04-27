# RTXGI SDK Change Log

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