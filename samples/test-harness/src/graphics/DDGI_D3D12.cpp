/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "graphics/DDGI.h"

#ifdef GFX_PERF_MARKERS
#include <pix.h>
#endif

using namespace rtxgi;
using namespace rtxgi::d3d12;

#if (RTXGI_DDGI_BINDLESS_RESOURCES && RTXGI_DDGI_RESOURCE_MANAGEMENT)
#error RTXGI SDK DDGI Managed Mode is not compatible with bindless resources!
#endif

namespace Graphics
{
    namespace D3D12
    {
        namespace DDGI
        {

            //----------------------------------------------------------------------------------------------------------
            // DDGIVolume Resource Creation Functions (Unmanaged Mode)
            //----------------------------------------------------------------------------------------------------------

        #if !RTXGI_DDGI_RESOURCE_MANAGEMENT
            /**
             * Create the resources used by a DDGIVolume.
             */
            bool CreateDDGIVolumeResources(
                Globals& d3d,
                GlobalResources& d3dResources,
                Resources& resources,
                const DDGIVolumeDesc& volumeDesc,
                DDGIVolumeResources& volumeResources,
                std::vector<Shaders::ShaderProgram>& shaders,
                std::ofstream& log)
            {
                log << "\tCreating resources for DDGIVolume: \"" << volumeDesc.name << "\"...";
                std::flush(log);

                UINT arraySize = 0;
                UINT variabilityAverageArraySize = 0;

                // Create the texture arrays
                {
                    UINT width = 0;
                    UINT height = 0;
                    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

                    // Probe ray data texture
                    {
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::RayData, width, height, arraySize);
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::RayData, volumeDesc.probeRayDataFormat);

                        TextureDesc desc = { width, height, arraySize, 1, format, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS };
                        CHECK(CreateTexture(d3d, desc, &volumeResources.unmanaged.probeRayData), "create DDGIVolume ray data texture array!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Ray Data";
                        volumeResources.unmanaged.probeRayData->SetName(name.c_str());
                    #endif
                    }

                    // Probe irradiance texture
                    {
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::Irradiance, width, height, arraySize);
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Irradiance, volumeDesc.probeIrradianceFormat);

                        TextureDesc desc = { width, height, arraySize, 1, format, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET };
                        CHECK(CreateTexture(d3d, desc, &volumeResources.unmanaged.probeIrradiance), "create DDGIVolume probe irradiance texture array!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Irradiance";
                        volumeResources.unmanaged.probeIrradiance->SetName(name.c_str());
                    #endif
                    }

                    // Probe distance texture
                    {
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::Distance, width, height, arraySize);
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Distance, volumeDesc.probeDistanceFormat);

                        TextureDesc desc = { width, height, arraySize, 1, format, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET };
                        CHECK(CreateTexture(d3d, desc, &volumeResources.unmanaged.probeDistance), "create DDGIVolume probe distance texture array!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Distance";
                        volumeResources.unmanaged.probeDistance->SetName(name.c_str());
                    #endif
                    }

                    // Probe data texture
                    {
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::Data, width, height, arraySize);
                        if (width <= 0 || height <= 0) return false;
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Data, volumeDesc.probeDataFormat);

                        TextureDesc desc = { width, height, arraySize, 1, format, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS };
                        CHECK(CreateTexture(d3d, desc, &volumeResources.unmanaged.probeData), "create DDGIVolume probe data texture array!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Data";
                        volumeResources.unmanaged.probeData->SetName(name.c_str());
                    #endif
                    }

