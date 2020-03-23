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

#include "Common.h"

namespace Visualization
{
    void RenderBuffers(D3D12Info &d3d, D3D12Resources &resources, const VizOptions &options);
    void RenderProbes(D3D12Info &d3d, DXRInfo &dxr, D3D12Resources &resources);
}

