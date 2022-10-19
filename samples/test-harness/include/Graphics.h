/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include "Common.h"
#include "Shaders.h"
#include "Scenes.h"
#include "Instrumentation.h"

const int MAX_TLAS = 2;
const int MAX_TEXTURES = 300;
const int MAX_DDGIVOLUMES = 6;
const int MAX_TIMESTAMPS = 200;

#define RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS 0
#define RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP 1

#define GFX_PERF_INSTRUMENTATION

#ifdef GFX_PERF_MARKERS
#define GFX_PERF_MARKER_RED 204, 28, 41
#define GFX_PERF_MARKER_GREEN 105, 148, 79
#define GFX_PERF_MARKER_BLUE 65, 126, 211
#define GFX_PERF_MARKER_ORANGE 217, 122, 46
#define GFX_PERF_MARKER_YELLOW 217, 207, 46
#define GFX_PERF_MARKER_PURPLE 152, 78, 163
#define GFX_PERF_MARKER_BROWN 166, 86, 40
#define GFX_PERF_MARKER_GREY 190, 190, 190
#endif

#if (defined(_WIN32) || defined(WIN32)) && defined(API_D3D12)
#include "Direct3D12.h"
#endif

#if defined(API_VULKAN)
#include "Vulkan.h"
#endif

namespace Graphics
{
    bool CreateDevice(Globals& gfx, Configs::Config& config);
    bool Initialize(const Configs::Config& config, Scenes::Scene& scene, Globals& gfx, GlobalResources& resources, std::ofstream& log);
    void Update(Globals& gfx, GlobalResources& gfxResources, const Configs::Config& config, Scenes::Scene& scene);
    bool Resize(Globals& gfx, GlobalResources& resources, int width, int height, std::ofstream& log);
    bool ToggleFullscreen(Globals& gfx);
    bool ResetCmdList(Globals& gfx);
    bool SubmitCmdList(Globals& gfx);
    bool Present(Globals& gfx);
    bool WaitForGPU(Globals& gfx);
    bool MoveToNextFrame(Globals& gfx);
    void Cleanup(Globals& gfx, GlobalResources& gfxResources);

#ifdef GFX_PERF_INSTRUMENTATION
    void BeginFrame(Globals& gfx, GlobalResources& gfxResources, Instrumentation::Performance& performance);
    void EndFrame(Globals& gfx, GlobalResources& gfxResources, Instrumentation::Performance& performance);
    void ResolveTimestamps(Globals& gfx, GlobalResources& gfxResources, Instrumentation::Performance& performance);
    bool UpdateTimestamps(Globals& gfx, GlobalResources& gfxResources, Instrumentation::Performance& performance);
#endif

    bool WriteBackBufferToDisk(Globals& gfx, std::string directory);

}
