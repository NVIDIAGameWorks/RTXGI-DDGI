/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "rtxgi/Random.h"
#include "rtxgi/Math.h"
#include "rtxgi/ddgi/DDGIVolume.h"

#include <cmath>

#if RTXGI_PERF_MARKERS
#define USE_PIX
#include "pix3.h"
#endif

namespace rtxgi
{

    //------------------------------------------------------------------------
    // Public RTXGI Namespace DDGI Functions
    //------------------------------------------------------------------------

    int GetDDGIVolumeConstantBufferSize() { return RTXGI_ALIGN(256, sizeof(DDGIVolumeDescGPU)); }

    int GetDDGIVolumeNumDescriptors() { return 5; }

    DXGI_FORMAT GetDDGIVolumeTextureFormat(EDDGITextureType type)
    {
        if (type == EDDGITextureType::RTRadiance)
        {
#if RTXGI_DDGI_DEBUG_FORMAT_RADIANCE
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
#else
            return DXGI_FORMAT_R32G32_FLOAT;
#endif
        }
        else if (type == EDDGITextureType::Irradiance)
        {
#if RTXGI_DDGI_DEBUG_FORMAT_IRRADIANCE
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
#else
            return DXGI_FORMAT_R10G10B10A2_UNORM;
#endif
        }
        else if (type == EDDGITextureType::Distance)
        {
            // Note: FP16 will cause artifacts in many scenarios
            return DXGI_FORMAT_R32G32_FLOAT;
        }
        else if (type == EDDGITextureType::Offsets)
        {
#if RTXGI_DDGI_DEBUG_FORMAT_OFFSETS
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
#else
            return DXGI_FORMAT_R16G16B16A16_FLOAT;
#endif
        }
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        else if (type == EDDGITextureType::States)
        {
            return DXGI_FORMAT_R8_UINT;
        }
#endif
        return DXGI_FORMAT_UNKNOWN;
    }

    void GetDDGIVolumeProbeCounts(const DDGIVolumeDesc &desc, UINT &probeCountX, UINT &probeCountY)
    {
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
        probeCountX = (desc.probeGridCounts.x * desc.probeGridCounts.y);
        probeCountY = desc.probeGridCounts.z;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
        probeCountX = (desc.probeGridCounts.y * desc.probeGridCounts.z);
        probeCountY = desc.probeGridCounts.x;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
        probeCountX = (desc.probeGridCounts.x * desc.probeGridCounts.z);
        probeCountY = desc.probeGridCounts.y;
#endif
    }

    void GetDDGIVolumeTextureDimensions(const DDGIVolumeDesc &desc, EDDGITextureType type, UINT &width, UINT &height)
    {
        if (type == EDDGITextureType::RTRadiance)
        {
            width = desc.numRaysPerProbe;
            height = (desc.probeGridCounts.x * desc.probeGridCounts.y * desc.probeGridCounts.z);
        }
        else
        {
            GetDDGIVolumeProbeCounts(desc, width, height);

            if (type == EDDGITextureType::Irradiance)
            {
                width *= (desc.numIrradianceTexels + 2);
                height *= (desc.numIrradianceTexels + 2);
            }
            else if (type == EDDGITextureType::Distance)
            {
                width *= (desc.numDistanceTexels + 2);
                height *= (desc.numDistanceTexels + 2);
            }
        }
    }

    bool GetDDGIVolumeRootSignatureDesc(int descriptorHeapOffset, ID3DBlob** signature)
    {
        // 1 UAV for the ray hit radiance (u0, space1)
        // 2 UAV for the probe irradiance or distance (u1 and u2, space1)
        // 1 UAV for the probe offsets (optional) (u3, space1)
        // 1 UAV for the probe states (optional) (u4, space1)
        D3D12_DESCRIPTOR_RANGE ranges[1];

        ranges[0].BaseShaderRegister = 0;
        ranges[0].NumDescriptors = 5;
        ranges[0].RegisterSpace = 1;
        ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        ranges[0].OffsetInDescriptorsFromTableStart = descriptorHeapOffset;

        // Root constants
        D3D12_ROOT_PARAMETER param0 = {};
        param0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        param0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        param0.Constants.Num32BitValues = 4;
        param0.Constants.RegisterSpace = 1;
        param0.Constants.ShaderRegister = 0;

        // Volume Constant Buffer (b1, space1)
        D3D12_ROOT_PARAMETER param1 = {};
        param1.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        param1.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        param1.Descriptor.RegisterSpace = 1;
        param1.Descriptor.ShaderRegister = 1;

        // Descriptor table
        D3D12_ROOT_PARAMETER param2 = {};
        param2.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param2.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        param2.DescriptorTable.NumDescriptorRanges = _countof(ranges);
        param2.DescriptorTable.pDescriptorRanges = ranges;

        D3D12_ROOT_PARAMETER rootParams[3] = { param0, param1, param2 };

        D3D12_ROOT_SIGNATURE_DESC desc = {};
        desc.NumParameters = _countof(rootParams);
        desc.pParameters = rootParams;
        desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ID3DBlob* error = nullptr;
        HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, signature, &error);
        if (FAILED(hr))
        {
            RTXGI_SAFE_RELEASE(error);
            return false;
        }

