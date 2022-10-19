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
#include "DDGIDefines.h"

#ifdef GFX_PERF_INSTRUMENTATION
#include "Instrumentation.h"
#endif

#if defined(API_D3D12)
#include "DDGI_D3D12.h"
#include "DDGIVisualizations_D3D12.h"
#elif defined(API_VULKAN)
#include "DDGI_VK.h"
#include "DDGIVisualizations_VK.h"
#endif

namespace Graphics
{
    namespace DDGI
    {
        namespace Visualizations
        {
            enum VIS_SHOW_FLAGS
            {
                VIS_FLAG_SHOW_NONE = 0,
                VIS_FLAG_SHOW_PROBES = 0x2,
                VIS_FLAG_SHOW_TEXTURES = 0x4
            };

            bool Initialize(Globals& globals, GlobalResources& gfxResources, DDGI::Resources& ddgiResources, Resources& resources, Instrumentation::Performance& perf, Configs::Config& config, std::ofstream& log);
            bool Reload(Globals& globals, GlobalResources& gfxResources, DDGI::Resources& ddgiResources, Resources& resources, Configs::Config& config, std::ofstream& log);
            bool Resize(Globals& globals, GlobalResources& gfxResources, Resources& resources, std::ofstream& log);
            void Update(Globals& globals, GlobalResources& gfxResources, Resources& resources, const Configs::Config& config);
            void Execute(Globals& globals, GlobalResources& gfxResources, Resources& resources);
            void Cleanup(Globals& globals, Resources& resources);
        }
    }
}
