/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Common.h"
#include "Configs.h"
#include "Scenes.h"
#include "Inputs.h"
#include "Instrumentation.h"
#include "Graphics.h"
#include "UI.h"
#include "Window.h"
#include "Benchmark.h"

#include "graphics/PathTracing.h"
#include "graphics/GBuffer.h"
#include "graphics/DDGI.h"
#include "graphics/DDGIVisualizations.h"
#include "graphics/RTAO.h"
#include "graphics/Composite.h"

#include <filesystem>

#if _WIN32
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 606; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\"; }
#endif

void StoreImages(
    Inputs::EInputEvent& event,
    Configs::Config& config,
    Graphics::Globals& gfx,
    Graphics::GlobalResources& gfxResources,
    Graphics::RTAO::Resources& rtao,
    Graphics::DDGI::Resources& ddgi)
{
    if(config.app.benchmarkRunning) return; // Not allowed while benchmark is running

    std::filesystem::create_directories(config.scene.screenshotPath.c_str());

    if (event == Inputs::EInputEvent::SCREENSHOT)
    {
        Graphics::WriteBackBufferToDisk(gfx, config.scene.screenshotPath);
        event = Inputs::EInputEvent::NONE;
    }
    else if (event == Inputs::EInputEvent::SAVE_IMAGES)
    {
        Graphics::GBuffer::WriteGBufferToDisk(gfx, gfxResources, config.scene.screenshotPath);
        Graphics::RTAO::WriteRTAOBuffersToDisk(gfx, gfxResources, rtao, config.scene.screenshotPath);
        Graphics::DDGI::WriteVolumesToDisk(gfx, gfxResources, ddgi, config.scene.screenshotPath);
        event = Inputs::EInputEvent::NONE;
    }
}

/**
 * Run the Test Harness.
 */
