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
#include "Configs.h"
#include "Scenes.h"

namespace Inputs
{
    enum class EInputEvent
    {
        NONE = 0,
        QUIT,
        RELOAD,
        SCREENSHOT,
        SAVE_IMAGES,
        CAMERA_MOVEMENT,
        FULLSCREEN_CHANGE,
        RUN_BENCHMARK,
        COUNT
    };

    struct Input
    {
        EInputEvent event  = EInputEvent::NONE;
        DirectX::XMINT2 mousePos = { INT_MAX, INT_MAX };
        DirectX::XMINT2 prevMousePos = { INT_MAX, INT_MAX };
        bool mouseLeftBtnDown = false;
        bool mouseRightBtnDown = false;
    };

    bool Initialize(GLFWwindow* window, Input& input, Configs::Config& config, Scenes::Scene& scene);
    void PollInputs(GLFWwindow* window);
}
