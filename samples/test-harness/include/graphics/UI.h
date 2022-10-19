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
#include "DDGI.h"
#include "Graphics.h"
#include "Inputs.h"
#include "Instrumentation.h"
#include "Scenes.h"

#if defined(API_D3D12)
#include "UI_D3D12.h"
#elif defined(API_VULKAN)
#include "UI_VK.h"
#endif

namespace Graphics
{
    namespace UI
    {
        extern bool s_initialized;

        bool Initialize(Graphics::Globals& gfx, Graphics::GlobalResources& gfxResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log);
        void Update(Graphics::Globals& gfx, Resources& resources, Configs::Config& config, Inputs::Input& input, Scenes::Scene& scene, std::vector<DDGIVolumeBase*>& volumes, const Instrumentation::Performance& performance);
        bool MessageBox(std::string message);
        bool MessageRetryBox(std::string message);
        bool CapturedMouse();
        bool CapturedKeyboard();
        void Execute(Graphics::Globals& gfx, Graphics::GlobalResources& gfxResources, Resources& resources, const Configs::Config& config);
        void Cleanup();

        void CreateDebugWindow(Graphics::Globals& gfx, Configs::Config& config, Inputs::Input& input, Scenes::Scene& scene, std::vector<DDGIVolumeBase*>& volumes);
        void CreatePerfWindow(Graphics::Globals& gfx, const Configs::Config& config, const Instrumentation::Performance& performance);
    }
}