int Run(const std::vector<std::string>& arguments)
{
    std::ofstream log;
    log.open("log.txt", std::ios::out);
    if (!log.is_open()) return EXIT_FAILURE;

    // Global Data Structures
    Configs::Config config;
    Scenes::Scene scene;

    // Graphics Globals
    Graphics::Globals gfx;
    Graphics::GlobalResources gfxResources;

    // Graphics Workloads
    Graphics::PathTracing::Resources pt;
    Graphics::GBuffer::Resources gbuffer;
    Graphics::DDGI::Resources ddgi;
    Graphics::DDGI::Visualizations::Resources ddgiVis;
    Graphics::RTAO::Resources rtao;
    Graphics::Composite::Resources composite;
    Graphics::UI::Resources ui;

    // Performance Timers
    Instrumentation::Stat startupShutdown;
    Instrumentation::Performance perf;
    perf.AddCPUStat("Frame");
    perf.AddGPUStat("Frame");
    perf.AddCPUStat("Input");
    perf.AddCPUStat("Update");
    Benchmark::BenchmarkRun benchmarkRun;

    CPU_TIMESTAMP_BEGIN(&startupShutdown);

    // Parse the command line and get the config file path
    log << "Parsing command line...";
    if (!Configs::ParseCommandLine(arguments, config, log))
    {
        log << "Failed to parse the command line!";
        log.close();
        return EXIT_FAILURE;
    }
    log << "done.\n";

    // Load and parse the config file
    log << "Loading config file...";
    if (!Configs::Load(config, log))
    {
        log.close();
        return EXIT_FAILURE;
    }
    log << "done.\n";

    // Create a window
    log << "Creating a window...";
    if(!Windows::Create(config, gfx.window))
    {
        log << "\nFailed to create the window!";
        log.close();
        return EXIT_FAILURE;
    }
    log << "done.\n";

    // Input
    log << "Initializing input system...";
    Inputs::Input input;
    if(!Inputs::Initialize(gfx.window, input, config, scene))
    {
        log << "\nFailed to initialize input!";
        log.close();
        return EXIT_FAILURE;
    }
    log << "done.\n";

    // Create a device
    log << "Creating graphics device...";
    if (!Graphics::CreateDevice(gfx, config))
    {
        log << "\nFailed to create the graphics device!";
        log.close();
        return EXIT_FAILURE;
    }
    log << "done.\n";

#ifdef GPU_COMPRESSION
    // Initialize the texture system
    log << "Initializing texture system...";
    if (!Textures::Initialize())
    {
        log << "\nFailed to initialize texture system!";
        log.close();
        return EXIT_FAILURE;
    }
    log << "done.\n";
#endif

    // Initialize the scene
    log << "Initializing the scene...";
    if (!Scenes::Initialize(config, scene, log))
    {
        log << "\nFailed to initialize the scene!";
        log.close();
        return EXIT_FAILURE;
    }
    log << "done.\n";

    // Initialize the graphics system
    log << "Initializing graphics...";
    if (!Graphics::Initialize(config, scene, gfx, gfxResources, log))
    {
        log << "\nFailed to initialize graphics!";
        log.close();
        return EXIT_FAILURE;
    }

    // Initialize the graphics workloads
    CHECK(Graphics::PathTracing::Initialize(gfx, gfxResources, pt, perf, log), "initialize path tracing workload!\n", log);
    CHECK(Graphics::GBuffer::Initialize(gfx, gfxResources, gbuffer, perf, log), "initialize gbuffer workload!\n", log);
    CHECK(Graphics::DDGI::Initialize(gfx, gfxResources, ddgi, config, perf, log), "initialize dynamic diffuse global illumination workload!\n", log);
    CHECK(Graphics::DDGI::Visualizations::Initialize(gfx, gfxResources, ddgi, ddgiVis, perf, config, log), "initialize dynamic diffuse global illumination visualization workload!\n", log);
    CHECK(Graphics::RTAO::Initialize(gfx, gfxResources, rtao, perf, log), "initialize ray traced ambient occlusion workload!\n", log);
    CHECK(Graphics::Composite::Initialize(gfx, gfxResources, composite, perf, log), "initialize composition workload!\n", log);

    // Initialize the user interface system
    log << "Initializing user interface...";
    if (!Graphics::UI::Initialize(gfx, gfxResources, ui, perf, log))
    {
        log << "\nFailed to initialize user interface!";
        log.close();
        return EXIT_FAILURE;
    }
    log << "done.\n";

    perf.AddCPUStat("Submit/Present");

    CPU_TIMESTAMP_END(&startupShutdown);
    log << "Startup complete in " << startupShutdown.elapsed << " milliseconds\n";

    log << "Main loop...\n";
    std::flush(log);

#ifdef GFX_PERF_INSTRUMENTATION
    Graphics::BeginFrame(gfx, gfxResources, perf);
#endif

    // Main loop
    while(!glfwWindowShouldClose(gfx.window))
    {
        CPU_TIMESTAMP_BEGIN(perf.cpuTimes[0]);   // frame
        CPU_TIMESTAMP_BEGIN(perf.cpuTimes[1]);   // input

        glfwPollEvents();

        // Handle resize events
        if (Windows::GetWindowEvent() == Windows::EWindowEvent::RESIZE)
        {
            Graphics::WaitForGPU(gfx);

            // Get the new back buffer dimensions from GLFW
            int width, height;
            glfwGetFramebufferSize(gfx.window, &width, &height);

            // Wait for the window to have valid dimensions
            while(width == 0 || height == 0)
            {
                glfwGetFramebufferSize(gfx.window, &width, &height);
                glfwWaitEvents();
            }

            // Resize all screen-space buffers
            if (!Graphics::Resize(gfx, gfxResources, width, height, log)) break;                  // Back buffers and GBuffer textures
            if (!Graphics::PathTracing::Resize(gfx, gfxResources, pt, log)) break;                // PT Output and Accumulation
            if (!Graphics::GBuffer::Resize(gfx, gfxResources, gbuffer, log)) break;               // GBuffer
            if (!Graphics::DDGI::Resize(gfx, gfxResources, ddgi, log)) break;                     // DDGI
            if (!Graphics::DDGI::Visualizations::Resize(gfx, gfxResources, ddgiVis, log)) break;  // DDGI Visualizations
            if (!Graphics::RTAO::Resize(gfx, gfxResources, rtao, log)) break;                     // RTAO Raw and Output textures
            if (!Graphics::Composite::Resize(gfx, gfxResources, composite, log)) break;           // Composite
            Windows::ResetWindowEvent();

            CPU_TIMESTAMP_ENDANDRESOLVE(perf.cpuTimes[0]); // frame ended early
            continue;
        }

        // Initialize the benchmark
        if (!config.app.benchmarkRunning && input.event == Inputs::EInputEvent::RUN_BENCHMARK)
        {
            Benchmark::StartBenchmark(benchmarkRun, perf, config, gfx);
            input.event = Inputs::EInputEvent::NONE;
        }

        // Reload shaders and PSOs for graphics workloads
        {
            if (config.pathTrace.reload)
            {
                if (!Graphics::PathTracing::Reload(gfx, gfxResources, pt, log)) break;
                config.pathTrace.reload = false;
                CPU_TIMESTAMP_ENDANDRESOLVE(perf.cpuTimes[0]); // frame ended early
            #ifdef GFX_PERF_INSTRUMENTATION
                Graphics::BeginFrame(gfx, gfxResources, perf);
            #endif
                continue;
            }

            if (config.ddgi.reload)
            {
                if (!Graphics::DDGI::Reload(gfx, gfxResources, ddgi, config, log)) break;
                if (!Graphics::DDGI::Visualizations::Reload(gfx, gfxResources, ddgi, ddgiVis, config, log)) break;
                config.ddgi.reload = false;
                CPU_TIMESTAMP_ENDANDRESOLVE(perf.cpuTimes[0]); // frame ended early
            #ifdef GFX_PERF_INSTRUMENTATION
                Graphics::EndFrame(gfx, gfxResources, perf);
                Graphics::ResolveTimestamps(gfx, gfxResources, perf);
                if (!Graphics::UpdateTimestamps(gfx, gfxResources, perf)) break;
                Graphics::BeginFrame(gfx, gfxResources, perf);
            #endif
                continue;
            }

            if (config.rtao.reload)
            {
                if (!Graphics::RTAO::Reload(gfx, gfxResources, rtao, log)) break;
                config.rtao.reload = false;
                CPU_TIMESTAMP_ENDANDRESOLVE(perf.cpuTimes[0]); // frame ended early
            #ifdef GFX_PERF_INSTRUMENTATION
                Graphics::BeginFrame(gfx, gfxResources, perf);
            #endif
                continue;
            }

            if (config.postProcess.reload)
            {
                if (!Graphics::Composite::Reload(gfx, gfxResources, composite, log)) break;
                config.postProcess.reload = false;
                CPU_TIMESTAMP_ENDANDRESOLVE(perf.cpuTimes[0]); // frame ended early
            #ifdef GFX_PERF_INSTRUMENTATION
                Graphics::BeginFrame(gfx, gfxResources, perf);
            #endif
                continue;
            }
        }

        // Exit the application
        if (input.event == Inputs::EInputEvent::QUIT) break;

        // Fullscreen transition
        if (input.event == Inputs::EInputEvent::FULLSCREEN_CHANGE || gfx.fullscreenChanged)
        {
            Graphics::ToggleFullscreen(gfx);
            input.event = Inputs::EInputEvent::NONE;
            CPU_TIMESTAMP_ENDANDRESOLVE(perf.cpuTimes[0]); // frame ended early
        #ifdef GFX_PERF_INSTRUMENTATION
            Graphics::BeginFrame(gfx, gfxResources, perf);
        #endif
            continue;
        }

        // Handle mouse and keyboard input
        Inputs::PollInputs(gfx.window);

        // Reset the frame number on camera movement (for path tracer accumulation reset)
        if (input.event == Inputs::EInputEvent::CAMERA_MOVEMENT)
        {
            gfx.frameNumber = 1;
            input.event = Inputs::EInputEvent::NONE;
        }

        CPU_TIMESTAMP_ENDANDRESOLVE(perf.cpuTimes[1]);  // input

        // Update constant buffers
        CPU_TIMESTAMP_BEGIN(perf.cpuTimes[2]);
        Graphics::Update(gfx, gfxResources, config, scene);
        CPU_TIMESTAMP_ENDANDRESOLVE(perf.cpuTimes[2]);

        if(config.app.renderMode == ERenderMode::PATH_TRACE)
        {
            Graphics::PathTracing::Update(gfx, gfxResources, pt, config);
            Graphics::PathTracing::Execute(gfx, gfxResources, pt);
        }
        else if(config.app.renderMode == ERenderMode::DDGI)
        {
            // GBuffer
            Graphics::GBuffer::Update(gfx, gfxResources, gbuffer, config);
            Graphics::GBuffer::Execute(gfx, gfxResources, gbuffer);

            // RTXGI: DDGI
            Graphics::DDGI::Update(gfx, gfxResources, ddgi, config);
            Graphics::DDGI::Execute(gfx, gfxResources, ddgi);

            // RTXGI: DDGI Visualizations
            Graphics::DDGI::Visualizations::Update(gfx, gfxResources, ddgiVis, config);
            Graphics::DDGI::Visualizations::Execute(gfx, gfxResources, ddgiVis);

            // Ray Traced Ambient Occlusion
            Graphics::RTAO::Update(gfx, gfxResources, rtao, config);
            Graphics::RTAO::Execute(gfx, gfxResources, rtao);

            // Composite & Post Processing
            Graphics::Composite::Update(gfx, gfxResources, composite, config);
            Graphics::Composite::Execute(gfx, gfxResources, composite);
        }

        // UI
        CPU_TIMESTAMP_BEGIN(perf.cpuTimes[perf.cpuTimes.size() - 2]);
        Graphics::UI::Update(gfx, ui, config, input, scene, ddgi.volumes, perf);
        Graphics::UI::Execute(gfx, gfxResources, ui, config);
        CPU_TIMESTAMP_ENDANDRESOLVE(perf.cpuTimes[perf.cpuTimes.size() - 2]);

        // Timestamps
    #ifdef GFX_PERF_INSTRUMENTATION
        Graphics::EndFrame(gfx, gfxResources, perf);
        Graphics::ResolveTimestamps(gfx, gfxResources, perf);
    #endif

        // Submit / Present
        CPU_TIMESTAMP_BEGIN(perf.cpuTimes.back());
        if (!Graphics::SubmitCmdList(gfx)) break;
        if (!Graphics::Present(gfx)) continue;
        if (!Graphics::WaitForGPU(gfx)) { log << "GPU took too long to complete, device removed!"; break; }

        // Image Capture (user triggered)
        StoreImages(input.event, config, gfx, gfxResources, rtao, ddgi);

        if (!Graphics::MoveToNextFrame(gfx)) break;
        if (!Graphics::ResetCmdList(gfx)) break;
        CPU_TIMESTAMP_ENDANDRESOLVE(perf.cpuTimes.back());
        CPU_TIMESTAMP_ENDANDRESOLVE(perf.cpuTimes[0]);

    #ifdef GFX_PERF_INSTRUMENTATION
        if (!Graphics::UpdateTimestamps(gfx, gfxResources, perf)) break;
        if (config.app.benchmarkRunning)
        {
            if (Benchmark::UpdateBenchmark(benchmarkRun, perf, config, gfx, log))
            {
                // Store intermediate images when the benchmark ends
                Inputs::EInputEvent e = Inputs::EInputEvent::SCREENSHOT;
                StoreImages(e, config, gfx, gfxResources, rtao, ddgi);

                e = Inputs::EInputEvent::SAVE_IMAGES;
                StoreImages(e, config, gfx, gfxResources, rtao, ddgi);
            }
        }
        Graphics::BeginFrame(gfx, gfxResources, perf);
    #endif
    }

    Graphics::WaitForGPU(gfx);

    CPU_TIMESTAMP_BEGIN(&startupShutdown);

    log << "Shutting down and cleaning up...\n";

    perf.Cleanup();

    Graphics::UI::Cleanup();
    Graphics::Composite::Cleanup(gfx, composite);
    Graphics::RTAO::Cleanup(gfx, rtao);
    Graphics::DDGI::Visualizations::Cleanup(gfx, ddgiVis);
    Graphics::DDGI::Cleanup(gfx, ddgi);
    Graphics::GBuffer::Cleanup(gfx, gbuffer);
    Graphics::PathTracing::Cleanup(gfx, pt);
    Graphics::Cleanup(gfx, gfxResources);

#ifdef GPU_COMPRESSION
    Textures::Cleanup();
#endif

    Windows::Close(gfx.window);

    CPU_TIMESTAMP_END(&startupShutdown);
    log << "Shutdown complete in " << startupShutdown.elapsed << " milliseconds\n";

    log << "Done.\n";
    log.close();

    return EXIT_SUCCESS;
}

