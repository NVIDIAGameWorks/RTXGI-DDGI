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

namespace Windows
{
    enum class EWindowEvent
    {
        NONE = 0,
        RESIZE,
        QUIT,
        COUNT
    };

    bool Create(Configs::Config& config, GLFWwindow*& window);
    bool Close(GLFWwindow*& window);

    const EWindowEvent GetWindowEvent();
    void ResetWindowEvent();
}
