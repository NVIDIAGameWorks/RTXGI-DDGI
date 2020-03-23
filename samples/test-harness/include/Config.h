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

#include <fstream>

#include "Common.h"
#include "Input.h"
#include "rtxgi/ddgi/DDGIVolume.h"

namespace Config
{
    bool ParseCommandLine(LPWSTR lpCmdLine, ConfigInfo &config, ofstream &log);
    bool Load(
        ConfigInfo &config,
        LightInfo &lights,
        CameraInfo &camera,
        rtxgi::DDGIVolumeDesc &desc,
        InputInfo &inputInfo,
        InputOptions &inputOptions,
        RTOptions &rtOptions,
        PostProcessOptions &postOptions,
        VizOptions &vizOptions,
        ofstream &log);
}