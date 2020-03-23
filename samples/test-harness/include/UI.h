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

#include "rtxgi/ddgi/DDGIVolume.h"

#include "Common.h"
#include "Input.h"

namespace UI
{
    void Initialize(D3D12Info &d3d, D3D12Resources &resources, HWND window);
    void Cleanup();

    bool WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    void OnNewFrame(
        D3D12Info &d3d,
        DXRInfo &dxr,
        ConfigInfo &config,
        CameraInfo &camera,
        LightInfo &lights,
        rtxgi::DDGIVolume* volume,
        InputInfo &input,
        InputOptions &inputOptions,
        RTOptions &rtOptions,
        VizOptions &vizOptions,
        PostProcessOptions &postOptions);
    void OnRender(D3D12Info &d3d, D3D12Resources &resources);

    bool WantsMouseCapture();
}
