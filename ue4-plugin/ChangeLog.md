# RTXGI Unreal Engine 4 Plugin Change Log

## 1.1.40

Features:
  * Increased the number of volumes supported in the indirect lighting pass from 6 to 12.
  * Screen-space indirect lighting resolution is now adjustable through cvars.
  * Support blending of more than 2 volumes.
  * Extended Blueprint functionality to enable control of DDGIVolume properties at runtime.
  * Added a button to the DDGIVolume component that clears all probe data for the volume.
  * Added new UI settings for volume probe visualization. Includes options for the different visualization modes (Irradiance, Hit Distance, etc).
  * Added an option to the RTXGI project settings to enable / disable DDGIVolume serialization (storing volume data to disk).
  * Improved the RTXGI Editor UI in a number of places. Updated and improved UI tooltips.

Bug Fixes:
  * Fixed several resource barrier and transition issues (PC and console).
  * Fixed incorrect sRGB conversion when loading GBuffer data in the ApplyLighting pass.
  * Fixed compilation issues when ray tracing is disabled.
  * Fixed compilation issues for Linux / Vulkan compilation.
  * Fixed an issue where DDGIVolumes with the infinite scroll option enabled incorrectly applied the parent actor's rotation transform to the volume.
  * Fixed an issue where DDGIVolumes with the infinite scroll option enabled would not clear probe data from scrolled probes on movement.
  * Fixed an issue where DDGIVolumes with the infinite scroll option *disabled* would be black when another DDGIVolume in the scene had this option enabled.

***

## 1.1.31

Features:
  * Adds the ability to rotate DDGIVolumes in the editor and in-game.
  * Adds the ability to enable or disable individual DDGIVolumes in the editor and in-game.
  * Updates engine-to-plugin interface and delegate callbacks. Use ```r.GlobalIllumination.ExperimentalPlugin [0|1]``` to enable or disable the plugin from the engine side.
  * Adds several improvements for infinite scrolling volumes.
  * Adds a GPU stat for RTXGI visualization passes.

Bug Fixes:
  * Fixes a control flow issue in the probe classification compute shader that stops back face hits from being treated as front faces hits.
  * Fixes a compilation issue on non-windows platforms.
  * Fixes an issue with perfect diffuse reflectors. Adds a BRDF absorption factor since perfect diffuse reflectors don't exist in the real world!
  * Fixes a numerical precision issue preventing radiance from reaching black when using 10-bit texture formats (in some cases).
  * Fixes an issue where inactive probes were contributing to indirect lighting.
  * Adds a fix for resource transition issues.
  * Adds a fix for RTXGI not running in shipping builds.
  * Adds a fix for RTXGI running even when ```r.RayTracing.ForceRayTracingEffects=0```.
  * Adds fixes for compilation errors on non-windows platforms.
  * Vulkan: adds a fix for loaded DDGIVolume textures not being copied to the GPU.
  * Vulkan: adds a fix for a crash on shutdown.

***

## 1.1.23

Features:
  * Adds support for 4.26.0 (Epic and NvRTX).
  * Adds a "LightingPriority" property to DDGIVolumes that allow a user to specify priority when volumes are being selected for application to lighting.
  * Cleans up the volume culling and sorting code. Volumes are sorted by light priority, density, and then again by density across all light priorities after the final truncated list is determined.
  * Swaps the probe states resource to an SRV to avoid the UAV limitations UE4 imposes to enable the inactive probes contribution fix from the SDK.
  * Updates documentation.
  
Bug Fixes:
  * Swaps packed irradiance textures to R10G10B10 format. Using 10-bits for all channels prevents numerical precision issues from causing discoloration artifacts in some cases.
  * Adds a fix for numerical precision issues preventing radiance from reaching black when using 10-bit texture formats in some cases.
  * Adds a fix that prevents inactive probes from contributing to indirect lighting.
  * Adds a fix for Issue #30: a crash when running a packaged build from projects that have the async loading thread feature enabled (contributed by Robert Kihl, Embark Studios).
  * Updates resource barriers to the new 4.26 transition API (4.26 plugins only).
  * Corrects and updates the plugin metadata.

***

## 1.1.0
  * Initial release.
  * Mainline UE4.25.3 and NVIDIA RTX 4.25.3 (NvRTX) supported.
