# RTX Global Illumination (RTXGI)

This repository contains the RTX Global Illumination (RTXGI) SDK, a sample application that demonstrates how to use the SDK, patches and source code for the RTXGI Unreal Engine 4 plugin, and associated documentation for the SDK and UE4 plugin.

The `SDK` is in the `rtxgi-sdk` directory<br>
The `SDK Sample Application` is the `samples` directory<br>
The `Unreal Engine 4 plugin` patches and source code are in the `ue4-plugin` directory<br>
The `User Guide and Documentation` are in the `docs` directory<br>

The change log for the SDK and UE4 plugin is now available in the [Change Log](docs/manual/Changelog.html) section of the documentation.

# RTXGI SDK

The RTXGI SDK is a software development kit that leverages GPU ray tracing to provide scalable solutions for the computation of Global Illumination (GI) in real-time graphics applications.

A primary goal of this SDK is to maintain optimized implementations of global lighting algorithms that are flexible enough to be useful in both real-time and pre-computed lighting scenarios. This scalable design allows RTXGI to be an effective tool across a wide range of platforms of varying computational capability. Algorithm implementations provided in this SDK require support for Microsoft's DirectX® Raytracing (DXR) API since the ability to trace arbitrary rays is an essential component that provides the flexibly to dynamically update lighting information at runtime. It is also possible to use RTXGI on development machines with DXR support and then load pre-computed RTXGI data for runtime use on platforms without DXR support.

The SDK implements the _Dynamic Diffuse Global Illumination (DDGI)_ algorithm, [first described in this academic publication from NVIDIA Research](http://jcgt.org/published/0008/02/01/) along with collaborators at McGill University and the University of Montreal. Since this algorithm is based on precomputed irradiance probes, a common solution employed in real-time rendering today, the DDGI algorithm serves as an ideal entry point for developers to bring the benefits of real-time ray tracing to their tools, technology, and applications without substantial effort or runtime performance penalties. Furthermore, the implementation in this SDK includes performance improvements and optimization techniques not available elsewhere.

### System Requirements
* Windows 10 v1809 or higher
* Visual Studio 2017 or 2019
* Windows 10 SDK version 10.0.17763 or higher (can be installed using the Visual Studio Installer)
* CMake 3.14.5 or higher, [download here](https://cmake.org/download).
* Latest drivers for your GPU. NVIDIA drivers are [available here](http://www.nvidia.com/drivers).
* Any DXR enabled GPU. NVIDIA DXR enabled GPUs:
	* RTX 3090, 3080 Ti, 3080, 3070 Ti, 3070, 3060 Ti, 3060
	* RTX 2080 Ti, 2080 SUPER, 2080, 2070 SUPER, 2070, 2060 SUPER, 2060
	* GTX 1660 Ti, 1660 SUPER, 1660
	* GTX 1080 Ti, 1080, 1070, 1060 6GB (or higher)

### Getting Started
Follow the below steps to build the SDK and run the Test Harness Sample Application:

1. Open CMake (cmake-gui)
2. Copy the path to where you cloned this source package. Add `/samples` to this path on the "source code" line
3. Copy the path to where you cloned this source package. Add `/samples/build` to this path on the the "binaries" line (below "source code")
4. Select `Configure` in CMake<br>
	4a. If using VS2017, you must also select `x64` as platform for generator in the dropdown<br>
	4b. If using VS2019, the platform is `x64` by default
5. Select `Generate` in CMake
6. Open `samples/build/RTXGISamples.sln` in the build directory (created by CMake) and build the solution
7. Run `samples/runTestHarness.bat` to run the sample application with default settings

### Building the SDK Only
If you want to build just the RTXGI SDK library, follow these steps:

1. Open CMake (cmake-gui)
2. Copy the path to where you cloned this source package, and add `/rtxgi-sdk/` to this path on the "source code" line
3. Copy the path to where you cloned this source package, and add `/rtxgi-sdk/build` to this path on the "source code" line
4. Select `Configure` in CMake<br>
	4a. If using VS2017, you must also select `x64` as platform for generator in the dropdown<br>
	4b. If using VS2019, the platform is `x64` by default	
5. Select `Generate` in CMake
6. Open `rtxgi-sdk/build/RTXGI.sln` and build the solution. A static library named `RTXGI.lib` is generated.

### Sample Application Notes
- You can change the loaded scene through an initialization file passed to the application on the command line. See the `config/` directory.
- DDGIVolume, lights, camera, visualization, input, and scene settings can be modified in the initialization file.
- Two test scenes are included with the SDK distribution in GLTF format, a Cornell Box and the "Two Rooms" scene.
- An initialization file is also included for a third scene, the Crytek Sponza. You can download the scene from the [GLTF GitHub](https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/Sponza).
- A progressive path tracing mode is included for reference and can be toggled at runtime.
- A user interface is implemented with ImGui. It displays information that is useful when debugging and setting up new scenes / probe configurations.

## UE4 Plugin

To bring the benefits of RTXGI to as many developers as possible, all RTXGI 1.1 features are now available in Unreal Engine 4 through the RTXGI UE4 plugin.

### Getting Started (Artists)
A binary version of the latest RTXGI plugin is available from [the plugin's Unreal Engine Marketplace page](https://www.unrealengine.com/marketplace/en-US/product/nvidia-rtx-global-illumination). This version of the RTXGI plugin is compatible with Unreal Engine 4.27.

### Getting Started (Programmers)

**We strongly recommend you upgrade to 4.27 and use the latest plugin code**.

| **Version Recommendations** |
| -------- |
| **Use Plugin v1.1.40 (and later) with UE4.26.2 (and later)** |
| Use Plugin v1.1.30 with earlier versions of UE4.26
| Use Plugin v1.1.23 with UE4.25 |

#### Vanilla UE4 and Custom Engine Branches
* Source code and [documentation](https://github.com/NVIDIAGameWorks/RTXGI/blob/main/ue4-plugin/4.27/RTXGI/README.md) for the latest RTXGI plugin is available in the `ue4-plugin/4.27/` directory. 
* If you are using the 4.26.1 engine version, either backport the latest plugin source or apply the v1.1.30 plugin patch.
* If you are using older 4.25 or 4.26 engine versions, either backport the latest plugin source or apply the v1.1.23 plugin patch.

#### NvRTX
* The latest RTXGI plugin also now comes *pre-installed* in the [NVIDIA RTX branch of UE4 (NvRTX)](https://developer.nvidia.com/unrealengine)!
* To use NvRTX, you’ll need a GitHub account and an Epic Games account that are linked. Instructions to do this are available at https://www.unrealengine.com/en-US/ue4-on-github. Once that process is complete, you will be able to access the NvRTX GitHub repository at https://github.com/NvRTX/UnrealEngine. 

