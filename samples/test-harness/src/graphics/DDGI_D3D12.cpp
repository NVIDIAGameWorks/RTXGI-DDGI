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
            * Create resources used by a DDGIVolume.
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

                // Create the volume's textures
                {
                    UINT width = 0;
                    UINT height = 0;
                    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

                    // Probe ray data texture
                    {
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::RayData, width, height);
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::RayData, volumeDesc.probeRayDataFormat);

                        TextureDesc desc = { width, height, 1, format, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS };
                        CHECK(CreateTexture(d3d, desc, &volumeResources.unmanaged.probeRayData), "create DDGIVolume ray data texture!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Ray Data";
                        volumeResources.unmanaged.probeRayData->SetName(name.c_str());
                    #endif
                    }

                    // Probe irradiance texture
                    {
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::Irradiance, width, height);
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Irradiance, volumeDesc.probeIrradianceFormat);

                        TextureDesc desc = { width, height, 1, format, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET };
                        CHECK(CreateTexture(d3d, desc, &volumeResources.unmanaged.probeIrradiance), "create DDGIVolume probe irradiance texture!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Irradiance";
                        volumeResources.unmanaged.probeIrradiance->SetName(name.c_str());
                    #endif
                    }

                    // Probe distance texture
                    {
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::Distance, width, height);
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Distance, volumeDesc.probeDistanceFormat);

                        TextureDesc desc = { width, height, 1, format, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET };
                        CHECK(CreateTexture(d3d, desc, &volumeResources.unmanaged.probeDistance), "create DDGIVolume probe distance texture!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Distance";
                        volumeResources.unmanaged.probeDistance->SetName(name.c_str());
                    #endif
                    }

                    // Probe data texture
                    {
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::Data, width, height);
                        if (width <= 0) return false;
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Data, volumeDesc.probeDataFormat);

                        TextureDesc desc = { width, height, 1, format, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS };
                        CHECK(CreateTexture(d3d, desc, &volumeResources.unmanaged.probeData), "create DDGIVolume probe data texture!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Data";
                        volumeResources.unmanaged.probeData->SetName(name.c_str());
                    #endif
                    }
                }

                // Create the volume's resource descriptors
                {
                    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle, uavHandle, rtvHandle;
                    srvHandle = uavHandle = volumeResources.descriptorHeapDesc.heap->GetCPUDescriptorHandleForHeapStart();
                    rtvHandle = resources.rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

                    UINT rtvDescSize = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

                    // Move to the start index of the volume's UAVs and SRVs on the descriptor heap
                    uavHandle.ptr += (volumeResources.descriptorHeapDesc.uavOffset * d3dResources.srvDescHeapEntrySize);
                    srvHandle.ptr += (volumeResources.descriptorHeapDesc.srvOffset * d3dResources.srvDescHeapEntrySize);

                    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

                    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    srvDesc.Texture2D.MipLevels = 1;
                    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

                    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
                    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

                    // Probe ray data texture descriptors
                    {
                        srvDesc.Format = uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::RayData, volumeDesc.probeRayDataFormat);
                        d3d.device->CreateUnorderedAccessView(volumeResources.unmanaged.probeRayData, nullptr, &uavDesc, uavHandle);
                        d3d.device->CreateShaderResourceView(volumeResources.unmanaged.probeRayData, &srvDesc, srvHandle);
                    }

                    uavHandle.ptr += d3dResources.srvDescHeapEntrySize;
                    srvHandle.ptr += d3dResources.srvDescHeapEntrySize;

                    // Probe irradiance texture descriptors
                    {
                        srvDesc.Format = uavDesc.Format = rtvDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Irradiance, volumeDesc.probeIrradianceFormat);
                        d3d.device->CreateUnorderedAccessView(volumeResources.unmanaged.probeIrradiance, nullptr, &uavDesc, uavHandle);
                        d3d.device->CreateShaderResourceView(volumeResources.unmanaged.probeIrradiance, &srvDesc, srvHandle);

                        volumeResources.unmanaged.probeIrradianceRTV.ptr = rtvHandle.ptr + (volumeDesc.index * GetDDGIVolumeNumRTVDescriptors() * rtvDescSize);
                        d3d.device->CreateRenderTargetView(volumeResources.unmanaged.probeIrradiance, &rtvDesc, volumeResources.unmanaged.probeIrradianceRTV);
                    }

                    uavHandle.ptr += d3dResources.srvDescHeapEntrySize;
                    srvHandle.ptr += d3dResources.srvDescHeapEntrySize;
                    rtvHandle.ptr += d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

                    // Probe distance texture descriptors
                    {
                        srvDesc.Format = uavDesc.Format = rtvDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Distance, volumeDesc.probeDistanceFormat);
                        d3d.device->CreateUnorderedAccessView(volumeResources.unmanaged.probeDistance, nullptr, &uavDesc, uavHandle);
                        d3d.device->CreateShaderResourceView(volumeResources.unmanaged.probeDistance, &srvDesc, srvHandle);

                        volumeResources.unmanaged.probeDistanceRTV.ptr = volumeResources.unmanaged.probeIrradianceRTV.ptr + rtvDescSize;
                        d3d.device->CreateRenderTargetView(volumeResources.unmanaged.probeDistance, &rtvDesc, volumeResources.unmanaged.probeDistanceRTV);
                    }

                    uavHandle.ptr += d3dResources.srvDescHeapEntrySize;
                    srvHandle.ptr += d3dResources.srvDescHeapEntrySize;

                    // Probe data texture descriptors
                    {
                        srvDesc.Format = uavDesc.Format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Data, volumeDesc.probeDataFormat);
                        d3d.device->CreateUnorderedAccessView(volumeResources.unmanaged.probeData, nullptr, &uavDesc, uavHandle);
                        d3d.device->CreateShaderResourceView(volumeResources.unmanaged.probeData, &srvDesc, srvHandle);
                    }
                }

            #if !RTXGI_DDGI_BINDLESS_RESOURCES
                // Create the volume's root signature (if not using bindless with a global root signature)
                {
                    ID3DBlob* signature = nullptr;
                    if (!GetDDGIVolumeRootSignatureDesc(volumeResources.descriptorHeapDesc.constsOffset, volumeResources.descriptorHeapDesc.uavOffset, signature)) return false;

                    D3DCHECK(d3d.device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&volumeResources.unmanaged.rootSignature)));
                    SAFE_RELEASE(signature);
                #ifdef GFX_NAME_OBJECTS
                    std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Root Signature ";
                    volumeResources.unmanaged.rootSignature->SetName(name.c_str());
                #endif
                }
            #else
                // Pass a pointer to the global root signature (bindless)
                volumeResources.unmanaged.rootSignature = d3dResources.rootSignature;
            #endif

                // Create the volume's pipeline state objects
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

                    // Border Row Update (Irradiance) PSO
                    {
                        desc.CS.BytecodeLength = shaders[shaderIndex].bytecode->GetBufferSize();
                        desc.CS.pShaderBytecode = shaders[shaderIndex].bytecode->GetBufferPointer();
                        D3DCHECK(d3d.device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&volumeResources.unmanaged.probeBorderRowUpdateIrradiancePSO)));
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Border Row Update (Irradiance) PSO";
                        volumeResources.unmanaged.probeBorderRowUpdateIrradiancePSO->SetName(name.c_str());
                    #endif
                        shaderIndex++;
                    }

                    // Border Column Update (Irradiance) PSO
                    {
                        desc.CS.BytecodeLength = shaders[shaderIndex].bytecode->GetBufferSize();
                        desc.CS.pShaderBytecode = shaders[shaderIndex].bytecode->GetBufferPointer();
                        D3DCHECK(d3d.device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&volumeResources.unmanaged.probeBorderColumnUpdateIrradiancePSO)));
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Border Column Update (Irradiance) PSO";
                        volumeResources.unmanaged.probeBorderColumnUpdateIrradiancePSO->SetName(name.c_str());
                    #endif
                        shaderIndex++;
                    }

                    // Border Row Update (Distance) PSO
                    {
                        desc.CS.BytecodeLength = shaders[shaderIndex].bytecode->GetBufferSize();
                        desc.CS.pShaderBytecode = shaders[shaderIndex].bytecode->GetBufferPointer();
                        D3DCHECK(d3d.device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&volumeResources.unmanaged.probeBorderRowUpdateDistancePSO)));
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Border Row Update (Distance) PSO";
                        volumeResources.unmanaged.probeBorderRowUpdateDistancePSO->SetName(name.c_str());
                    #endif
                        shaderIndex++;
                    }

                    // Border Column Update (Distance) PSO
                    {
                        desc.CS.BytecodeLength = shaders[shaderIndex].bytecode->GetBufferSize();
                        desc.CS.pShaderBytecode = shaders[shaderIndex].bytecode->GetBufferPointer();
                        D3DCHECK(d3d.device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&volumeResources.unmanaged.probeBorderColumnUpdateDistancePSO)));
                    #ifdef GFX_NAME_OBJECTS
                        std::wstring name = L"DDGIVolume[" + std::to_wstring(volumeDesc.index) + L"], Probe Border Column Update (Distance) PSO";
                        volumeResources.unmanaged.probeBorderColumnUpdateDistancePSO->SetName(name.c_str());
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

                // Release PSOs
                if (volume->GetProbeBlendingIrradiancePSO()) volume->GetProbeBlendingIrradiancePSO()->Release();
                if (volume->GetProbeBlendingDistancePSO()) volume->GetProbeBlendingDistancePSO()->Release();
                if (volume->GetProbeBorderRowUpdateIrradiancePSO()) volume->GetProbeBorderRowUpdateIrradiancePSO()->Release();
                if (volume->GetProbeBorderColumnUpdateIrradiancePSO()) volume->GetProbeBorderColumnUpdateIrradiancePSO()->Release();
                if (volume->GetProbeBorderRowUpdateDistancePSO()) volume->GetProbeBorderRowUpdateDistancePSO()->Release();
                if (volume->GetProbeBorderColumnUpdateDistancePSO()) volume->GetProbeBorderColumnUpdateDistancePSO()->Release();
                if (volume->GetProbeRelocationPSO()) volume->GetProbeRelocationPSO()->Release();
                if (volume->GetProbeRelocationResetPSO()) volume->GetProbeRelocationResetPSO()->Release();
                if (volume->GetProbeClassificationPSO()) volume->GetProbeClassificationPSO()->Release();
                if (volume->GetProbeClassificationResetPSO()) volume->GetProbeClassificationResetPSO()->Release();

                // Clear pointers
                volume->Destroy();
            }
        #endif

            //----------------------------------------------------------------------------------------------------------
            // DDGIVolume Creation Helper Functions
            //----------------------------------------------------------------------------------------------------------

            /**
             * Populates a DDGIVolumeDesc structure from configuration data.
             */
            void GetDDGIVolumeDesc(const Configs::DDGIVolume& config, DDGIVolumeDesc& volumeDesc)
            {
                volumeDesc.name = config.name;
                volumeDesc.index = config.index;
                volumeDesc.rngSeed = config.rngSeed;
                volumeDesc.origin = { config.origin.x, config.origin.y, config.origin.z };
                volumeDesc.eulerAngles = { config.eulerAngles.x, config.eulerAngles.y, config.eulerAngles.z, };
                volumeDesc.probeSpacing = { config.probeSpacing.x, config.probeSpacing.y, config.probeSpacing.z };
                volumeDesc.probeCounts = { config.probeCounts.x, config.probeCounts.y, config.probeCounts.z, };
                volumeDesc.probeNumRays = config.probeNumRays;
                volumeDesc.probeNumIrradianceTexels = config.probeNumIrradianceTexels;
                volumeDesc.probeNumDistanceTexels = config.probeNumDistanceTexels;
                volumeDesc.probeHysteresis = config.probeHysteresis;
                volumeDesc.probeNormalBias = config.probeNormalBias;
                volumeDesc.probeViewBias = config.probeViewBias;
                volumeDesc.probeMaxRayDistance = config.probeMaxRayDistance;
                volumeDesc.probeIrradianceThreshold = config.probeIrradianceThreshold;
                volumeDesc.probeBrightnessThreshold = config.probeBrightnessThreshold;

                volumeDesc.showProbes = config.showProbes;

                volumeDesc.probeRayDataFormat = config.textureFormats.rayDataFormat;
                volumeDesc.probeIrradianceFormat = config.textureFormats.irradianceFormat;
                volumeDesc.probeDistanceFormat = config.textureFormats.distanceFormat;
                volumeDesc.probeDataFormat = config.textureFormats.dataFormat;

                volumeDesc.probeRelocationEnabled = config.probeRelocationEnabled;
                volumeDesc.probeMinFrontfaceDistance = config.probeMinFrontfaceDistance;

                volumeDesc.probeClassificationEnabled = config.probeClassificationEnabled;

                if (config.infiniteScrollingEnabled)
                {
                    volumeDesc.movementType = EDDGIVolumeMovementType::Scrolling;
                }
                else
                {
                    volumeDesc.movementType = EDDGIVolumeMovementType::Default;
                }
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

                // Descriptor heap pointer
                volumeResources.descriptorHeapDesc.heap = d3dResources.srvDescHeap;

                // Set the offset of the DDGIVolume constants structured buffer on the descriptor heap
                volumeResources.descriptorHeapDesc.constsOffset = DescriptorHeapOffsets::STB_DDGI_VOLUMES;

                // Set the offset of the start of volume UAVs and SRVs on the descriptor heap
                volumeResources.descriptorHeapDesc.uavOffset = DescriptorHeapOffsets::UAV_DDGI_VOLUME;
                volumeResources.descriptorHeapDesc.srvOffset = DescriptorHeapOffsets::SRV_DDGI_VOLUME;

                // Increment the offsets to *this* volume's UAV and SRV locations on the descriptor heap
                volumeResources.descriptorHeapDesc.uavOffset += volumeDesc.index * GetDDGIVolumeNumUAVDescriptors();
                volumeResources.descriptorHeapDesc.srvOffset += volumeDesc.index * GetDDGIVolumeNumUAVDescriptors();

                // Indices of the volume's UAV and SRV locations in bindless resource arrays
                // This value is set here since the Test Harness *always* access resources bindlessly when ray tracing. See RayTraceVolume().
                // When the SDK shaders are not in bindless mode, they ignore these values.
                volumeResources.descriptorBindlessDesc.uavOffset = (DescriptorHeapOffsets::UAV_DDGI_VOLUME - DescriptorHeapOffsets::UAV_START);
                volumeResources.descriptorBindlessDesc.srvOffset = (DescriptorHeapOffsets::SRV_DDGI_VOLUME - DescriptorHeapOffsets::SRV_TEXTURE2D_START);

                // Constants structured buffer pointers and size
                volumeResources.constantsBuffer = resources.constantsSTB;
                volumeResources.constantsBufferUpload = resources.constantsSTBUpload;
                volumeResources.constantsBufferSizeInBytes = resources.constantsSTBSizeInBytes;

            #if RTXGI_DDGI_RESOURCE_MANAGEMENT
                // Enable "Managed Mode", the RTXGI SDK creates graphics objects
                volumeResources.managed.enabled = true;

                // Pass the D3D device to use for resource creation
                volumeResources.managed.device = d3d.device;

                // Pass compiled shader bytecode
                assert(volumeShaders.size() >= 6);
                volumeResources.managed.probeBlendingIrradianceCS = { volumeShaders[0].bytecode->GetBufferPointer(), volumeShaders[0].bytecode->GetBufferSize() };
                volumeResources.managed.probeBlendingDistanceCS = { volumeShaders[1].bytecode->GetBufferPointer(), volumeShaders[1].bytecode->GetBufferSize() };
                volumeResources.managed.probeBorderRowUpdateIrradianceCS = { volumeShaders[2].bytecode->GetBufferPointer(), volumeShaders[2].bytecode->GetBufferSize() };
                volumeResources.managed.probeBorderColumnUpdateIrradianceCS = { volumeShaders[3].bytecode->GetBufferPointer(), volumeShaders[3].bytecode->GetBufferSize() };
                volumeResources.managed.probeBorderRowUpdateDistanceCS = { volumeShaders[4].bytecode->GetBufferPointer(), volumeShaders[4].bytecode->GetBufferSize() };
                volumeResources.managed.probeBorderColumnUpdateDistanceCS = { volumeShaders[5].bytecode->GetBufferPointer(), volumeShaders[5].bytecode->GetBufferSize() };

                assert(volumeShaders.size() >= 8);
                volumeResources.managed.probeRelocation.updateCS = { volumeShaders[6].bytecode->GetBufferPointer(), volumeShaders[6].bytecode->GetBufferSize() };
                volumeResources.managed.probeRelocation.resetCS = { volumeShaders[7].bytecode->GetBufferPointer(), volumeShaders[7].bytecode->GetBufferSize() };

                assert(volumeShaders.size() == 10);
                volumeResources.managed.probeClassification.updateCS = { volumeShaders[8].bytecode->GetBufferPointer(), volumeShaders[8].bytecode->GetBufferSize() };
                volumeResources.managed.probeClassification.resetCS = { volumeShaders[9].bytecode->GetBufferPointer(), volumeShaders[9].bytecode->GetBufferSize() };
            #else
                // Enable "Unmanaged Mode", the application creates graphics objects
                volumeResources.unmanaged.enabled = true;

                #if RTXGI_DDGI_BINDLESS_RESOURCES
                    // Root parameter slot locations in the global root signature
                    // See Direct3D12.cpp::CreateGlobalRootSignature()
                    volumeResources.unmanaged.rootParamSlotRootConstants = 3;
                    volumeResources.unmanaged.rootParamSlotDescriptorTable = 1;
                #else
                    // Root parameter slot locations in the RTXGI DDGIVolume root signature
                    // See DDGIVolume_D3D12.cpp::GetDDGIVolumeRootSignatureDesc()
                    volumeResources.unmanaged.rootParamSlotRootConstants = 0;
                    volumeResources.unmanaged.rootParamSlotDescriptorTable = 1;
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
                        delete resources.volumes[volumeConfig.index];
                        resources.volumes[volumeConfig.index] = nullptr;
                    }
                }
                else
                {
                    resources.volumes.emplace_back();
                }

                // Describe the DDGIVolume's properties
                DDGIVolumeDesc volumeDesc;
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
                    log << "\nError: failed to create the DDGIVolume!\n";
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
             * Creates the DDGIVolume constants structured buffer.
             */
            bool CreateDDGIVolumeConstantsBuffer(Globals& d3d, GlobalResources& d3dResources, Resources& resources, UINT volumeCount, std::ofstream& log)
            {
                resources.constantsSTBSizeInBytes = sizeof(DDGIVolumeDescGPUPacked) * volumeCount;
                if (resources.constantsSTBSizeInBytes == 0) return true; // scenes with no DDGIVolumes are valid

                // Create the DDGIVolume constants upload buffer resource (double buffered)
                BufferDesc desc = { 2 * resources.constantsSTBSizeInBytes, 0, EHeapType::UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE };
                CHECK(CreateBuffer(d3d, desc, &resources.constantsSTBUpload), "create DDGIVolume constants upload structured buffer!\n", log);
            #ifdef GFX_NAME_OBJECTS
                resources.constantsSTBUpload->SetName(L"DDGIVolume Constants Upload Structured Buffer");
            #endif

                // Create the DDGIVolume constants device buffer resource
                desc = { resources.constantsSTBSizeInBytes, 0, EHeapType::DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE };
                CHECK(CreateBuffer(d3d, desc, &resources.constantsSTB), "create DDGIVolume constants structured buffer!\n", log);
            #ifdef GFX_NAME_OBJECTS
                resources.constantsSTB->SetName(L"DDGIVolume Constants Structured Buffer");
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
                handle.ptr = d3dResources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::STB_DDGI_VOLUMES * d3dResources.srvDescHeapEntrySize);
                d3d.device->CreateShaderResourceView(resources.constantsSTB, &srvDesc, handle);
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
                TextureDesc desc = { static_cast<uint32_t>(d3d.width), static_cast<uint32_t>(d3d.height), 1, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS };
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

                    Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_COORDINATE_SYSTEM", std::to_wstring(RTXGI_COORDINATE_SYSTEM));

                    CHECK(Shaders::Compile(d3d.shaderCompiler, resources.rtShaders.rgs, true), "compile DDGI probe tracing ray generation shader!\n", log);
                }

                // Load and compile the miss shader
                {
                    resources.rtShaders.miss.filepath = root + L"shaders/Miss.hlsl";
                    resources.rtShaders.miss.entryPoint = L"Miss";
                    resources.rtShaders.miss.exportName = L"DDGIProbeTraceMiss";
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
                    CHECK(Shaders::Compile(d3d.shaderCompiler, group.chs, true), "compile DDGI probe tracing closest hit shader!\n", log);

                    // Load and compile the AHS
                    group.ahs.filepath = root + L"shaders/AHS.hlsl";
                    group.ahs.entryPoint = L"AHS_GI";
                    group.ahs.exportName = L"DDGIProbeTraceAHS";
                    CHECK(Shaders::Compile(d3d.shaderCompiler, group.ahs, true), "compile DDGI probe tracing any hit shader!\n", log);

                    // Set the payload size
                    resources.rtShaders.payloadSizeInBytes = sizeof(PackedPayload);
                }

                // Load and compile the indirect lighting compute shader
                {
                    std::wstring shaderPath = root + L"shaders/IndirectCS.hlsl";
                    resources.indirectCS.filepath = shaderPath.c_str();
                    resources.indirectCS.entryPoint = L"CS";
                    resources.indirectCS.targetProfile = L"cs_6_0";

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
                desc = { resources.shaderTableSize, 0, EHeapType::DEFAULT, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE };
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
                PIXBeginEvent(d3d.cmdList, PIX_COLOR(GFX_PERF_MARKER_GREEN), "Ray Trace Volumes");
            #endif

                // Set the descriptor heaps
                ID3D12DescriptorHeap* ppHeaps[] = { d3dResources.srvDescHeap, d3dResources.samplerDescHeap };
                d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

                // Set the global root signature
                d3d.cmdList->SetComputeRootSignature(d3dResources.rootSignature);

                // Set the root parameters
                d3d.cmdList->SetComputeRootDescriptorTable(0, d3dResources.samplerDescHeap->GetGPUDescriptorHandleForHeapStart());
                d3d.cmdList->SetComputeRootDescriptorTable(1, d3dResources.srvDescHeap->GetGPUDescriptorHandleForHeapStart());

                // Update the global root constants
                UINT offset = 0;
                GlobalConstants consts = d3dResources.constants;
                d3d.cmdList->SetComputeRoot32BitConstants(2, AppConsts::GetNum32BitValues(), consts.app.GetData(), offset);
                offset += AppConsts::GetAlignedNum32BitValues();
                d3d.cmdList->SetComputeRoot32BitConstants(2, PathTraceConsts::GetNum32BitValues(), consts.pt.GetData(), offset);
                offset += PathTraceConsts::GetAlignedNum32BitValues();
                d3d.cmdList->SetComputeRoot32BitConstants(2, LightingConsts::GetNum32BitValues(), consts.lights.GetData(), offset);

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

                // Constants
                DDGIConstants ddgi = {};

                // Barriers
                std::vector<D3D12_RESOURCE_BARRIER> barriers;
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;

                UINT volumeIndex;
                for(volumeIndex = 0; volumeIndex < static_cast<UINT>(resources.selectedVolumes.size()); volumeIndex++)
                {
                    const DDGIVolume* volume = resources.selectedVolumes[volumeIndex];

                    // Update the DDGI root constants
                    ddgi.volumeIndex = volume->GetIndex();
                    ddgi.uavOffset = volume->GetDescriptorBindlessUAVOffset();
                    ddgi.srvOffset = volume->GetDescriptorBindlessSRVOffset();

                    d3d.cmdList->SetComputeRoot32BitConstants(3, DDGIConstants::GetNum32BitValues(), ddgi.GetData(), 0);

                    // Trace probe rays
                    desc.Width = volume->GetNumRaysPerProbe();
                    desc.Height = volume->GetNumProbes();
                    desc.Depth = 1;

                    d3d.cmdList->DispatchRays(&desc);

                    // Barrier(s)
                    barrier.UAV.pResource = volume->GetProbeRayData();
                    barriers.push_back(barrier);
                }

                // Wait for the ray traces to complete
                d3d.cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

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
                assert(RTXGI_VERSION::minor == 2);
                assert(RTXGI_VERSION::revision == 12);
                assert(std::strcmp(RTXGI_VERSION::getVersionString(), "1.2.12") == 0);

                UINT numVolumes = static_cast<UINT>(config.ddgi.volumes.size());

                if (!CreateTextures(d3d, d3dResources, resources, log)) return false;
                if (!LoadAndCompileShaders(d3d, resources, numVolumes, log)) return false;
                if (!CreatePSOs(d3d, d3dResources, resources, log)) return false;
                if (!CreateShaderTable(d3d, d3dResources, resources, log)) return false;
                if (!UpdateShaderTable(d3d, d3dResources, resources, log)) return false;

                // Create the DDGIVolume constants structured buffer
                if (!CreateDDGIVolumeConstantsBuffer(d3d, d3dResources, resources, numVolumes, log)) return false;

                // Create the RTV descriptor heap
                if (!CreateRTVDescriptorHeap(d3d, resources, numVolumes)) return false;

                // Initialize the DDGIVolumes
                for (UINT volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
                {
                    Configs::DDGIVolume volumeConfig = config.ddgi.volumes[volumeIndex];
                    if (!CreateDDGIVolume(d3d, d3dResources, resources, volumeConfig, log)) return false;
                }

                // Setup performance stats
                perf.AddStat("DDGI", resources.cpuStat, resources.gpuStat);
                resources.classifyStat = perf.AddGPUStat("  Classify");
                resources.rtStat = perf.AddGPUStat("  Probe Trace");
                resources.blendStat = perf.AddGPUStat("  Blend");
                resources.relocateStat = perf.AddGPUStat("  Relocate");
                resources.lightingStat = perf.AddGPUStat("  Lighting");

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
                    if (!CreateDDGIVolume(
                        d3d,
                        d3dResources,
                        resources,
                        volumeConfig,
                        log)) return false;
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

                    // Clear the selected volume, if necessary
                    if (config.ddgi.volumes[config.ddgi.selectedVolume].clearProbes)
                    {
                        DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[config.ddgi.selectedVolume]);
                        volume->ClearProbes(d3d.cmdList);

                        config.ddgi.volumes[config.ddgi.selectedVolume].clearProbes = 0;
                    }

                    // Select the active volumes
                    resources.selectedVolumes.clear();
                    for (UINT volumeIndex = 0; volumeIndex < static_cast<UINT>(resources.volumes.size()); volumeIndex++)
                    {
                        // TODO: processing to determine which volumes are in-frustum, active, and prioritized for update / render
                        // For now, just select all volumes
                        DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[volumeIndex]);
                        resources.selectedVolumes.push_back(volume);
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
                GPU_TIMESTAMP_BEGIN(resources.gpuStat->GetQueryBeginIndex());
                if (resources.enabled)
                {
                    UINT numVolumes = static_cast<UINT>(resources.selectedVolumes.size());

                    // Upload volume constants
                    rtxgi::d3d12::UploadDDGIVolumeConstants(d3d.cmdList, d3d.frameIndex, numVolumes, resources.selectedVolumes.data());

                    // Trace rays from probes to sample the environment
                    GPU_TIMESTAMP_BEGIN(resources.rtStat->GetQueryBeginIndex());
                    RayTraceVolumes(d3d, d3dResources, resources);
                    GPU_TIMESTAMP_END(resources.rtStat->GetQueryEndIndex());

                    // Update volume probes
                    GPU_TIMESTAMP_BEGIN(resources.blendStat->GetQueryBeginIndex());
                    rtxgi::d3d12::UpdateDDGIVolumeProbes(d3d.cmdList, numVolumes, resources.selectedVolumes.data());
                    GPU_TIMESTAMP_END(resources.blendStat->GetQueryEndIndex());

                    // Relocate probes if the feature is enabled
                    GPU_TIMESTAMP_BEGIN(resources.relocateStat->GetQueryBeginIndex());
                    rtxgi::d3d12::RelocateDDGIVolumeProbes(d3d.cmdList, numVolumes, resources.selectedVolumes.data());
                    GPU_TIMESTAMP_END(resources.relocateStat->GetQueryEndIndex());

                    // Classify probes if the feature is enabled
                    GPU_TIMESTAMP_BEGIN(resources.classifyStat->GetQueryBeginIndex());
                    rtxgi::d3d12::ClassifyDDGIVolumeProbes(d3d.cmdList, numVolumes, resources.selectedVolumes.data());
                    GPU_TIMESTAMP_END(resources.classifyStat->GetQueryEndIndex());

                    // Render the indirect lighting to screen-space
                    {
                    #ifdef GFX_PERF_MARKERS
                        PIXBeginEvent(d3d.cmdList, PIX_COLOR(GFX_PERF_MARKER_GREEN), "Indirect Lighting");
                    #endif
                        GPU_TIMESTAMP_BEGIN(resources.lightingStat->GetQueryBeginIndex());

                        // Set the descriptor heaps
                        ID3D12DescriptorHeap* ppHeaps[] = { d3dResources.srvDescHeap, d3dResources.samplerDescHeap };
                        d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

                        // Set the global root signature
                        d3d.cmdList->SetComputeRootSignature(d3dResources.rootSignature);

                        // Set the root parameters
                        d3d.cmdList->SetComputeRootDescriptorTable(0, d3dResources.samplerDescHeap->GetGPUDescriptorHandleForHeapStart());
                        d3d.cmdList->SetComputeRootDescriptorTable(1, d3dResources.srvDescHeap->GetGPUDescriptorHandleForHeapStart());

                        // Set the PSO and dispatch threads
                        d3d.cmdList->SetPipelineState(resources.indirectPSO);

                        UINT groupsX = DivRoundUp(d3d.width, 8);
                        UINT groupsY = DivRoundUp(d3d.height, 4);
                        d3d.cmdList->Dispatch(groupsX, groupsY, 1);

                        // Wait for the compute pass to finish
                        D3D12_RESOURCE_BARRIER barrier = {};
                        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                        barrier.UAV.pResource = resources.output;
                        d3d.cmdList->ResourceBarrier(1, &barrier);

                        GPU_TIMESTAMP_END(resources.lightingStat->GetQueryEndIndex());

                    #ifdef GFX_PERF_MARKERS
                        PIXEndEvent(d3d.cmdList);
                    #endif
                    }

                }
                GPU_TIMESTAMP_END(resources.gpuStat->GetQueryEndIndex());
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
                SAFE_RELEASE(resources.constantsSTB);
                SAFE_RELEASE(resources.constantsSTBUpload);

                // Release volumes
                for (size_t volumeIndex = 0; volumeIndex < resources.volumes.size(); volumeIndex++)
                {
                #if !RTXGI_DDGI_RESOURCE_MANAGEMENT
                    DestroyDDGIVolumeResources(resources, volumeIndex);
                #endif
                    resources.volumes[volumeIndex]->Destroy();
                    delete resources.volumes[volumeIndex];
                    resources.volumes[volumeIndex] = nullptr;
                }
            }

            /**
             * Write the DDGI Volume texture resources to disk.
             */
            bool WriteVolumesToDisk(Globals& globals, GlobalResources& gfxResources, Resources& resources, std::string directory)
            {
                CoInitialize(NULL);
                bool success = true;
                for (rtxgi::DDGIVolumeBase* volumeBase : resources.volumes)
                {
                    std::string baseName = directory + "/" + volumeBase->GetName();
                    std::string filename = baseName + "-irradiance.png";

                    rtxgi::d3d12::DDGIVolume* volume = static_cast<DDGIVolume*>(volumeBase);
                    success &= WriteResourceToDisk(globals, filename, volume->GetProbeIrradiance(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

                    // not capturing distances because WIC doesn't like two-channel textures
                    //filename = baseName + "-distance.png";
                    //success &= WriteResourceToDisk(globals, filename, volume->GetProbeDistance(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

                    filename = baseName + "-data.png";
                    success &= WriteResourceToDisk(globals, filename, volume->GetProbeData(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
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

        bool WriteVolumesToDisk(Globals& globals, GlobalResources& gfxResources, Resources& resources, std::string directory)
        {
            return Graphics::D3D12::DDGI::WriteVolumesToDisk(globals, gfxResources, resources, directory);
        }

    } // namespace Graphics::DDGI
}
