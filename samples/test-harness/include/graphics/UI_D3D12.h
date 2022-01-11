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

namespace Graphics
{
    namespace D3D12
    {
        namespace UI
        {
            struct Resources
            {
                Instrumentation::Stat*       cpuStat = nullptr;
                Instrumentation::Stat*       gpuStat = nullptr;
            };
        }
    }

    namespace UI
    {
        using Resources = Graphics::D3D12::UI::Resources;
    }
}
