/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include "Common.h"

#include <Keyboard.h>
#include <Mouse.h>
#include <rtxgi/Types.h>

struct InputInfo
{
    DirectX::Keyboard keyboard;
    DirectX::Keyboard::KeyboardStateTracker kbTracker;

    DirectX::Mouse mouse;
    rtxgi::int2 lastMouseXY = { INT_MAX, INT_MAX };
    int scrollWheelValue = INT_MAX;

    float   pitch = 0.f;
    float   yaw = 0.f;
    int     width = 0;
    int     height = 0;
    bool    saveImage = false;
    bool    initialized = false;
};

namespace Input
{
    bool KeyHandler(
        InputInfo &input,
        ConfigInfo &config,
        InputOptions &inputOptions,
        VizOptions &vizOptions,
        Camera &camera,
        float3 &translation,
        bool &useDDGI,
        bool &hotReload);
    
    bool MouseHandler(InputInfo &input, Camera &camera, InputOptions &inputOptions);
}
