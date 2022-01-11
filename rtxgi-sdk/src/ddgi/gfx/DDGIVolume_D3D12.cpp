/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "rtxgi/ddgi/gfx/DDGIVolume_D3D12.h"

#if (defined(_WIN32) || defined(WIN32))
#include <pix.h>
#endif

namespace rtxgi
{
    namespace d3d12
    {
        //------------------------------------------------------------------------
        // Private RTXGI Namespace Helper Functions
        //------------------------------------------------------------------------

        ERTXGIStatus ValidateManagedResourcesDesc(const DDGIVolumeManagedResourcesDesc& desc)
        {
            // D3D device
            if (desc.device == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_DEVICE;

            // Shader bytecode
            if (!ValidateShaderBytecode(desc.probeBlendingIrradianceCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_BLENDING_IRRADIANCE;
            if (!ValidateShaderBytecode(desc.probeBlendingDistanceCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_BLENDING_DISTANCE;
            if (!ValidateShaderBytecode(desc.probeBorderRowUpdateIrradianceCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_BORDER_ROW_UPDATE_IRRADIANCE;
            if (!ValidateShaderBytecode(desc.probeBorderRowUpdateDistanceCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_BORDER_ROW_UPDATE_DISTANCE;
            if (!ValidateShaderBytecode(desc.probeBorderColumnUpdateIrradianceCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_BORDER_COLUMN_UPDATE_IRRADIANCE;
            if (!ValidateShaderBytecode(desc.probeBorderColumnUpdateDistanceCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_BORDER_COLUMN_UPDATE_DISTANCE;

            if (!ValidateShaderBytecode(desc.probeRelocation.updateCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_RELOCATION;
            if (!ValidateShaderBytecode(desc.probeRelocation.resetCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_RELOCATION_RESET;

            if (!ValidateShaderBytecode(desc.probeClassification.updateCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_CLASSIFICATION;
            if (!ValidateShaderBytecode(desc.probeClassification.resetCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_CLASSIFICATION_RESET;

            return ERTXGIStatus::OK;
        }

        ERTXGIStatus ValidateUnmanagedResourcesDesc(const DDGIVolumeUnmanagedResourcesDesc& desc)
        {
            // Root Signature
            if (desc.rootSignature == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_ROOT_SIGNATURE;

            // Textures
            if (desc.probeRayData == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_RAY_DATA;
            if (desc.probeIrradiance == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_IRRADIANCE;
            if (desc.probeDistance == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_DISTANCE;
            if (desc.probeData == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_DATA;

            // Render Target Views
            if (desc.probeIrradianceRTV.ptr == 0) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_DESCRIPTOR;
            if (desc.probeDistanceRTV.ptr == 0) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_DESCRIPTOR;

            // Pipeline State Objects
            if (desc.probeBlendingIrradiancePSO == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_PSO_PROBE_BLENDING_IRRADIANCE;
            if (desc.probeBlendingDistancePSO == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_PSO_PROBE_BLENDING_DISTANCE;
            if (desc.probeBorderRowUpdateIrradiancePSO == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_PSO_BORDER_ROW_UPDATE_IRRADIANCE;
            if (desc.probeBorderRowUpdateDistancePSO == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_PSO_BORDER_ROW_UPDATE_DISTANCE;
            if (desc.probeBorderColumnUpdateIrradiancePSO == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_PSO_BORDER_COLUMN_UPDATE_IRRADIANCE;
            if (desc.probeBorderColumnUpdateDistancePSO == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_PSO_BORDER_COLUMN_UPDATE_DISTANCE;

            if (desc.probeRelocation.updatePSO == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_PSO_PROBE_RELOCATION;
            if (desc.probeRelocation.resetPSO == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_PSO_PROBE_RELOCATION_RESET;

            if (desc.probeClassification.updatePSO == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_PSO_PROBE_CLASSIFICATION;
            if (desc.probeClassification.resetPSO == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_PSO_PROBE_CLASSIFICATION_RESET;

            return ERTXGIStatus::OK;
        }

        //------------------------------------------------------------------------
        // Public RTXGI D3D12 Namespace Functions
        //------------------------------------------------------------------------

        DXGI_FORMAT GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType type, UINT format)
        {
            if (type == EDDGIVolumeTextureType::RayData)
            {
                if (format == 0) return DXGI_FORMAT_R32G32_FLOAT;
                else if (format == 1) return DXGI_FORMAT_R32G32B32A32_FLOAT;
            }
            else if (type == EDDGIVolumeTextureType::Irradiance)
            {
                if (format == 0) return DXGI_FORMAT_R10G10B10A2_UNORM;
                else if(format == 1) return DXGI_FORMAT_R32G32B32A32_FLOAT;
            }
            else if (type == EDDGIVolumeTextureType::Distance)
            {
                if (format == 0) return DXGI_FORMAT_R16G16_FLOAT ;  // Note: in large environments FP16 may not be sufficient
                else if(format == 1) return DXGI_FORMAT_R32G32_FLOAT;
            }
            else if (type == EDDGIVolumeTextureType::Data)
            {
                if (format == 0) return DXGI_FORMAT_R16G16B16A16_FLOAT;
                else if(format == 1) return DXGI_FORMAT_R32G32B32A32_FLOAT;
            }
            return DXGI_FORMAT_UNKNOWN;
        }

        bool GetDDGIVolumeRootSignatureDesc(UINT constsOffset, UINT uavOffset, ID3DBlob*& signature)
        {
            // Descriptor Table
            // 1 SRV for constants structured buffer SRV  (t0, space1)
            // 1 UAV for probe ray hit data               (u0, space1)
            // 1 UAV for probe irradiance                 (u1, space1)
            // 1 UAV for probe distance                   (u2, space1)
            // 1 UAV for probe data                       (u3, space1)
            D3D12_DESCRIPTOR_RANGE ranges[2];

            // Volume Constants Structured Buffer (t0, space1)
            ranges[0].NumDescriptors = 1;
            ranges[0].BaseShaderRegister = 0;
            ranges[0].RegisterSpace = 1;
            ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            ranges[0].OffsetInDescriptorsFromTableStart = constsOffset;

            // RWTex2D UAVs (u0-u3, space1)
            ranges[1].NumDescriptors = GetDDGIVolumeNumUAVDescriptors();
            ranges[1].BaseShaderRegister = 0;
            ranges[1].RegisterSpace = 1;
            ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            ranges[1].OffsetInDescriptorsFromTableStart = uavOffset;

            // Root Constants (b0, space1)
            D3D12_ROOT_PARAMETER param0 = {};
            param0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            param0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            param0.Constants.Num32BitValues = DDGIConstants::GetAlignedNum32BitValues();
            param0.Constants.ShaderRegister = 0;
            param0.Constants.RegisterSpace = 1;

            // Descriptor Table
            D3D12_ROOT_PARAMETER param1 = {};
            param1.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            param1.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            param1.DescriptorTable.NumDescriptorRanges = _countof(ranges);
            param1.DescriptorTable.pDescriptorRanges = ranges;

            D3D12_ROOT_PARAMETER rootParams[2] = { param0, param1 };

            D3D12_ROOT_SIGNATURE_DESC desc = {};
            desc.NumParameters = _countof(rootParams);
            desc.pParameters = rootParams;
            desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

            ID3DBlob* error = nullptr;
            HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
            if (FAILED(hr))
            {
                RTXGI_SAFE_RELEASE(error);
                return false;
            }

            return true;
        }

        ERTXGIStatus UploadDDGIVolumeConstants(ID3D12GraphicsCommandList* cmdList, UINT bufferingIndex, UINT numVolumes, DDGIVolume** volumes)
        {
            // Copy the constants for each volume
            for(UINT volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                // Get the volume
                const DDGIVolume* volume = volumes[volumeIndex];

                // Validate the upload and device buffers
                if (volume->GetConstantsBuffer() == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_CONSTANTS_BUFFER;
                if (volume->GetConstantsBufferUpload() == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_CONSTANTS_UPLOAD_BUFFER;

                // Map the constant buffer and update it
                UINT8* pData = nullptr;
                HRESULT hr = volume->GetConstantsBufferUpload()->Map(0, nullptr, reinterpret_cast<void**>(&pData));
                if (FAILED(hr)) return ERTXGIStatus::ERROR_DDGI_MAP_FAILURE_CONSTANTS_UPLOAD_BUFFER;

                // Offset to the constants data to write to (e.g. double buffering)
                UINT64 bufferOffset = volume->GetConstantsBufferSizeInBytes() * bufferingIndex;

                // Offset to the volume in current constants buffer
                UINT volumeOffset = (volume->GetIndex() * sizeof(DDGIVolumeDescGPUPacked));

                // Offset to the volume constants in the upload buffer
                UINT64 srcOffset = (bufferOffset + volumeOffset);

                // Get the packed DDGIVolume GPU descriptor
                DDGIVolumeDescGPUPacked gpuDesc = volume->GetDescGPUPacked();

                pData += srcOffset;
                memcpy(pData, &gpuDesc, sizeof(DDGIVolumeDescGPUPacked));

                volume->GetConstantsBufferUpload()->Unmap(0, nullptr);

                // Schedule a copy of the upload buffer to the device buffer
                cmdList->CopyBufferRegion(volume->GetConstantsBuffer(), volumeOffset, volume->GetConstantsBufferUpload(), srcOffset, sizeof(DDGIVolumeDescGPUPacked));
            }

            return ERTXGIStatus::OK;
        }

        ERTXGIStatus UpdateDDGIVolumeProbes(ID3D12GraphicsCommandList* cmdList, UINT numVolumes, DDGIVolume** volumes)
        {
            if (bInsertPerfMarkers) PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), "Update Probes");

            UINT volumeIndex;
            std::vector<D3D12_RESOURCE_BARRIER> barriers;

            // Transition volume textures to unordered access for read/write
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            // Transition(s)
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];

                // Transition the volume's irradiance and distance textures to unordered access
                barrier.Transition.pResource = volume->GetProbeIrradiance();
                barriers.push_back(barrier);

                barrier.Transition.pResource = volume->GetProbeDistance();
                barriers.push_back(barrier);
            }

            // Wait for the resource transitions to complete
            if (!barriers.empty()) cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

            barriers.clear();
            barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;

            // Probe Blending
            if (bInsertPerfMarkers) PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), "Probe Blending");

            // Irradiance
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];

                DDGIConstants consts =
                {
                    volume->GetIndex(),
                    volume->GetDescriptorBindlessUAVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                    volume->GetDescriptorBindlessSRVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                };

                // Set the descriptor heap
                ID3D12DescriptorHeap* heaps = { volume->GetDescriptorHeap() };
                cmdList->SetDescriptorHeaps(1, &heaps);

                // Set root signature, descriptor table, and root constants
                cmdList->SetComputeRootSignature(volume->GetRootSignature());
                cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotDescriptorTable(), volume->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                cmdList->SetComputeRoot32BitConstants(volume->GetRootParamSlotRootConstants(), DDGIConstants::GetNum32BitValues(), consts.GetData(), 0);

                // Get the number of probes on the X and Y dimensions of the texture
                UINT probeCountX, probeCountY;
                GetDDGIVolumeProbeCounts(volume->GetDesc(), probeCountX, probeCountY);

                // Probe irradiance blending
                {
                    if(bInsertPerfMarkers && volume->GetInsertPerfMarkers())
                    {
                        std::string msg = "Irradiance, DDGIVolume[" + std::to_string(volume->GetIndex()) + "] - \"" + volume->GetName() + "\"";
                        PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), msg.c_str());
                    }

                    // Set the PSO and dispatch threads
                    cmdList->SetPipelineState(volume->GetProbeBlendingIrradiancePSO());
                    cmdList->Dispatch(probeCountX, probeCountY, 1);

                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers()) PIXEndEvent(cmdList);
                }

                // Add a barrier
                barrier.UAV.pResource = volume->GetProbeIrradiance();
                barriers.push_back(barrier);
            }

            // Distance
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];

                DDGIConstants consts =
                {
                    volume->GetIndex(),
                    volume->GetDescriptorBindlessUAVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                    volume->GetDescriptorBindlessSRVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                };

                // Set the descriptor heap
                ID3D12DescriptorHeap* heaps = { volume->GetDescriptorHeap() };
                cmdList->SetDescriptorHeaps(1, &heaps);

                // Set root signature, descriptor table, and root constants
                cmdList->SetComputeRootSignature(volume->GetRootSignature());
                cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotDescriptorTable(), volume->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                cmdList->SetComputeRoot32BitConstants(volume->GetRootParamSlotRootConstants(), DDGIConstants::GetNum32BitValues(), consts.GetData(), 0);

                // Get the number of probes on the X and Y dimensions of the texture
                UINT probeCountX, probeCountY;
                GetDDGIVolumeProbeCounts(volume->GetDesc(), probeCountX, probeCountY);

                // Probe distance blending
                {
                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers())
                    {
                        std::string msg = "Distance, DDGIVolume[" + std::to_string(volume->GetIndex()) + "] - \"" + volume->GetName() + "\"";
                        PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), msg.c_str());
                    }

                    // Set the PSO and dispatch threads
                    cmdList->SetPipelineState(volume->GetProbeBlendingDistancePSO());
                    cmdList->Dispatch(probeCountX, probeCountY, 1);

                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers()) PIXEndEvent(cmdList);
                }

                // Add a barrier
                barrier.UAV.pResource = volume->GetProbeDistance();
                barriers.push_back(barrier);
            }

            if (bInsertPerfMarkers) PIXEndEvent(cmdList);

            // Wait for the irradiance and distance blending passes
            // to complete before updating the borders
            if (!barriers.empty()) cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

            // Probe Border Update
            if (bInsertPerfMarkers) PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), "Probe Border Update");

            float groupSize = 8.f;
            UINT numThreadsX, numThreadsY;
            UINT numGroupsX, numGroupsY;
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];

                DDGIConstants consts =
                {
                    volume->GetIndex(),
                    volume->GetDescriptorBindlessUAVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                    volume->GetDescriptorBindlessSRVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                };

                // Set the descriptor heap
                ID3D12DescriptorHeap* heaps = { volume->GetDescriptorHeap() };
                cmdList->SetDescriptorHeaps(1, &heaps);

                // Set root signature, descriptor table, and root constants
                cmdList->SetComputeRootSignature(volume->GetRootSignature());
                cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotDescriptorTable(), volume->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                cmdList->SetComputeRoot32BitConstants(volume->GetRootParamSlotRootConstants(), DDGIConstants::GetNum32BitValues(), consts.GetData(), 0);

                // Get the number of probes on the X and Y dimensions of the texture
                UINT probeCountX, probeCountY;
                GetDDGIVolumeProbeCounts(volume->GetDesc(), probeCountX, probeCountY);

                // Probe irradiance border update
                {
                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers())
                    {
                        std::string msg = "Irradiance, DDGIVolume[" + std::to_string(volume->GetIndex()) + "] - \"" + volume->GetName() + "\"";
                        PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), msg.c_str());
                    }

                    // Rows
                    numThreadsX = (probeCountX * (volume->GetDesc().probeNumIrradianceTexels + 2));
                    numThreadsY = probeCountY;
                    numGroupsX = (UINT)ceil((float)numThreadsX / groupSize);
                    numGroupsY = (UINT)ceil((float)numThreadsY / groupSize);

                    cmdList->SetPipelineState(volume->GetProbeBorderRowUpdateIrradiancePSO());
                    cmdList->Dispatch(numGroupsX, numGroupsY, 1);

                    // Columns
                    numThreadsX = probeCountX;
                    numThreadsY = (probeCountY * (volume->GetDesc().probeNumIrradianceTexels + 2));
                    numGroupsX = (UINT)ceil((float)numThreadsX / groupSize);
                    numGroupsY = (UINT)ceil((float)numThreadsY / groupSize);

                    // Set the PSO and dispatch threads
                    cmdList->SetPipelineState(volume->GetProbeBorderColumnUpdateIrradiancePSO());
                    cmdList->Dispatch(numGroupsX, numGroupsY, 1);

                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers()) PIXEndEvent(cmdList);
                }

                // Probe distance border update
                {
                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers())
                    {
                        std::string msg = "Distance, DDGIVolume[" + std::to_string(volume->GetIndex()) + "] - \"" + volume->GetName() + "\"";
                        PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), msg.c_str());
                    }

                    // Rows
                    numThreadsX = (probeCountX * (volume->GetDesc().probeNumDistanceTexels + 2));
                    numThreadsY = probeCountY;
                    numGroupsX = (UINT)ceil((float)numThreadsX / groupSize);
                    numGroupsY = (UINT)ceil((float)numThreadsY / groupSize);

                    cmdList->SetPipelineState(volume->GetProbeBorderRowUpdateDistancePSO());
                    cmdList->Dispatch(numGroupsX, numGroupsY, 1);

                    // Columns
                    numThreadsX = probeCountX;
                    numThreadsY = (probeCountY * (volume->GetDesc().probeNumDistanceTexels + 2));
                    numGroupsX = (UINT)ceil((float)numThreadsX / groupSize);
                    numGroupsY = (UINT)ceil((float)numThreadsY / groupSize);

                    // Set the PSO and dispatch threads
                    cmdList->SetPipelineState(volume->GetProbeBorderColumnUpdateDistancePSO());
                    cmdList->Dispatch(numGroupsX, numGroupsY, 1);

                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers()) PIXEndEvent(cmdList);
                }
            }

            if (bInsertPerfMarkers) PIXEndEvent(cmdList);

            // Barrier(s)
            // Wait for the irradiance and distance border update passes
            // to complete before using the textures
            if (!barriers.empty()) cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

            // Remove previous barriers
            barriers.clear();

            // Transition volume textures back to pixel shader resources for read
            barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            // Transition(s)
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];

                // Transition the volume's irradiance and distance textures to unordered access
                barrier.Transition.pResource = volume->GetProbeIrradiance();
                barriers.push_back(barrier);

                barrier.Transition.pResource = volume->GetProbeDistance();
                barriers.push_back(barrier);
            }

            // Wait for the resource transitions to complete
            if (!barriers.empty()) cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

            if (bInsertPerfMarkers) PIXEndEvent(cmdList);

            return ERTXGIStatus::OK;
        }

        ERTXGIStatus RelocateDDGIVolumeProbes(ID3D12GraphicsCommandList* cmdList, UINT numVolumes, DDGIVolume** volumes)
        {
            if (bInsertPerfMarkers) PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), "Relocate Probes");

            UINT volumeIndex;
            std::vector<D3D12_RESOURCE_BARRIER> barriers;

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;

            // Probe Relocation Reset
            for(volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];
                if (!volume->GetProbeRelocationEnabled()) continue;     // Skip if relocation is not enabled for this volume
                if (!volume->GetProbeRelocationNeedsReset()) continue;  // Skip if the volume doesn't need to be reset

                DDGIConstants consts =
                {
                    volume->GetIndex(),
                    volume->GetDescriptorBindlessUAVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                    volume->GetDescriptorBindlessSRVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                };

                // Set the descriptor heap
                ID3D12DescriptorHeap* heaps = { volume->GetDescriptorHeap() };
                cmdList->SetDescriptorHeaps(1, &heaps);

                // Set root signature, descriptor table, and root constants
                cmdList->SetComputeRootSignature(volume->GetRootSignature());
                cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotDescriptorTable(), volume->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                cmdList->SetComputeRoot32BitConstants(volume->GetRootParamSlotRootConstants(), DDGIConstants::GetNum32BitValues(), consts.GetData(), 0);

                const float groupSizeX = 32.f;

                // Reset all probe offsets to zero
                UINT numGroupsX = (UINT)ceil((float)volume->GetNumProbes() / groupSizeX);
                cmdList->SetPipelineState(volume->GetProbeRelocationResetPSO());
                cmdList->Dispatch(numGroupsX, 1, 1);

                // Update the reset flag
                volumes[volumeIndex]->SetProbeRelocationNeedsReset(false);

                // Add a barrier
                barrier.UAV.pResource = volume->GetProbeData();
                barriers.push_back(barrier);
            }

            // Probe Relocation Reset Barrier(s)
            if(!barriers.empty()) cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

            barriers.clear();

            // Probe Relocation
            for(volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];
                if(!volume->GetProbeRelocationEnabled()) continue;  // Skip if relocation is not enabled for this volume

                DDGIConstants consts =
                {
                    volume->GetIndex(),
                    volume->GetDescriptorBindlessUAVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                    volume->GetDescriptorBindlessSRVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                };

                // Set the descriptor heap
                ID3D12DescriptorHeap* heaps = { volume->GetDescriptorHeap() };
                cmdList->SetDescriptorHeaps(1, &heaps);

                // Set root signature, descriptor table, and root constants
                cmdList->SetComputeRootSignature(volume->GetRootSignature());
                cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotDescriptorTable(), volume->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                cmdList->SetComputeRoot32BitConstants(volume->GetRootParamSlotRootConstants(), DDGIConstants::GetNum32BitValues(), consts.GetData(), 0);

                float groupSizeX = 32.f;

                // Probe relocation
                UINT numGroupsX = (UINT)ceil((float)volume->GetNumProbes() / groupSizeX);
                cmdList->SetPipelineState(volume->GetProbeRelocationPSO());
                cmdList->Dispatch(numGroupsX, 1, 1);

                // Add a barrier
                barrier.UAV.pResource = volume->GetProbeData();
                barriers.push_back(barrier);
            }

            // Probe Relocation Barrier(s)
            if(!barriers.empty()) cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

            if (bInsertPerfMarkers) PIXEndEvent(cmdList);

            return ERTXGIStatus::OK;
        }

        ERTXGIStatus ClassifyDDGIVolumeProbes(ID3D12GraphicsCommandList* cmdList, UINT numVolumes, DDGIVolume** volumes)
        {
            if (bInsertPerfMarkers) PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), "Classify Probes");

            UINT volumeIndex;
            std::vector<D3D12_RESOURCE_BARRIER> barriers;

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;

            // Probe Classification Reset
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];
                if (!volume->GetProbeClassificationEnabled()) continue;     // Skip if classification is not enabled for this volume
                if (!volume->GetProbeClassificationNeedsReset()) continue;  // Skip if the volume doesn't need to be reset

