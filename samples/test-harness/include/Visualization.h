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

#include "Graphics.h"

namespace Graphics
{
    namespace Vis
    {
        namespace DDGI
        {
            struct Resources
            {
                // TLAS instances
                // ProbeVis TLAS / BLAS
                // Shaders

                // Scene Ray Tracing Acceleration Structures
                AccelerationStructure                  blas;
                AccelerationStructure                  tlas;

                // Procedural Geometry
                ID3D12Resource*                        sphereVB = nullptr;
                ID3D12Resource*                        sphereIB = nullptr;
                D3D12_VERTEX_BUFFER_VIEW               sphereVBView;
                D3D12_INDEX_BUFFER_VIEW                sphereIBView;

                // Shader Table
                ID3D12Resource*                        shaderTable = nullptr;
                UINT                                   shaderTableRecordSize = 0;

                // A global root signature for bindless resource access
                ID3D12RootSignature*                   rootSignature = nullptr;


            };

            void Initialize(GfxGlobals& gfx, Resources& resources, Shaders::ShaderCompiler& shaderCompiler);

            // TODO: need config options and target buffer
            void RenderBuffers(GfxGlobals& gfx, Resources& resources, size_t volumeIndex = 0);

            // TODO: need a camera and target buffer
            void RenderProbes(GfxGlobals& gfx, Resources &resources, size_t volumeIndex = 0);

            void Cleanup(Resources& resources);

            enum class DescriptorHeapOffsets
            {
                // Constant Buffer Views
                CBV_CAMERAS = 0,                                                 // 0:  1  CBV for the cameras constant buffer
                CBV_DDGIVOLUMES = CBV_CAMERAS + 1,                               // 1:  1  CBV for the DDGIVolumes constant buffer

                // Unordered Access Views
                UAV_GBUFFER = CBV_DDGIVOLUMES + 1,                               // 2:  2  UAV for the GBuffer A, B RWTextures
                UAV_DDGIVOLUME = UAV_GBUFFER + 2,                                // 4:  8  UAV, 2 UAV per DDGIVolume (Radiance and OffsetStates)
                UAV_TLAS_INST = UAV_DDGIVOLUME + (2 * MAX_DDGIVOLUMES),          // 12: 2  UAV for the TLAS Instances

                // Shader Resource Views
                SRV_PROBEVIS_BVH = UAV_TLAS_INST + 1,                            // 13: 1  SRV for the ProbeVis BVH
                SRV_DDGIVOLUME = SRV_PROBEVIS_BVH + 1,                           // 14: 8  SRV, 2 SRV per DDGIVolume (Irradiance and Distance)
                SRV_INDICES = SRV_DDGIVOLUME + (2 * MAX_DDGIVOLUMES),            // 23: 1  SRV for the Sphere Index Buffer
                SRV_VERTICES = SRV_INDICES + 1,                                  // 24: 1  SRV for the Sphere Vertex Buffer
            };
        }
    }
}
