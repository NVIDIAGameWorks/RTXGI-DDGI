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
#include "Input.h"
#include "rtxgi/ddgi/DDGIVolume.h"

#include <fstream>

namespace Config
{
    bool ParseCommandLine(LPWSTR lpCmdLine, ConfigInfo &config, std::ofstream &log);
    bool Load(
        ConfigInfo &config,
        LightInfo &lights,
        Camera &camera,
        std::vector<rtxgi::DDGIVolumeDesc> &descs,
        InputInfo &inputInfo,
        InputOptions &inputOptions,
        RTOptions &rtOptions,
        PostProcessOptions &postOptions,
        VizOptions &vizOptions,
        std::ofstream &log);
}