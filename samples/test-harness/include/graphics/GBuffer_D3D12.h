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
        namespace GBuffer
        {
            struct Resources
            {
                ID3D12Resource*              shaderTable = nullptr;
                ID3D12Resource*              shaderTableUpload = nullptr;
                Shaders::ShaderRTPipeline    shaders;

                ID3D12StateObject*           rtpso = nullptr;
                ID3D12StateObjectProperties* rtpsoInfo = nullptr;

                uint32_t                     shaderTableSize = 0;
                uint32_t                     shaderTableRecordSize = 0;
                uint32_t                     shaderTableMissTableSize = 0;
                uint32_t                     shaderTableHitGroupTableSize = 0;

                D3D12_GPU_VIRTUAL_ADDRESS    shaderTableRGSStartAddress = 0;
                D3D12_GPU_VIRTUAL_ADDRESS    shaderTableMissTableStartAddress = 0;
                D3D12_GPU_VIRTUAL_ADDRESS    shaderTableHitGroupTableStartAddress = 0;

                Instrumentation::Stat*       cpuStat = nullptr;
                Instrumentation::Stat*       gpuStat = nullptr;
            };
        }
    }

    namespace GBuffer
    {
        using Resources = Graphics::D3D12::GBuffer::Resources;
    }
}


