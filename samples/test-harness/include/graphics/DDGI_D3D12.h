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
#include <rtxgi/ddgi/gfx/DDGIVolume_D3D12.h>

namespace Graphics
{
    namespace D3D12
    {
        namespace DDGI
        {
            struct Resources
            {
                // Textures
                ID3D12Resource*              output = nullptr;

                // Shaders
                Shaders::ShaderRTPipeline    rtShaders;
                Shaders::ShaderProgram       indirectCS;

                // Ray Tracing
                ID3D12Resource*              shaderTable = nullptr;
                ID3D12Resource*              shaderTableUpload = nullptr;

                // Pipeline State Objects
                ID3D12StateObject*           rtpso = nullptr;
                ID3D12StateObjectProperties* rtpsoInfo = nullptr;
                ID3D12PipelineState*         indirectPSO = nullptr;

                // Shader Table
                UINT                         shaderTableSize = 0;
                UINT                         shaderTableRecordSize = 0;
                UINT                         shaderTableMissTableSize = 0;
                UINT                         shaderTableHitGroupTableSize = 0;

                D3D12_GPU_VIRTUAL_ADDRESS    shaderTableRGSStartAddress = 0;
                D3D12_GPU_VIRTUAL_ADDRESS    shaderTableMissTableStartAddress = 0;
                D3D12_GPU_VIRTUAL_ADDRESS    shaderTableHitGroupTableStartAddress = 0;

                // DDGI
                std::vector<rtxgi::DDGIVolumeDesc> volumeDescs;
                std::vector<rtxgi::DDGIVolumeBase*> volumes;
                std::vector<rtxgi::d3d12::DDGIVolume*> selectedVolumes;

                ID3D12DescriptorHeap*        rtvDescriptorHeap = nullptr;

                ID3D12Resource*              volumeResourceIndicesSTB = nullptr;
                ID3D12Resource*              volumeResourceIndicesSTBUpload = nullptr;
                UINT                         volumeResourceIndicesSTBSizeInBytes = 0;

                ID3D12Resource*              volumeConstantsSTB = nullptr;
                ID3D12Resource*              volumeConstantsSTBUpload = nullptr;
                UINT                         volumeConstantsSTBSizeInBytes = 0;

                // Variability Tracking
                std::vector<uint32_t>        numVolumeVariabilitySamples;

                // Performance Stats
                Instrumentation::Stat*       cpuStat = nullptr;
                Instrumentation::Stat*       gpuStat = nullptr;

                Instrumentation::Stat*       classifyStat = nullptr;
                Instrumentation::Stat*       rtStat = nullptr;
                Instrumentation::Stat*       blendStat = nullptr;
                Instrumentation::Stat*       relocateStat = nullptr;
                Instrumentation::Stat*       lightingStat = nullptr;
                Instrumentation::Stat*       variabilityStat = nullptr;

                bool                         enabled = false;
            };
        }
    }

    namespace DDGI
    {
        using Resources = Graphics::D3D12::DDGI::Resources;
    }
}
