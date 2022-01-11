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

#include "Graphics.h"

#ifdef GFX_PERF_INSTRUMENTATION
#include "Instrumentation.h"
#endif

#if defined(API_D3D12)
#include "GBuffer_D3D12.h"
#elif defined(API_VULKAN)
#include "GBuffer_VK.h"
#endif

namespace Graphics
{
    namespace GBuffer
    {
        bool Initialize(Globals& globals, GlobalResources& gfxResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log);
        bool Reload(Globals& globals, GlobalResources& gfxResources, Resources& resources, std::ofstream& log);
        bool Resize(Globals& globals, GlobalResources& gfxResources, Resources& resources, std::ofstream& log);
        void Update(Globals& globals, GlobalResources& gfxResources, Resources& resources, const Configs::Config& config);
        void Execute(Globals& globals, GlobalResources& gfxResources, Resources& resources);
        bool WriteGBufferToDisk(Globals& globals, GlobalResources& gfxResources, std::string directory);
        void Cleanup(Globals& globals, Resources& resources);
    }
}