                DDGIConstants consts =
                {
                    volume->GetIndex(),
                    volume->GetDescriptorBindlessUAVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                    volume->GetDescriptorBindlessSRVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                };

                // Set the descriptor heap
                ID3D12DescriptorHeap* heaps = { volume->GetDescriptorHeap() };
                cmdList->SetDescriptorHeaps(1, &heaps);

                // Set root signature, descriptor table, and root constants
                cmdList->SetComputeRootSignature(volume->GetRootSignature());
                cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotDescriptorTable(), volume->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                cmdList->SetComputeRoot32BitConstants(volume->GetRootParamSlotRootConstants(), DDGIConstants::GetNum32BitValues(), consts.GetData(), 0);

                const float groupSizeX = 32.f;

                // Reset all probe offsets to zero
                UINT numGroupsX = (UINT)ceil((float)volume->GetNumProbes() / groupSizeX);
                cmdList->SetPipelineState(volume->GetProbeClassificationResetPSO());
                cmdList->Dispatch(numGroupsX, 1, 1);

                // Update the reset flag
                volumes[volumeIndex]->SetProbeClassificationNeedsReset(false);

                // Add a barrier
                barrier.UAV.pResource = volume->GetProbeData();
                barriers.push_back(barrier);
            }

            // Probe Classification Reset Barrier(s)
            if (!barriers.empty()) cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

