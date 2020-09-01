/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include <rtxgi/Types.h>
#include <rtxgi/ddgi/DDGIVolume.h>

#include "Common.h"
#include "Config.h"
#include "GLTF.h"
#include "Harness.h"
#include "ImGui.h"
#include "Input.h"
#include "Shaders.h"
#include "UI.h"
#include "Visualization.h"
#include "Window.h"

#include <iostream>
#include <fstream>

/**
 * Test Harness entry point.
 */
int Run(HINSTANCE hInstance, LPWSTR lpCmdLine)
{
    std::ofstream log;
    log.open("log.txt", std::ios::out);
    if (!log.is_open()) return EXIT_FAILURE;

    D3D12Global d3d;
    DXRGlobal dxr;
    D3D12Resources resources;
    ShaderCompiler shaderCompiler;
    std::vector<std::vector<ShaderProgram>> shaders;

    ConfigInfo config;
    Camera camera;
    Scene scene;
    LightInfo lights;
    
    InputInfo input;
    InputOptions inputOptions;
    RTOptions rtOptions;
    PostProcessOptions postOptions;
    VizOptions vizOptions;

    std::vector<rtxgi::DDGIVolume*> volumes;
    std::vector<rtxgi::DDGIVolumeDesc> volumeDescs;
    std::vector<rtxgi::DDGIVolumeResources> volumeResources;

    MSG msg = { 0 };
    HWND window;

    // Parse the command line and get the configuration filepath
    if (!Config::ParseCommandLine(lpCmdLine, config, log))
    {
        log.close();
        return EXIT_FAILURE;
    }

    // Read the config file from disk and initialize variables
    if (!Config::Load(config, lights, camera, volumeDescs, input, inputOptions, rtOptions, postOptions, vizOptions, log))
    {
        log.close();
        return EXIT_FAILURE;
    }

    d3d.width = input.width = config.width;
    d3d.height = input.height = config.height;
    d3d.vsync = config.vsync;

    shaderCompiler.root = config.root;
    shaderCompiler.rtxgi = config.rtxgi;

    // Create a D3D12 device
    log << "Creating D3D12 device...";
    if (!D3D12::CreateDevice(d3d))
    {
        log << "Failed to create the D3D12 device!\n";
        log.close();
        return EXIT_FAILURE;
    }
    log << "done.\n";

    // Create a window
    log << "Creating a window...";
    HRESULT hr = Window::Create(d3d.width, d3d.height, hInstance, window, L"RTXGI SDK Test Harness");
    if (!SUCCEEDED(hr))
    {
        log.close();
        return EXIT_FAILURE;
    }
    log << "done.\n";

    // Perform initialization tasks
    if (!Harness::Initialize(config, scene, d3d, dxr, resources, shaderCompiler, window, log))
    {
        log.close();
        return EXIT_FAILURE;
    }

    // Load and compile shaders
    if (!Harness::CompileVolumeShadersMulti(shaders, shaderCompiler, volumeDescs, log))
    {
        log.close();
        return EXIT_FAILURE;
    }

    // Create a RTXGI DDGIVolume
    if (!Harness::CreateVolumeMulti(d3d, resources, shaders, volumes, volumeDescs, volumeResources, log))
    {
        log.close();
        return EXIT_FAILURE;
    }

    // Create descriptors for the DDGIVolume probe textures and additional engine textures
    if (!Harness::CreateDescriptorsMulti(d3d, resources, volumes, log))
    {
        log.close();
        return EXIT_FAILURE;
    }

    // Create resources used to visualize the volume's probes
    if(!Harness::CreateProbeVisResourcesMulti(d3d, dxr, resources, volumes, log))
    {
        log.close();
        return EXIT_FAILURE;
    }
    log << "done.\n";

    log << "Main loop...\n";

    // Main loop
    bool hotReload = false;
    bool keyboardInput = false;
    bool mouseInput = false;
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Handle mouse and keyboard input
        float3 translation = { 0, 0, 0 };
        keyboardInput = Input::KeyHandler(input, config, inputOptions, vizOptions, camera, translation, postOptions.useDDGI, hotReload);
        mouseInput = Input::MouseHandler(input, camera, inputOptions);

        // Reset the frame number on camera movement (for path tracer)
        if (keyboardInput || mouseInput) d3d.frameNumber = 1;

        // Update the camera constant buffer
        memcpy(resources.cameraCBStart, &camera, sizeof(Camera));

        // Update the lights constant buffer
        memcpy(resources.lightsCBStart, &lights, sizeof(LightInfo));

        if(config.mode == ERenderMode::PathTrace)
        {
            if(config.ui) UI::OnNewFrame(d3d, dxr, config, camera, lights, volumes, input, inputOptions, rtOptions, vizOptions, postOptions);
            Harness::PathTrace(d3d, dxr, resources, rtOptions, postOptions);
            if (config.ui) UI::OnRender(d3d, resources);
        }
        else if(config.mode == ERenderMode::DDGI)
        {
            if (hotReload)
            {
                if (!Harness::HotReload(
                    config, lights, camera,
                    d3d, dxr, resources, shaders,
                    volumes, volumeDescs, volumeResources,
                    input, inputOptions, rtOptions,
                    postOptions, vizOptions, log))
                {
                    log << "Error: hot reload failed!\n";
                    log.close();
                    return EXIT_FAILURE;
                }
                continue;
            }

            // ImGui
            if (config.ui) UI::OnNewFrame(d3d, dxr, config, camera, lights, volumes, input, inputOptions, rtOptions, vizOptions, postOptions);

            // Determine which volume is selected
            int volumeSelect = min(inputOptions.volumeSelect, static_cast<int>(volumes.size() - 1));

            if (keyboardInput)
            {
                // Move the volume
                DDGIVolume* volume = volumes[volumeSelect];
#if RTXGI_DDGI_PROBE_SCROLL
                if (volume->GetMovementType() == EDDGIVolumeMovementType::Scrolling)
                {
                    // When the deadzone radii match the probe grid spacing, a single "layer" of
                    // probes will move when translation moves out of the deadzone ellipsoid
                    float3 deadzoneRadii = volume->GetProbeGridSpacing();
                    volume->Scroll(translation, deadzoneRadii);
                }
                else
                {
                    volume->Move(translation);
                }
#else
                volume->Move(translation);
#endif
            }

            for (size_t volumeIndex = 0; volumeIndex < volumes.size(); volumeIndex++)
            {
                // Update the volume's random rotation and constant buffer
                DDGIVolume* volume = volumes[volumeIndex];
                if (resources.volumeGroupCB)
                {
                    UINT64 groupOffset = (d3d.frameIndex * volumes.size() + volumeIndex) * GetDDGIVolumeConstantBufferSize();
                    volume->Update(resources.volumeGroupCB, groupOffset);
                }
            }

            for (size_t volumeIndex = 0; volumeIndex < volumes.size(); volumeIndex++)
            {
                // Ray trace from the probes
                DDGIVolume* volume = volumes[volumeIndex];
                Harness::RayTraceProbes(d3d, dxr, resources,
                    volume->GetProbeRTRadianceTexture(),
                    rtOptions,
                    volume->GetNumRaysPerProbe(),
                    volume->GetNumProbes(), static_cast<int>(volumeIndex));
            }

            for (size_t volumeIndex = 0; volumeIndex < volumes.size(); volumeIndex++)
            {
                // Update the RTXGI DDGIVolume data structure
                volumes[volumeIndex]->UpdateProbes(d3d.cmdList);
            }

            // Ray trace primary rays, compute and store the direct lighting
            Harness::RayTracePrimary(d3d, dxr, resources, rtOptions);

            // Ray trace AO rays at 1 sample per pixel, then bilaterally filter the results
            if (postOptions.useRTAO)
            {
                Harness::RayTraceAO(d3d, dxr, resources, postOptions);
                Harness::FilterAO(d3d, resources, postOptions);
            }

            // Render a debug visualization of the DDGIVolume probes
            if (vizOptions.showDDGIVolumeProbes)
            {
                for (size_t volumeIndex = 0; volumeIndex < volumes.size(); volumeIndex++)
                {
                    DXR::UpdateVisTLAS(d3d, dxr, resources, volumes[volumeIndex]->GetNumProbes(), vizOptions.probeRadius, volumeIndex);
                    Visualization::RenderProbes(d3d, dxr, resources, volumeIndex);
                }
            }

#if RTXGI_DDGI_PROBE_RELOCATION
            if (inputOptions.enableProbeRelocation)
            {
                // run every frame with full distance scale value for continuous relocation
                volumes[volumeSelect]->RelocateProbes(d3d.cmdList, 1.f);
            }
#endif // RTXGI_DDGI_PROBE_RELOCATION

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
            // Activate all probes.
            // This can be useful if the classifier has disabled probes and new geometry is spawned into the scene.
            if (inputOptions.activateAllProbes)
            {
                volumes[volumeSelect]->ActivateAllProbes(d3d.cmdList);
                inputOptions.activateAllProbes = false;
            }
            else
            {
                // Run probe classification
                if (inputOptions.enableProbeClassification)
                {
                    volumes[volumeSelect]->ClassifyProbes(d3d.cmdList);
                }
            }
#endif // RTXGI_DDGI_PROBE_STATE_CLASSIFIER

            // Run a fullscreen pass and composite direct lighting
            // with indirect lighting from the DDGIVolume
            Harness::RenderIndirect(d3d, dxr, resources, postOptions);

            // Render a debug visualization of the DDGIVolume buffers
            if (vizOptions.showDDGIVolumeBuffers)
            {
                Visualization::RenderBuffers(d3d, resources, vizOptions, volumeSelect);
            }

            // Render the user interface with ImGui
            if (config.ui)
            {
                UI::OnRender(d3d, resources);
            }
        }

        D3D12::SubmitCmdList(d3d);
        D3D12::Present(d3d);

        if(input.saveImage)
        {
            D3D12::ScreenCapture(d3d, config.screenshotFile);
            input.saveImage = false;
        }

        D3D12::MoveToNextFrame(d3d);
        D3D12::ResetCmdList(d3d);
    }

    D3D12::WaitForGPU(d3d);
    CloseHandle(d3d.fenceEvent);

    log << "Shutting down and cleaning up.\n";

    // Release the volumes and their resources
    for (size_t volumeIndex = 0; volumeIndex < volumes.size(); volumeIndex++)
    {
        volumes[volumeIndex]->Destroy();
#if !RTXGI_DDGI_SDK_MANAGED_RESOURCES
        Harness::DestroyVolumeResources(volumeResources[volumeIndex]);
#endif
        // Release the volume's shaders
        for (size_t shaderIndex = 0; shaderIndex < shaders[volumeIndex].size(); shaderIndex++)
        {
            RTXGI_SAFE_RELEASE(shaders[volumeIndex][shaderIndex].bytecode);
        }
        delete volumes[volumeIndex];
    }
    RTXGI_SAFE_RELEASE(resources.volumeGroupCB);

    UI::Cleanup();
    DXR::Cleanup(dxr);
    D3DResources::Cleanup(resources);
    D3D12::Cleanup(d3d);
    Shaders::Cleanup(shaderCompiler);
    GLTF::Cleanup(scene);

    DestroyWindow(window);
    log << "Done\n";
    log.close();

    return EXIT_SUCCESS;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Run the application
    int result = Run(hInstance, lpCmdLine);

    // Check for memory leaks
#if defined _CRTDBG_MAP_ALLOC
    _CrtDumpMemoryLeaks();
#endif

    // If an error occurred, spawn a message box
    if (result != EXIT_SUCCESS)
    {
        MessageBox(NULL, "An error occurred. See log.txt for details.", NULL, MB_OK);
    }

    return result;
}
