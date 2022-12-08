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
        namespace DDGI
        {
            namespace Visualizations
            {

                struct Resources
                {
                    // Flags
                    UINT                                        flags = 0;

                    // Shaders
                    Shaders::ShaderRTPipeline                   rtShaders;
                    Shaders::ShaderRTPipeline                   rtShaders2;
                    Shaders::ShaderProgram                      textureVisCS;
                    Shaders::ShaderProgram                      updateTlasCS;

                    // Ray Tracing
                    ID3D12Resource*                             shaderTable = nullptr;
                    ID3D12Resource*                             shaderTableUpload = nullptr;

                    ID3D12StateObject*                          rtpso = nullptr;
                    ID3D12StateObject*                          rtpso2 = nullptr;
                    ID3D12StateObjectProperties*                rtpsoInfo = nullptr;
                    ID3D12StateObjectProperties*                rtpsoInfo2 = nullptr;
                    ID3D12PipelineState*                        texturesVisPSO = nullptr;
                    ID3D12PipelineState*                        updateTlasPSO = nullptr;

                    UINT                                        shaderTableSize = 0;
                    UINT                                        shaderTableRecordSize = 0;
                    UINT                                        shaderTableMissTableSize = 0;
                    UINT                                        shaderTableHitGroupTableSize = 0;

                    D3D12_GPU_VIRTUAL_ADDRESS                   shaderTableRGSStartAddress;
                    D3D12_GPU_VIRTUAL_ADDRESS                   shaderTableRGS2StartAddress;
                    D3D12_GPU_VIRTUAL_ADDRESS                   shaderTableMissTableStartAddress;
                    D3D12_GPU_VIRTUAL_ADDRESS                   shaderTableHitGroupTableStartAddress;

                    // Probe Sphere Resources
                    ID3D12Resource*                             probeVB = nullptr;
                    ID3D12Resource*                             probeVBUpload = nullptr;
                    D3D12_VERTEX_BUFFER_VIEW                    probeVBView;

                    ID3D12Resource*                             probeIB = nullptr;
                    ID3D12Resource*                             probeIBUpload = nullptr;
                    D3D12_INDEX_BUFFER_VIEW                     probeIBView;

                    Scenes::Mesh                                probe;
                    AccelerationStructure                       blas;
                    AccelerationStructure                       tlas;

                    UINT                                        maxProbeInstances = 0;
                    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> probeInstances;

                    // DDGI Resources
                    UINT                                        selectedVolume = 0;
                    std::vector<rtxgi::DDGIVolumeBase*>*        volumes;
                    ID3D12Resource*                             ddgiConstantsCB = nullptr;
                    ID3D12Resource*                             volumeConstantsSTB = nullptr;

                    Instrumentation::Stat*                      cpuStat = nullptr;
                    Instrumentation::Stat*                      gpuProbeStat = nullptr;
                    Instrumentation::Stat*                      gpuTextureStat = nullptr;

                    bool                                        enabled = false;
                };
            }
        }
    }

    namespace DDGI::Visualizations
    {
        using Resources = Graphics::D3D12::DDGI::Visualizations::Resources;
    }
}
