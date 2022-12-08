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

#include <random>
#include <string>
#include <vector>

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
            if (!ValidateShaderBytecode(desc.probeRelocation.updateCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_RELOCATION;
            if (!ValidateShaderBytecode(desc.probeRelocation.resetCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_RELOCATION_RESET;
            if (!ValidateShaderBytecode(desc.probeClassification.updateCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_CLASSIFICATION;
            if (!ValidateShaderBytecode(desc.probeClassification.resetCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_CLASSIFICATION_RESET;
            if (!ValidateShaderBytecode(desc.probeVariability.reductionCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_VARIABILITY_REDUCTION;
            if (!ValidateShaderBytecode(desc.probeVariability.extraReductionCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_VARIABILITY_EXTRA_REDUCTION;

            return ERTXGIStatus::OK;
        }

        ERTXGIStatus ValidateUnmanagedResourcesDesc(const DDGIVolumeUnmanagedResourcesDesc& desc)
        {
            // Root Signature
            if (desc.rootSignature == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_ROOT_SIGNATURE;

            // Texture Arrays
            if (desc.probeRayData == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_RAY_DATA;
            if (desc.probeIrradiance == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_IRRADIANCE;
            if (desc.probeDistance == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_DISTANCE;
            if (desc.probeData == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_DATA;
            if (desc.probeVariability == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_VARIABILITY;
            if (desc.probeVariabilityAverage == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_VARIABILITY_AVERAGE;
            if (desc.probeVariabilityReadback == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_VARIABILITY_READBACK;

            // Render Target Views
            if (desc.probeIrradianceRTV.ptr == 0) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_DESCRIPTOR;
            if (desc.probeDistanceRTV.ptr == 0) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_DESCRIPTOR;

            // Pipeline State Objects
            if (desc.probeBlendingIrradiancePSO == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_PSO_PROBE_BLENDING_IRRADIANCE;
            if (desc.probeBlendingDistancePSO == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_PSO_PROBE_BLENDING_DISTANCE;
            if (desc.probeRelocation.updatePSO == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_PSO_PROBE_RELOCATION;
            if (desc.probeRelocation.resetPSO == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_PSO_PROBE_RELOCATION_RESET;
            if (desc.probeClassification.updatePSO == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_PSO_PROBE_CLASSIFICATION;
            if (desc.probeClassification.resetPSO == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_PSO_PROBE_CLASSIFICATION_RESET;
            if (desc.probeVariabilityPSOs.reductionPSO == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_PSO_PROBE_REDUCTION;
            if (desc.probeVariabilityPSOs.extraReductionPSO == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_PSO_PROBE_EXTRA_REDUCTION;

            return ERTXGIStatus::OK;
        }

        //------------------------------------------------------------------------
        // Public RTXGI D3D12 Namespace Functions
        //------------------------------------------------------------------------

        DXGI_FORMAT GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType type, EDDGIVolumeTextureFormat format)
        {
            if (type == EDDGIVolumeTextureType::RayData)
            {
                if (format == EDDGIVolumeTextureFormat::F32x2) return DXGI_FORMAT_R32G32_FLOAT;
                else if (format == EDDGIVolumeTextureFormat::F32x4) return DXGI_FORMAT_R32G32B32A32_FLOAT;
            }
            else if (type == EDDGIVolumeTextureType::Irradiance)
            {
                if (format == EDDGIVolumeTextureFormat::U32) return DXGI_FORMAT_R10G10B10A2_UNORM;
                else if (format == EDDGIVolumeTextureFormat::F16x4) return DXGI_FORMAT_R16G16B16A16_FLOAT;
                else if (format == EDDGIVolumeTextureFormat::F32x4) return DXGI_FORMAT_R32G32B32A32_FLOAT;
            }
            else if (type == EDDGIVolumeTextureType::Distance)
            {
                if (format == EDDGIVolumeTextureFormat::F16x2) return DXGI_FORMAT_R16G16_FLOAT;  // Note: in large environments FP16 may not be sufficient
                else if (format == EDDGIVolumeTextureFormat::F32x2) return DXGI_FORMAT_R32G32_FLOAT;
            }
            else if (type == EDDGIVolumeTextureType::Data)
            {
                if (format == EDDGIVolumeTextureFormat::F16x4) return DXGI_FORMAT_R16G16B16A16_FLOAT;
                else if (format == EDDGIVolumeTextureFormat::F32x4) return DXGI_FORMAT_R32G32B32A32_FLOAT;
            }
            else if (type == EDDGIVolumeTextureType::Variability)
            {
                if (format == EDDGIVolumeTextureFormat::F16) return DXGI_FORMAT_R16_FLOAT;
                else if(format == EDDGIVolumeTextureFormat::F32) return DXGI_FORMAT_R32_FLOAT;
            }
            else if (type == EDDGIVolumeTextureType::VariabilityAverage)
            {
                return DXGI_FORMAT_R32G32_FLOAT;
            }
            return DXGI_FORMAT_UNKNOWN;
        }

        bool GetDDGIVolumeRootSignatureDesc(const DDGIVolumeDescriptorHeapDesc& heapDesc, ID3DBlob*& signature)
        {
            // Resource Descriptor Table
            // 1 SRV for constants structured buffer      (t0, space1)
            // 1 UAV for ray data texture array           (u0, space1)
            // 1 UAV for probe irradiance texture array   (u1, space1)
            // 1 UAV for probe distance texture array     (u2, space1)
            // 1 UAV for probe data texture array         (u3, space1)
            // 1 UAV for probe variation array            (u4, space1)
            // 1 UAV for probe variation average array    (u5, space1)
            D3D12_DESCRIPTOR_RANGE ranges[7];

            // Volume Constants Structured Buffer (t0, space1)
            ranges[0].NumDescriptors = 1;
            ranges[0].BaseShaderRegister = 0;
            ranges[0].RegisterSpace = 1;
            ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            ranges[0].OffsetInDescriptorsFromTableStart = heapDesc.constantsIndex;

            // Ray Data Texture Array UAV (u0, space1)
            ranges[1].NumDescriptors = 1;
            ranges[1].BaseShaderRegister = 0;
            ranges[1].RegisterSpace = 1;
            ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            ranges[1].OffsetInDescriptorsFromTableStart = heapDesc.resourceIndices.rayDataUAVIndex;

            // Probe Irradiance Texture Array UAV (u1, space1)
            ranges[2].NumDescriptors = 1;
            ranges[2].BaseShaderRegister = 1;
            ranges[2].RegisterSpace = 1;
            ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            ranges[2].OffsetInDescriptorsFromTableStart = heapDesc.resourceIndices.probeIrradianceUAVIndex;

            // Probe Distance Texture Array UAV (u2, space1)
            ranges[3].NumDescriptors = 1;
            ranges[3].BaseShaderRegister = 2;
            ranges[3].RegisterSpace = 1;
            ranges[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            ranges[3].OffsetInDescriptorsFromTableStart = heapDesc.resourceIndices.probeDistanceUAVIndex;

            // Probe Data Texture Array UAV (u3, space1)
            ranges[4].NumDescriptors = 1;
            ranges[4].BaseShaderRegister = 3;
            ranges[4].RegisterSpace = 1;
            ranges[4].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            ranges[4].OffsetInDescriptorsFromTableStart = heapDesc.resourceIndices.probeDataUAVIndex;

            // Probe Variability Texture Array UAV (u4, space1)
            ranges[5].NumDescriptors = 1;
            ranges[5].BaseShaderRegister = 4;
            ranges[5].RegisterSpace = 1;
            ranges[5].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            ranges[5].OffsetInDescriptorsFromTableStart = heapDesc.resourceIndices.probeVariabilityUAVIndex;

            // Probe Variability Average Texture Array UAV (u5, space1)
            ranges[6].NumDescriptors = 1;
            ranges[6].BaseShaderRegister = 5;
            ranges[6].RegisterSpace = 1;
            ranges[6].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            ranges[6].OffsetInDescriptorsFromTableStart = heapDesc.resourceIndices.probeVariabilityAverageUAVIndex;

            // Root Parameters
            std::vector<D3D12_ROOT_PARAMETER> rootParameters;

            // Root Parameter 0: DDGI Root Constants (b0, space1)
            {
                D3D12_ROOT_PARAMETER param = {};
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
                param.Constants.ShaderRegister = 0;
                param.Constants.RegisterSpace = 1;
                param.Constants.Num32BitValues = DDGIRootConstants::GetAlignedNum32BitValues();
                rootParameters.push_back(param);
            }

            // Root Parameter 1: Resource Descriptor Table
            {
                D3D12_ROOT_PARAMETER param = {};
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
                param.DescriptorTable.NumDescriptorRanges = _countof(ranges);
                param.DescriptorTable.pDescriptorRanges = ranges;
                rootParameters.push_back(param);
            }

            // Describe the root signature
            D3D12_ROOT_SIGNATURE_DESC desc = {};
            desc.NumParameters = (UINT)rootParameters.size();
            desc.pParameters = rootParameters.data();
            desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

            // Create the root signature desc blob
            ID3DBlob* error = nullptr;
            HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
            if (FAILED(hr))
            {
                RTXGI_SAFE_RELEASE(error);
                return false;
            }

            return true;
        }

        ERTXGIStatus UploadDDGIVolumeResourceIndices(ID3D12GraphicsCommandList* cmdList, UINT bufferingIndex, UINT numVolumes, DDGIVolume** volumes)
        {
            // Copy the resource indices for each volume
            for (UINT volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                // Get the volume
                const DDGIVolume* volume = volumes[volumeIndex];

                // Validate the upload and device buffers
                if (volume->GetResourceIndicesBuffer() == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_INDICES_BUFFER;
                if (volume->GetResourceIndicesBufferUpload() == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_INDICES_UPLOAD_BUFFER;

                // Map the resource indices buffer and update it
                UINT8* pData = nullptr;
                HRESULT hr = volume->GetResourceIndicesBufferUpload()->Map(0, nullptr, reinterpret_cast<void**>(&pData));
                if (FAILED(hr)) return ERTXGIStatus::ERROR_DDGI_MAP_FAILURE_RESOURCE_INDICES_UPLOAD_BUFFER;

                // Offset to the resource indices data to write to (e.g. double buffering)
                UINT64 bufferOffset = volume->GetResourceIndicesBufferSizeInBytes() * bufferingIndex;

                // Offset to the volume in current resource indices buffer
                UINT volumeOffset = (volume->GetIndex() * sizeof(DDGIVolumeResourceIndices));

                // Offset to the volume resource indices in the upload buffer
                UINT64 srcOffset = (bufferOffset + volumeOffset);

                // Get the DDGIVolume's bindless resource indices
                DDGIVolumeResourceIndices resourceIndices = volume->GetResourceIndices();

                pData += srcOffset;
                memcpy(pData, &resourceIndices, sizeof(DDGIVolumeResourceIndices));

                volume->GetResourceIndicesBufferUpload()->Unmap(0, nullptr);

                // Schedule a copy of the upload buffer to the device buffer
                cmdList->CopyBufferRegion(volume->GetResourceIndicesBuffer(), volumeOffset, volume->GetResourceIndicesBufferUpload(), srcOffset, sizeof(DDGIVolumeResourceIndices));
            }

            return ERTXGIStatus::OK;
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
                const DDGIVolumeDescGPUPacked gpuDesc = volume->GetDescGPUPacked();

            #ifdef _DEBUG
                volume->ValidatePackedData(gpuDesc);
            #endif

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
            if (bInsertPerfMarkers) PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), "RTXGI DDGI Update Probes");

            UINT volumeIndex;
            std::vector<D3D12_RESOURCE_BARRIER> barriers;

            // Irradiance Blending
            if (bInsertPerfMarkers) PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), "Probe Irradiance");
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                // Get the volume
                const DDGIVolume* volume = volumes[volumeIndex];

                // Set the descriptor heap(s)
                std::vector<ID3D12DescriptorHeap*> heaps;
                heaps.push_back(volume->GetResourceDescriptorHeap());
                if(volume->GetSamplerDescriptorHeap()) heaps.push_back(volume->GetSamplerDescriptorHeap());
                cmdList->SetDescriptorHeaps((UINT)heaps.size(), heaps.data());

                // Set root signature and root constants
                cmdList->SetComputeRootSignature(volume->GetRootSignature());
                cmdList->SetComputeRoot32BitConstants(volume->GetRootParamSlotRootConstants(), DDGIRootConstants::GetNum32BitValues(), volume->GetRootConstants().GetData(), 0);

                // Set the descriptor tables (when relevant)
                if(volume->GetBindlessEnabled())
                {
                    // Bindless resources, using application's root signature
                    if(volume->GetBindlessType() == EBindlessType::RESOURCE_ARRAYS)
                    {
                        // Only need to set descriptor tables when using traditional resource array bindless
                        cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotResourceDescriptorTable(), volume->GetResourceDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                        if (volume->GetSamplerDescriptorHeap()) cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotSamplerDescriptorTable(), volume->GetSamplerDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                    }
                }
                else
                {
                    // Bound resources, using the SDK's root signature
                    cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotResourceDescriptorTable(), volume->GetResourceDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                }

                // Get the number of probes on each axis
                UINT probeCountX, probeCountY, probeCountZ;
                GetDDGIVolumeProbeCounts(volume->GetDesc(), probeCountX, probeCountY, probeCountZ);

                // Probe irradiance blending
                {
                    if(bInsertPerfMarkers && volume->GetInsertPerfMarkers())
                    {
                        std::string msg = "Irradiance, DDGIVolume[" + std::to_string(volume->GetIndex()) + "] - \"" + volume->GetName() + "\"";
                        PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), msg.c_str());
                    }

                    // Set the PSO and dispatch threads
                    cmdList->SetPipelineState(volume->GetProbeBlendingIrradiancePSO());
                    cmdList->Dispatch(probeCountX, probeCountY, probeCountZ);

                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers()) PIXEndEvent(cmdList);
                }

                // Add a barrier
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                barrier.UAV.pResource = volume->GetProbeIrradiance();
                barriers.push_back(barrier);
                barrier.UAV.pResource = volume->GetProbeVariability();
                barriers.push_back(barrier);
            }
            if (bInsertPerfMarkers) PIXEndEvent(cmdList);

            // Distance
            if (bInsertPerfMarkers) PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), "Probe Distance");
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                // Get the volume
                const DDGIVolume* volume = volumes[volumeIndex];

                // Set the descriptor heap(s)
                std::vector<ID3D12DescriptorHeap*> heaps;
                heaps.push_back(volume->GetResourceDescriptorHeap());
                if (volume->GetSamplerDescriptorHeap()) heaps.push_back(volume->GetSamplerDescriptorHeap());
                cmdList->SetDescriptorHeaps((UINT)heaps.size(), heaps.data());

                // Set root signature and root constants
                cmdList->SetComputeRootSignature(volume->GetRootSignature());
                cmdList->SetComputeRoot32BitConstants(volume->GetRootParamSlotRootConstants(), DDGIRootConstants::GetNum32BitValues(), volume->GetRootConstants().GetData(), 0);

                // Set the descriptor tables (when relevant)
                if (volume->GetBindlessEnabled())
                {
                    // Bindless resources, using application's root signature
                    if (volume->GetBindlessType() == EBindlessType::RESOURCE_ARRAYS)
                    {
                        // Only need to set descriptor tables when using traditional resource array bindless
                        cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotResourceDescriptorTable(), volume->GetResourceDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                        if (volume->GetSamplerDescriptorHeap()) cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotSamplerDescriptorTable(), volume->GetSamplerDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                    }
                }
                else
                {
                    // Bound resources, using the SDK's root signature
                    cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotResourceDescriptorTable(), volume->GetResourceDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                }

                // Get the number of probes on each axis
                UINT probeCountX, probeCountY, probeCountZ;
                GetDDGIVolumeProbeCounts(volume->GetDesc(), probeCountX, probeCountY, probeCountZ);

                // Probe distance blending
                {
                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers())
                    {
                        std::string msg = "Distance, DDGIVolume[" + std::to_string(volume->GetIndex()) + "] - \"" + volume->GetName() + "\"";
                        PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), msg.c_str());
                    }

                    // Set the PSO and dispatch threads
                    cmdList->SetPipelineState(volume->GetProbeBlendingDistancePSO());
                    cmdList->Dispatch(probeCountX, probeCountY, probeCountZ);

                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers()) PIXEndEvent(cmdList);
                }

                // Add a barrier
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                barrier.UAV.pResource = volume->GetProbeDistance();
                barriers.push_back(barrier);
            }
            if (bInsertPerfMarkers) PIXEndEvent(cmdList);

            // Barrier(s)
            // Wait for the irradiance and distance blending passes to complete before using the textures
            if (!barriers.empty()) cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

            if (bInsertPerfMarkers) PIXEndEvent(cmdList);

            return ERTXGIStatus::OK;
        }

        ERTXGIStatus RelocateDDGIVolumeProbes(ID3D12GraphicsCommandList* cmdList, UINT numVolumes, DDGIVolume** volumes)
        {
            if (bInsertPerfMarkers) PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), "RTXGI DDGI Relocate Probes");

            UINT volumeIndex;
            std::vector<D3D12_RESOURCE_BARRIER> barriers;

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;

            // Probe Relocation Reset
            for(volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                // Get the volume
                const DDGIVolume* volume = volumes[volumeIndex];
                if (!volume->GetProbeRelocationNeedsReset()) continue;  // Skip if the volume doesn't need to be reset

                // Set the descriptor heap(s)
                std::vector<ID3D12DescriptorHeap*> heaps;
                heaps.push_back(volume->GetResourceDescriptorHeap());
                if (volume->GetSamplerDescriptorHeap()) heaps.push_back(volume->GetSamplerDescriptorHeap());
                cmdList->SetDescriptorHeaps((UINT)heaps.size(), heaps.data());

                // Set root signature and root constants
                cmdList->SetComputeRootSignature(volume->GetRootSignature());
                cmdList->SetComputeRoot32BitConstants(volume->GetRootParamSlotRootConstants(), DDGIRootConstants::GetNum32BitValues(), volume->GetRootConstants().GetData(), 0);

                // Set the descriptor tables (when relevant)
                if (volume->GetBindlessEnabled())
                {
                    // Bindless resources, using application's root signature
                    if (volume->GetBindlessType() == EBindlessType::RESOURCE_ARRAYS)
                    {
                        // Only need to set descriptor tables when using traditional resource array bindless
                        cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotResourceDescriptorTable(), volume->GetResourceDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                        if (volume->GetSamplerDescriptorHeap()) cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotSamplerDescriptorTable(), volume->GetSamplerDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                    }
                }
                else
                {
                    // Bound resources, using the SDK's root signature
                    cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotResourceDescriptorTable(), volume->GetResourceDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                }

                // Reset all probe offsets to zero
                const float groupSizeX = 32.f;
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
            if (!barriers.empty())
            {
                cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
                barriers.clear();
            }

            // Probe Relocation
            for(volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                // Get the volume
                const DDGIVolume* volume = volumes[volumeIndex];
                if(!volume->GetProbeRelocationEnabled()) continue;  // Skip if relocation is not enabled for this volume

                // Set the descriptor heap(s)
                std::vector<ID3D12DescriptorHeap*> heaps;
                heaps.push_back(volume->GetResourceDescriptorHeap());
                if (volume->GetSamplerDescriptorHeap()) heaps.push_back(volume->GetSamplerDescriptorHeap());
                cmdList->SetDescriptorHeaps((UINT)heaps.size(), heaps.data());

                // Set root signature and root constants
                cmdList->SetComputeRootSignature(volume->GetRootSignature());
                cmdList->SetComputeRoot32BitConstants(volume->GetRootParamSlotRootConstants(), DDGIRootConstants::GetNum32BitValues(), volume->GetRootConstants().GetData(), 0);

                // Set the descriptor tables (when relevant)
                if (volume->GetBindlessEnabled())
                {
                    // Bindless resources, using application's root signature
                    if (volume->GetBindlessType() == EBindlessType::RESOURCE_ARRAYS)
                    {
                        // Only need to set descriptor tables when using traditional resource array bindless
                        cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotResourceDescriptorTable(), volume->GetResourceDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                        if (volume->GetSamplerDescriptorHeap()) cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotSamplerDescriptorTable(), volume->GetSamplerDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                    }
                }
                else
                {
                    // Bound resources, using the SDK's root signature
                    cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotResourceDescriptorTable(), volume->GetResourceDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                }

                // Probe relocation
                float groupSizeX = 32.f;
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
            if (bInsertPerfMarkers) PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), "RTXGI DDGI Classify Probes");

            UINT volumeIndex;
            std::vector<D3D12_RESOURCE_BARRIER> barriers;

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;

            // Probe Classification Reset
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];
                if (!volume->GetProbeClassificationNeedsReset()) continue;  // Skip if the volume doesn't need to be reset

                // Set the descriptor heap(s)
                std::vector<ID3D12DescriptorHeap*> heaps;
                heaps.push_back(volume->GetResourceDescriptorHeap());
                if (volume->GetSamplerDescriptorHeap()) heaps.push_back(volume->GetSamplerDescriptorHeap());
                cmdList->SetDescriptorHeaps((UINT)heaps.size(), heaps.data());

                // Set root signature and root constants
                cmdList->SetComputeRootSignature(volume->GetRootSignature());
                cmdList->SetComputeRoot32BitConstants(volume->GetRootParamSlotRootConstants(), DDGIRootConstants::GetNum32BitValues(), volume->GetRootConstants().GetData(), 0);
 
                // Set the descriptor tables (when relevant)
                if (volume->GetBindlessEnabled())
                {
                    // Bindless resources, using application's root signature
                    if (volume->GetBindlessType() == EBindlessType::RESOURCE_ARRAYS)
                    {
                        // Only need to set descriptor tables when using traditional resource array bindless
                        cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotResourceDescriptorTable(), volume->GetResourceDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                        if (volume->GetSamplerDescriptorHeap()) cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotSamplerDescriptorTable(), volume->GetSamplerDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                    }
                }
                else
                {
                    // Bound resources, using the SDK's root signature
                    cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotResourceDescriptorTable(), volume->GetResourceDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                }

                // Reset all probe offsets to zero
                const float groupSizeX = 32.f;
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
            if (!barriers.empty())
            {
                cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
                barriers.clear();
            }

            // Probe Classification
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                // Get the volume
                const DDGIVolume* volume = volumes[volumeIndex];
                if (!volume->GetProbeClassificationEnabled()) continue;  // Skip if classification is not enabled for this volume

                // Set the descriptor heap(s)
                std::vector<ID3D12DescriptorHeap*> heaps;
                heaps.push_back(volume->GetResourceDescriptorHeap());
                if (volume->GetSamplerDescriptorHeap()) heaps.push_back(volume->GetSamplerDescriptorHeap());
                cmdList->SetDescriptorHeaps((UINT)heaps.size(), heaps.data());

                // Set root signature and root constants
                cmdList->SetComputeRootSignature(volume->GetRootSignature());
                cmdList->SetComputeRoot32BitConstants(volume->GetRootParamSlotRootConstants(), DDGIRootConstants::GetNum32BitValues(), volume->GetRootConstants().GetData(), 0);

                // Set the descriptor tables (when relevant)
                if (volume->GetBindlessEnabled())
                {
                    // Bindless resources, using application's root signature
                    if (volume->GetBindlessType() == EBindlessType::RESOURCE_ARRAYS)
                    {
                        // Only need to set descriptor tables when using traditional resource array bindless
                        cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotResourceDescriptorTable(), volume->GetResourceDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                        if (volume->GetSamplerDescriptorHeap()) cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotSamplerDescriptorTable(), volume->GetSamplerDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                    }
                }
                else
                {
                    // Bound resources, using the SDK's root signature
                    cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotResourceDescriptorTable(), volume->GetResourceDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                }

                // Probe classification
                const float groupSizeX = 32.f;
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

        ERTXGIStatus CalculateDDGIVolumeVariability(ID3D12GraphicsCommandList* cmdList, UINT numVolumes, DDGIVolume** volumes)
        {
            if (bInsertPerfMarkers) PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), "Probe Variability Calculation");

            UINT volumeIndex;

            // Reduction
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];
                if (!volume->GetProbeVariabilityEnabled()) continue;  // Skip if the volume is not calculating variability

                // Set the descriptor heap(s)
                std::vector<ID3D12DescriptorHeap*> heaps;
                heaps.push_back(volume->GetResourceDescriptorHeap());
                if (volume->GetSamplerDescriptorHeap()) heaps.push_back(volume->GetSamplerDescriptorHeap());
                cmdList->SetDescriptorHeaps((UINT)heaps.size(), heaps.data());

                // Set root signature and root constants
                cmdList->SetComputeRootSignature(volume->GetRootSignature());
                cmdList->SetComputeRoot32BitConstants(volume->GetRootParamSlotRootConstants(), DDGIRootConstants::GetNum32BitValues(), volume->GetRootConstants().GetData(), 0);

                // Set the descriptor tables (when relevant)
                if (volume->GetBindlessEnabled())
                {
                    // Bindless resources, using application's root signature
                    if (volume->GetBindlessType() == EBindlessType::RESOURCE_ARRAYS)
                    {
                        // Only need to set descriptor tables when using traditional resource array bindless
                        cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotResourceDescriptorTable(), volume->GetResourceDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                        if (volume->GetSamplerDescriptorHeap()) cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotSamplerDescriptorTable(), volume->GetSamplerDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                    }
                }
                else
                {
                    // Bound resources, using the SDK's root signature
                    cmdList->SetComputeRootDescriptorTable(volume->GetRootParamSlotResourceDescriptorTable(), volume->GetResourceDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
                }

                // Get the number of probes on the XYZ dimensions of the texture
                UINT probeCountX, probeCountY, probeCountZ;
                GetDDGIVolumeProbeCounts(volume->GetDesc(), probeCountX, probeCountY, probeCountZ);

                // Initially, the reduction input is the full variability size (same as irradiance texture without border texels)
                UINT inputTexelsX = probeCountX * volume->GetDesc().probeNumIrradianceInteriorTexels;
                UINT inputTexelsY = probeCountY * volume->GetDesc().probeNumIrradianceInteriorTexels;
                UINT inputTexelsZ = probeCountZ;

                const uint3 NumThreadsInGroup = { 4, 8, 4 }; // Each thread group will have 8x8x8 threads
                constexpr uint2 ThreadSampleFootprint = { 4, 2 }; // Each thread will sample 4x2 texels

                DDGIRootConstants consts = volume->GetRootConstants();

                // First pass reduction takes probe irradiance data and calculates variability, reduces as much as possible
                {
                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers())
                    {
                        std::string msg = "Reduction, DDGIVolume[" + std::to_string(volume->GetIndex()) + "] - \"" + volume->GetName() + "\"";
                        PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), msg.c_str());
                    }

                    // Set the PSO and dispatch threads
                    cmdList->SetPipelineState(volume->GetProbeVariabilityReductionPSO());

                    // One thread group per output texel
                    UINT outputTexelsX = (UINT)ceil((float)inputTexelsX / (NumThreadsInGroup.x * ThreadSampleFootprint.x));
                    UINT outputTexelsY = (UINT)ceil((float)inputTexelsY / (NumThreadsInGroup.y * ThreadSampleFootprint.y));
                    UINT outputTexelsZ = (UINT)ceil((float)inputTexelsZ / NumThreadsInGroup.z);

                    consts.reductionInputSizeX = inputTexelsX;
                    consts.reductionInputSizeY = inputTexelsY;
                    consts.reductionInputSizeZ = inputTexelsZ;
                    cmdList->SetComputeRoot32BitConstants(volume->GetRootParamSlotRootConstants(), DDGIRootConstants::GetNum32BitValues(), consts.GetData(), 0);

                    cmdList->Dispatch(outputTexelsX, outputTexelsY, outputTexelsZ);

                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers()) PIXEndEvent(cmdList);

                    // Each thread group will write out a value to the averaging texture
                    // If there is more than one thread group, we will need to do extra averaging passes
                    inputTexelsX = outputTexelsX;
                    inputTexelsY = outputTexelsY;
                    inputTexelsZ = outputTexelsZ;
                }

                // UAV barrier needed after each reduction pass
                D3D12_RESOURCE_BARRIER reductionBarrier = {};
                reductionBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                reductionBarrier.UAV.pResource = volume->GetProbeVariabilityAverage();
                cmdList->ResourceBarrier(1, &reductionBarrier);

                // Extra reduction passes average values in variability texture down to single value
                while (inputTexelsX > 1 || inputTexelsY > 1 || inputTexelsZ > 1)
                {
                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers())
                    {
                        std::string msg = "Extra Reduction, DDGIVolume[" + std::to_string(volume->GetIndex()) + "] - \"" + volume->GetName() + "\"";
                        PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), msg.c_str());
                    }

                    cmdList->SetPipelineState(volume->GetProbeVariabilityExtraReductionPSO());

                    // One thread group per output texel
                    UINT outputTexelsX = (UINT)ceil((float)inputTexelsX / (NumThreadsInGroup.x * ThreadSampleFootprint.x));
                    UINT outputTexelsY = (UINT)ceil((float)inputTexelsY / (NumThreadsInGroup.y * ThreadSampleFootprint.y));
                    UINT outputTexelsZ = (UINT)ceil((float)inputTexelsZ / NumThreadsInGroup.z);

                    consts.reductionInputSizeX = inputTexelsX;
                    consts.reductionInputSizeY = inputTexelsY;
                    consts.reductionInputSizeZ = inputTexelsZ;
                    cmdList->SetComputeRoot32BitConstants(volume->GetRootParamSlotRootConstants(), DDGIRootConstants::GetNum32BitValues(), consts.GetData(), 0);

                    cmdList->Dispatch(outputTexelsX, outputTexelsY, outputTexelsZ);

                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers()) PIXEndEvent(cmdList);

                    inputTexelsX = outputTexelsX;
                    inputTexelsY = outputTexelsY;
                    inputTexelsZ = outputTexelsZ;

                    // Need a barrier in between each reduction pass
                    cmdList->ResourceBarrier(1, &reductionBarrier);
                }
            }

            if (bInsertPerfMarkers) PIXEndEvent(cmdList);

            // Copy readback buffer
            std::vector<D3D12_RESOURCE_BARRIER> barriers;
            if (bInsertPerfMarkers) PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), "Probe Variability Readback");

            {
                D3D12_RESOURCE_BARRIER beforeBarrier = {};
                beforeBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                beforeBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                beforeBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
                beforeBarrier.Transition.Subresource = 0;

                D3D12_RESOURCE_BARRIER afterBarrier = beforeBarrier;
                afterBarrier.Transition.StateBefore = beforeBarrier.Transition.StateAfter;
                afterBarrier.Transition.StateAfter = beforeBarrier.Transition.StateBefore;

                for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
                {
                    const DDGIVolume* volume = volumes[volumeIndex];
                    if (!volume->GetProbeVariabilityEnabled()) continue;  // Skip if the volume is not calculating variability

                    beforeBarrier.Transition.pResource = volume->GetProbeVariabilityAverage();
                    barriers.push_back(beforeBarrier);
                }

                if (!barriers.empty())
                {
                    cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
                    barriers.clear();
                }

                for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
                {
                    const DDGIVolume* volume = volumes[volumeIndex];
                    if (!volume->GetProbeVariabilityEnabled()) continue;  // Skip if the volume is not calculating variability

                    D3D12_TEXTURE_COPY_LOCATION copyLocSrc = {};
                    copyLocSrc.pResource = volume->GetProbeVariabilityAverage();
                    copyLocSrc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                    copyLocSrc.SubresourceIndex = 0;

                    D3D12_TEXTURE_COPY_LOCATION copyLocDst = {};
                    copyLocDst.pResource = volume->GetProbeVariabilityReadback();
                    copyLocDst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                    copyLocDst.PlacedFootprint.Offset = 0;
                    copyLocDst.PlacedFootprint.Footprint.Width = 1;
                    copyLocDst.PlacedFootprint.Footprint.Height = 1;
                    copyLocDst.PlacedFootprint.Footprint.Depth = 1;
                    copyLocDst.PlacedFootprint.Footprint.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::VariabilityAverage, volume->GetDesc().probeVariabilityFormat);
                    copyLocDst.PlacedFootprint.Footprint.RowPitch = D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;

                    D3D12_BOX box = { 0, 0, 0, 1, 1, 1};
                    cmdList->CopyTextureRegion(&copyLocDst, 0, 0, 0, &copyLocSrc, &box);

                    afterBarrier.Transition.pResource = volume->GetProbeVariabilityAverage();
                    barriers.push_back(afterBarrier);
                }

                if (!barriers.empty()) cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
            }

            if (bInsertPerfMarkers) PIXEndEvent(cmdList);

            return ERTXGIStatus::OK;
        }

        ERTXGIStatus ReadbackDDGIVolumeVariability(UINT numVolumes, DDGIVolume** volumes)
        {
            for (UINT volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                // Get the volume
                DDGIVolume* volume = volumes[volumeIndex];
                if (!volume->GetProbeVariabilityEnabled()) continue;  // Skip if the volume is not calculating variability

                // Get the probe variability readback buffer
                ID3D12Resource* readback = volume->GetProbeVariabilityReadback();

                // Read the first 32-bits of the readback buffer
                float* pMappedMemory = nullptr;
                D3D12_RANGE readRange = { 0, sizeof(float) };
                D3D12_RANGE writeRange = {};
                HRESULT hr = readback->Map(0, &readRange, (void**)&pMappedMemory);
                if (FAILED(hr)) return ERTXGIStatus::ERROR_DDGI_MAP_FAILURE_VARIABILITY_READBACK_BUFFER;
                float value = pMappedMemory[0];
                readback->Unmap(0, &writeRange);

                volume->SetVolumeAverageVariability(value);
            }
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
            RTXGI_SAFE_RELEASE(m_probeRelocationPSO);
            RTXGI_SAFE_RELEASE(m_probeRelocationResetPSO);
            RTXGI_SAFE_RELEASE(m_probeClassificationPSO);
            RTXGI_SAFE_RELEASE(m_probeClassificationResetPSO);
            RTXGI_SAFE_RELEASE(m_probeVariabilityReductionPSO);
            RTXGI_SAFE_RELEASE(m_probeVariabilityExtraReductionPSO);
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
                    "Probe Irradiance Blending")) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_PSO;

                if (!CreateComputePSO(
                    managed.probeBlendingDistanceCS,
                    &m_probeBlendingDistancePSO,
                    "Probe Distance Blending")) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_PSO;

                if (!CreateComputePSO(
                    managed.probeRelocation.updateCS,
                    &m_probeRelocationPSO,
                    "Probe Relocation")) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_PSO;

                if (!CreateComputePSO(
                    managed.probeRelocation.resetCS,
                    &m_probeRelocationResetPSO,
                    "Probe Relocation Reset")) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_PSO;

                if (!CreateComputePSO(
                    managed.probeClassification.updateCS,
                    &m_probeClassificationPSO,
                    "Probe Classification")) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_PSO;

                if (!CreateComputePSO(
                    managed.probeClassification.resetCS,
                    &m_probeClassificationResetPSO,
                    "Probe Classification Reset")) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_PSO;

                if (!CreateComputePSO(
                    managed.probeVariability.reductionCS,
                    &m_probeVariabilityReductionPSO,
                    "Probe Variability Reduction")) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_PSO;

                if (!CreateComputePSO(
                    managed.probeVariability.extraReductionCS,
                    &m_probeVariabilityExtraReductionPSO,
                    "Probe Variability Extra Reduction")) return ERTXGIStatus::ERROR_DDGI_D3D12_CREATE_FAILURE_PSO;
            }

            // Create the textures
            if (deviceChanged || m_desc.ShouldAllocateProbes(desc))
            {
                // Probe counts have changed. The texture arrays are the wrong size or aren't allocated yet.
                // (Re)allocate the probe ray data, irradiance, distance, data, and variability textures.
                if (!CreateProbeRayData(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_RAY_DATA;
                if (!CreateProbeIrradiance(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_IRRADIANCE;
                if (!CreateProbeDistance(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_DISTANCE;
                if (!CreateProbeData(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_DATA;
                if (!CreateProbeVariability(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_VARIABILITY;
                if (!CreateProbeVariabilityAverage(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_VARIABILITY_AVERAGE;
            }
            else
            {
                if (m_desc.ShouldAllocateRayData(desc))
                {
                    // The number of rays to trace per probe has changed. Reallocate the ray data texture array.
                    if (!CreateProbeRayData(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_RAY_DATA;
                }

                if (m_desc.ShouldAllocateIrradiance(desc))
                {
                    // The number of irradiance texels per probe has changed. Reallocate the irradiance texture array.
                    if (!CreateProbeIrradiance(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_IRRADIANCE;
                }

                if (m_desc.ShouldAllocateDistance(desc))
                {
                    // The number of distance texels per probe has changed. Reallocate the distance texture array.
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
            m_rootParamSlotResourceDescriptorTable = unmanaged.rootParamSlotResourceDescriptorTable;
            m_rootParamSlotSamplerDescriptorTable = unmanaged.rootParamSlotSamplerDescriptorTable;

            // Texture Arrays
            m_probeRayData = unmanaged.probeRayData;
            m_probeIrradiance = unmanaged.probeIrradiance;
            m_probeDistance = unmanaged.probeDistance;
            m_probeData = unmanaged.probeData;
            m_probeVariability = unmanaged.probeVariability;
            m_probeVariabilityAverage = unmanaged.probeVariabilityAverage;
            m_probeVariabilityReadback = unmanaged.probeVariabilityReadback;

            // Render Target Views
            m_probeIrradianceRTV = unmanaged.probeIrradianceRTV;
            m_probeDistanceRTV = unmanaged.probeDistanceRTV;

            // Pipeline State Objects
            m_probeBlendingIrradiancePSO = unmanaged.probeBlendingIrradiancePSO;
            m_probeBlendingDistancePSO = unmanaged.probeBlendingDistancePSO;
            m_probeRelocationPSO = unmanaged.probeRelocation.updatePSO;
            m_probeRelocationResetPSO = unmanaged.probeRelocation.resetPSO;
            m_probeClassificationPSO = unmanaged.probeClassification.updatePSO;
            m_probeClassificationResetPSO = unmanaged.probeClassification.resetPSO;
            m_probeVariabilityReductionPSO = unmanaged.probeVariabilityPSOs.reductionPSO;
            m_probeVariabilityExtraReductionPSO = unmanaged.probeVariabilityPSOs.extraReductionPSO;
        }
    #endif

        //------------------------------------------------------------------------
        // Public DDGIVolume Functions
        //------------------------------------------------------------------------

        ERTXGIStatus DDGIVolume::Create(const DDGIVolumeDesc& desc, const DDGIVolumeResources& resources)
        {
            // Validate the probe counts
            if (desc.probeCounts.x <= 0 || desc.probeCounts.y <= 0 || desc.probeCounts.z <= 0) return ERTXGIStatus::ERROR_DDGI_INVALID_PROBE_COUNTS;

            // Validate the resource descriptor heap
            if (resources.descriptorHeap.resources == nullptr) return ERTXGIStatus::ERROR_DDGI_D3D12_INVALID_RESOURCE_DESCRIPTOR_HEAP;

            // Validate the resource indices buffer (when necessary)
            if (resources.bindless.enabled)
            {
                if(resources.bindless.resourceIndicesBuffer == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_INDICES_BUFFER;
            }

            // Validate the volume constants buffer
            if (resources.constantsBuffer == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_CONSTANTS_BUFFER;

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

            // Store the descriptor heap descriptor
            m_descriptorHeapDesc = resources.descriptorHeap;

            // Store the bindless resources descriptor
            m_bindlessResources = resources.bindless;

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
            m_rotationMatrix = EulerAnglesToRotationMatrix(m_desc.eulerAngles);
            m_rotationQuaternion = RotationMatrixToQuaternion(m_rotationMatrix);

            // Set the default scroll anchor to the origin
            m_probeScrollAnchor = m_desc.origin;

            // Initialize the random number generator if a seed is provided, otherwise use the default std::random_device()
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
            if (bInsertPerfMarkers) PIXBeginEvent(cmdList, PIX_COLOR(RTXGI_PERF_MARKER_GREEN), "RTXGI DDGI Clear Probes");

            // Transition the probe textures render targets
            D3D12_RESOURCE_BARRIER barriers[2] = {};
            barriers[0].Transition.pResource = m_probeIrradiance;
            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            barriers[1].Transition.pResource = m_probeDistance;
            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            // Wait for the transitions
            cmdList->ResourceBarrier(2, barriers);

            float values[4] = { 0.f, 0.f, 0.f, 1.f };

            // Clear the probe data
            cmdList->ClearRenderTargetView(m_probeIrradianceRTV, values, 0, nullptr); // Clear probe irradiance
            cmdList->ClearRenderTargetView(m_probeDistanceRTV, values, 0, nullptr);   // Clear probe distance

            // Transition the probe textures back to unordered access
            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

            // Wait for the transitions
            cmdList->ResourceBarrier(2, barriers);

            if (bInsertPerfMarkers) PIXEndEvent(cmdList);

            return ERTXGIStatus::OK;
        }

        void DDGIVolume::TransitionResources(ID3D12GraphicsCommandList* cmdList, EDDGIExecutionStage stage) const
        {
            std::vector<D3D12_RESOURCE_BARRIER> barriers;

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            if (stage == EDDGIExecutionStage::POST_PROBE_TRACE)
            {
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            }
            else if (stage == EDDGIExecutionStage::PRE_GATHER_CS)
            {
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            }
            else if (stage == EDDGIExecutionStage::PRE_GATHER_PS)
            {
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            }
            else if (stage == EDDGIExecutionStage::POST_GATHER_PS)
            {
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            }

            // Add the volume texture array resources
            barrier.Transition.pResource = m_probeIrradiance;
            barriers.push_back(barrier);
            barrier.Transition.pResource = m_probeDistance;
            barriers.push_back(barrier);
            barrier.Transition.pResource = m_probeData;
            barriers.push_back(barrier);

            cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
        }

        DDGIVolumeResourceIndices DDGIVolume::GetResourceIndices() const
        {
            if(m_bindlessResources.type == EBindlessType::DESCRIPTOR_HEAP) return m_descriptorHeapDesc.resourceIndices;
            else if(m_bindlessResources.type == EBindlessType::RESOURCE_ARRAYS) return m_bindlessResources.resourceIndices;
            return DDGIVolumeResourceIndices();
        }

        UINT DDGIVolume::GetResourceDescriptorHeapIndex(EDDGIVolumeTextureType type, EResourceViewType view) const
        {
            if(type == EDDGIVolumeTextureType::RayData)
            {
                if(view == EResourceViewType::UAV) return m_descriptorHeapDesc.resourceIndices.rayDataUAVIndex;
                if(view == EResourceViewType::SRV) return m_descriptorHeapDesc.resourceIndices.rayDataSRVIndex;
            }
            else if(type == EDDGIVolumeTextureType::Irradiance)
            {
                if (view == EResourceViewType::UAV) return m_descriptorHeapDesc.resourceIndices.probeIrradianceUAVIndex;
                if (view == EResourceViewType::SRV) return m_descriptorHeapDesc.resourceIndices.probeIrradianceSRVIndex;
            }
            else if (type == EDDGIVolumeTextureType::Distance)
            {
                if (view == EResourceViewType::UAV) return m_descriptorHeapDesc.resourceIndices.probeDistanceUAVIndex;
                if (view == EResourceViewType::SRV) return m_descriptorHeapDesc.resourceIndices.probeDistanceSRVIndex;
            }
            else if (type == EDDGIVolumeTextureType::Data)
            {
                if (view == EResourceViewType::UAV) return m_descriptorHeapDesc.resourceIndices.probeDataUAVIndex;
                if (view == EResourceViewType::SRV) return m_descriptorHeapDesc.resourceIndices.probeDataSRVIndex;
            }
            else if (type == EDDGIVolumeTextureType::Variability)
            {
                if (view == EResourceViewType::UAV) return m_descriptorHeapDesc.resourceIndices.probeVariabilityUAVIndex;
                if (view == EResourceViewType::SRV) return m_descriptorHeapDesc.resourceIndices.probeVariabilitySRVIndex;
            }
            else if (type == EDDGIVolumeTextureType::VariabilityAverage)
            {
                if (view == EResourceViewType::UAV) return m_descriptorHeapDesc.resourceIndices.probeVariabilityAverageUAVIndex;
                if (view == EResourceViewType::SRV) return m_descriptorHeapDesc.resourceIndices.probeVariabilityAverageSRVIndex;
            }

            return 0;
        }

        void DDGIVolume::SetResourceDescriptorHeapIndex(EDDGIVolumeTextureType type, EResourceViewType view, UINT index)
        {
            if (type == EDDGIVolumeTextureType::RayData)
            {
                if (view == EResourceViewType::UAV) m_descriptorHeapDesc.resourceIndices.rayDataUAVIndex = index;
                if (view == EResourceViewType::SRV) m_descriptorHeapDesc.resourceIndices.rayDataSRVIndex = index;
            }
            else if (type == EDDGIVolumeTextureType::Irradiance)
            {
                if (view == EResourceViewType::UAV) m_descriptorHeapDesc.resourceIndices.probeIrradianceUAVIndex = index;
                if (view == EResourceViewType::SRV) m_descriptorHeapDesc.resourceIndices.probeIrradianceSRVIndex = index;
            }
            else if (type == EDDGIVolumeTextureType::Distance)
            {
                if (view == EResourceViewType::UAV) m_descriptorHeapDesc.resourceIndices.probeDistanceUAVIndex = index;
                if (view == EResourceViewType::SRV) m_descriptorHeapDesc.resourceIndices.probeDistanceSRVIndex = index;
            }
            else if (type == EDDGIVolumeTextureType::Data)
            {
                if (view == EResourceViewType::UAV) m_descriptorHeapDesc.resourceIndices.probeDataUAVIndex = index;
                if (view == EResourceViewType::SRV) m_descriptorHeapDesc.resourceIndices.probeDataSRVIndex = index;
            }
            else if (type == EDDGIVolumeTextureType::Variability)
            {
                if (view == EResourceViewType::UAV) m_descriptorHeapDesc.resourceIndices.probeVariabilityUAVIndex = index;
                if (view == EResourceViewType::SRV) m_descriptorHeapDesc.resourceIndices.probeVariabilitySRVIndex = index;
            }
            else if (type == EDDGIVolumeTextureType::VariabilityAverage)
            {
                if (view == EResourceViewType::UAV) m_descriptorHeapDesc.resourceIndices.probeVariabilityAverageUAVIndex = index;
                if (view == EResourceViewType::SRV) m_descriptorHeapDesc.resourceIndices.probeVariabilityAverageSRVIndex = index;
            }
        }

        void DDGIVolume::Destroy()
        {
            m_descriptorHeapDesc = {};
            m_bindlessResources = {};

            m_constantsBuffer = nullptr;
            m_constantsBufferUpload = nullptr;
            m_constantsBufferSizeInBytes = 0;

            m_rootParamSlotRootConstants = 0;
            m_rootParamSlotResourceDescriptorTable = 0;
            m_rootParamSlotSamplerDescriptorTable = 0;

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
            RTXGI_SAFE_RELEASE(m_probeVariability);
            RTXGI_SAFE_RELEASE(m_probeVariabilityAverage);
            RTXGI_SAFE_RELEASE(m_probeVariabilityReadback);

            RTXGI_SAFE_RELEASE(m_probeBlendingIrradiancePSO);
            RTXGI_SAFE_RELEASE(m_probeBlendingDistancePSO);
            RTXGI_SAFE_RELEASE(m_probeRelocationPSO);
            RTXGI_SAFE_RELEASE(m_probeRelocationResetPSO);
            RTXGI_SAFE_RELEASE(m_probeClassificationPSO);
            RTXGI_SAFE_RELEASE(m_probeClassificationResetPSO);
            RTXGI_SAFE_RELEASE(m_probeVariabilityReductionPSO);
            RTXGI_SAFE_RELEASE(m_probeVariabilityExtraReductionPSO);
        #else
            m_rootSignature = nullptr;

            m_probeRayData = nullptr;
            m_probeIrradiance = nullptr;
            m_probeDistance = nullptr;
            m_probeData = nullptr;
            m_probeVariability = nullptr;
            m_probeVariabilityAverage = nullptr;
            m_probeVariabilityReadback = nullptr;

            m_probeBlendingIrradiancePSO = nullptr;
            m_probeBlendingDistancePSO = nullptr;
            m_probeRelocationPSO = nullptr;
            m_probeRelocationResetPSO = nullptr;
            m_probeClassificationPSO = nullptr;
            m_probeClassificationResetPSO = nullptr;
            m_probeVariabilityReductionPSO = nullptr;
            m_probeVariabilityExtraReductionPSO = nullptr;
        #endif;
        }

        uint32_t DDGIVolume::GetGPUMemoryUsedInBytes() const
        {
            uint32_t bytesPerVolume = DDGIVolumeBase::GetGPUMemoryUsedInBytes();

            if (m_bindlessResources.enabled)
            {
                // Add the memory used for the GPU-side DDGIVolumeResourceIndices (32B)
                bytesPerVolume += sizeof(DDGIVolumeResourceIndices);
            }

            return bytesPerVolume;
        }

        //------------------------------------------------------------------------
        // Private Resource Allocation Helper Functions (Managed Resources)
        //------------------------------------------------------------------------

    #if RTXGI_DDGI_RESOURCE_MANAGEMENT
        bool DDGIVolume::CreateDescriptors()
        {
            D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle, srvHandle, uavHandle;
            D3D12_CPU_DESCRIPTOR_HANDLE heapStart = m_descriptorHeapDesc.resources->GetCPUDescriptorHandleForHeapStart();

            // Constants structured buffer descriptor
            {
                D3D12_SHADER_RESOURCE_VIEW_DESC cbvDesc = {};
                cbvDesc.Format = DXGI_FORMAT_UNKNOWN;
                cbvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                cbvDesc.Buffer.NumElements = (m_desc.index + 1);
                cbvDesc.Buffer.StructureByteStride = sizeof(DDGIVolumeDescGPUPacked);
                cbvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

                cbvHandle.ptr = heapStart.ptr + (m_descriptorHeapDesc.constantsIndex * m_descriptorHeapDesc.entrySize);

                m_device->CreateShaderResourceView(m_constantsBuffer, &cbvDesc, cbvHandle);
            }

            UINT width, height, arraySize;
            GetDDGIVolumeProbeCounts(m_desc, width, height, arraySize);

            // Describe resource views
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Texture2DArray.ArraySize = arraySize;

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Texture2DArray.ArraySize = arraySize;
            srvDesc.Texture2DArray.MipLevels = 1;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

            // Probe Ray Data texture array descriptors
            {
                uavHandle.ptr = heapStart.ptr + (m_descriptorHeapDesc.resourceIndices.rayDataUAVIndex * m_descriptorHeapDesc.entrySize);
                srvHandle.ptr = heapStart.ptr + (m_descriptorHeapDesc.resourceIndices.rayDataSRVIndex * m_descriptorHeapDesc.entrySize);

                srvDesc.Format = uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::RayData, m_desc.probeRayDataFormat);
                m_device->CreateUnorderedAccessView(m_probeRayData, nullptr, &uavDesc, uavHandle);
                m_device->CreateShaderResourceView(m_probeRayData, &srvDesc, srvHandle);
            }

            // Probe Irradiance texture array descriptors
            {
                uavHandle.ptr = heapStart.ptr + (m_descriptorHeapDesc.resourceIndices.probeIrradianceUAVIndex * m_descriptorHeapDesc.entrySize);
                srvHandle.ptr = heapStart.ptr + (m_descriptorHeapDesc.resourceIndices.probeIrradianceSRVIndex * m_descriptorHeapDesc.entrySize);

                srvDesc.Format = uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Irradiance, m_desc.probeIrradianceFormat);
                m_device->CreateUnorderedAccessView(m_probeIrradiance, nullptr, &uavDesc, uavHandle);
                m_device->CreateShaderResourceView(m_probeIrradiance, &srvDesc, srvHandle);
            }

            // Probe Distance texture array descriptors
            {
                uavHandle.ptr = heapStart.ptr + (m_descriptorHeapDesc.resourceIndices.probeDistanceUAVIndex * m_descriptorHeapDesc.entrySize);
                srvHandle.ptr = heapStart.ptr + (m_descriptorHeapDesc.resourceIndices.probeDistanceSRVIndex * m_descriptorHeapDesc.entrySize);

                srvDesc.Format = uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Distance, m_desc.probeDistanceFormat);
                m_device->CreateUnorderedAccessView(m_probeDistance, nullptr, &uavDesc, uavHandle);
                m_device->CreateShaderResourceView(m_probeDistance, &srvDesc, srvHandle);
            }

            // Probe Data texture array descriptors
            {
                uavHandle.ptr = heapStart.ptr + (m_descriptorHeapDesc.resourceIndices.probeDataUAVIndex * m_descriptorHeapDesc.entrySize);
                srvHandle.ptr = heapStart.ptr + (m_descriptorHeapDesc.resourceIndices.probeDataSRVIndex * m_descriptorHeapDesc.entrySize);

                srvDesc.Format = uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Data, m_desc.probeDataFormat);
                m_device->CreateUnorderedAccessView(m_probeData, nullptr, &uavDesc, uavHandle);
                m_device->CreateShaderResourceView(m_probeData, &srvDesc, srvHandle);
            }

            // Probe variability texture descriptors
            {
                uavHandle.ptr = heapStart.ptr + (m_descriptorHeapDesc.resourceIndices.probeVariabilityUAVIndex * m_descriptorHeapDesc.entrySize);
                srvHandle.ptr = heapStart.ptr + (m_descriptorHeapDesc.resourceIndices.probeVariabilitySRVIndex * m_descriptorHeapDesc.entrySize);

                srvDesc.Format = uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Variability, m_desc.probeVariabilityFormat);
                m_device->CreateUnorderedAccessView(m_probeVariability, nullptr, &uavDesc, uavHandle);
                m_device->CreateShaderResourceView(m_probeVariability, &srvDesc, srvHandle);
            }

            // Probe variability average texture descriptors
            {
                uavHandle.ptr = heapStart.ptr + (m_descriptorHeapDesc.resourceIndices.probeVariabilityAverageUAVIndex * m_descriptorHeapDesc.entrySize);
                srvHandle.ptr = heapStart.ptr + (m_descriptorHeapDesc.resourceIndices.probeVariabilityAverageSRVIndex * m_descriptorHeapDesc.entrySize);

                UINT variabilityAverageArraySize;
                GetDDGIVolumeTextureDimensions(m_desc, EDDGIVolumeTextureType::VariabilityAverage, width, height, variabilityAverageArraySize);
                srvDesc.Format = uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::VariabilityAverage, m_desc.probeVariabilityFormat);
                uavDesc.Texture2DArray.ArraySize = variabilityAverageArraySize;
                srvDesc.Texture2DArray.ArraySize = variabilityAverageArraySize;
                m_device->CreateUnorderedAccessView(m_probeVariabilityAverage, nullptr, &uavDesc, uavHandle);
                m_device->CreateShaderResourceView(m_probeVariabilityAverage, &srvDesc, srvHandle);
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
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.ArraySize = arraySize;

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
            if (!GetDDGIVolumeRootSignatureDesc(m_descriptorHeapDesc, signature)) return false;

            // Set the root parameter slots
            m_rootParamSlotRootConstants = 0;
            m_rootParamSlotResourceDescriptorTable = 1;

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

        bool DDGIVolume::CreateComputePSO(ShaderBytecode shader, ID3D12PipelineState** pipeline, const char* debugName)
        {
            if (m_rootSignature == nullptr) return false;

            D3D12_COMPUTE_PIPELINE_STATE_DESC pipeDesc = {};
            pipeDesc.CS.BytecodeLength = shader.size;
            pipeDesc.CS.pShaderBytecode = shader.pData;
            pipeDesc.pRootSignature = m_rootSignature;

            HRESULT hr = m_device->CreateComputePipelineState(&pipeDesc, IID_PPV_ARGS(pipeline));
            if (FAILED(hr)) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::string n = std::string(debugName);
            std::wstring wn = std::wstring(n.begin(), n.end());
            std::wstring name = L"DDGIVolume[" + std::to_wstring(m_desc.index) + L"]," + wn.c_str() + L" PSO";
            (*pipeline)->SetName(name.c_str());
        #endif

            return true;
        }

        bool DDGIVolume::CreateTexture(UINT64 width, UINT height, UINT arraySize, DXGI_FORMAT format, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_FLAGS flags, ID3D12Resource** resource)
        {
            D3D12_HEAP_PROPERTIES defaultHeapProperties = {};
            defaultHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

            // Describe the texture
            D3D12_RESOURCE_DESC desc = {};
            desc.Format = format;
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = 1;
            desc.DepthOrArraySize = (UINT16)arraySize;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Flags = flags;

            // Setup the optimized clear value
            D3D12_CLEAR_VALUE clear = {};
            clear.Color[3] = 1.f;
            clear.Format = format;

            // Create the texture
            bool useClear = (flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
            HRESULT hr = m_device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, state, useClear ? &clear : nullptr, IID_PPV_ARGS(resource));
            if (FAILED(hr)) return false;
            return true;
        }

        bool DDGIVolume::CreateProbeRayData(const DDGIVolumeDesc& desc)
        {
            RTXGI_SAFE_RELEASE(m_probeRayData);

            UINT width = 0;
            UINT height = 0;
            UINT arraySize = 0;

            // Get the texture dimensions and format
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::RayData, width, height, arraySize);
            DXGI_FORMAT format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::RayData, desc.probeRayDataFormat);

            // Check for problems
            if (width <= 0 || height <= 0 || arraySize <= 0) return false;

            // Create the texture resource
            bool result = CreateTexture(width, height, arraySize, format, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &m_probeRayData);
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
            UINT arraySize = 0;

            // Get the texture dimensions and format
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::Irradiance, width, height, arraySize);
            DXGI_FORMAT format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Irradiance, desc.probeIrradianceFormat);

            // Check for problems
            if (width <= 0 || height <= 0 || arraySize <= 0) return false;

            // Create the texture resource
            D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            bool result = CreateTexture(width, height, arraySize, format, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, flags, &m_probeIrradiance);
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
            UINT arraySize = 0;

            // Get the texture dimensions and format
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::Distance, width, height, arraySize);
            DXGI_FORMAT format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Distance, desc.probeDistanceFormat);

            // Check for problems
            if (width <= 0 || height <= 0 || arraySize <= 0) return false;

            // Create the texture resource
            D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            bool result = CreateTexture(width, height, arraySize, format, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, flags, &m_probeDistance);
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
            UINT arraySize = 0;

            // Get the texture dimensions and format
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::Data, width, height, arraySize);
            DXGI_FORMAT format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Data, desc.probeDataFormat);

            // Check for problems
            if (width <= 0 || height <= 0 || arraySize <= 0) return false;

            // Create the texture resource
            bool result = CreateTexture(width, height, arraySize, format, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &m_probeData);
            if (!result) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::wstring name = L"DDGIVolume[" + std::to_wstring(desc.index) + L"], Probe Data";
            m_probeData->SetName(name.c_str());
        #endif

            return true;
        }

        bool DDGIVolume::CreateProbeVariability(const DDGIVolumeDesc& desc)
        {
            RTXGI_SAFE_RELEASE(m_probeVariability);

            UINT width = 0;
            UINT height = 0;
            UINT arraySize = 0;
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

            // Get the texture dimensions and format
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::Variability, width, height, arraySize);
            format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Variability, desc.probeVariabilityFormat);

            // Check for problems
            if (width <= 0 || height <= 0 || arraySize <= 0) return false;

            // Create the texture resource
            bool result = CreateTexture(width, height, arraySize, format, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &m_probeVariability);
            if (!result) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::wstring name = L"DDGIVolume[" + std::to_wstring(desc.index) + L"], Probe Variability";
            m_probeVariability->SetName(name.c_str());
        #endif

            return true;
        }

        bool DDGIVolume::CreateProbeVariabilityAverage(const DDGIVolumeDesc& desc)
        {
            RTXGI_SAFE_RELEASE(m_probeVariabilityAverage);

            UINT width = 0;
            UINT height = 0;
            UINT arraySize = 0;
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

            // Get the texture dimensions and format
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::VariabilityAverage, width, height, arraySize);
            format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::VariabilityAverage, desc.probeVariabilityFormat);

            // Check for problems
            if (width <= 0 || height <= 0 || arraySize <= 0) return false;

            // Create the texture resource
            bool result = CreateTexture(width, height, arraySize, format, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &m_probeVariabilityAverage);
            if (!result) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::wstring name = L"DDGIVolume[" + std::to_wstring(desc.index) + L"], Probe Variability Average";
            m_probeVariabilityAverage->SetName(name.c_str());
        #endif

            // Create the readback texture
            RTXGI_SAFE_RELEASE(m_probeVariabilityReadback);

            // Readback texture is always in "full" format (R32G32F)
            format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::VariabilityAverage, desc.probeVariabilityFormat);
            {
                D3D12_HEAP_PROPERTIES readbackHeapProperties = {};
                readbackHeapProperties.Type = D3D12_HEAP_TYPE_READBACK;

                D3D12_RESOURCE_DESC desc = {};
                desc.Format = DXGI_FORMAT_UNKNOWN;
                desc.Width = sizeof(float) * 2;
                desc.Height = 1;
                desc.MipLevels = 1;
                desc.DepthOrArraySize = 1;
                desc.SampleDesc.Count = 1;
                desc.SampleDesc.Quality = 0;
                desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
                desc.Flags = D3D12_RESOURCE_FLAG_NONE;

                HRESULT hr = m_device->CreateCommittedResource(&readbackHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_probeVariabilityReadback));
                result = SUCCEEDED(hr);
            }
            if (!result) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            name = L"DDGIVolume[" + std::to_wstring(desc.index) + L"], Probe Variability Readback";
            m_probeVariabilityReadback->SetName(name.c_str());
        #endif

            return true;
        }

    #endif // RTXGI_DDGI_RESOURCE_MANAGEMENT

    } // namespace d3d12
} // namespace rtxgi