        return true;
    }

    DDGIVolumeDescGPU GetDDGIVolumeGPUDesc(DDGIVolumeDesc &desc)
    {
        DDGIVolumeDescGPU descGPU = {};
        descGPU.origin = desc.origin;
        descGPU.probeGridCounts = desc.probeGridCounts;
        descGPU.probeGridSpacing = desc.probeGridSpacing;
        descGPU.numRaysPerProbe = static_cast<int>(desc.numRaysPerProbe);
        descGPU.probeMaxRayDistance = desc.probeMaxRayDistance;
        descGPU.probeHysteresis = desc.probeHysteresis;
        descGPU.probeChangeThreshold = desc.probeChangeThreshold;
        descGPU.probeBrightnessThreshold = desc.probeBrightnessThreshold;
        descGPU.probeDistanceExponent = desc.probeDistanceExponent;
        descGPU.probeIrradianceEncodingGamma = desc.probeIrradianceEncodingGamma;
        descGPU.probeInverseIrradianceEncodingGamma = (1.f / desc.probeIrradianceEncodingGamma);
        descGPU.probeNumIrradianceTexels = desc.numIrradianceTexels;
        descGPU.probeNumDistanceTexels = desc.numDistanceTexels;
        descGPU.normalBias = desc.normalBias;
        descGPU.viewBias = desc.viewBias;
#if RTXGI_DDGI_PROBE_SCROLL
        descGPU.volumeMovementType = static_cast<int>(desc.movementType);
        descGPU.probeScrollOffsets = desc.probeScrollOffsets;
#endif
#if RTXGI_DDGI_PROBE_RELOCATION
        descGPU.probeMinFrontfaceDistance = desc.probeMinFrontfaceDistance;
#endif
#if RTXGI_DDGI_PROBE_RELOCATION || RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        descGPU.probeBackfaceThreshold = desc.probeBackfaceThreshold;
#endif

        return descGPU;
    }

    //------------------------------------------------------------------------
    // Public DDGIVolume Functions
    //------------------------------------------------------------------------

    ERTXGIStatus DDGIVolume::Create(DDGIVolumeDesc &desc, DDGIVolumeResources &resources)
    {
        // Validate the probe counts
        if (desc.probeGridCounts.x <= 0 || desc.probeGridCounts.y <= 0 || desc.probeGridCounts.z <= 0)
        {
            return ERTXGIStatus::ERROR_DDGI_INVALID_PROBE_COUNTS;
        }

        // Validate the descriptor heap
        if(resources.descriptorHeap == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_DESCRIPTOR_HEAP;

        // Validate the resources
#if RTXGI_DDGI_SDK_MANAGED_RESOURCES
        if (resources.device == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_D3D_DEVICE;
        if (resources.probeRadianceBlendingCS == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_PROBE_RADIANCE_BLENDING_BYTECODE;
        if (resources.probeDistanceBlendingCS == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_PROBE_DISTANCE_BLENDING_BYTECODE;
        if (resources.probeBorderRowCS == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_PROBE_BORDER_ROW_BYTECODE;
        if (resources.probeBorderColumnCS == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_PROBE_BORDER_COLUMN_BYTECODE;
#if RTXGI_DDGI_PROBE_RELOCATION
        if (resources.probeRelocationCS == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_PROBE_RELOCATION_BYTECODE;
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        if (resources.probeStateClassifierCS == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_PROBE_STATE_CLASSIFIER_BYTECODE;
        if (resources.probeStateClassifierActivateAllCS == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_PROBE_STATE_CLASSIFIER_ACTIVATE_ALL_BYTECODE;
#endif
#else
        if (resources.rootSignature == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_ROOT_SIGNATURE;
        if (resources.probeRTRadiance == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_RT_RADIANCE_TEXTURE;
        if (resources.probeIrradiance == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_IRRADIANCE_TEXTURE;
        if (resources.probeDistance == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_DISTANCE_TEXTURE;
#if RTXGI_DDGI_PROBE_RELOCATION
        if (resources.probeOffsets == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_OFFSETS_TEXTURE;
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        if (resources.probeStates == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_STATES_TEXTURE;
#endif

        if (resources.probeRadianceBlendingPSO == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_RADIANCE_BLENDING_PSO;
        if (resources.probeDistanceBlendingPSO == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_DISTANCE_BLENDING_PSO;
        if (resources.probeBorderRowPSO == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_BORDER_ROW_PSO;
        if (resources.probeBorderColumnPSO == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_BORDER_COLUMN_PSO;
#if RTXGI_DDGI_PROBE_RELOCATION
        if (resources.probeRelocationPSO == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_PROBE_RELOCATION_PSO;
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        if (resources.probeStateClassifierPSO == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_PROBE_STATE_CLASSIFIER_PSO;
        if (resources.probeStateClassifierActivateAllPSO == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_PROBE_STATE_CLASSIFIER_ACTIVATE_ALL_PSO;
#endif
#endif

        // Store the descriptor heap pointer, size, and offset
        m_descriptorHeap = resources.descriptorHeap;
        m_descriptorDescSize = resources.descriptorHeapDescSize;
        m_descriptorHeapOffset = resources.descriptorHeapOffset;

        // Allocate or store pointers to the texture and PSO resources
#if RTXGI_DDGI_SDK_MANAGED_RESOURCES
        if (m_resources.IsDeviceChanged(resources))
        {
            // The D3D device has changed, release and reallocate resources

            // Create the root signature
            RTXGI_SAFE_RELEASE(m_rootSignature);
            if (!CreateRootSignature(resources.device)) return ERTXGIStatus::ERROR_DDGI_CREATE_FAILURE_ROOT_SIGNATURE;

            // Create the compute PSOs
            RTXGI_SAFE_RELEASE(m_probeRadianceBlendingPSO);
            RTXGI_SAFE_RELEASE(m_probeDistanceBlendingPSO);
            RTXGI_SAFE_RELEASE(m_probeBorderRowPSO);
            RTXGI_SAFE_RELEASE(m_probeBorderColumnPSO);
            if (!CreateComputePSO(resources.device, resources.probeRadianceBlendingCS, &m_probeRadianceBlendingPSO)) return ERTXGIStatus::ERROR_DDGI_CREATE_FAILURE_COMPUTE_PSO;
            if (!CreateComputePSO(resources.device, resources.probeDistanceBlendingCS, &m_probeDistanceBlendingPSO)) return ERTXGIStatus::ERROR_DDGI_CREATE_FAILURE_COMPUTE_PSO;
            if (!CreateComputePSO(resources.device, resources.probeBorderRowCS, &m_probeBorderRowPSO)) return ERTXGIStatus::ERROR_DDGI_CREATE_FAILURE_COMPUTE_PSO;
            if (!CreateComputePSO(resources.device, resources.probeBorderColumnCS, &m_probeBorderColumnPSO)) return ERTXGIStatus::ERROR_DDGI_CREATE_FAILURE_COMPUTE_PSO;
#if RTXGI_DDGI_PROBE_RELOCATION
            RTXGI_SAFE_RELEASE(m_probeRelocationPSO);
            if (!CreateComputePSO(resources.device, resources.probeRelocationCS, &m_probeRelocationPSO)) return ERTXGIStatus::ERROR_DDGI_CREATE_FAILURE_COMPUTE_PSO;
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
            RTXGI_SAFE_RELEASE(m_probeClassifierPSO);
            RTXGI_SAFE_RELEASE(m_probeClassifierActivateAllPSO);
            if (!CreateComputePSO(resources.device, resources.probeStateClassifierCS, &m_probeClassifierPSO)) return ERTXGIStatus::ERROR_DDGI_CREATE_FAILURE_COMPUTE_PSO;
            if (!CreateComputePSO(resources.device, resources.probeStateClassifierActivateAllCS, &m_probeClassifierActivateAllPSO)) return ERTXGIStatus::ERROR_DDGI_CREATE_FAILURE_COMPUTE_PSO;
#endif
        }

        if (m_desc.ShouldAllocateProbes(desc) || m_resources.IsDeviceChanged(resources))
        {
            // Probe counts have changed. The textures are the wrong size or aren't allocated yet
            // (Re)allocate the irradiance, distance, and probe position offset textures (if position preprocess is enabled)
            if (!CreateProbeRTRadianceTexture(desc, resources.device))
            {
                return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_RT_RADIANCE_TEXTURE;
            }

            if (!CreateProbeIrradianceTexture(desc, resources.device))
            {
                return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_IRRADIANCE_TEXTURE;
            }

            if (!CreateProbeDistanceTexture(desc, resources.device))
            {
                return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_DISTANCE_TEXTURE;
            }

#if RTXGI_DDGI_PROBE_RELOCATION
            if (!CreateProbeOffsetTexture(desc, resources.device))
            {
                return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_OFFSETS_TEXTURE;
            }
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
            if (!CreateProbeStatesTexture(desc, resources.device))
            {
                return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_STATES_TEXTURE;
            }
#endif
        }
        else
        {
            if (m_desc.ShouldAllocateRTRadiance(desc))
            {
                // The number of probe rays to trace per frame has changed. Reallocate the RT radiance texture.
                if (!CreateProbeRTRadianceTexture(desc, resources.device))
                {
                    return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_RT_RADIANCE_TEXTURE;
                }
            }

            if (m_desc.ShouldAllocateIrradiance(desc))
            {
                // The number of irradiance texels per probe has changed. Reallocate the irradiance texture.
                if (!CreateProbeIrradianceTexture(desc, resources.device))
                {
                    return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_IRRADIANCE_TEXTURE;
                }
            }

            if (m_desc.ShouldAllocateDistance(desc))
            {
                // The number of distance texels per probe has changed. Reallocate the distance texture.
                if (!CreateProbeDistanceTexture(desc, resources.device))
                {
                    return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_DISTANCE_TEXTURE;
                }
            }
        }

        // Create the resource descriptors
        CreateDescriptors(resources.device);
#else
        m_rootSignature = resources.rootSignature;
        m_probeRTRadiance = resources.probeRTRadiance;
        m_probeIrradiance = resources.probeIrradiance;
        m_probeDistance = resources.probeDistance;
#if RTXGI_DDGI_PROBE_RELOCATION
        m_probeOffsets = resources.probeOffsets;
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        m_probeStates = resources.probeStates;
#endif
        m_probeRadianceBlendingPSO = resources.probeRadianceBlendingPSO;
        m_probeDistanceBlendingPSO = resources.probeDistanceBlendingPSO;
        m_probeBorderRowPSO = resources.probeBorderRowPSO;
        m_probeBorderColumnPSO = resources.probeBorderColumnPSO;
#if RTXGI_DDGI_PROBE_RELOCATION
        m_probeRelocationPSO = resources.probeRelocationPSO;
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        m_probeClassifierPSO = resources.probeStateClassifierPSO;
        m_probeClassifierActivateAllPSO = resources.probeStateClassifierActivateAllPSO;
#endif
#endif

        // Store the new descriptors
        m_desc = desc;
        m_resources = resources;
        m_origin = desc.origin;
        m_boundingBox = GetBoundingBox();
        m_name = desc.name;

        // Init the random seed and compute a rotation transform
        InitRandomSeed();

        return ERTXGIStatus::OK;
    }

    ERTXGIStatus DDGIVolume::Move(float3 translation)
    {
        m_desc.origin += translation;
        m_origin = m_desc.origin;

        return ERTXGIStatus::OK;
    }

#if RTXGI_DDGI_PROBE_SCROLL
    ERTXGIStatus DDGIVolume::Scroll(int3 translation)
    {
        // Update the volume's origin
        Move(m_desc.probeGridSpacing * translation);
        
        // Make the probe grid offsets positive for simpler shader code
        m_probeScrollOffsets += translation;
        m_desc.probeScrollOffsets = ((m_probeScrollOffsets % m_desc.probeGridCounts) + m_desc.probeGridCounts) % m_desc.probeGridCounts;

        return ERTXGIStatus::OK;
    }

    ERTXGIStatus DDGIVolume::Scroll(float3 translation, float3 deadzoneRadii)
    {
        // Accumulate the translation
        m_probeScrollTranslation += translation;

        // Check if the accumulated translation has moved out of the specified deadzone
        bool leftDeadzone = (abs(m_probeScrollTranslation.x) >= deadzoneRadii.x) || (abs(m_probeScrollTranslation.y) >= deadzoneRadii.y) || (abs(m_probeScrollTranslation.z) >= deadzoneRadii.z);
        if (leftDeadzone)
        {
            // Compute the number of probe grid cells the accumulated translation has crossed
            int3 scroll = { AbsFloor(m_probeScrollTranslation.x / m_desc.probeGridSpacing.x), AbsFloor(m_probeScrollTranslation.y / m_desc.probeGridSpacing.y), AbsFloor(m_probeScrollTranslation.z / m_desc.probeGridSpacing.z) };
            if (abs(scroll.x) > 0 || abs(scroll.y) > 0 || abs(scroll.z) > 0)
            {
                // Scroll the grid cells and reset the accumulated translation based on the number of scrolled grid cells
                Scroll(scroll);
                float3 scaledScroll = { float(scroll.x), float(scroll.y), float(scroll.z) };
                m_probeScrollTranslation = m_probeScrollTranslation - (scaledScroll * deadzoneRadii);
            }
        }

        return ERTXGIStatus::OK;
    }

    ERTXGIStatus DDGIVolume::Scroll(float3 translation, AABB deadzoneBBox)
    {
        // TODO
        return ERTXGIStatus::OK;
    }
#endif /* RTXGI_DDGI_PROBE_SCROLL */

    ERTXGIStatus DDGIVolume::Update(ID3D12Resource* constantBuffer, UINT64 offsetInBytes)
    {
        if (!constantBuffer) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_CONSTANT_BUFFER;

        // Set the constant buffer and offset
        m_volumeCB = constantBuffer;
        m_volumeCBOffsetInBytes = offsetInBytes;

        // Compute a rotation transform
        ComputeRandomRotation();

        // Get the compact GPU descriptor
        DDGIVolumeDescGPU gpuDesc = GetDDGIVolumeGPUDesc(m_desc);
        gpuDesc.probeRayRotationTransform = m_rotationTransform;

        // Map the constant buffer and update it
        UINT8* pData = nullptr;
        HRESULT hr = m_volumeCB->Map(0, nullptr, reinterpret_cast<void**>(&pData));
        if (FAILED(hr)) return ERTXGIStatus::ERROR_DDGI_MAP_FAILURE_CONSTANT_BUFFER;

        pData += m_volumeCBOffsetInBytes;
        memcpy(pData, &gpuDesc, sizeof(DDGIVolumeDescGPU));

        m_volumeCB->Unmap(0, nullptr);

        return ERTXGIStatus::OK;
    }

    ERTXGIStatus DDGIVolume::UpdateProbes(ID3D12GraphicsCommandList4* cmdList)
    {
#if RTXGI_PERF_MARKERS
        UINT pixColor = PIX_COLOR(25, 140, 215);
        PIXScopedEvent(cmdList, pixColor, "RTXGI: Update Probes");
#endif

        // Transition the irradiance and distance buffers to unordered access
        D3D12_RESOURCE_BARRIER transitionBarriers[2] = {};
        transitionBarriers[0].Transition.pResource = m_probeIrradiance;
        transitionBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        transitionBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        transitionBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        transitionBarriers[1].Transition.pResource = m_probeDistance;
        transitionBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        transitionBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        transitionBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        // Wait for the transition to complete
        cmdList->ResourceBarrier(2, transitionBarriers);

        // Run compute shaders, set common values
        cmdList->SetComputeRootSignature(m_rootSignature);
        cmdList->SetDescriptorHeaps(1, &m_descriptorHeap);
        cmdList->SetComputeRootConstantBufferView(1, m_volumeCB->GetGPUVirtualAddress() + m_volumeCBOffsetInBytes);
        cmdList->SetComputeRootDescriptorTable(2, m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());

        // Get the number of probes on the X and Y dimensions of the texture
        UINT probeCountX, probeCountY;
        GetDDGIVolumeProbeCounts(m_desc, probeCountX, probeCountY);

        // Probe irradiance blending
        {
#if RTXGI_PERF_MARKERS
            PIXScopedEvent(cmdList, pixColor, "Irradiance Blending");
#endif
            cmdList->SetPipelineState(m_probeRadianceBlendingPSO);
            cmdList->Dispatch(probeCountX, probeCountY, 1);
        }

        // Probe distance blending
        {
#if RTXGI_PERF_MARKERS
            PIXScopedEvent(cmdList, pixColor, "Distance Blending");
#endif
            cmdList->SetPipelineState(m_probeDistanceBlendingPSO);
            cmdList->Dispatch(probeCountX, probeCountY, 1);
        }

        // Wait for the irradiance and distance blending passes 
        // to complete before updating the borders
        D3D12_RESOURCE_BARRIER barriers[2] = {};
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barriers[0].UAV.pResource = m_probeIrradiance;
        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barriers[1].UAV.pResource = m_probeDistance;

        cmdList->ResourceBarrier(2, barriers);

        float groupSize = 8.f;
        UINT numThreadsX, numThreadsY;
        UINT numGroupsX, numGroupsY;

        // Probe irradiance border update
        {
#if RTXGI_PERF_MARKERS
            PIXScopedEvent(cmdList, pixColor, "Irradiance Border Update");
#endif
            cmdList->SetComputeRoot32BitConstant(0, *(UINT*)&m_desc.numIrradianceTexels, 0);
            cmdList->SetComputeRoot32BitConstant(0, 0, 1);

            // Rows
            numThreadsX = (probeCountX * (m_desc.numIrradianceTexels + 2));
            numThreadsY = probeCountY;
            numGroupsX = (UINT)ceil((float)numThreadsX / groupSize);
            numGroupsY = (UINT)ceil((float)numThreadsY / groupSize);

            cmdList->SetPipelineState(m_probeBorderRowPSO);
            cmdList->Dispatch(numGroupsX, numGroupsY, 1);

            // Columns
            numThreadsX = probeCountX;
            numThreadsY = (probeCountY * (m_desc.numIrradianceTexels + 2));
            numGroupsX = (UINT)ceil((float)numThreadsX / groupSize);
            numGroupsY = (UINT)ceil((float)numThreadsY / groupSize);

            cmdList->SetPipelineState(m_probeBorderColumnPSO);
            cmdList->Dispatch(numGroupsX, numGroupsY, 1);
        }

        // Probe distance border update
        {
#if RTXGI_PERF_MARKERS
            PIXScopedEvent(cmdList, pixColor, "Distance Border Update");
#endif
            cmdList->SetComputeRoot32BitConstant(0, *(UINT*)&m_desc.numDistanceTexels, 0);
            cmdList->SetComputeRoot32BitConstant(0, 1, 1);

            // Rows
            numThreadsX = (probeCountX * (m_desc.numDistanceTexels + 2));
            numThreadsY = probeCountY;
            numGroupsX = (UINT)ceil((float)numThreadsX / groupSize);
            numGroupsY = (UINT)ceil((float)numThreadsY / groupSize);

            cmdList->SetPipelineState(m_probeBorderRowPSO);
            cmdList->Dispatch(numGroupsX, numGroupsY, 1);

            // Columns
            numThreadsX = probeCountX;
            numThreadsY = (probeCountY * (m_desc.numDistanceTexels + 2));
            numGroupsX = (UINT)ceil((float)numThreadsX / groupSize);
            numGroupsY = (UINT)ceil((float)numThreadsY / groupSize);

            cmdList->SetPipelineState(m_probeBorderColumnPSO);
            cmdList->Dispatch(numGroupsX, numGroupsY, 1);
        }

        // Wait for the irradiance and distance border update passes
        // to complete before using the textures
        cmdList->ResourceBarrier(2, barriers);

        // Transition back to pixel shader resources for reading
        transitionBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        transitionBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        transitionBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        transitionBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        // Wait for the transition to complete
        cmdList->ResourceBarrier(2, transitionBarriers);

        return ERTXGIStatus::OK;
    }

#if RTXGI_DDGI_PROBE_RELOCATION
    ERTXGIStatus DDGIVolume::RelocateProbes(ID3D12GraphicsCommandList4* cmdList, float probeDistanceScale)
    {
#if RTXGI_PERF_MARKERS
        PIXScopedEvent(cmdList, PIX_COLOR(118, 185, 0), "RTXGI: Relocate Probes");
#endif

        // Set common values
        cmdList->SetComputeRootSignature(m_rootSignature);
        cmdList->SetDescriptorHeaps(1, &m_descriptorHeap);
        cmdList->SetComputeRootConstantBufferView(1, m_volumeCB->GetGPUVirtualAddress() + m_volumeCBOffsetInBytes);
        cmdList->SetComputeRootDescriptorTable(2, m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());

        // Distance scale is recommended between (1.f, 0.f) see comment in DDGIVolume.h
        cmdList->SetComputeRoot32BitConstant(0, *(UINT*)&probeDistanceScale, 0);

        // Get the number of probes on the X and Y dimensions of the texture
        UINT probeCountX, probeCountY;
        GetDDGIVolumeProbeCounts(m_desc, probeCountX, probeCountY);

        float groupSizeX = 8.f;
        float groupSizeY = 4.f;

        // Probe relocation
        {
            UINT numThreadsX = probeCountX;
            UINT numThreadsY = probeCountY;
            UINT numGroupsX = (UINT)ceil((float)numThreadsX / groupSizeX);
            UINT numGroupsY = (UINT)ceil((float)numThreadsY / groupSizeY);

            cmdList->SetPipelineState(m_probeRelocationPSO);
            cmdList->Dispatch(numGroupsX, numGroupsY, 1);
        }

        return ERTXGIStatus::OK;
    }
#endif /* RTXGI_DDGI_PROBE_RELOCATION */

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    ERTXGIStatus DDGIVolume::ClassifyProbes(ID3D12GraphicsCommandList4* cmdList)
    {
#if RTXGI_PERF_MARKERS
        PIXScopedEvent(cmdList, PIX_COLOR(118, 185, 0), "RTXGI: Classify Probes");
#endif

        // Run compute shaders, set common values
        cmdList->SetComputeRootSignature(m_rootSignature);
        cmdList->SetDescriptorHeaps(1, &m_descriptorHeap);
        cmdList->SetComputeRootConstantBufferView(1, m_volumeCB->GetGPUVirtualAddress() + m_volumeCBOffsetInBytes);
        cmdList->SetComputeRootDescriptorTable(2, m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());

        // Get the number of probes on the X and Y dimensions of the texture
        UINT probeCountX, probeCountY;
        GetDDGIVolumeProbeCounts(m_desc, probeCountX, probeCountY);

        const float groupSizeX = 8.f;
        const float groupSizeY = 4.f;

        // Probe classification
        {
            UINT numThreadsX = probeCountX;
            UINT numThreadsY = probeCountY;
            UINT numGroupsX = (UINT)ceil((float)numThreadsX / groupSizeX);
            UINT numGroupsY = (UINT)ceil((float)numThreadsY / groupSizeY);

            cmdList->SetPipelineState(m_probeClassifierPSO);
            cmdList->Dispatch(numGroupsX, numGroupsY, 1);
        }

        D3D12_RESOURCE_BARRIER uavBarrier = {};
        uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uavBarrier.UAV.pResource = m_probeStates;
        uavBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        cmdList->ResourceBarrier(1, &uavBarrier);

        return ERTXGIStatus::OK;
    }

    ERTXGIStatus DDGIVolume::ActivateAllProbes(ID3D12GraphicsCommandList4* cmdList)
    {
#if RTXGI_PERF_MARKERS
        PIXScopedEvent(cmdList, PIX_COLOR(118, 185, 0), "RTXGI: Activate All Probes");
#endif
        // Run compute shaders, set common values
        cmdList->SetComputeRootSignature(m_rootSignature);
        cmdList->SetDescriptorHeaps(1, &m_descriptorHeap);
        cmdList->SetComputeRootConstantBufferView(1, m_volumeCB->GetGPUVirtualAddress() + m_volumeCBOffsetInBytes);
        cmdList->SetComputeRootDescriptorTable(2, m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());

        // Get the number of probes on the X and Y dimensions of the texture
        UINT probeCountX, probeCountY;
        GetDDGIVolumeProbeCounts(m_desc, probeCountX, probeCountY);

        const float groupSizeX = 8.f;
        const float groupSizeY = 4.f;

        // Set all probe states to ACTIVE
        {
            UINT numThreadsX = probeCountX;
            UINT numThreadsY = probeCountY;
            UINT numGroupsX = (UINT)ceil((float)numThreadsX / groupSizeX);
            UINT numGroupsY = (UINT)ceil((float)numThreadsY / groupSizeY);

            cmdList->SetPipelineState(m_probeClassifierActivateAllPSO);
            cmdList->Dispatch(numGroupsX, numGroupsY, 1);
        }

        return ERTXGIStatus::OK;
    }
#endif /* RTXGI_DDGI_PROBE_STATE_CLASSIFIER */

    void DDGIVolume::Destroy()
    {
        m_descriptorHeap = nullptr;
        m_descriptorHeapStart.ptr = 0;
        m_descriptorDescSize = 0;
        m_descriptorHeapOffset = 0;

        m_desc = {};
        m_resources = {};
        m_boundingBox = {};
        m_origin = {};
        m_rootSignatureDesc = {};
        m_rotationTransform = {};
#if RTXGI_DDGI_PROBE_SCROLL
        m_probeScrollOffsets = {};
#endif

#if RTXGI_DDGI_SDK_MANAGED_RESOURCES
        RTXGI_SAFE_RELEASE(m_rootSignature);
        RTXGI_SAFE_RELEASE(m_probeRTRadiance);
        RTXGI_SAFE_RELEASE(m_probeIrradiance);
        RTXGI_SAFE_RELEASE(m_probeDistance);
#if RTXGI_DDGI_PROBE_RELOCATION
        RTXGI_SAFE_RELEASE(m_probeOffsets);
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        RTXGI_SAFE_RELEASE(m_probeStates);
#endif
        RTXGI_SAFE_RELEASE(m_probeRadianceBlendingPSO);
        RTXGI_SAFE_RELEASE(m_probeDistanceBlendingPSO);
        RTXGI_SAFE_RELEASE(m_probeBorderRowPSO);
        RTXGI_SAFE_RELEASE(m_probeBorderColumnPSO);
#if RTXGI_DDGI_PROBE_RELOCATION
        RTXGI_SAFE_RELEASE(m_probeRelocationPSO);
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        RTXGI_SAFE_RELEASE(m_probeClassifierPSO);
        RTXGI_SAFE_RELEASE(m_probeClassifierActivateAllPSO);
#endif
#else
        m_rootSignature = nullptr;
        m_probeRTRadiance = nullptr;
        m_probeIrradiance = nullptr;
        m_probeDistance = nullptr;
#if RTXGI_DDGI_PROBE_RELOCATION
        m_probeOffsets = nullptr;
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        m_probeStates = nullptr;
#endif
        m_probeRadianceBlendingPSO = nullptr;
        m_probeDistanceBlendingPSO = nullptr;
        m_probeBorderRowPSO = nullptr;
        m_probeBorderColumnPSO = nullptr;
#if RTXGI_DDGI_PROBE_RELOCATION
        m_probeRelocationPSO = nullptr;
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        m_probeClassifierPSO = nullptr;
        m_probeClassifierActivateAllPSO = nullptr;
#endif
#endif;
    }

    //------------------------------------------------------------------------
    // Accessors
    //------------------------------------------------------------------------

    float3 DDGIVolume::GetProbeWorldPosition(int probeIndex) const
    {
        // NOTE: If the probe position preprocess was run, the probe position offset textures need to be read and added to this value.
        int3 probeCoords = GetProbeGridCoords(probeIndex);
        float3 probeGridWorldPosition = m_desc.probeGridSpacing * probeCoords;
        float3 probeGridShift = (m_desc.probeGridSpacing * (m_desc.probeGridCounts - 1)) / 2.f;

        return (m_desc.origin + probeGridWorldPosition - probeGridShift);
    }

    AABB DDGIVolume::GetBoundingBox() const
    {
        float3 origin = m_desc.origin;
        float3 extent = float3(m_desc.probeGridSpacing * (m_desc.probeGridCounts - 1)) / 2.f;
        return { (origin - extent), (origin + extent) };
    }

    //------------------------------------------------------------------------
    // Private Resource Allocation Helper Functions (Managed Resources)
    //------------------------------------------------------------------------

#if RTXGI_DDGI_SDK_MANAGED_RESOURCES
    void DDGIVolume::CreateDescriptors(ID3D12Device* device)
    {
        // Compute the start of the descriptor heap's open slots
        m_descriptorHeapStart = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
        m_descriptorHeapStart.ptr += (m_descriptorDescSize * m_descriptorHeapOffset);

        D3D12_CPU_DESCRIPTOR_HANDLE handle = m_descriptorHeapStart;

        // Create the RT radiance UAV 
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGITextureType::RTRadiance);
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(m_probeRTRadiance, nullptr, &uavDesc, handle);

        handle.ptr += m_descriptorDescSize;

        // Create the irradiance UAV
        uavDesc = {};
        uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGITextureType::Irradiance);
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(m_probeIrradiance, nullptr, &uavDesc, handle);

        handle.ptr += m_descriptorDescSize;

        // Create the distance UAV
        uavDesc = {};
        uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGITextureType::Distance);
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(m_probeDistance, nullptr, &uavDesc, handle);

#if RTXGI_DDGI_PROBE_RELOCATION
        handle.ptr += m_descriptorDescSize;

        // Create the probe position offsets UAV
        uavDesc = {};
        uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGITextureType::Offsets);
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(m_probeOffsets, nullptr, &uavDesc, handle);
#elif RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        handle.ptr += m_descriptorDescSize;
#endif

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        handle.ptr += m_descriptorDescSize;

        // Create the probe position offsets UAV
        uavDesc = {};
        uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGITextureType::States);
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(m_probeStates, nullptr, &uavDesc, handle);
#endif
    }

    bool DDGIVolume::CreateRootSignature(ID3D12Device* device)
    {
        ID3DBlob* signature;
        if (!GetDDGIVolumeRootSignatureDesc(m_descriptorHeapOffset, &signature)) return false;      

        HRESULT hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
        RTXGI_SAFE_RELEASE(signature);
        if (FAILED(hr)) return false;

#if RTXGI_NAME_D3D_OBJECTS
        m_rootSignature->SetName(L"RTXGI DDGIVolume Root Signature");
#endif

        return true;
    }

    bool DDGIVolume::CreateTexture(UINT64 width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_STATES state, ID3D12Resource** resource, ID3D12Device* device)
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
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        // Create the texture
        HRESULT hr = device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(resource));
        if (hr != S_OK) return false;
        return true;
    }

    bool DDGIVolume::CreateComputePSO(ID3D12Device* device, ID3DBlob* shader, ID3D12PipelineState** pipeline)
    {
        if (shader == nullptr) return false;
        if (m_rootSignature == nullptr) return false;

        D3D12_COMPUTE_PIPELINE_STATE_DESC pipeDesc = {};
        pipeDesc.CS.BytecodeLength = shader->GetBufferSize();
        pipeDesc.CS.pShaderBytecode = shader->GetBufferPointer();
        pipeDesc.pRootSignature = m_rootSignature;

        HRESULT hr = device->CreateComputePipelineState(&pipeDesc, IID_PPV_ARGS(pipeline));
        if (FAILED(hr)) return false;

        return true;
    }

    bool DDGIVolume::CreateProbeRTRadianceTexture(DDGIVolumeDesc &desc, ID3D12Device* device)
    {
        RTXGI_SAFE_RELEASE(m_probeRTRadiance);

        UINT width = 0;
        UINT height = 0;
        GetDDGIVolumeTextureDimensions(desc, EDDGITextureType::RTRadiance, width, height);

        DXGI_FORMAT format = GetDDGIVolumeTextureFormat(EDDGITextureType::RTRadiance);

        // Create the texture, RGB store radiance, A stores hit distance
        bool result = CreateTexture(width, height, format, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, &m_probeRTRadiance, device);
        if (!result) return false;
#if RTXGI_NAME_D3D_OBJECTS
        m_probeRTRadiance->SetName(L"RTXGI DDGIVolume Probe RT Radiance");
#endif
        return true;
    }

    bool DDGIVolume::CreateProbeIrradianceTexture(DDGIVolumeDesc &desc, ID3D12Device* device)
    {
        RTXGI_SAFE_RELEASE(m_probeIrradiance);

        UINT width = 0;
        UINT height = 0;
        GetDDGIVolumeTextureDimensions(desc, EDDGITextureType::Irradiance, width, height);

        DXGI_FORMAT format = GetDDGIVolumeTextureFormat(EDDGITextureType::Irradiance);

        // Create the texture
        bool result = CreateTexture(width, height, format, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &m_probeIrradiance, device);
        if (!result) return false;
#if RTXGI_NAME_D3D_OBJECTS
        m_probeIrradiance->SetName(L"RTXGI DDGIVolume Probe Irradiance");
#endif
        return true;
    }

    bool DDGIVolume::CreateProbeDistanceTexture(DDGIVolumeDesc &desc, ID3D12Device* device)
    {
        RTXGI_SAFE_RELEASE(m_probeDistance);

        UINT width = 0;
        UINT height = 0;
        GetDDGIVolumeTextureDimensions(desc, EDDGITextureType::Distance, width, height);

        DXGI_FORMAT format = GetDDGIVolumeTextureFormat(EDDGITextureType::Distance);

        bool result = CreateTexture(width, height, format, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &m_probeDistance, device);
        if (!result) return false;
#if RTXGI_NAME_D3D_OBJECTS
        m_probeDistance->SetName(L"RTXGI DDGIVolume Probe Distance");
#endif

        return true;
    }
    
#if RTXGI_DDGI_PROBE_RELOCATION
    bool DDGIVolume::CreateProbeOffsetTexture(DDGIVolumeDesc &desc, ID3D12Device* device)
    {
        UINT width = 0;
        UINT height = 0;
        GetDDGIVolumeTextureDimensions(desc, EDDGITextureType::Offsets, width, height);
        if (width <= 0) return false;

        DXGI_FORMAT format = GetDDGIVolumeTextureFormat(EDDGITextureType::Offsets);

        bool result = CreateTexture(width, height, format, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, &m_probeOffsets, device);
        if (!result) return false;
#if RTXGI_NAME_D3D_OBJECTS
        m_probeOffsets->SetName(L"RTXGI DDGIVolume Probe Offsets");
#endif
        return true;
    }
#endif

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    bool DDGIVolume::CreateProbeStatesTexture(DDGIVolumeDesc &desc, ID3D12Device* device)
    {
        UINT width = 0;
        UINT height = 0;
        GetDDGIVolumeTextureDimensions(desc, EDDGITextureType::States, width, height);
        if (width <= 0) return false;

        DXGI_FORMAT format = GetDDGIVolumeTextureFormat(EDDGITextureType::States);

        bool result = CreateTexture(width, height, format, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, &m_probeStates, device);
        if (!result) return false;
#if RTXGI_NAME_D3D_OBJECTS
        m_probeStates->SetName(L"RTXGI DDGIVolume Probe States");
#endif
        return true;
    }
#endif

#endif /* RTXGI_MANAGED_RESOURCES */

    //------------------------------------------------------------------------
    // Private Helper Functions
    //------------------------------------------------------------------------

    void DDGIVolume::ComputeRandomRotation()
    {
        // This approach is based on James Arvo's implementation from Graphics Gems 3 (pg 117-120).
        // Also available at: http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.53.1357&rep=rep1&type=pdf

        // Setup a random rotation matrix using 3 uniform RVs
        float u1 = 2.f * RTXGI_PI * GetRandomNumber();
        float cos1 = std::cosf(u1);
        float sin1 = std::sinf(u1);

        float u2 = 2.f * RTXGI_PI * GetRandomNumber();
        float cos2 = std::cosf(u2);
        float sin2 = std::sinf(u2);
        
        float u3 = GetRandomNumber();
        float sq3 = 2.f * std::sqrtf(u3 * (1.f - u3));
        
        float s2 = 2.f * u3 * sin2 * sin2 - 1.f; 
        float c2 = 2.f * u3 * cos2 * cos2 - 1.f; 
        float sc = 2.f * u3 * sin2 * cos2;

        // Create the random rotation matrix
        float _11 = cos1 * c2 - sin1 * sc;
        float _12 = sin1 * c2 + cos1 * sc;
        float _13 = sq3 * cos2;

        float _21 = cos1 * sc - sin1 * s2;
        float _22 = sin1 * sc + cos1 * s2;
        float _23 = sq3 * sin2;

        float _31 = cos1 * (sq3 * cos2) - sin1 * (sq3 * sin2);
        float _32 = sin1 * (sq3 * cos2) + cos1 * (sq3 * sin2);
        float _33 = 1.f - 2.f * u3;

        // HLSL is column-major
        float4x4 transform;
        transform.r0 = { _11, _12, _13, 0.f };
        transform.r1 = { _21, _22, _23, 0.f };
        transform.r2 = { _31, _32, _33, 0.f };
        transform.r3 = { 0.f, 0.f, 0.f, 1.f };
    
        m_rotationTransform = transform;
    }

    int3 DDGIVolume::GetProbeGridCoords(int probeIndex) const
    {   
#if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
        int x = probeIndex % m_desc.probeGridCounts.x;
        int y = probeIndex / (m_desc.probeGridCounts.x * m_desc.probeGridCounts.z);
        int z = (probeIndex / m_desc.probeGridCounts.x) % m_desc.probeGridCounts.z;
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
        int x = (probeIndex / m_desc.probeGridCounts.y) % m_desc.probeGridCounts.x;
        int y = probeIndex % m_desc.probeGridCounts.y;
        int z = probeIndex / (m_desc.probeGridCounts.x * m_desc.probeGridCounts.y);
#elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
        int x = probeIndex % m_desc.probeGridCounts.x;
        int y = (probeIndex / m_desc.probeGridCounts.x) % m_desc.probeGridCounts.y;
        int z = probeIndex / (m_desc.probeGridCounts.x * m_desc.probeGridCounts.y);
#endif
        return { x, y, z };
    }

}