                    // Probe variability texture
                    {
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::Variability, width, height, arraySize);
                        if (width <= 0 || height <= 0) return false;
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Variability, volumeDesc.probeVariabilityFormat);

                        TextureDesc desc = { width, height, arraySize, 1, format, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS };
                        CHECK(CreateTexture(d3d, desc, &volumeResources.unmanaged.probeVariability), "create DDGIVolume Probe variability texture!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Variability";
                        volumeResources.unmanaged.probeVariability->SetName(name.c_str());
                    #endif
                    }

                    // Probe variability average
                    {
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::VariabilityAverage, width, height, variabilityAverageArraySize);
                        if (width <= 0 || height <= 0) return false;
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::VariabilityAverage, volumeDesc.probeVariabilityFormat);

                        TextureDesc desc = { width, height, variabilityAverageArraySize, 1, format, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS };
                        CHECK(CreateTexture(d3d, desc, &volumeResources.unmanaged.probeVariabilityAverage), "create DDGIVolume Probe variability average texture!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Variability Average";
                        volumeResources.unmanaged.probeVariabilityAverage->SetName(name.c_str());
                    #endif
                        BufferDesc readbackDesc = { sizeof(float)*2, 0, EHeapType::READBACK, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE };
                        CHECK(CreateBuffer(d3d, readbackDesc, &volumeResources.unmanaged.probeVariabilityReadback), "create DDGIVolume Probe variability readback buffer!", log);
                    #ifdef GFX_NAME_OBJECTS
                        name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Variability Readback";
                        volumeResources.unmanaged.probeVariabilityReadback->SetName(name.c_str());
                    #endif
                    }
                }

                // Create the resource descriptors
                {
                    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle, uavHandle;
                    D3D12_CPU_DESCRIPTOR_HANDLE heapStart = volumeResources.descriptorHeap.resources->GetCPUDescriptorHandleForHeapStart();
                    D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapStart = resources.rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

                    UINT rtvDescSize = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

                    // Describe resource views (RTVs are used for clearing the texture arrays)
                    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
                    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                    rtvDesc.Texture2DArray.ArraySize = arraySize;

                    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                    uavDesc.Texture2DArray.ArraySize = arraySize;

                    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                    srvDesc.Texture2DArray.ArraySize = arraySize;
                    srvDesc.Texture2DArray.MipLevels = 1;
                    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

                    // Get the volume's descriptor heap info
                    const DDGIVolumeDescriptorHeapDesc& heapDesc = volumeResources.descriptorHeap;

                    // Get the volume's resource indices (on the descriptor heap)
                    const DDGIVolumeResourceIndices& resourceIndices = heapDesc.resourceIndices;

                    // Ray Data texture descriptors
                    {
                        uavHandle.ptr = heapStart.ptr + (resourceIndices.rayDataUAVIndex * heapDesc.entrySize);
                        srvHandle.ptr = heapStart.ptr + (resourceIndices.rayDataSRVIndex * heapDesc.entrySize);

                        srvDesc.Format = uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::RayData, volumeDesc.probeRayDataFormat);
                        d3d.device->CreateUnorderedAccessView(volumeResources.unmanaged.probeRayData, nullptr, &uavDesc, uavHandle);
                        d3d.device->CreateShaderResourceView(volumeResources.unmanaged.probeRayData, &srvDesc, srvHandle);
                    }

                    // Probe Irradiance texture array descriptors
                    {
                        uavHandle.ptr = heapStart.ptr + (resourceIndices.probeIrradianceUAVIndex * heapDesc.entrySize);
                        srvHandle.ptr = heapStart.ptr + (resourceIndices.probeIrradianceSRVIndex * heapDesc.entrySize);
                        volumeResources.unmanaged.probeIrradianceRTV.ptr = rtvHeapStart.ptr + (volumeDesc.index * GetDDGIVolumeNumRTVDescriptors() * rtvDescSize);

                        srvDesc.Format = uavDesc.Format = rtvDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Irradiance, volumeDesc.probeIrradianceFormat);
                        d3d.device->CreateUnorderedAccessView(volumeResources.unmanaged.probeIrradiance, nullptr, &uavDesc, uavHandle);
                        d3d.device->CreateShaderResourceView(volumeResources.unmanaged.probeIrradiance, &srvDesc, srvHandle);
                        d3d.device->CreateRenderTargetView(volumeResources.unmanaged.probeIrradiance, &rtvDesc, volumeResources.unmanaged.probeIrradianceRTV);
                    }

                    // Probe Distance texture array descriptors
                    {
                        uavHandle.ptr = heapStart.ptr + (resourceIndices.probeDistanceUAVIndex * heapDesc.entrySize);
                        srvHandle.ptr = heapStart.ptr + (resourceIndices.probeDistanceSRVIndex * heapDesc.entrySize);
                        volumeResources.unmanaged.probeDistanceRTV.ptr = rtvHeapStart.ptr + ((volumeDesc.index * GetDDGIVolumeNumRTVDescriptors() + 1) * rtvDescSize);

                        srvDesc.Format = uavDesc.Format = rtvDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Distance, volumeDesc.probeDistanceFormat);
                        d3d.device->CreateUnorderedAccessView(volumeResources.unmanaged.probeDistance, nullptr, &uavDesc, uavHandle);
                        d3d.device->CreateShaderResourceView(volumeResources.unmanaged.probeDistance, &srvDesc, srvHandle);
                        d3d.device->CreateRenderTargetView(volumeResources.unmanaged.probeDistance, &rtvDesc, volumeResources.unmanaged.probeDistanceRTV);
                    }

                    // Probe Data texture array descriptors
                    {
                        uavHandle.ptr = heapStart.ptr + (resourceIndices.probeDataUAVIndex * heapDesc.entrySize);
                        srvHandle.ptr = heapStart.ptr + (resourceIndices.probeDataSRVIndex * heapDesc.entrySize);

                        srvDesc.Format = uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Data, volumeDesc.probeDataFormat);
                        d3d.device->CreateUnorderedAccessView(volumeResources.unmanaged.probeData, nullptr, &uavDesc, uavHandle);
                        d3d.device->CreateShaderResourceView(volumeResources.unmanaged.probeData, &srvDesc, srvHandle);
                    }

                    // Probe variability texture descriptors
                    {
                        uavHandle.ptr = heapStart.ptr + (resourceIndices.probeVariabilityUAVIndex * heapDesc.entrySize);
                        srvHandle.ptr = heapStart.ptr + (resourceIndices.probeVariabilitySRVIndex * heapDesc.entrySize);

                        srvDesc.Format = uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Variability, volumeDesc.probeVariabilityFormat);
                        d3d.device->CreateUnorderedAccessView(volumeResources.unmanaged.probeVariability, nullptr, &uavDesc, uavHandle);
                        d3d.device->CreateShaderResourceView(volumeResources.unmanaged.probeVariability, &srvDesc, srvHandle);
                    }

                    // Probe variability average texture descriptors
                    {
                        uavHandle.ptr = heapStart.ptr + (resourceIndices.probeVariabilityAverageUAVIndex * heapDesc.entrySize);
                        srvHandle.ptr = heapStart.ptr + (resourceIndices.probeVariabilityAverageSRVIndex * heapDesc.entrySize);

                        srvDesc.Format = uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::VariabilityAverage, volumeDesc.probeVariabilityFormat);
                        srvDesc.Texture2DArray.ArraySize = uavDesc.Texture2DArray.ArraySize = variabilityAverageArraySize;
                        d3d.device->CreateUnorderedAccessView(volumeResources.unmanaged.probeVariabilityAverage, nullptr, &uavDesc, uavHandle);
                        d3d.device->CreateShaderResourceView(volumeResources.unmanaged.probeVariabilityAverage, &srvDesc, srvHandle);
                    }
                }

                // Set or create the root signature
                {
            #if RTXGI_DDGI_BINDLESS_RESOURCES
                // Pass a pointer to the global root signature
                volumeResources.unmanaged.rootSignature = d3dResources.rootSignature;
            #else
                // Create the volume's root signature (when not using bindless and the global root signature)
                ID3DBlob* signature = nullptr;
                if (!GetDDGIVolumeRootSignatureDesc(volumeResources.descriptorHeap, signature)) return false;

                D3DCHECK(d3d.device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&volumeResources.unmanaged.rootSignature)));
                SAFE_RELEASE(signature);
                #ifdef GFX_NAME_OBJECTS
                    std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Root Signature ";
                    volumeResources.unmanaged.rootSignature->SetName(name.c_str());
                #endif
            #endif
                }

                // Create the pipeline state objects
                {
                    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
                    desc.pRootSignature = volumeResources.unmanaged.rootSignature;

                    UINT shaderIndex = 0;

                    // Probe Irradiance Blending PSO
                    {
                        desc.CS.BytecodeLength = shaders[shaderIndex].bytecode->GetBufferSize();
                        desc.CS.pShaderBytecode = shaders[shaderIndex].bytecode->GetBufferPointer();
                        D3DCHECK(d3d.device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&volumeResources.unmanaged.probeBlendingIrradiancePSO)));
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Irradiance Blending PSO";
                        volumeResources.unmanaged.probeBlendingIrradiancePSO->SetName(name.c_str());
                    #endif
                        shaderIndex++;
                    }

                    // Probe Distance Blending PSO
                    {
                        desc.CS.BytecodeLength = shaders[shaderIndex].bytecode->GetBufferSize();
                        desc.CS.pShaderBytecode = shaders[shaderIndex].bytecode->GetBufferPointer();
                        D3DCHECK(d3d.device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&volumeResources.unmanaged.probeBlendingDistancePSO)));
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Distance Blending PSO";
                        volumeResources.unmanaged.probeBlendingDistancePSO->SetName(name.c_str());
                    #endif
                        shaderIndex++;
                    }

                    // Probe Relocation PSO
                    {
                        desc.CS.BytecodeLength = shaders[shaderIndex].bytecode->GetBufferSize();
                        desc.CS.pShaderBytecode = shaders[shaderIndex].bytecode->GetBufferPointer();
                        D3DCHECK(d3d.device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&volumeResources.unmanaged.probeRelocation.updatePSO)));
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Relocation PSO";
                        volumeResources.unmanaged.probeRelocation.updatePSO->SetName(name.c_str());
                    #endif
                        shaderIndex++;
                    }

                    // Probe Relocation Reset PSO
                    {
                        desc.CS.BytecodeLength = shaders[shaderIndex].bytecode->GetBufferSize();
                        desc.CS.pShaderBytecode = shaders[shaderIndex].bytecode->GetBufferPointer();
                        D3DCHECK(d3d.device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&volumeResources.unmanaged.probeRelocation.resetPSO)));
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Relocation Reset PSO";
                        volumeResources.unmanaged.probeRelocation.resetPSO->SetName(name.c_str());
                    #endif
                        shaderIndex++;
                    }

                    // Probe Classification PSO
                    {
                        desc.CS.BytecodeLength = shaders[shaderIndex].bytecode->GetBufferSize();
                        desc.CS.pShaderBytecode = shaders[shaderIndex].bytecode->GetBufferPointer();
                        D3DCHECK(d3d.device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&volumeResources.unmanaged.probeClassification.updatePSO)));
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Classification PSO";
                        volumeResources.unmanaged.probeClassification.updatePSO->SetName(name.c_str());
                    #endif
                        shaderIndex++;
                    }

                    // Probe Classification Reset PSO
                    {
                        desc.CS.BytecodeLength = shaders[shaderIndex].bytecode->GetBufferSize();
                        desc.CS.pShaderBytecode = shaders[shaderIndex].bytecode->GetBufferPointer();
                        D3DCHECK(d3d.device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&volumeResources.unmanaged.probeClassification.resetPSO)));
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Classification Reset PSO";
                        volumeResources.unmanaged.probeClassification.resetPSO->SetName(name.c_str());
                    #endif
                        shaderIndex++;
                    }

                    // Probe Variability Reduction PSO
                    {
                        desc.CS.BytecodeLength = shaders[shaderIndex].bytecode->GetBufferSize();
                        desc.CS.pShaderBytecode = shaders[shaderIndex].bytecode->GetBufferPointer();
                        D3DCHECK(d3d.device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&volumeResources.unmanaged.probeVariabilityPSOs.reductionPSO)));
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Variability Reduction PSO";
                        volumeResources.unmanaged.probeVariabilityPSOs.reductionPSO->SetName(name.c_str());
                    #endif
                        shaderIndex++;

                    }

                    // Probe Variability Extra Reduction PSO
                    {
                        desc.CS.BytecodeLength = shaders[shaderIndex].bytecode->GetBufferSize();
                        desc.CS.pShaderBytecode = shaders[shaderIndex].bytecode->GetBufferPointer();
                        D3DCHECK(d3d.device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&volumeResources.unmanaged.probeVariabilityPSOs.extraReductionPSO)));
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Variability Extra Reduction PSO";
                        volumeResources.unmanaged.probeVariabilityPSOs.extraReductionPSO->SetName(name.c_str());
                    #endif
                    }
                }

                log << "done.";
                std::flush(log);
                return true;
            }

            /**
             * Release resources used by a DDGIVolume.
             */
            void DestroyDDGIVolumeResources(Resources& resources, size_t volumeIndex)
            {
                // Get the volume
                DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[volumeIndex]);

            #if !RTXGI_DDGI_BINDLESS_RESOURCES
                if (volume->GetRootSignature()) volume->GetRootSignature()->Release();
            #endif

                // Release textures
                if (volume->GetProbeRayData()) volume->GetProbeRayData()->Release();
                if (volume->GetProbeIrradiance()) volume->GetProbeIrradiance()->Release();
                if (volume->GetProbeDistance()) volume->GetProbeDistance()->Release();
                if (volume->GetProbeData()) volume->GetProbeData()->Release();
                if (volume->GetProbeVariability()) volume->GetProbeVariability()->Release();
                if (volume->GetProbeVariabilityAverage()) volume->GetProbeVariabilityAverage()->Release();
                if (volume->GetProbeVariabilityReadback()) volume->GetProbeVariabilityReadback()->Release();

                // Release PSOs
                if (volume->GetProbeBlendingIrradiancePSO()) volume->GetProbeBlendingIrradiancePSO()->Release();
                if (volume->GetProbeBlendingDistancePSO()) volume->GetProbeBlendingDistancePSO()->Release();
                if (volume->GetProbeRelocationPSO()) volume->GetProbeRelocationPSO()->Release();
                if (volume->GetProbeRelocationResetPSO()) volume->GetProbeRelocationResetPSO()->Release();
                if (volume->GetProbeClassificationPSO()) volume->GetProbeClassificationPSO()->Release();
                if (volume->GetProbeClassificationResetPSO()) volume->GetProbeClassificationResetPSO()->Release();
                if (volume->GetProbeVariabilityReductionPSO()) volume->GetProbeVariabilityReductionPSO()->Release();
                if (volume->GetProbeVariabilityExtraReductionPSO()) volume->GetProbeVariabilityExtraReductionPSO()->Release();

                // Clear pointers
                volume->Destroy();
            }
        #endif // !RTXGI_DDGI_RESOURCE_MANAGEMENT

            //----------------------------------------------------------------------------------------------------------
            // DDGIVolume Creation Helper Functions
            //----------------------------------------------------------------------------------------------------------

            /**
             * Populates a DDGIVolumeDesc structure from configuration data.
             */
            void GetDDGIVolumeDesc(const Configs::DDGIVolume& config, DDGIVolumeDesc& volumeDesc)
            {
                size_t size = config.name.size();
                volumeDesc.name = new char[size + 1];
                memset(volumeDesc.name, 0, size + 1);
                memcpy(volumeDesc.name, config.name.c_str(), size);

                volumeDesc.index = config.index;
                volumeDesc.rngSeed = config.rngSeed;
                volumeDesc.origin = { config.origin.x, config.origin.y, config.origin.z };
                volumeDesc.eulerAngles = { config.eulerAngles.x, config.eulerAngles.y, config.eulerAngles.z, };
                volumeDesc.probeSpacing = { config.probeSpacing.x, config.probeSpacing.y, config.probeSpacing.z };
                volumeDesc.probeCounts = { config.probeCounts.x, config.probeCounts.y, config.probeCounts.z, };
                volumeDesc.probeNumRays = config.probeNumRays;
                volumeDesc.probeNumIrradianceTexels = config.probeNumIrradianceTexels;
                volumeDesc.probeNumIrradianceInteriorTexels = (config.probeNumIrradianceTexels - 2);
                volumeDesc.probeNumDistanceTexels = config.probeNumDistanceTexels;
                volumeDesc.probeNumDistanceInteriorTexels = (config.probeNumDistanceTexels - 2);
                volumeDesc.probeHysteresis = config.probeHysteresis;
                volumeDesc.probeNormalBias = config.probeNormalBias;
                volumeDesc.probeViewBias = config.probeViewBias;
                volumeDesc.probeMaxRayDistance = config.probeMaxRayDistance;
                volumeDesc.probeIrradianceThreshold = config.probeIrradianceThreshold;
                volumeDesc.probeBrightnessThreshold = config.probeBrightnessThreshold;

                volumeDesc.showProbes = config.showProbes;
                volumeDesc.probeVisType = config.probeVisType;

                volumeDesc.probeRayDataFormat = config.textureFormats.rayDataFormat;
                volumeDesc.probeIrradianceFormat = config.textureFormats.irradianceFormat;
                volumeDesc.probeDistanceFormat = config.textureFormats.distanceFormat;
                volumeDesc.probeDataFormat = config.textureFormats.dataFormat;
                volumeDesc.probeVariabilityFormat = config.textureFormats.variabilityFormat;

                volumeDesc.probeRelocationEnabled = config.probeRelocationEnabled;
                volumeDesc.probeMinFrontfaceDistance = config.probeMinFrontfaceDistance;
                volumeDesc.probeClassificationEnabled = config.probeClassificationEnabled;
                volumeDesc.probeVariabilityEnabled = config.probeVariabilityEnabled;

                if (config.infiniteScrollingEnabled) volumeDesc.movementType = EDDGIVolumeMovementType::Scrolling;
                else volumeDesc.movementType = EDDGIVolumeMovementType::Default;
            }

            /**
             * Populates a DDGIVolumeResource structure.
             * In unmanaged resource mode, the application creates DDGIVolume graphics resources in CreateDDGIVolumeResources().
             * In managed resource mode, the RTXGI SDK creates DDGIVolume graphics resources.
             */
            bool GetDDGIVolumeResources(
                Globals& d3d,
                GlobalResources& d3dResources,
                Resources& resources,
                const DDGIVolumeDesc& volumeDesc,
                DDGIVolumeResources& volumeResources,
                std::vector<Shaders::ShaderProgram>& volumeShaders,
                std::ofstream& log)
            {
                std::string msg;

                // Load and compile the volume's shaders
                msg = "failed to compile shaders for DDGIVolume[" + std::to_string(volumeDesc.index) + "] (\"" + volumeDesc.name + "\")!\n";
                CHECK(Graphics::DDGI::CompileDDGIVolumeShaders(d3d, volumeDesc, volumeShaders, false, log), msg.c_str(), log);

                // Set the descriptor heap pointer and entry size
                DDGIVolumeDescriptorHeapDesc& descHeap = volumeResources.descriptorHeap;
                descHeap.resources = d3dResources.srvDescHeap;
            #if RTXGI_DDGI_BINDLESS_RESOURCES
                descHeap.samplers = d3dResources.samplerDescHeap;
            #endif
                descHeap.entrySize = d3dResources.srvDescHeapEntrySize;

                // Set the volume's resource indices on the descriptor heap
                descHeap.constantsIndex = DescriptorHeapOffsets::STB_DDGI_VOLUME_CONSTS;
                descHeap.resourceIndicesIndex = DescriptorHeapOffsets::STB_DDGI_VOLUME_RESOURCE_INDICES;
                descHeap.resourceIndices.rayDataUAVIndex = DescriptorHeapOffsets::UAV_DDGI_VOLUME_TEX2DARRAY + (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors());
                descHeap.resourceIndices.rayDataSRVIndex = DescriptorHeapOffsets::SRV_DDGI_VOLUME_TEX2DARRAY + (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors());
                descHeap.resourceIndices.probeIrradianceUAVIndex = DescriptorHeapOffsets::UAV_DDGI_VOLUME_TEX2DARRAY + (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 1;
                descHeap.resourceIndices.probeIrradianceSRVIndex = DescriptorHeapOffsets::SRV_DDGI_VOLUME_TEX2DARRAY + (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 1;
                descHeap.resourceIndices.probeDistanceUAVIndex = DescriptorHeapOffsets::UAV_DDGI_VOLUME_TEX2DARRAY + (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 2;
                descHeap.resourceIndices.probeDistanceSRVIndex = DescriptorHeapOffsets::SRV_DDGI_VOLUME_TEX2DARRAY + (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 2;
                descHeap.resourceIndices.probeDataUAVIndex = DescriptorHeapOffsets::UAV_DDGI_VOLUME_TEX2DARRAY + (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 3;
                descHeap.resourceIndices.probeDataSRVIndex = DescriptorHeapOffsets::SRV_DDGI_VOLUME_TEX2DARRAY + (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 3;
                descHeap.resourceIndices.probeVariabilityUAVIndex = DescriptorHeapOffsets::UAV_DDGI_VOLUME_TEX2DARRAY + (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 4;
                descHeap.resourceIndices.probeVariabilitySRVIndex = DescriptorHeapOffsets::SRV_DDGI_VOLUME_TEX2DARRAY + (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 4;
                descHeap.resourceIndices.probeVariabilityAverageUAVIndex = DescriptorHeapOffsets::UAV_DDGI_VOLUME_TEX2DARRAY + (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 5;
                descHeap.resourceIndices.probeVariabilityAverageSRVIndex = DescriptorHeapOffsets::SRV_DDGI_VOLUME_TEX2DARRAY + (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 5;

                // Set the volume constants structured buffer pointers and size
                volumeResources.constantsBuffer = resources.volumeConstantsSTB;
                volumeResources.constantsBufferUpload = resources.volumeConstantsSTBUpload;
                volumeResources.constantsBufferSizeInBytes = resources.volumeConstantsSTBSizeInBytes;

                // The Test Harness *always* accesses resources bindlessly when ray tracing, see RayTraceVolume().
                // We store the bindless resource indices with the DDGIVolume so we can use the UploadDDGIVolumeResourceIndices() helper
                // function to transfer the bindless indices to the GPU. SDK shaders ignore these values when not in bindless mode.
            #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS
                volumeResources.bindless.type = EBindlessType::RESOURCE_ARRAYS;
            #elif RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP
                volumeResources.bindless.type = EBindlessType::DESCRIPTOR_HEAP;
            #endif

                // Regardless of what the host application chooses for resource binding, all SDK shaders can operate in either bound or bindless modes
                volumeResources.bindless.enabled = (bool)RTXGI_DDGI_BINDLESS_RESOURCES;

                // Set the resource indices structured buffer pointers and size
                volumeResources.bindless.resourceIndicesBuffer = resources.volumeResourceIndicesSTB;
                volumeResources.bindless.resourceIndicesBufferUpload = resources.volumeResourceIndicesSTBUpload;
                volumeResources.bindless.resourceIndicesBufferSizeInBytes = resources.volumeResourceIndicesSTBSizeInBytes;

                // Set the resource array indices of volume resources
                DDGIVolumeResourceIndices& resourceIndices = volumeResources.bindless.resourceIndices;
                resourceIndices.rayDataUAVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors());
                resourceIndices.rayDataSRVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors());
                resourceIndices.probeIrradianceUAVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 1;
                resourceIndices.probeIrradianceSRVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 1;
                resourceIndices.probeDistanceUAVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 2;
                resourceIndices.probeDistanceSRVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 2;
                resourceIndices.probeDataUAVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 3;
                resourceIndices.probeDataSRVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 3;
                resourceIndices.probeVariabilityUAVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 4;
                resourceIndices.probeVariabilitySRVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 4;
                resourceIndices.probeVariabilityAverageUAVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 5;
                resourceIndices.probeVariabilityAverageSRVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 5;

            #if RTXGI_DDGI_RESOURCE_MANAGEMENT
                // Enable "Managed Mode", the RTXGI SDK creates graphics objects
                volumeResources.managed.enabled = true;

                // Pass the D3D device to use for resource creation
                volumeResources.managed.device = d3d.device;

                // Pass compiled shader bytecode
                assert(volumeShaders.size() >= 2);
                volumeResources.managed.probeBlendingIrradianceCS = { volumeShaders[0].bytecode->GetBufferPointer(), volumeShaders[0].bytecode->GetBufferSize() };
                volumeResources.managed.probeBlendingDistanceCS = { volumeShaders[1].bytecode->GetBufferPointer(), volumeShaders[1].bytecode->GetBufferSize() };

                assert(volumeShaders.size() >= 4);
                volumeResources.managed.probeRelocation.updateCS = { volumeShaders[2].bytecode->GetBufferPointer(), volumeShaders[2].bytecode->GetBufferSize() };
                volumeResources.managed.probeRelocation.resetCS = { volumeShaders[3].bytecode->GetBufferPointer(), volumeShaders[3].bytecode->GetBufferSize() };

                assert(volumeShaders.size() >= 6);
                volumeResources.managed.probeClassification.updateCS = { volumeShaders[4].bytecode->GetBufferPointer(), volumeShaders[4].bytecode->GetBufferSize() };
                volumeResources.managed.probeClassification.resetCS = { volumeShaders[5].bytecode->GetBufferPointer(), volumeShaders[5].bytecode->GetBufferSize() };

                assert(volumeShaders.size() == 8);
                volumeResources.managed.probeVariability.reductionCS = { volumeShaders[6].bytecode->GetBufferPointer(), volumeShaders[6].bytecode->GetBufferSize() };
                volumeResources.managed.probeVariability.extraReductionCS = { volumeShaders[7].bytecode->GetBufferPointer(), volumeShaders[7].bytecode->GetBufferSize() };
            #else
                // Enable "Unmanaged Mode", the application creates graphics objects
                volumeResources.unmanaged.enabled = true;

                #if RTXGI_DDGI_BINDLESS_RESOURCES
                    // Root parameter slot locations in the application's root signature
                    // See Direct3D12.cpp::CreateGlobalRootSignature()
                    volumeResources.unmanaged.rootParamSlotRootConstants = 1;
                    volumeResources.unmanaged.rootParamSlotSamplerDescriptorTable = 2;
                    volumeResources.unmanaged.rootParamSlotResourceDescriptorTable = 3;
                #else
                    // Root parameter slot locations in the RTXGI DDGIVolume root signature
                    // See DDGIVolume_D3D12.cpp::GetDDGIVolumeRootSignatureDesc()
                    volumeResources.unmanaged.rootParamSlotRootConstants = 0;
                    volumeResources.unmanaged.rootParamSlotResourceDescriptorTable = 1;
                #endif

                // Create the volume's resources
                msg = "failed to create resources for DDGIVolume[" + std::to_string(volumeDesc.index) + "] (\"" + volumeDesc.name + "\")!\n";
                CHECK(CreateDDGIVolumeResources(d3d, d3dResources, resources, volumeDesc, volumeResources, volumeShaders, log), msg.c_str(), log);
            #endif

                return true;
            }

            /**
             * Create a DDGIVolume.
             */
            bool CreateDDGIVolume(
                Globals& d3d,
                GlobalResources& d3dResources,
                Resources& resources,
                const Configs::DDGIVolume& volumeConfig,
                std::ofstream& log)
            {
                // Destroy the volume if one already exists at the given index
                if (volumeConfig.index < static_cast<UINT>(resources.volumes.size()))
                {
                    if (resources.volumes[volumeConfig.index])
                    {
                    #if RTXGI_DDGI_RESOURCE_MANAGEMENT
                        resources.volumes[volumeConfig.index]->Destroy();
                    #else
                        DestroyDDGIVolumeResources(resources, volumeConfig.index);
                    #endif
                        SAFE_DELETE(resources.volumeDescs[volumeConfig.index].name);
                        SAFE_DELETE(resources.volumes[volumeConfig.index]);
                        resources.numVolumeVariabilitySamples[volumeConfig.index] = 0;
                    }
                }
                else
                {
                    resources.volumeDescs.emplace_back();
                    resources.volumes.emplace_back();
                    resources.numVolumeVariabilitySamples.emplace_back();
                }

                // Describe the DDGIVolume's properties
                DDGIVolumeDesc& volumeDesc = resources.volumeDescs[volumeConfig.index];
                GetDDGIVolumeDesc(volumeConfig, volumeDesc);

                // Describe the DDGIVolume's resources and shaders
                DDGIVolumeResources volumeResources;
                std::vector<Shaders::ShaderProgram> volumeShaders;
                GetDDGIVolumeResources(d3d, d3dResources, resources, volumeDesc, volumeResources, volumeShaders, log);

                // Create a new DDGIVolume
                DDGIVolume* volume = new DDGIVolume();
                ERTXGIStatus status = volume->Create(volumeDesc, volumeResources);
                if (status != ERTXGIStatus::OK)
                {
                    log << "\nError: failed to create the DDGIVolume!";
                    std::flush(log);
                    return false;
                }

                // Store the volume's pointer
                resources.volumes[volumeConfig.index] = volume;

                // Release the volume's shader bytecode
                for (size_t shaderIndex = 0; shaderIndex < volumeShaders.size(); shaderIndex++)
                {
                    volumeShaders[shaderIndex].Release();
                }
                volumeShaders.clear();

                return true;
            }

            /**
             * Creates the RTV descriptor heap for all DDGIVolumes.
             */
            bool CreateRTVDescriptorHeap(Globals& d3d, Resources& resources, UINT volumeCount)
            {
                // Describe the RTV heap
                D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
                heapDesc.NumDescriptors = volumeCount * GetDDGIVolumeNumRTVDescriptors();
                heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

                // Create the RTV heap
                HRESULT hr = d3d.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&resources.rtvDescriptorHeap));
                if (FAILED(hr)) return false;
            #ifdef RTXGI_GFX_NAME_OBJECTS
                resources.rtvDescriptorHeap->SetName(L"DDGI RTV Descriptor Heap");
            #endif

                return true;
            }

            /**
             * Creates the DDGIVolume resource indices structured buffer.
             */
            bool CreateDDGIVolumeResourceIndicesBuffer(Globals& d3d, GlobalResources& d3dResources, Resources& resources, UINT volumeCount, std::ofstream& log)
            {
                resources.volumeResourceIndicesSTBSizeInBytes = sizeof(DDGIVolumeResourceIndices) * volumeCount;
                if (resources.volumeResourceIndicesSTBSizeInBytes == 0) return true; // scenes with no DDGIVolumes are valid

                // Create the DDGIVolume resource indices upload buffer resource (double buffered)
                BufferDesc desc = { 2 * resources.volumeResourceIndicesSTBSizeInBytes, 0, EHeapType::UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE };
                CHECK(CreateBuffer(d3d, desc, &resources.volumeResourceIndicesSTBUpload), "create DDGIVolume resource indices upload structured buffer!\n", log);
            #ifdef GFX_NAME_OBJECTS
                resources.volumeResourceIndicesSTBUpload->SetName(L"DDGIVolume Resource Indices Upload Structured Buffer");
            #endif

                // Create the DDGIVolume resource indices device buffer resource
                desc = { resources.volumeResourceIndicesSTBSizeInBytes, 0, EHeapType::DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE };
                CHECK(CreateBuffer(d3d, desc, &resources.volumeResourceIndicesSTB), "create DDGIVolume resource indices structured buffer!\n", log);
            #ifdef GFX_NAME_OBJECTS
                resources.volumeResourceIndicesSTB->SetName(L"DDGIVolume Resource Indices Structured Buffer");
            #endif

                // Add the resource indices structured buffer SRV to the descriptor heap
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.Format = DXGI_FORMAT_UNKNOWN;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                srvDesc.Buffer.NumElements = volumeCount;
                srvDesc.Buffer.StructureByteStride = sizeof(DDGIVolumeResourceIndices);
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

                D3D12_CPU_DESCRIPTOR_HANDLE handle;
                handle.ptr = d3dResources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::STB_DDGI_VOLUME_RESOURCE_INDICES * d3dResources.srvDescHeapEntrySize);
                d3d.device->CreateShaderResourceView(resources.volumeResourceIndicesSTB, &srvDesc, handle);

                return true;
            }

            /**
             * Creates the DDGIVolume constants structured buffer.
             */
            bool CreateDDGIVolumeConstantsBuffer(Globals& d3d, GlobalResources& d3dResources, Resources& resources, UINT volumeCount, std::ofstream& log)
            {
                resources.volumeConstantsSTBSizeInBytes = sizeof(DDGIVolumeDescGPUPacked) * volumeCount;
                if (resources.volumeConstantsSTBSizeInBytes == 0) return true; // scenes with no DDGIVolumes are valid

                // Create the DDGIVolume constants upload buffer resource (double buffered)
                BufferDesc desc = { 2 * resources.volumeConstantsSTBSizeInBytes, 0, EHeapType::UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE };
                CHECK(CreateBuffer(d3d, desc, &resources.volumeConstantsSTBUpload), "create DDGIVolume constants upload structured buffer!\n", log);
            #ifdef GFX_NAME_OBJECTS
                resources.volumeConstantsSTBUpload->SetName(L"DDGIVolume Constants Upload Structured Buffer");
            #endif

                // Create the DDGIVolume constants device buffer resource
                desc = { resources.volumeConstantsSTBSizeInBytes, 0, EHeapType::DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE };
                CHECK(CreateBuffer(d3d, desc, &resources.volumeConstantsSTB), "create DDGIVolume constants structured buffer!\n", log);
            #ifdef GFX_NAME_OBJECTS
                resources.volumeConstantsSTB->SetName(L"DDGIVolume Constants Structured Buffer");
            #endif

            #if !RTXGI_DDGI_RESOURCE_MANAGEMENT
                // Add the constants structured buffer SRV to the descriptor heap
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.Format = DXGI_FORMAT_UNKNOWN;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                srvDesc.Buffer.NumElements = volumeCount;
                srvDesc.Buffer.StructureByteStride = sizeof(DDGIVolumeDescGPUPacked);
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

                D3D12_CPU_DESCRIPTOR_HANDLE handle;
                handle.ptr = d3dResources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::STB_DDGI_VOLUME_CONSTS * d3dResources.srvDescHeapEntrySize);
                d3d.device->CreateShaderResourceView(resources.volumeConstantsSTB, &srvDesc, handle);
            #endif

                return true;
            }

            //----------------------------------------------------------------------------------------------------------
            // Private Functions
            //----------------------------------------------------------------------------------------------------------

            bool CreateTextures(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
            {
                SAFE_RELEASE(resources.output);

                // Create the output (R16G16B16A16_FLOAT) texture resource
                TextureDesc desc = { static_cast<uint32_t>(d3d.width), static_cast<uint32_t>(d3d.height), 1, 1, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS };
                CHECK(CreateTexture(d3d, desc, &resources.output), "create DDGI output texture resource!\n", log);
            #ifdef GFX_NAME_OBJECTS
                resources.output->SetName(L"DDGI Output");
            #endif

                // Add the DDGIOutput texture UAV to the descriptor heap
                D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                uavDesc.Format = desc.format;

                D3D12_CPU_DESCRIPTOR_HANDLE handle;
                handle.ptr = d3dResources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::UAV_DDGI_OUTPUT * d3dResources.srvDescHeapEntrySize);
                d3d.device->CreateUnorderedAccessView(resources.output, nullptr, &uavDesc, handle);

                return true;
            }

            bool LoadAndCompileShaders(Globals& d3d, Resources& resources, UINT numVolumes, std::ofstream& log)
            {
                // Release existing shaders
                resources.rtShaders.Release();
                resources.indirectCS.Release();

                std::wstring root = std::wstring(d3d.shaderCompiler.root.begin(), d3d.shaderCompiler.root.end());

                // Load and compile the ray generation shader
                {
                    resources.rtShaders.rgs.filepath = root + L"shaders/ddgi/ProbeTraceRGS.hlsl";
                    resources.rtShaders.rgs.entryPoint = L"RayGen";
                    resources.rtShaders.rgs.exportName = L"DDGIProbeTraceRGS";
                    Shaders::AddDefine(resources.rtShaders.rgs, L"GFX_NVAPI", std::to_wstring(1));
                    Shaders::AddDefine(resources.rtShaders.rgs, L"CONSTS_REGISTER", L"b0");   // for DDGIRootConstants, see Direct3D12.cpp::CreateGlobalRootSignature(...)
                    Shaders::AddDefine(resources.rtShaders.rgs, L"CONSTS_SPACE", L"space1");  // for DDGIRootConstants, see Direct3D12.cpp::CreateGlobalRootSignature(...)
                    Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                    Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_COORDINATE_SYSTEM", std::to_wstring(RTXGI_COORDINATE_SYSTEM));
                    CHECK(Shaders::Compile(d3d.shaderCompiler, resources.rtShaders.rgs, true), "compile DDGI probe tracing ray generation shader!\n", log);
                }

                // Load and compile the miss shader
                {
                    resources.rtShaders.miss.filepath = root + L"shaders/Miss.hlsl";
                    resources.rtShaders.miss.entryPoint = L"Miss";
                    resources.rtShaders.miss.exportName = L"DDGIProbeTraceMiss";
                    Shaders::AddDefine(resources.rtShaders.miss, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                    CHECK(Shaders::Compile(d3d.shaderCompiler, resources.rtShaders.miss, true), "compile DDGI probe tracing miss shader!\n", log);
                }

                // Add the hit group
                {
                    resources.rtShaders.hitGroups.emplace_back();

                    Shaders::ShaderRTHitGroup& group = resources.rtShaders.hitGroups[0];
                    group.exportName = L"DDGIProbeTraceHitGroup";

                    // Load and compile the CHS
                    group.chs.filepath = root + L"shaders/CHS.hlsl";
                    group.chs.entryPoint = L"CHS_GI";
                    group.chs.exportName = L"DDGIProbeTraceCHS";
                    Shaders::AddDefine(group.chs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                    CHECK(Shaders::Compile(d3d.shaderCompiler, group.chs, true), "compile DDGI probe tracing closest hit shader!\n", log);

                    // Load and compile the AHS
                    group.ahs.filepath = root + L"shaders/AHS.hlsl";
                    group.ahs.entryPoint = L"AHS_GI";
                    group.ahs.exportName = L"DDGIProbeTraceAHS";
                    Shaders::AddDefine(group.ahs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                    CHECK(Shaders::Compile(d3d.shaderCompiler, group.ahs, true), "compile DDGI probe tracing any hit shader!\n", log);

                    // Set the payload size
                    resources.rtShaders.payloadSizeInBytes = sizeof(PackedPayload);
                }

                // Load and compile the indirect lighting compute shader
                {
                    std::wstring shaderPath = root + L"shaders/IndirectCS.hlsl";
                    resources.indirectCS.filepath = shaderPath.c_str();
                    resources.indirectCS.entryPoint = L"CS";
                    resources.indirectCS.targetProfile = L"cs_6_6";

                    Shaders::AddDefine(resources.indirectCS, L"CONSTS_REGISTER", L"b0");   // for DDGIRootConstants, see Direct3D12.cpp::CreateGlobalRootSignature(...)
                    Shaders::AddDefine(resources.indirectCS, L"CONSTS_SPACE", L"space1");  // for DDGIRootConstants, see Direct3D12.cpp::CreateGlobalRootSignature(...)
                    Shaders::AddDefine(resources.indirectCS, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                    Shaders::AddDefine(resources.indirectCS, L"RTXGI_COORDINATE_SYSTEM", std::to_wstring(RTXGI_COORDINATE_SYSTEM));
                    Shaders::AddDefine(resources.indirectCS, L"RTXGI_DDGI_NUM_VOLUMES", std::to_wstring(numVolumes));
                    Shaders::AddDefine(resources.indirectCS, L"THGP_DIM_X", L"8");
                    Shaders::AddDefine(resources.indirectCS, L"THGP_DIM_Y", L"4");
                    CHECK(Shaders::Compile(d3d.shaderCompiler, resources.indirectCS, true), "compile indirect lighting compute shader!\n", log);
                }

                return true;
            }

            bool CreatePSOs(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
            {
                // Release existing PSOs
                SAFE_RELEASE(resources.rtpso);
                SAFE_RELEASE(resources.rtpsoInfo);
                SAFE_RELEASE(resources.indirectPSO);

                // Create the RTPSO
                CHECK(CreateRayTracingPSO(
                    d3d.device,
                    d3dResources.rootSignature,
                    resources.rtShaders,
                    &resources.rtpso,
                    &resources.rtpsoInfo),
                    "create DDGI RTPSO!\n", log);

            #ifdef GFX_NAME_OBJECTS
                resources.rtpso->SetName(L"DDGI RTPSO");
            #endif

                CHECK(CreateComputePSO(
                    d3d.device,
                    d3dResources.rootSignature,
                    resources.indirectCS,
                    &resources.indirectPSO),
                    "create indirect lighting PSO!\n", log);
            #ifdef GFX_NAME_OBJECTS
                resources.indirectPSO->SetName(L"Indirect Lighting (DDGI) PSO");
            #endif

                return true;
            }

            bool CreateShaderTable(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
            {
                // The Shader Table layout is as follows:
                //    Entry 0:  DDGI Ray Generation Shader
                //    Entry 1:  DDGI Miss Shader
                //    Entry 2+: DDGI HitGroups
                // All shader records in the Shader Table must have the same size, so shader record size will be based on the largest required entry.
                // The entries must be aligned up to D3D12_RAYTRACING_SHADER_BINDING_TABLE_RECORD_BYTE_ALIGNMENT.
                // The CHS requires the largest entry:
                //   32 bytes for the shader identifier
                // +  8 bytes for descriptor table VA
                // +  8 bytes for sampler descriptor table VA
                // = 48 bytes ->> aligns to 64 bytes

                // Release the existing shader table
                resources.shaderTableSize = 0;
                SAFE_RELEASE(resources.shaderTable);

                uint32_t shaderIdSize = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;

                resources.shaderTableRecordSize = shaderIdSize;
                resources.shaderTableRecordSize += 8;              // descriptor table GPUVA
                resources.shaderTableRecordSize += 8;              // sampler descriptor table GPUVA
                resources.shaderTableRecordSize = ALIGN(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, resources.shaderTableRecordSize);

                // 2 + numHitGroups shader records in the table
                resources.shaderTableSize = (2 + static_cast<uint32_t>(resources.rtShaders.hitGroups.size())) * resources.shaderTableRecordSize;
                resources.shaderTableSize = ALIGN(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, resources.shaderTableSize);

                // Create the shader table upload buffer resource
                BufferDesc desc = { resources.shaderTableSize, 0, EHeapType::UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE };
                CHECK(CreateBuffer(d3d, desc, &resources.shaderTableUpload), "create DDGI shader table upload buffer!", log);
            #ifdef GFX_NAME_OBJECTS
                resources.shaderTableUpload->SetName(L"DDGI Shader Table Upload");
            #endif

                // Create the shader table buffer resource
                desc = { resources.shaderTableSize, 0, EHeapType::DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE };
                CHECK(CreateBuffer(d3d, desc, &resources.shaderTable), "create DDGI shader table!", log);
            #ifdef GFX_NAME_OBJECTS
                resources.shaderTable->SetName(L"DDGI Shader Table");
            #endif

                return true;
            }

            bool UpdateShaderTable(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
            {
                uint32_t shaderIdSize = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;

                // Write shader table records
                uint8_t* pData = nullptr;
                D3D12_RANGE readRange = {};
                if (FAILED(resources.shaderTableUpload->Map(0, &readRange, reinterpret_cast<void**>(&pData)))) return false;

                // Entry 0: Ray Generation Shader and descriptor heap pointer
                memcpy(pData, resources.rtpsoInfo->GetShaderIdentifier(resources.rtShaders.rgs.exportName.c_str()), shaderIdSize);
                *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = d3dResources.srvDescHeap->GetGPUDescriptorHandleForHeapStart();
                resources.shaderTableRGSStartAddress = resources.shaderTable->GetGPUVirtualAddress();

                // Entry 1: Miss Shader
                pData += resources.shaderTableRecordSize;
                memcpy(pData, resources.rtpsoInfo->GetShaderIdentifier(resources.rtShaders.miss.exportName.c_str()), shaderIdSize);
                resources.shaderTableMissTableStartAddress = resources.shaderTableRGSStartAddress + resources.shaderTableRecordSize;
                resources.shaderTableMissTableSize = resources.shaderTableRecordSize;

                // Entries 2+: Hit Groups and descriptor heap pointers
                for (uint32_t hitGroupIndex = 0; hitGroupIndex < static_cast<uint32_t>(resources.rtShaders.hitGroups.size()); hitGroupIndex++)
                {
                    pData += resources.shaderTableRecordSize;
                    memcpy(pData, resources.rtpsoInfo->GetShaderIdentifier(resources.rtShaders.hitGroups[hitGroupIndex].exportName), shaderIdSize);
                    *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = d3dResources.srvDescHeap->GetGPUDescriptorHandleForHeapStart();
                    *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize + 8) = d3dResources.samplerDescHeap->GetGPUDescriptorHandleForHeapStart();
                }
                resources.shaderTableHitGroupTableStartAddress = resources.shaderTableMissTableStartAddress + resources.shaderTableMissTableSize;
                resources.shaderTableHitGroupTableSize = static_cast<uint32_t>(resources.rtShaders.hitGroups.size()) * resources.shaderTableRecordSize;

                // Unmap
                resources.shaderTableUpload->Unmap(0, nullptr);

                // Schedule a copy of the upload buffer to the device buffer
                d3d.cmdList->CopyBufferRegion(resources.shaderTable, 0, resources.shaderTableUpload, 0, resources.shaderTableSize);

                // Transition the default heap resource to generic read after the copy is complete
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = resources.shaderTable;
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

                d3d.cmdList->ResourceBarrier(1, &barrier);

                return true;
            }

            void RayTraceVolumes(Globals& d3d, GlobalResources& d3dResources, Resources& resources)
            {
            #ifdef GFX_PERF_MARKERS
                PIXBeginEvent(d3d.cmdList, PIX_COLOR(GFX_PERF_MARKER_GREEN), "Ray Trace DDGIVolumes");
            #endif

                // Set the descriptor heaps
                ID3D12DescriptorHeap* ppHeaps[] = { d3dResources.srvDescHeap, d3dResources.samplerDescHeap };
                d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

                // Set the root signature
                d3d.cmdList->SetComputeRootSignature(d3dResources.rootSignature);

                // Update the root constants
                UINT offset = 0;
                GlobalConstants consts = d3dResources.constants;
                d3d.cmdList->SetComputeRoot32BitConstants(0, AppConsts::GetNum32BitValues(), consts.app.GetData(), offset);
                offset += AppConsts::GetAlignedNum32BitValues();
                d3d.cmdList->SetComputeRoot32BitConstants(0, PathTraceConsts::GetNum32BitValues(), consts.pt.GetData(), offset);
                offset += PathTraceConsts::GetAlignedNum32BitValues();
                d3d.cmdList->SetComputeRoot32BitConstants(0, LightingConsts::GetNum32BitValues(), consts.lights.GetData(), offset);

                // Set the root parameter descriptor tables
            #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS
                d3d.cmdList->SetComputeRootDescriptorTable(2, d3dResources.samplerDescHeap->GetGPUDescriptorHandleForHeapStart());
                d3d.cmdList->SetComputeRootDescriptorTable(3, d3dResources.srvDescHeap->GetGPUDescriptorHandleForHeapStart());
            #endif

                // Set the RTPSO
                d3d.cmdList->SetPipelineState1(resources.rtpso);

                // Describe the shader table
                D3D12_DISPATCH_RAYS_DESC desc = {};
                desc.RayGenerationShaderRecord.StartAddress = resources.shaderTableRGSStartAddress;
                desc.RayGenerationShaderRecord.SizeInBytes = resources.shaderTableRecordSize;

                desc.MissShaderTable.StartAddress = resources.shaderTableMissTableStartAddress;
                desc.MissShaderTable.SizeInBytes = resources.shaderTableMissTableSize;
                desc.MissShaderTable.StrideInBytes = resources.shaderTableRecordSize;

                desc.HitGroupTable.StartAddress = resources.shaderTableHitGroupTableStartAddress;
                desc.HitGroupTable.SizeInBytes = resources.shaderTableHitGroupTableSize;
                desc.HitGroupTable.StrideInBytes = resources.shaderTableRecordSize;

                // Barriers
                std::vector<D3D12_RESOURCE_BARRIER> barriers;
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;

                // Trace probe rays for each volume
                for(UINT volumeIndex = 0; volumeIndex < static_cast<UINT>(resources.selectedVolumes.size()); volumeIndex++)
                {
                    // Get the volume
                    const DDGIVolume* volume = resources.selectedVolumes[volumeIndex];

                    // Update the root constants
                    d3d.cmdList->SetComputeRoot32BitConstants(1, DDGIRootConstants::GetNum32BitValues(), volume->GetRootConstants().GetData(), 0);

                    // Get the ray dispatch dimensions
                    volume->GetRayDispatchDimensions(desc.Width, desc.Height, desc.Depth);

                    // Dispatch the rays
                    d3d.cmdList->DispatchRays(&desc);

                    // Transition the volume's irradiance, distance, and probe data texture arrays from read-only (non-pixel shader) to read-write (UAV)
                    volume->TransitionResources(d3d.cmdList, EDDGIExecutionStage::POST_PROBE_TRACE);

                    // Barrier(s)
                    barrier.UAV.pResource = volume->GetProbeRayData();
                    barriers.push_back(barrier);
                }

                // Wait for the ray traces to complete
                if (!barriers.empty())
                {
                    d3d.cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
                }

            #ifdef GFX_PERF_MARKERS
                PIXEndEvent(d3d.cmdList);
            #endif
            }

            void GatherIndirectLighting(Globals& d3d, GlobalResources& d3dResources, Resources& resources)
            {
            #ifdef GFX_PERF_MARKERS
                PIXBeginEvent(d3d.cmdList, PIX_COLOR(GFX_PERF_MARKER_GREEN), "Indirect Lighting");
            #endif

                // Transition the selected volume's irradiance, distance, and data texture arrays from read-write (UAV) to read-only (non-pixel shader)
                // Note: use PRE_GATHER_PS if using the pixel shader (instead of compute) to gather indirect light
                for (UINT volumeIndex = 0; volumeIndex < static_cast<UINT>(resources.selectedVolumes.size()); volumeIndex++)
                {
                    const DDGIVolume* volume = resources.selectedVolumes[volumeIndex];
                    volume->TransitionResources(d3d.cmdList, EDDGIExecutionStage::PRE_GATHER_CS);
                }

                // Set the descriptor heaps
                ID3D12DescriptorHeap* ppHeaps[] = { d3dResources.srvDescHeap, d3dResources.samplerDescHeap };
                d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

                // Set the root signature
                d3d.cmdList->SetComputeRootSignature(d3dResources.rootSignature);

                // Set the root parameter descriptor tables
            #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS
                d3d.cmdList->SetComputeRootDescriptorTable(2, d3dResources.samplerDescHeap->GetGPUDescriptorHandleForHeapStart());
                d3d.cmdList->SetComputeRootDescriptorTable(3, d3dResources.srvDescHeap->GetGPUDescriptorHandleForHeapStart());
            #endif

                // Set the PSO
                d3d.cmdList->SetPipelineState(resources.indirectPSO);

                // Dispatch threads
                UINT groupsX = DivRoundUp(d3d.width, 8);
                UINT groupsY = DivRoundUp(d3d.height, 4);
                d3d.cmdList->Dispatch(groupsX, groupsY, 1);

                // Note: if using the pixel shader (instead of compute) to gather indirect light, transition
                // the selected volume's resources to D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
              //for (UINT volumeIndex = 0; volumeIndex < static_cast<UINT>(resources.selectedVolumes.size()); volumeIndex++)
              //{
              //    const DDGIVolume* volume = resources.selectedVolumes[volumeIndex];
              //    volume->TransitionResources(d3d.cmdList, EDDGIExecutionStage::POST_GATHER_PS);
              //}

                // Wait for the compute pass to finish
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                barrier.UAV.pResource = resources.output;
                d3d.cmdList->ResourceBarrier(1, &barrier);

            #ifdef GFX_PERF_MARKERS
                PIXEndEvent(d3d.cmdList);
            #endif
            }

            //----------------------------------------------------------------------------------------------------------
            // Public Functions
            //----------------------------------------------------------------------------------------------------------

            /**
             * Create resources used by the DDGI passes.
             */
            bool Initialize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, const Configs::Config& config, Instrumentation::Performance& perf, std::ofstream& log)
            {
                // Validate the SDK version
                assert(RTXGI_VERSION::major == 1);
                assert(RTXGI_VERSION::minor == 3);
                assert(RTXGI_VERSION::revision == 5);
                assert(std::strcmp(RTXGI_VERSION::getVersionString(), "1.3.5") == 0);

                UINT numVolumes = static_cast<UINT>(config.ddgi.volumes.size());

                if (!CreateTextures(d3d, d3dResources, resources, log)) return false;
                if (!LoadAndCompileShaders(d3d, resources, numVolumes, log)) return false;
                if (!CreatePSOs(d3d, d3dResources, resources, log)) return false;
                if (!CreateShaderTable(d3d, d3dResources, resources, log)) return false;
                if (!UpdateShaderTable(d3d, d3dResources, resources, log)) return false;

                // Create the DDGIVolume resource indices structured buffer
                if (!CreateDDGIVolumeResourceIndicesBuffer(d3d, d3dResources, resources, numVolumes, log)) return false;

                // Create the DDGIVolume constants structured buffer
                if (!CreateDDGIVolumeConstantsBuffer(d3d, d3dResources, resources, numVolumes, log)) return false;

                // Create the RTV descriptor heap
                if (!CreateRTVDescriptorHeap(d3d, resources, numVolumes)) return false;

                // Initialize the DDGIVolumes
                for (UINT volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
                {
                    Configs::DDGIVolume volumeConfig = config.ddgi.volumes[volumeIndex];
                    if (!CreateDDGIVolume(d3d, d3dResources, resources, volumeConfig, log)) return false;

                    // Clear the volume's probes at initialization
                    DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[volumeIndex]);
                    volume->ClearProbes(d3d.cmdList);
                }

                // Setup performance stats
                perf.AddStat("DDGI", resources.cpuStat, resources.gpuStat);
                resources.rtStat = perf.AddGPUStat("  Probe Trace");
                resources.blendStat = perf.AddGPUStat("  Blend");
                resources.relocateStat = perf.AddGPUStat("  Relocate");
                resources.classifyStat = perf.AddGPUStat("  Classify");
                resources.lightingStat = perf.AddGPUStat("  Lighting");
                resources.variabilityStat = perf.AddGPUStat("  Variability");

                return true;
            }

            /**
             * Reload and compile shaders, recreate PSOs, and recreate the shader table.
             */
            bool Reload(Globals& d3d, GlobalResources& d3dResources, Resources& resources, const Configs::Config& config, std::ofstream& log)
            {
                log << "Reloading DDGI shaders...";
                if (!LoadAndCompileShaders(d3d, resources, static_cast<UINT>(config.ddgi.volumes.size()), log)) return false;
                if (!CreatePSOs(d3d, d3dResources, resources, log)) return false;
                if (!UpdateShaderTable(d3d, d3dResources, resources, log)) return false;

                // Reinitialize the DDGIVolumes
                for (UINT volumeIndex = 0; volumeIndex < static_cast<UINT>(resources.volumes.size()); volumeIndex++)
                {
                    Configs::DDGIVolume volumeConfig = config.ddgi.volumes[volumeIndex];
                    if (!CreateDDGIVolume(d3d, d3dResources, resources, volumeConfig, log)) return false;
                }
                log << "done.\n";
                log << std::flush;

                return true;
            }

            /**
             * Resize screen-space buffers and update descriptors.
             */
            bool Resize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
            {
                if (!CreateTextures(d3d, d3dResources, resources, log)) return false;
                log << "DDGI resize, " << d3d.width << "x" << d3d.height << "\n";
                std::flush(log);
                return true;
            }

            /**
             * Update data before execute.
             */
            void Update(Globals& d3d, GlobalResources& d3dResources, Resources& resources, Configs::Config& config)
            {
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);

                resources.enabled = config.ddgi.enabled;
                if (resources.enabled)
                {
                    // Path Trace constants
                    d3dResources.constants.pt.rayNormalBias = config.pathTrace.rayNormalBias;
                    d3dResources.constants.pt.rayViewBias = config.pathTrace.rayViewBias;
                    d3dResources.constants.pt.samplesPerPixel = config.pathTrace.samplesPerPixel;
                    d3dResources.constants.pt.SetShaderExecutionReordering(config.ddgi.shaderExecutionReordering);

                    // Clear the selected volume, if necessary
                    if (config.ddgi.volumes[config.ddgi.selectedVolume].clearProbes)
                    {
                        DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[config.ddgi.selectedVolume]);
                        volume->ClearProbes(d3d.cmdList);

                        config.ddgi.volumes[config.ddgi.selectedVolume].clearProbes = 0;
                        resources.numVolumeVariabilitySamples[config.ddgi.selectedVolume] = 0;
                    }

                    // Select the active volumes
                    resources.selectedVolumes.clear();
                    for (UINT volumeIndex = 0; volumeIndex < static_cast<UINT>(resources.volumes.size()); volumeIndex++)
                    {
                        // TODO: processing to determine which volumes are in-frustum, active, and prioritized for update / render

                        // Get the volume
                        DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[volumeIndex]);

                        // If the scene's lights, skylight, or geometry have changed *or* the volume moves *or* the probes are reset, reset the variability
                        if (config.ddgi.volumes[volumeIndex].clearProbeVariability) resources.numVolumeVariabilitySamples[volumeIndex] = 0;

                        // Don't update volumes whose variability measurement is low enough to be considered converged
                        // Enforce a minimum of 16 samples to filter out early outliers
                        const uint32_t MinimumVariabilitySamples = 16;
                        float volumeAverageVariability = volume->GetVolumeAverageVariability();
                        bool isConverged = volume->GetProbeVariabilityEnabled()
                                                && (resources.numVolumeVariabilitySamples[volumeIndex]++ > MinimumVariabilitySamples)
                                                && (volumeAverageVariability < config.ddgi.volumes[config.ddgi.selectedVolume].probeVariabilityThreshold);

                        // Add the volume to the list of volumes to update (it hasn't converged)
                        if (!isConverged) resources.selectedVolumes.push_back(volume);
                    }

                    // Update the constants for the selected DDGIVolumes
                    for (UINT volumeIndex = 0; volumeIndex < static_cast<UINT>(resources.selectedVolumes.size()); volumeIndex++)
                    {
                        resources.selectedVolumes[volumeIndex]->Update();
                    }

                }
                CPU_TIMESTAMP_END(resources.cpuStat);
            }

            /**
             * Record the graphics workload to the global command list.
             */
            void Execute(Globals& d3d, GlobalResources& d3dResources, Resources& resources)
            {
            #ifdef GFX_PERF_MARKERS
                PIXBeginEvent(d3d.cmdList, PIX_COLOR(GFX_PERF_MARKER_GREEN), "RTXGI: DDGI");
            #endif
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);
                GPU_TIMESTAMP_BEGIN(resources.gpuStat->GetGPUQueryBeginIndex());
                if (resources.enabled)
                {
                    UINT numVolumes = static_cast<UINT>(resources.selectedVolumes.size());

                    // Upload volume resource indices and constants
                    rtxgi::d3d12::UploadDDGIVolumeResourceIndices(d3d.cmdList, d3d.frameIndex, numVolumes, resources.selectedVolumes.data());
                    rtxgi::d3d12::UploadDDGIVolumeConstants(d3d.cmdList, d3d.frameIndex, numVolumes, resources.selectedVolumes.data());

                    // Trace rays from DDGI probes to sample the environment
                    GPU_TIMESTAMP_BEGIN(resources.rtStat->GetGPUQueryBeginIndex());
                    RayTraceVolumes(d3d, d3dResources, resources);
                    GPU_TIMESTAMP_END(resources.rtStat->GetGPUQueryEndIndex());

                    // Update volume probes
                    GPU_TIMESTAMP_BEGIN(resources.blendStat->GetGPUQueryBeginIndex());
                    rtxgi::d3d12::UpdateDDGIVolumeProbes(d3d.cmdList, numVolumes, resources.selectedVolumes.data());
                    GPU_TIMESTAMP_END(resources.blendStat->GetGPUQueryEndIndex());

                    // Relocate probes if the feature is enabled
                    GPU_TIMESTAMP_BEGIN(resources.relocateStat->GetGPUQueryBeginIndex());
                    rtxgi::d3d12::RelocateDDGIVolumeProbes(d3d.cmdList, numVolumes, resources.selectedVolumes.data());
                    GPU_TIMESTAMP_END(resources.relocateStat->GetGPUQueryEndIndex());

                    // Classify probes if the feature is enabled
                    GPU_TIMESTAMP_BEGIN(resources.classifyStat->GetGPUQueryBeginIndex());
                    rtxgi::d3d12::ClassifyDDGIVolumeProbes(d3d.cmdList, numVolumes, resources.selectedVolumes.data());
                    GPU_TIMESTAMP_END(resources.classifyStat->GetGPUQueryEndIndex());

                    // Calculate variability
                    GPU_TIMESTAMP_BEGIN(resources.variabilityStat->GetGPUQueryBeginIndex());
                    rtxgi::d3d12::CalculateDDGIVolumeVariability(d3d.cmdList, numVolumes, resources.selectedVolumes.data());
                    // The readback happens immediately, not recorded on the command list, so will return a value from a previous update
                    rtxgi::d3d12::ReadbackDDGIVolumeVariability(numVolumes, resources.selectedVolumes.data());
                    GPU_TIMESTAMP_END(resources.variabilityStat->GetGPUQueryEndIndex());

                    // Gather indirect lighting in screen-space
                    GPU_TIMESTAMP_BEGIN(resources.lightingStat->GetGPUQueryBeginIndex());
                    GatherIndirectLighting(d3d, d3dResources, resources);
                    GPU_TIMESTAMP_END(resources.lightingStat->GetGPUQueryEndIndex());
                }
                GPU_TIMESTAMP_END(resources.gpuStat->GetGPUQueryEndIndex());
                CPU_TIMESTAMP_ENDANDRESOLVE(resources.cpuStat);

            #ifdef GFX_PERF_MARKERS
                PIXEndEvent(d3d.cmdList);
            #endif
            }

            /**
             * Release resources.
             */
            void Cleanup(Resources& resources)
            {
                SAFE_RELEASE(resources.output);

                SAFE_RELEASE(resources.shaderTable);
                SAFE_RELEASE(resources.shaderTableUpload);
                resources.rtShaders.Release();
                resources.indirectCS.Release();

                SAFE_RELEASE(resources.rtpso);
                SAFE_RELEASE(resources.rtpsoInfo);
                SAFE_RELEASE(resources.indirectPSO);

                resources.shaderTableSize = 0;
                resources.shaderTableRecordSize = 0;
                resources.shaderTableMissTableSize = 0;
                resources.shaderTableHitGroupTableSize = 0;

                resources.shaderTableRGSStartAddress = 0;
                resources.shaderTableMissTableStartAddress = 0;
                resources.shaderTableHitGroupTableStartAddress = 0;

                SAFE_RELEASE(resources.rtvDescriptorHeap);
                SAFE_RELEASE(resources.volumeResourceIndicesSTB);
                SAFE_RELEASE(resources.volumeResourceIndicesSTBUpload);

                resources.volumeResourceIndicesSTBSizeInBytes = 0;
                SAFE_RELEASE(resources.volumeConstantsSTB);
                SAFE_RELEASE(resources.volumeConstantsSTBUpload);
                resources.volumeConstantsSTBSizeInBytes = 0;

                // Release volumes
                for (size_t volumeIndex = 0; volumeIndex < resources.volumes.size(); volumeIndex++)
                {
                #if !RTXGI_DDGI_RESOURCE_MANAGEMENT
                    DestroyDDGIVolumeResources(resources, volumeIndex);
                #endif
                    SAFE_DELETE(resources.volumeDescs[volumeIndex].name);
                    resources.volumes[volumeIndex]->Destroy();
                    SAFE_DELETE(resources.volumes[volumeIndex]);
                }
                resources.volumeDescs.clear();
                resources.volumes.clear();
                resources.selectedVolumes.clear();
            }

            /**
             * Write the DDGI Volume texture resources to disk.
             * Note: not storing ray data or probe distance (for now) since WIC doesn't auto-convert 2 channel texture formats
             */
            bool WriteVolumesToDisk(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::string directory)
            {
                CoInitialize(NULL);
                bool success = true;
                for (UINT volumeIndex = 0; volumeIndex < static_cast<UINT>(resources.volumes.size()); volumeIndex++)
                {
                    // Get the volume
                    const DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[volumeIndex]);

                    // Start constructing the filename
                    std::string baseName = directory + "/DDGIVolume[" + volume->GetName() + "]";

                    // Write probe irradiance
                    std::string filename = baseName + "-Irradiance";
                    success &= WriteResourceToDisk(d3d, filename, volume->GetProbeIrradiance(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                    // Write probe data
                    if(volume->GetProbeRelocationEnabled() || volume->GetProbeClassificationEnabled())
                    {
                        filename = baseName + "-Probe-Data";
                        success &= WriteResourceToDisk(d3d, filename, volume->GetProbeData(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                    }

                    // Write probe variability
                    if (volume->GetProbeVariabilityEnabled())
                    {
                        filename = baseName + "-Probe-Variability";
                        success &= WriteResourceToDisk(d3d, filename, volume->GetProbeVariability(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                        filename = baseName + "-Probe-Variability-Average";
                        success &= WriteResourceToDisk(d3d, filename, volume->GetProbeVariabilityAverage(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                    }
                }
                return success;
            }

        } // namespace Graphics::D3D12::RTAO

    } // namespace Graphics::D3D12

    namespace DDGI
    {

        bool Initialize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, const Configs::Config& config, Instrumentation::Performance& perf, std::ofstream& log)
        {
            return Graphics::D3D12::DDGI::Initialize(d3d, d3dResources, resources, config, perf, log);
        }

        bool Reload(Globals& d3d, GlobalResources& d3dResources, Resources& resources, const Configs::Config& config, std::ofstream& log)
        {
            return Graphics::D3D12::DDGI::Reload(d3d, d3dResources, resources, config, log);
        }

        bool Resize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::D3D12::DDGI::Resize(d3d, d3dResources, resources, log);
        }

        void Update(Globals& d3d, GlobalResources& d3dResources, Resources& resources, Configs::Config& config)
        {
            return Graphics::D3D12::DDGI::Update(d3d, d3dResources, resources, config);
        }

        void Execute(Globals& d3d, GlobalResources& d3dResources, Resources& resources)
        {
            return Graphics::D3D12::DDGI::Execute(d3d, d3dResources, resources);
        }

        void Cleanup(Globals& d3d, Resources& resources)
        {
            Graphics::D3D12::DDGI::Cleanup(resources);
        }

        bool WriteVolumesToDisk(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::string directory)
        {
            return Graphics::D3D12::DDGI::WriteVolumesToDisk(d3d, d3dResources, resources, directory);
        }

    } // namespace Graphics::DDGI
}
