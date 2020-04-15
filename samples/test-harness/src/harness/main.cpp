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

#include <iostream>
#include <fstream>

#include <rtxgi/Types.h>
#include <rtxgi/ddgi/DDGIVolume.h>

#include "Common.h"
#include "Config.h"
#include "Geometry.h"
#include "Harness.h"
#include "ImGui.h"
#include "Input.h"
#include "Shaders.h"
#include "UI.h"
#include "Visualization.h"
#include "Window.h"

/**
 * Test Harness entry point.
 */
int Run(HINSTANCE hInstance, LPWSTR lpCmdLine)
{
    ofstream log;
    log.open("log.txt", ios::out);
    if (!log.is_open()) return EXIT_FAILURE;

    D3D12Info d3d;
    DXRInfo dxr;
    D3D12Resources resources;
    D3D12ShaderCompiler shaderCompiler;
    vector<D3D12ShaderInfo> shaders;

    ConfigInfo config;
    LightInfo lights;
    CameraInfo camera;
    InputInfo input;
    InputOptions inputOptions;
    RTOptions rtOptions;
    PostProcessOptions postOptions;
    VizOptions vizOptions;

    rtxgi::DDGIVolume* volume = nullptr;
    rtxgi::DDGIVolumeDesc volumeDesc = {};
    rtxgi::DDGIVolumeResources volumeResources = {};

    MSG msg = { 0 };
    HWND window;

    // Parse the command line and get the configuration filepath
    if (!Config::ParseCommandLine(lpCmdLine, config, log))
    {
        log.close();
        return EXIT_FAILURE;
    }
            
    // Read the config file from disk and initialize variables
    if (!Config::Load(config, lights, camera, volumeDesc, input, inputOptions, rtOptions, postOptions, vizOptions, log))
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
    if (!Harness::Initialize(config, d3d, dxr, resources, shaderCompiler, window, log))
    {
        log.close();
        return EXIT_FAILURE;
    }

    // Load and compile shaders
    if (!Harness::CompileShaders(shaders, shaderCompiler, volumeDesc, log))
    {
        log.close();
        return EXIT_FAILURE;
    }
            
    // Create a RTXGI DDGIVolume
    if (!Harness::CreateVolume(d3d, resources, shaders, volume, volumeDesc, volumeResources, log))
    {
        log.close();
        return EXIT_FAILURE;
    }

    // Create descriptors for the DDGIVolume probe textures
    if (!Harness::CreateDescriptors(d3d, resources, volume, log))
    {
        log.close();
        return EXIT_FAILURE;
    }
            
    // Create resources used to visualize the volume's probes
    if(!Harness::CreateProbeVisResources(d3d, dxr, resources, volume, log))
    {
        log.close();
        return EXIT_FAILURE;
    }
    log << "done.\n";

    log << "Main loop...\n";

#if RTXGI_DDGI_PROBE_RELOCATION
    // Initialize probe relocation
    int remainingRelocationIterations = RTXGI_DDGI_MAX_PROBE_RELOCATION_ITERATIONS;
#endif

    // Main loop
    bool hotReload;
    bool movement = false;
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        float3 volumeTranslation = { 0, 0, 0 };
        movement = Input::KeyHandler(input, config, inputOptions, vizOptions, camera, volumeTranslation, postOptions.useDDGI, hotReload);
        movement |= Input::MouseHandler(input, camera, inputOptions);

        if (movement) d3d.frameNumber = 1;
#if RTXGI_DDGI_PROBE_RELOCATION 
        if (inputOptions.runProbeRelocation)
        {
            remainingRelocationIterations = RTXGI_DDGI_MAX_PROBE_RELOCATION_ITERATIONS;
            inputOptions.runProbeRelocation = false;
        }
#endif

        // Update the camera constant buffer
        memcpy(resources.cameraCBStart, &camera, sizeof(CameraInfo));

        // Update the lights constant buffer
        memcpy(resources.lightsCBStart, &lights, sizeof(LightInfo));

        if(config.mode == ERenderMode::PathTrace)
        {
            if(config.ui) UI::OnNewFrame(d3d, dxr, config, camera, lights, volume, input, inputOptions, rtOptions, vizOptions, postOptions);
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
                    volume, volumeDesc, volumeResources,
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
            if (config.ui) UI::OnNewFrame(d3d, dxr, config, camera, lights, volume, input, inputOptions, rtOptions, vizOptions, postOptions);

            // Move the volume
            volume->Move(volumeTranslation);

            // Update the volume's random rotation and constant buffer
            UINT64 offset = d3d.frameIndex * GetDDGIVolumeConstantBufferSize();
            volume->Update(resources.volumeCB, offset);

            // Ray trace from the probes
            Harness::RayTraceProbes(d3d, dxr, resources,
                volume->GetProbeRTRadianceTexture(),
                rtOptions,
                volume->GetNumRaysPerProbe(),
                volume->GetNumProbes());

            // Update the RTXGI DDGIVolume data structure
            volume->UpdateProbes(d3d.cmdList);

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
                DXR::UpdateVisTLAS(d3d, dxr, resources, volume->GetNumProbes(), vizOptions.probeRadius);
                Visualization::RenderProbes(d3d, dxr, resources);
            }

#if RTXGI_DDGI_PROBE_RELOCATION
            if (remainingRelocationIterations >= 0)
            {
                // Compute distance scale which starts at 1.f on the first iteration and descends to 0.f on the last
                float probeDistanceScale = ((float)remainingRelocationIterations / (float)RTXGI_DDGI_MAX_PROBE_RELOCATION_ITERATIONS);
                volume->RelocateProbes(d3d.cmdList, probeDistanceScale);
                remainingRelocationIterations--;
            }
#endif /* RTXGI_DDGI_PROBE_RELOCATION */

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
            // Activate all probes if necessary.
            // This is necessary if the classifier has disabled probes
            // and any of the following happen:
            //      1) the volume moves
            //      2) new geometry is spawned into the scene
            //      3) existing geometry moves quickly in the scene
            if (inputOptions.activateAllProbes)
            {
                volume->ActivateAllProbes(d3d.cmdList);
                inputOptions.activateAllProbes = false;
            }
            else
            {
#if RTXGI_DDGI_PROBE_RELOCATION
                // Run probe classification, if enabled and probe relocation optimizer is finished
                if (inputOptions.enableProbeClassification && remainingRelocationIterations <= 0)
                {
                    volume->ClassifyProbes(d3d.cmdList);
                }
#else /* !RTXGI_DDGI_PROBE_RELOCATION */
                // Run probe classification, if enabled
                if (inputOptions.enableProbeClassification)
                {
                    volume->ClassifyProbes(d3d.cmdList);
                }
#endif /* RTXGI_DDGI_PROBE_RELOCATION */
            }
#endif /* RTXGI_DDGI_PROBE_STATE_CLASSIFIER */

            // Run a fullscreen pass and composite direct lighting
            // with indirect lighting from the DDGIVolume
            Harness::RenderIndirect(d3d, dxr, resources, postOptions);

            // Render a debug visualization of the DDGIVolume buffers
            if (vizOptions.showDDGIVolumeBuffers)
            {
                Visualization::RenderBuffers(d3d, resources, vizOptions);
            }

            // Render the user interface with ImGui
            if (config.ui)
            {
                UI::OnRender(d3d, resources);
            }
        }
                
        D3D12::SubmitCmdList(d3d);
        D3D12::Present(d3d);
        D3D12::MoveToNextFrame(d3d);
        D3D12::ResetCmdList(d3d);
    }

    D3D12::WaitForGPU(d3d);
    CloseHandle(d3d.fenceEvent);

    log << "Shutting down and cleaning up.\n";

    volume->Destroy();

    RTXGI_SAFE_RELEASE(resources.volumeCB);
#if !RTXGI_DDGI_SDK_MANAGED_RESOURCES
    Harness::DestroyVolumeResources(volumeResources);
#endif

    for (UINT i = 0; i < shaders.size(); i++)
    {
        RTXGI_SAFE_RELEASE(shaders[i].bytecode);
    }

    UI::Cleanup();
    DXR::Cleanup(dxr, resources);
    D3D12::Cleanup(d3d, resources);
    Shaders::Cleanup(shaderCompiler);
    delete volume;

    DestroyWindow(window);
    log << "Done\n";
    log.close();

    return EXIT_SUCCESS;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Run the application.
    int result = Run(hInstance, lpCmdLine);

    // Check for memory leaks.
#if defined _CRTDBG_MAP_ALLOC
    _CrtDumpMemoryLeaks();
#endif

    // If an error occurred, spawn a message box.
    if (result != EXIT_SUCCESS)
    {
        ::MessageBox(NULL, "An error occurred. See log.txt for details.", NULL, MB_OK);
    }

    return result;
}