            barriers.clear();

            // Probe Classification
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];
                if (!volume->GetProbeClassificationEnabled()) continue;  // Skip if classification is not enabled for this volume

                DDGIConstants consts =
                {
                    volume->GetIndex(),
                    volume->GetDescriptorBindlessUAVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                    volume->GetDescriptorBindlessSRVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                };

                // Set the descriptor heap
                ID3D12DescriptorHeap* heaps = { volume->GetDescriptorHeap() };
                cmdList->SetDescriptorHeaps(1, &heaps);

                // Set root signature, descriptor table, and root constants
                cmdList->SetComputeRootSignature(volume->GetRootSignature());
                cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotDescriptorTable(), volume->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                cmdList->SetComputeRoot32BitConstants(volume->GetRootParamSlotRootConstants(), DDGIConstants::GetNum32BitValues(), consts.GetData(), 0);

                const float groupSizeX = 32.f;

                // Probe classification
                UINT numGroupsX = (UINT)ceil((float)volume->GetNumProbes() / groupSizeX);
                cmdList->SetPipelineState(volume->GetProbeClassificationPSO());
                cmdList->Dispatch(numGroupsX, 1, 1);

                // Add a barrier
                barrier.UAV.pResource = volume->GetProbeData();
                barriers.push_back(barrier);
            }

            // Probe Classification Barrier(s)
            if (!barriers.empty()) cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

            if (bInsertPerfMarkers) PIXEndEvent(cmdList);

            return ERTXGIStatus::OK;
        }

        //------------------------------------------------------------------------
        // Private DDGIVolume Functions
        //------------------------------------------------------------------------

    #if RTXGI_DDGI_RESOURCE_MANAGEMENT
        void DDGIVolume::ReleaseManagedResources()
        {
            // Release the root signature and RTV descriptor heap
            RTXGI_SAFE_RELEASE(m_rootSignature);
            RTXGI_SAFE_RELEASE(m_rtvDescriptorHeap);

            // Release the existing compute PSOs
            RTXGI_SAFE_RELEASE(m_probeBlendingIrradiancePSO);
            RTXGI_SAFE_RELEASE(m_probeBlendingDistancePSO);
            RTXGI_SAFE_RELEASE(m_probeBorderRowUpdateIrradiancePSO);
            RTXGI_SAFE_RELEASE(m_probeBorderRowUpdateDistancePSO);
            RTXGI_SAFE_RELEASE(m_probeBorderColumnUpdateIrradiancePSO);
            RTXGI_SAFE_RELEASE(m_probeBorderColumnUpdateDistancePSO);
            RTXGI_SAFE_RELEASE(m_probeRelocationPSO);
            RTXGI_SAFE_RELEASE(m_probeRelocationResetPSO);
            RTXGI_SAFE_RELEASE(m_probeClassificationPSO);
            RTXGI_SAFE_RELEASE(m_probeClassificationResetPSO);
        }

        ERTXGIStatus DDGIVolume::CreateManagedResources(const DDGIVolumeDesc& desc, const DDGIVolumeManagedResourcesDesc& managed)
        {
            bool deviceChanged = IsDeviceChanged(managed);

            // Create the root signature and pipeline state objects
            if (deviceChanged)
            {
                // The device may have changed, release resources on that device
                if (m_device != nullptr) ReleaseManagedResources();

                // Store the handle to the new device
                m_device = managed.device;

                // Create the root signature
                if (!CreateRootSignature()) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_ROOT_SIGNATURE;

                // Create the pipeline state objects
                if (!CreateComputePSO(
                    managed.probeBlendingIrradianceCS,
                    &m_probeBlendingIrradiancePSO,
                    L"Probe Irradiance Blending")) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_PSO;

                if (!CreateComputePSO(
                    managed.probeBlendingDistanceCS,
                    &m_probeBlendingDistancePSO,
                    L"Probe Distance Blending")) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_PSO;

                if (!CreateComputePSO(
                    managed.probeBorderRowUpdateIrradianceCS,
                    &m_probeBorderRowUpdateIrradiancePSO,
                    L"Probe Border Row Update (Irradiance)")) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_PSO;

                if (!CreateComputePSO(
                    managed.probeBorderRowUpdateDistanceCS,
                    &m_probeBorderRowUpdateDistancePSO,
                    L"Probe Border Row Update (Distance)")) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_PSO;

                if (!CreateComputePSO(
                    managed.probeBorderColumnUpdateIrradianceCS,
                    &m_probeBorderColumnUpdateIrradiancePSO,
                    L"Probe Border Column Update (Irradiance)")) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_PSO;

                if (!CreateComputePSO(
                    managed.probeBorderColumnUpdateDistanceCS,
                    &m_probeBorderColumnUpdateDistancePSO,
                    L"Probe Border Column Update (Distance)")) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_PSO;

                if (!CreateComputePSO(
                    managed.probeRelocation.updateCS,
                    &m_probeRelocationPSO,
                    L"Probe Relocation")) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_PSO;

                if (!CreateComputePSO(
                    managed.probeRelocation.resetCS,
                    &m_probeRelocationResetPSO,
                    L"Probe Relocation Reset")) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_PSO;

                if (!CreateComputePSO(
                    managed.probeClassification.updateCS,
                    &m_probeClassificationPSO,
                    L"Probe Classification")) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_PSO;

                if (!CreateComputePSO(
                    managed.probeClassification.resetCS,
                    &m_probeClassificationResetPSO,
                    L"Probe Classification Reset")) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_PSO;
            }

            // Create the textures
            if (deviceChanged || m_desc.ShouldAllocateProbes(desc))
            {
                // Probe counts have changed. The textures are the wrong size or aren't allocated yet.
                // (Re)allocate the probe ray data, irradiance, distance, and data textures.
                if (!CreateProbeRayData(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_RAY_DATA;
                if (!CreateProbeIrradiance(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_IRRADIANCE;
                if (!CreateProbeDistance(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_DISTANCE;
                if (!CreateProbeData(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_DATA;
            }
            else
            {
                if (m_desc.ShouldAllocateRayData(desc))
                {
                    // The number of rays to trace per probe has changed. Reallocate the ray data texture.
                    if (!CreateProbeRayData(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_RAY_DATA;
                }

                if (m_desc.ShouldAllocateIrradiance(desc))
                {
                    // The number of irradiance texels per probe has changed. Reallocate the irradiance texture.
                    if (!CreateProbeIrradiance(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_IRRADIANCE;
                }

                if (m_desc.ShouldAllocateDistance(desc))
                {
                    // The number of distance texels per probe has changed. Reallocate the distance texture.
                    if (!CreateProbeDistance(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_DISTANCE;
                }
            }

            return ERTXGIStatus::OK;
        }
    #else
        void DDGIVolume::StoreUnmanagedResourcesDesc(const DDGIVolumeUnmanagedResourcesDesc& unmanaged)
        {
            // Root Signature
            m_rootSignature = unmanaged.rootSignature;

            // Store the root parameter slots. These values are set by GetDDGIVolumeRootSignatureDesc() in Managed Mode.
            m_rootParamSlotRootConstants = unmanaged.rootParamSlotRootConstants;
            m_rootParamSlotDescriptorTable = unmanaged.rootParamSlotDescriptorTable;

            // Textures
            m_probeRayData = unmanaged.probeRayData;
            m_probeIrradiance = unmanaged.probeIrradiance;
            m_probeDistance = unmanaged.probeDistance;
            m_probeData = unmanaged.probeData;

            // Render Target Views
            m_probeIrradianceRTV = unmanaged.probeIrradianceRTV;
            m_probeDistanceRTV = unmanaged.probeDistanceRTV;

            // Pipeline State Objects
            m_probeBlendingIrradiancePSO = unmanaged.probeBlendingIrradiancePSO;
            m_probeBlendingDistancePSO = unmanaged.probeBlendingDistancePSO;
            m_probeBorderRowUpdateIrradiancePSO = unmanaged.probeBorderRowUpdateIrradiancePSO;
            m_probeBorderRowUpdateDistancePSO = unmanaged.probeBorderRowUpdateDistancePSO;
            m_probeBorderColumnUpdateIrradiancePSO = unmanaged.probeBorderColumnUpdateIrradiancePSO;
            m_probeBorderColumnUpdateDistancePSO = unmanaged.probeBorderColumnUpdateDistancePSO;

            m_probeRelocationPSO = unmanaged.probeRelocation.updatePSO;
            m_probeRelocationResetPSO = unmanaged.probeRelocation.resetPSO;

            m_probeClassificationPSO = unmanaged.probeClassification.updatePSO;
            m_probeClassificationResetPSO = unmanaged.probeClassification.resetPSO;
        }
    #endif

        //------------------------------------------------------------------------
        // Public DDGIVolume Functions
        //------------------------------------------------------------------------

        ERTXGIStatus DDGIVolume::Create(const DDGIVolumeDesc& desc, const DDGIVolumeResources& resources)
        {
            // Validate the probe counts
            if (desc.probeCounts.x <= 0 || desc.probeCounts.y <= 0 || desc.probeCounts.z <= 0)
            {
                return ERTXGIStatus::ERROR_DDGI_INVALID_PROBE_COUNTS;
            }

            // Validate the descriptor heap
            if (resources.descriptorHeapDesc.heap == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_DESCRIPTOR_HEAP;

            // Validate the constants buffer
        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            if (resources.constantsBuffer == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_CONSTANTS_BUFFER;
        #endif

            // Validate the resource structures
            if (resources.managed.enabled && resources.unmanaged.enabled) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCES_DESC;
            if (!resources.managed.enabled && !resources.unmanaged.enabled) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCES_DESC;

            // Validate the resources
            ERTXGIStatus result = ERTXGIStatus::OK;
        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            result = ValidateManagedResourcesDesc(resources.managed);
        #else
            result = ValidateUnmanagedResourcesDesc(resources.unmanaged);
        #endif
            if (result != ERTXGIStatus::OK) return result;

            // Store the descriptor heap pointer and offsets
            m_descriptorHeap = resources.descriptorHeapDesc.heap;
            m_descriptorHeapConstsOffset = resources.descriptorHeapDesc.constsOffset;
            m_descriptorHeapUAVOffset = resources.descriptorHeapDesc.uavOffset;
            m_descriptorHeapSRVOffset = resources.descriptorHeapDesc.srvOffset;

            // Always stored (even in managed mode) for convenience. This is helpful when other parts of an application
            // (e.g. ray tracing passes) access resources bindlessly and use the volume to look up resource offsets.
            // See DDGI_D3D12.cpp::RayTraceVolume() for an example.
            m_descriptorBindlessUAVOffset = resources.descriptorBindlessDesc.uavOffset;
            m_descriptorBindlessSRVOffset = resources.descriptorBindlessDesc.srvOffset;

            // Store the constants structured buffer pointers and size
            if (resources.constantsBuffer) m_constantsBuffer = resources.constantsBuffer;
            if (resources.constantsBufferUpload) m_constantsBufferUpload = resources.constantsBufferUpload;
            m_constantsBufferSizeInBytes = resources.constantsBufferSizeInBytes;

            // Allocate or store pointers to the root signature, textures, and pipeline state objects
        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            result = CreateManagedResources(desc, resources.managed);
            if (result != ERTXGIStatus::OK) return result;
        #else
            StoreUnmanagedResourcesDesc(resources.unmanaged);
        #endif

            // Store the new volume descriptor
            m_desc = desc;

        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            // Create the resource descriptors
            if (!CreateDescriptors()) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_DESCRIPTORS;
        #endif

            // Store the volume rotation
            m_rotationMatrix = EulerAnglesToRotationMatrixYXZ(m_desc.eulerAngles);
            m_rotationQuaternion = RotationMatrixToQuaternion(m_rotationMatrix);

            // Set the default scroll anchor to the origin
            m_probeScrollAnchor = m_desc.origin;

            // Initialize the random number generator if a seed is provided,
            // otherwise the RNG uses the default std::random_device().
            if (desc.rngSeed != 0)
            {
                SeedRNG(desc.rngSeed);
            }
            else
            {
                std::random_device rd;
                SeedRNG(rd());
            }

            return ERTXGIStatus::OK;
        }

        ERTXGIStatus DDGIVolume::ClearProbes(ID3D12GraphicsCommandList* cmdList)
        {
            if (bInsertPerfMarkers) PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), "Clear Probes");

            // Transition the probe textures render targets
            D3D12_RESOURCE_BARRIER barriers[2] = {};
            barriers[0].Transition.pResource = m_probeIrradiance;
            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            barriers[1].Transition.pResource = m_probeDistance;
            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            // Wait for the transitions
            cmdList->ResourceBarrier(2, barriers);

            float values[4] = { 0.f, 0.f, 0.f, 0.f };

            // Clear the probe data
            cmdList->ClearRenderTargetView(m_probeIrradianceRTV, values, 0, nullptr); // Clear probe irradiance
            cmdList->ClearRenderTargetView(m_probeDistanceRTV, values, 0, nullptr);   // Clear probe distance

            // Transition the probe textures back to unordered access
            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

            // Wait for the transitions
            cmdList->ResourceBarrier(2, barriers);

            if (bInsertPerfMarkers) PIXEndEvent(cmdList);

            return ERTXGIStatus::OK;
        }

        void DDGIVolume::Destroy()
        {
            m_descriptorHeap = nullptr;
            m_descriptorHeapDescSize = 0;
            m_descriptorHeapConstsOffset = 0;
            m_descriptorHeapUAVOffset = 0;
            m_descriptorHeapSRVOffset = 0;

            m_descriptorBindlessUAVOffset = 0;
            m_descriptorBindlessSRVOffset = 0;

            m_constantsBuffer = nullptr;
            m_constantsBufferUpload = nullptr;
            m_constantsBufferSizeInBytes = 0;

            m_rootParamSlotRootConstants = 0;
            m_rootParamSlotDescriptorTable = 0;

            m_probeIrradianceRTV.ptr = 0;
            m_probeDistanceRTV.ptr = 0;

            m_desc = {};

            m_rotationQuaternion = { 0.f, 0.f, 0.f, 1.f };
            m_rotationMatrix = {
                { 1.f, 0.f, 0.f },
                { 0.f, 1.f, 0.f },
                { 0.f, 0.f, 1.f }
            };
            m_probeRayRotationQuaternion = { 0.f, 0.f, 0.f, 1.f };
            m_probeRayRotationMatrix = {
                { 1.f, 0.f, 0.f },
                { 0.f, 1.f, 0.f },
                { 0.f, 0.f, 1.f }
            };

            m_probeScrollOffsets = {};

        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            m_device = nullptr;

            RTXGI_SAFE_RELEASE(m_rootSignature);
            RTXGI_SAFE_RELEASE(m_rtvDescriptorHeap);

            RTXGI_SAFE_RELEASE(m_probeRayData);
            RTXGI_SAFE_RELEASE(m_probeIrradiance);
            RTXGI_SAFE_RELEASE(m_probeDistance);
            RTXGI_SAFE_RELEASE(m_probeData);

            RTXGI_SAFE_RELEASE(m_probeBlendingIrradiancePSO);
            RTXGI_SAFE_RELEASE(m_probeBlendingDistancePSO);
            RTXGI_SAFE_RELEASE(m_probeBorderRowUpdateIrradiancePSO);
            RTXGI_SAFE_RELEASE(m_probeBorderRowUpdateDistancePSO);
            RTXGI_SAFE_RELEASE(m_probeBorderColumnUpdateIrradiancePSO);
            RTXGI_SAFE_RELEASE(m_probeBorderColumnUpdateDistancePSO);
            RTXGI_SAFE_RELEASE(m_probeRelocationPSO);
            RTXGI_SAFE_RELEASE(m_probeRelocationResetPSO);
            RTXGI_SAFE_RELEASE(m_probeClassificationPSO);
            RTXGI_SAFE_RELEASE(m_probeClassificationResetPSO);
        #else
            m_rootSignature = nullptr;

            m_probeRayData = nullptr;
            m_probeIrradiance = nullptr;
            m_probeDistance = nullptr;
            m_probeData = nullptr;

            m_probeBlendingIrradiancePSO = nullptr;
            m_probeBlendingDistancePSO = nullptr;
            m_probeBorderRowUpdateIrradiancePSO = nullptr;
            m_probeBorderRowUpdateDistancePSO = nullptr;
            m_probeBorderColumnUpdateIrradiancePSO = nullptr;
            m_probeBorderColumnUpdateDistancePSO = nullptr;
            m_probeRelocationPSO = nullptr;
            m_probeRelocationResetPSO = nullptr;
            m_probeClassificationPSO = nullptr;
            m_probeClassificationResetPSO = nullptr;
        #endif;
        }

        //------------------------------------------------------------------------
        // Private Resource Allocation Helper Functions (Managed Resources)
        //------------------------------------------------------------------------

    #if RTXGI_DDGI_RESOURCE_MANAGEMENT
        bool DDGIVolume::CreateDescriptors()
        {
            D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle, srvHandle, uavHandle;
            cbvHandle = srvHandle = uavHandle = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();

            m_descriptorHeapDescSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            // Initialize descriptor handles
            cbvHandle.ptr += (m_descriptorHeapConstsOffset * m_descriptorHeapDescSize);
            uavHandle.ptr += (m_descriptorHeapUAVOffset * m_descriptorHeapDescSize);
            srvHandle.ptr += (m_descriptorHeapSRVOffset * m_descriptorHeapDescSize);

            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

            // Constants structured buffer descriptor
            {
                D3D12_SHADER_RESOURCE_VIEW_DESC cbvDesc = {};
                cbvDesc.Format = DXGI_FORMAT_UNKNOWN;
                cbvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                cbvDesc.Buffer.NumElements = (m_desc.index + 1);
                cbvDesc.Buffer.StructureByteStride = sizeof(DDGIVolumeDescGPUPacked);
                cbvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

                m_device->CreateShaderResourceView(m_constantsBuffer, &cbvDesc, cbvHandle);
            }

            // Probe ray data texture descriptors
            {
                srvDesc.Format = uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::RayData, m_desc.probeRayDataFormat);
                m_device->CreateUnorderedAccessView(m_probeRayData, nullptr, &uavDesc, uavHandle);
                m_device->CreateShaderResourceView(m_probeRayData, &srvDesc, srvHandle);
            }

            uavHandle.ptr += m_descriptorHeapDescSize;
            srvHandle.ptr += m_descriptorHeapDescSize;

            // Probe irradiance texture descriptors
            {
                srvDesc.Format = uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Irradiance, m_desc.probeIrradianceFormat);
                m_device->CreateUnorderedAccessView(m_probeIrradiance, nullptr, &uavDesc, uavHandle);
                m_device->CreateShaderResourceView(m_probeIrradiance, &srvDesc, srvHandle);
            }

            uavHandle.ptr += m_descriptorHeapDescSize;
            srvHandle.ptr += m_descriptorHeapDescSize;

            // Probe distance texture descriptors
            {
                srvDesc.Format = uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Distance, m_desc.probeDistanceFormat);
                m_device->CreateUnorderedAccessView(m_probeDistance, nullptr, &uavDesc, uavHandle);
                m_device->CreateShaderResourceView(m_probeDistance, &srvDesc, srvHandle);
            }

            uavHandle.ptr += m_descriptorHeapDescSize;
            srvHandle.ptr += m_descriptorHeapDescSize;

            // Probe data texture descriptors
            {
                srvDesc.Format = uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Data, m_desc.probeDataFormat);
                m_device->CreateUnorderedAccessView(m_probeData, nullptr, &uavDesc, uavHandle);
                m_device->CreateShaderResourceView(m_probeData, &srvDesc, srvHandle);
            }

            // Describe the RTV heap
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.NumDescriptors = GetDDGIVolumeNumRTVDescriptors();
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

            // Create the RTV heap
            HRESULT hr = m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_rtvDescriptorHeap));
            if (FAILED(hr)) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::wstring name = L"DDGIVolume[" + std::to_wstring(m_desc.index) + L"], RTV Descriptor Heap";
            m_rtvDescriptorHeap->SetName(name.c_str());
        #endif

            UINT rtvDescHeapEntrySize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

            // Describe the RTV
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

            // Probe Irradiance
            rtvDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Irradiance, m_desc.probeIrradianceFormat);
            m_probeIrradianceRTV = m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
            m_device->CreateRenderTargetView(m_probeIrradiance, &rtvDesc, m_probeIrradianceRTV);

            // Probe Distance
            rtvDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Distance, m_desc.probeDistanceFormat);
            m_probeDistanceRTV.ptr = m_probeIrradianceRTV.ptr + rtvDescHeapEntrySize;
            m_device->CreateRenderTargetView(m_probeDistance, &rtvDesc, m_probeDistanceRTV);

            return true;
        }

        bool DDGIVolume::CreateRootSignature()
        {
            ID3DBlob* signature = nullptr;
            if (!GetDDGIVolumeRootSignatureDesc(m_descriptorHeapConstsOffset, m_descriptorHeapUAVOffset, signature)) return false;

            // Set the root param slots
            m_rootParamSlotRootConstants = 0;
            m_rootParamSlotDescriptorTable = 1;

            // Create the root signature
            HRESULT hr = m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
            RTXGI_SAFE_RELEASE(signature);
            if (FAILED(hr)) return false;

        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::wstring name = L"DDGIVolume[" + std::to_wstring(m_desc.index) + L"], Root Signature";
            m_rootSignature->SetName(name.c_str());
        #endif

            return true;
        }

        bool DDGIVolume::CreateComputePSO(ShaderBytecode shader, ID3D12PipelineState** pipeline, std::wstring debugName = L"")
        {
            if (m_rootSignature == nullptr) return false;

            D3D12_COMPUTE_PIPELINE_STATE_DESC pipeDesc = {};
            pipeDesc.CS.BytecodeLength = shader.size;
            pipeDesc.CS.pShaderBytecode = shader.pData;
            pipeDesc.pRootSignature = m_rootSignature;

            HRESULT hr = m_device->CreateComputePipelineState(&pipeDesc, IID_PPV_ARGS(pipeline));
            if (FAILED(hr)) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::wstring name = L"DDGIVolume[" + std::to_wstring(m_desc.index) + L"]," + debugName + L" PSO";
            (*pipeline)->SetName(name.c_str());
        #endif

            return true;
        }

        bool DDGIVolume::CreateTexture(UINT64 width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_STATES state, ID3D12Resource** resource)
        {
            D3D12_HEAP_PROPERTIES defaultHeapProperties = {};
            defaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

            // Describe the texture
            D3D12_RESOURCE_DESC desc = {};
            desc.Format = format;
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = 1;
            desc.DepthOrArraySize = 1;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

            // Setup the optimized clear value
            D3D12_CLEAR_VALUE clear = {};
            clear.Format = format;

            // Create the texture
            HRESULT hr = m_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, state, &clear, IID_PPV_ARGS(resource));
            if (FAILED(hr)) return false;
            return true;
        }

        bool DDGIVolume::CreateProbeRayData(const DDGIVolumeDesc& desc)
        {
            RTXGI_SAFE_RELEASE(m_probeRayData);

            UINT width = 0;
            UINT height = 0;
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

            // Get the texture dimensions and format
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::RayData, width, height);
            format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::RayData, desc.probeRayDataFormat);

            // Check for problems
            if (width <= 0 || height <= 0) return false;

            // Create the texture resource
            bool result = CreateTexture(width, height, format, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, &m_probeRayData);
            if (!result) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::wstring name = L"DDGIVolume[" + std::to_wstring(desc.index) + L"], Probe Ray Data";
            m_probeRayData->SetName(name.c_str());
        #endif

            return true;
        }

        bool DDGIVolume::CreateProbeIrradiance(const DDGIVolumeDesc& desc)
        {
            RTXGI_SAFE_RELEASE(m_probeIrradiance);

            UINT width = 0;
            UINT height = 0;
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

            // Get the texture dimensions and format
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::Irradiance, width, height);
            format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Irradiance, desc.probeIrradianceFormat);

            // Check for problems
            if (width <= 0 || height <= 0) return false;

            // Create the texture resource
            bool result = CreateTexture(width, height, format, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &m_probeIrradiance);
            if (!result) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::wstring name = L"DDGIVolume[" + std::to_wstring(desc.index) + L"], Probe Irradiance";
            m_probeIrradiance->SetName(name.c_str());
        #endif
            return true;
        }

        bool DDGIVolume::CreateProbeDistance(const DDGIVolumeDesc& desc)
        {
            RTXGI_SAFE_RELEASE(m_probeDistance);

            UINT width = 0;
            UINT height = 0;
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

            // Get the texture dimensions and format
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::Distance, width, height);
            format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Distance, desc.probeDistanceFormat);

            // Check for problems
            if (width <= 0 || height <= 0) return false;

            // Create the texture resource
            bool result = CreateTexture(width, height, format, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &m_probeDistance);
            if (!result) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::wstring name = L"DDGIVolume[" + std::to_wstring(desc.index) + L"], Probe Distance";
            m_probeDistance->SetName(name.c_str());
        #endif

            return true;
        }

        bool DDGIVolume::CreateProbeData(const DDGIVolumeDesc& desc)
        {
            RTXGI_SAFE_RELEASE(m_probeData);

            UINT width = 0;
            UINT height = 0;
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

            // Get the texture dimensions and format
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::Data, width, height);
            format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Data, desc.probeDataFormat);

            // Check for problems
            if (width <= 0 || height <= 0) return false;

            // Create the texture resource
            bool result = CreateTexture(width, height, format, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, &m_probeData);
            if (!result) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::wstring name = L"DDGIVolume[" + std::to_wstring(desc.index) + L"], Probe Data";
            m_probeData->SetName(name.c_str());
        #endif

            return true;
        }

    #endif // RTXGI_DDGI_RESOURCE_MANAGEMENT

    } // namespace d3d12
} // namespace rtxgi
