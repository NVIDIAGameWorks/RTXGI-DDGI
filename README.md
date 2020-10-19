# RTXGI SDK v1.1

This repository contains the RTX Global Illumination (RTXGI) SDK v1.1, the "Test Harness" sample application, the Unreal Engine 4 RTXGI plugin, and associated documentation.

* The core RTXGI SDK is in the `rtxgi-sdk` directory
* The sample application is the `samples` directory
* The UE4 RTXGI plugin is in the `ue4-plugin` directory
* The User Guide and documentation is in the `docs` directory

### System Requirements
* Windows 10 v1809 or higher
* Visual Studio 2017 or 2019
* Windows 10 SDK version 10.0.17763 or higher (can be installed using the Visual Studio Installer)
* CMake 3.14.5 or higher, [download here](https://cmake.org/download).
* Latest drivers for your GPU. NVIDIA drivers are [available here](http://www.nvidia.com/drivers).
* Any DXR enabled GPU. NVIDIA DXR enabled GPUs:
	* RTX 3090, 3080, 3070
	* RTX 2080 Ti, 2080 SUPER, 2080, 2070 SUPER, 2070, 2060 SUPER, 2060
	* GTX 1660 Ti, 1660 SUPER, 1660
	* GTX 1080 Ti, 1080, 1070, 1060 6GB (or higher)

### Getting Started
To see RTXGI in action, follow the below steps to build the SDK and run the Test Harness sample application:

1. Open Cmake
2. Copy the path to where you downloaded this source package. Add `/samples` to this path on the "source code" line
3. Copy the path to where you downloaded this source package. Add `/samples/build` to this path on the the "binaries" line (below "source code")
4. Select `Configure` in Cmake<br>
	4a. If using VS2017, you must also select `x64` as platform for generator in the dropdown<br>
	4b. If using VS2019, the platform is `x64` by default
5. Select `Generate` in Cmake
6. Open `samples/build/RTXGISamples.sln` in the build directory (created by CMake) and build the solution
7. Run `samples/runTestHarness.bat` to run the test harness sample application with the default settings

### Building the SDK Only
If you want to build just the RTXGI SDK, and aren't interested in samples, follow these steps:

1. Open Cmake
2. Copy the path to where you downloaded this source package, and add `/rtxgi-sdk/` to this path on the "source code" line
3. Copy the path to where you downloaded this source package, and add `/rtxgi-sdk/build` to this path on the "source code" line
4. Select `Configure` in Cmake<br>
	4a. If using VS2017, you must also select `x64` as platform for generator in the dropdown<br>
	4b. If using VS2019, the platform is `x64` by default	
5. Select `Generate` in Cmake
6. Open `rtxgi-sdk/build/RTXGI.sln` and build the solution. By default a static library is generated

### Test Harness Notes
- You can change the loaded scene through an initialization file passed to the Test Harness on the command line. See the `config/` directory.
- DDGIVolume, lights, camera, visualization, input, and scene settings can be modified in the initialization file.
- Two test scenes are included with the SDK distribution in GLTF format, a Cornell Box and the "Two Rooms" scene.
- An initialization file is also included for a third scene, the Crytek Sponza. You can download the scene from the [GLTF GitHub](https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/Sponza).
- A progressive path tracing mode is included for reference and can be toggled at runtime.
- A user interface is implemented with ImGui. It displays information that is useful when debugging and setting up new scenes / probe configurations.

### RTXGI in Unreal Engine 4
To get started with the Unreal Engine 4 RTXGI plugin, follow the Getting Started guide in the `Unreal Engine 4 Plugin` section of the documentation.