/**
 * Test Harness entry point.
 */
#if defined(_WIN32) || defined(WIN32)
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

#if _DEBUG
    // Set CRT flags to automatically check for memory leaks at program termination
    int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    flags = (flags & 0x0000FFFF) | _CRTDBG_LEAK_CHECK_DF;
    _CrtSetDbgFlag(flags);
#endif

    // Convert command line arguments to vector
    char arg[256];
    std::vector<std::string> arguments;
    for(int i = 1; i < __argc; i++)
    {
        size_t len;
        size_t max = wcslen(__wargv[i]) + 1;
        memset(&arg, 0, 256);
        wcstombs_s(&len, arg, max, __wargv[i], max);
        arguments.push_back(std::string(arg));
    }

#elif __linux__
int main(int argc, char* argv[])
{
#if _DEBUG
    // TODO: Set flags to automatically check for memory leaks at program termination
#endif

    // Add command line arguments to vector
    std::vector<std::string> arguments;
    for(int i = 1; i < argc; i++)
    {
        arguments.push_back(std::string(argv[i]));
    }

#else
    #pragma message("Platform not supported!")
#endif

    // Run the application
    int result = Run(arguments);

    // If an error occurred, spawn a message box
    if (result != EXIT_SUCCESS)
    {
        std::string msg = "An error occurred. See log.txt for details.";
        Graphics::UI::MessageBox(msg);
    }

    return EXIT_SUCCESS;
}

