/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "graphics/RTAO.h"

namespace Graphics
{
    namespace D3D12
    {
        namespace RTAO
        {
            const int RTAO_FILTER_BLOCK_SIZE = 8; // Block is NxN, 32 maximum

            //----------------------------------------------------------------------------------------------------------
            // Private Functions
            //----------------------------------------------------------------------------------------------------------

            bool CreateTextures(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
            {
                // Create the output (R8_UNORM) texture resource
                TextureDesc desc = { static_cast<uint32_t>(d3d.width), static_cast<uint32_t>(d3d.height), 1, 1, DXGI_FORMAT_R8_UNORM, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS };
                CHECK(CreateTexture(d3d, desc, &resources.RTAOOutput), "create RTAO output texture resource!\n", log);
            #ifdef GFX_NAME_OBJECTS
                resources.RTAOOutput->SetName(L"RTAO Output");
            #endif

                // Add the filtered texture UAV to the descriptor heap
                D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                uavDesc.Format = desc.format;

                D3D12_CPU_DESCRIPTOR_HANDLE handle;
                handle.ptr = d3dResources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::UAV_RTAO_OUTPUT * d3dResources.srvDescHeapEntrySize);
                d3d.device->CreateUnorderedAccessView(resources.RTAOOutput, nullptr, &uavDesc, handle);

                // Create the raw occlusion (R8_UNORM) texture resource
                CHECK(CreateTexture(d3d, desc, &resources.RTAORaw), "create RTAO raw texture resource!\n", log);
            #ifdef GFX_NAME_OBJECTS
                resources.RTAORaw->SetName(L"RTAO Raw");
            #endif

                // Add the raw occlusion texture UAV to the descriptor heap
                handle.ptr = d3dResources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::UAV_RTAO_RAW * d3dResources.srvDescHeapEntrySize);
                d3d.device->CreateUnorderedAccessView(resources.RTAORaw, nullptr, &uavDesc, handle);

                return true;
            }

            bool LoadAndCompileShaders(Globals& d3d, Resources& resources, std::ofstream& log)
            {
                // Release existing shaders
                resources.rtShaders.Release();
                resources.filterCS.Release();

                std::wstring root = std::wstring(d3d.shaderCompiler.root.begin(), d3d.shaderCompiler.root.end());

                // Load and compile the ray generation shader
                resources.rtShaders.rgs.filepath = root + L"shaders/RTAOTraceRGS.hlsl";
                resources.rtShaders.rgs.entryPoint = L"RayGen";
                resources.rtShaders.rgs.exportName = L"RTAOTraceRGS";
                Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                CHECK(Shaders::Compile(d3d.shaderCompiler, resources.rtShaders.rgs, true), "compile RTAO ray generation shader!\n", log);

                // Load and compile the miss shader
                resources.rtShaders.miss.filepath = root + L"shaders/Miss.hlsl";
                resources.rtShaders.miss.entryPoint = L"Miss";
                resources.rtShaders.miss.exportName = L"RTAOMiss";
                Shaders::AddDefine(resources.rtShaders.miss, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                CHECK(Shaders::Compile(d3d.shaderCompiler, resources.rtShaders.miss, true), "compile RTAO miss shader!\n", log);

                // Add the hit group
                resources.rtShaders.hitGroups.emplace_back();

                Shaders::ShaderRTHitGroup& group = resources.rtShaders.hitGroups[0];
                group.exportName = L"RTAOHitGroup";

                // Load and compile the CHS
                group.chs.filepath = root + L"shaders/CHS.hlsl";
                group.chs.entryPoint = L"CHS_VISIBILITY";
                group.chs.exportName = L"RTAOCHS";
                Shaders::AddDefine(group.chs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                CHECK(Shaders::Compile(d3d.shaderCompiler, group.chs, true), "compile RTAO closest hit shader!\n", log);

                // Load and compile the AHS
                group.ahs.filepath = root + L"shaders/AHS.hlsl";
                group.ahs.entryPoint = L"AHS_GI";
                group.ahs.exportName = L"RTAOAHS";
                Shaders::AddDefine(group.ahs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                CHECK(Shaders::Compile(d3d.shaderCompiler, group.ahs, true), "compile RTAO any hit shader!\n", log);

                // Set the payload size
                resources.rtShaders.payloadSizeInBytes = sizeof(PackedPayload);

                // Load and compile the filter compute shader
                std::wstring blockSize = std::to_wstring(static_cast<int>(RTAO_FILTER_BLOCK_SIZE));

                resources.filterCS.filepath = root + L"shaders/RTAOFilterCS.hlsl";
                resources.filterCS.entryPoint = L"CS";
                resources.filterCS.targetProfile = L"cs_6_6";
                Shaders::AddDefine(resources.filterCS, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                Shaders::AddDefine(resources.filterCS, L"BLOCK_SIZE", blockSize);
                CHECK(Shaders::Compile(d3d.shaderCompiler, resources.filterCS, true), "compile RTAO filter compute shader!\n", log);

                return true;
            }

            bool CreatePSOs(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
            {
                // Release existing PSOs
                SAFE_RELEASE(resources.rtpso);
                SAFE_RELEASE(resources.rtpsoInfo);
                SAFE_RELEASE(resources.filterPSO);

                // Create the RTPSO
                CHECK(CreateRayTracingPSO(
                    d3d.device,
                    d3dResources.rootSignature,
                    resources.rtShaders,
                    &resources.rtpso,
                    &resources.rtpsoInfo),
                    "create RTAO RTPSO!\n", log);

            #ifdef GFX_NAME_OBJECTS
                resources.rtpso->SetName(L"RTAO RTPSO");
            #endif

                // Create the compute PSO
                CHECK(CreateComputePSO(
                    d3d.device,
                    d3dResources.rootSignature,
                    resources.filterCS,
                    &resources.filterPSO),
                    "create RTAO filter PSO!\n", log);
            #ifdef GFX_NAME_OBJECTS
                resources.filterPSO->SetName(L"RTAO Filter PSO");
            #endif
                return true;
            }

            bool CreateShaderTable(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
            {
                // The Shader Table layout is as follows:
                //    Entry 0:  RTAO Ray Generation Shader
                //    Entry 1:  RTAO Miss Shader
                //    Entry 2+: RTAO HitGroups
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

                // Find the shader table size
                resources.shaderTableSize = (2 + static_cast<uint32_t>(resources.rtShaders.hitGroups.size())) * resources.shaderTableRecordSize;
                resources.shaderTableSize = ALIGN(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, resources.shaderTableSize);

                // Create the shader table upload buffer resource
                BufferDesc desc = { resources.shaderTableSize, 0, EHeapType::UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE };
                CHECK(CreateBuffer(d3d, desc, &resources.shaderTableUpload), "create RTAO shader table upload buffer!", log);
            #ifdef GFX_NAME_OBJECTS
                resources.shaderTableUpload->SetName(L"RTAO Shader Table Upload");
            #endif

                // Create the shader table buffer resource
                desc = { resources.shaderTableSize, 0, EHeapType::DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE };
                CHECK(CreateBuffer(d3d, desc, &resources.shaderTable), "create RTAO shader table!", log);
            #ifdef GFX_NAME_OBJECTS
                resources.shaderTable->SetName(L"RTAO Shader Table");
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

            //----------------------------------------------------------------------------------------------------------
            // Public Functions
            //----------------------------------------------------------------------------------------------------------

            /**
             * Create resources used by the ray traced ambient occlusion pass.
             */
            bool Initialize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
            {
                if (!CreateTextures(d3d, d3dResources, resources, log)) return false;
                if (!LoadAndCompileShaders(d3d, resources, log)) return false;
                if (!CreatePSOs(d3d, d3dResources, resources, log)) return false;
                if (!CreateShaderTable(d3d, d3dResources, resources, log)) return false;
                if (!UpdateShaderTable(d3d, d3dResources, resources, log)) return false;

                perf.AddStat("RTAO", resources.cpuStat, resources.gpuStat);

                return true;
            }

            /**
             * Reload and compile shaders, recreate PSOs, and recreate the shader table.
             */
            bool Reload(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
            {
                log << "Reloading RTAO shaders...";
                if (!LoadAndCompileShaders(d3d, resources, log)) return false;
                if (!CreatePSOs(d3d, d3dResources, resources, log)) return false;
                if (!UpdateShaderTable(d3d, d3dResources, resources, log)) return false;
                log << "done.\n";
                log << std::flush;

                return true;
            }

            /**
             * Resize screen-space buffers.
             */
            bool Resize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
            {
                SAFE_RELEASE(resources.RTAOOutput);
                SAFE_RELEASE(resources.RTAORaw);

                if (!CreateTextures(d3d, d3dResources, resources, log)) return false;

                log << "RTAO resize, " << d3d.width << "x" << d3d.height << "\n";
                std::flush(log);
                return true;
            }

            /**
             * Update data before execute.
             */
            void Update(Globals& d3d, GlobalResources& d3dResources, Resources& resources, const Configs::Config& config)
            {
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);

                // RTAO constants
                resources.enabled = config.rtao.enabled;
                if (resources.enabled)
                {
                    d3dResources.constants.rtao.rayLength = config.rtao.rayLength;
                    d3dResources.constants.rtao.rayNormalBias = config.rtao.rayNormalBias;
                    d3dResources.constants.rtao.rayViewBias = config.rtao.rayViewBias;
                    d3dResources.constants.rtao.power = pow(2.f, config.rtao.powerLog);
                    d3dResources.constants.rtao.filterDistanceSigma = config.rtao.filterDistanceSigma;
                    d3dResources.constants.rtao.filterDepthSigma = config.rtao.filterDepthSigma;
                    d3dResources.constants.rtao.filterBufferWidth = static_cast<uint32_t>(d3d.width);
                    d3dResources.constants.rtao.filterBufferHeight = static_cast<uint32_t>(d3d.height);

                    float distanceKernel[6];
                    for (int i = 0; i < 6; ++i)
                    {
                        distanceKernel[i] = (float)exp(-float(i * i) / (2.f * config.rtao.filterDistanceSigma * config.rtao.filterDistanceSigma));
                    }

                    d3dResources.constants.rtao.filterDistKernel0 = distanceKernel[0];
                    d3dResources.constants.rtao.filterDistKernel1 = distanceKernel[1];
                    d3dResources.constants.rtao.filterDistKernel2 = distanceKernel[2];
                    d3dResources.constants.rtao.filterDistKernel3 = distanceKernel[3];
                    d3dResources.constants.rtao.filterDistKernel4 = distanceKernel[4];
                    d3dResources.constants.rtao.filterDistKernel5 = distanceKernel[5];
                }

                CPU_TIMESTAMP_END(resources.cpuStat);
            }

            /**
             * Record the graphics workload to the global command list.
             */
            void Execute(Globals& d3d, GlobalResources& d3dResources, Resources& resources)
            {
            #ifdef GFX_PERF_MARKERS
                PIXBeginEvent(d3d.cmdList, PIX_COLOR(GFX_PERF_MARKER_RED), "RTAO");
            #endif
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);
                GPU_TIMESTAMP_BEGIN(resources.gpuStat->GetGPUQueryBeginIndex());

                if (resources.enabled)
                {
                    // Set the descriptor heaps
                    ID3D12DescriptorHeap* ppHeaps[] = { d3dResources.srvDescHeap, d3dResources.samplerDescHeap };
                    d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

                    // Set the global root signature
                    d3d.cmdList->SetComputeRootSignature(d3dResources.rootSignature);

                    // Update the root constants
                    UINT offset = 0;
                    GlobalConstants consts = d3dResources.constants;
                    offset += AppConsts::GetAlignedNum32BitValues();
                    offset += PathTraceConsts::GetAlignedNum32BitValues();
                    offset += LightingConsts::GetAlignedNum32BitValues();
                    d3d.cmdList->SetComputeRoot32BitConstants(0, RTAOConsts::GetNum32BitValues(), consts.rtao.GetData(), offset);

                    // Set the root parameter descriptor tables
                #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS
                    d3d.cmdList->SetComputeRootDescriptorTable(2, d3dResources.samplerDescHeap->GetGPUDescriptorHandleForHeapStart());
                    d3d.cmdList->SetComputeRootDescriptorTable(3, d3dResources.srvDescHeap->GetGPUDescriptorHandleForHeapStart());
                #endif

                    // Dispatch rays
                    D3D12_DISPATCH_RAYS_DESC desc = {};
                    desc.RayGenerationShaderRecord.StartAddress = resources.shaderTableRGSStartAddress;
                    desc.RayGenerationShaderRecord.SizeInBytes = resources.shaderTableRecordSize;

                    desc.MissShaderTable.StartAddress = resources.shaderTableMissTableStartAddress;
                    desc.MissShaderTable.SizeInBytes = resources.shaderTableMissTableSize;
                    desc.MissShaderTable.StrideInBytes = resources.shaderTableRecordSize;

                    desc.HitGroupTable.StartAddress = resources.shaderTableHitGroupTableStartAddress;
                    desc.HitGroupTable.SizeInBytes = resources.shaderTableHitGroupTableSize;
                    desc.HitGroupTable.StrideInBytes = resources.shaderTableRecordSize;

                    desc.Width = d3d.width;
                    desc.Height = d3d.height;
                    desc.Depth = 1;

                    // Set the PSO
                    d3d.cmdList->SetPipelineState1(resources.rtpso);

                    // Dispatch rays
                    d3d.cmdList->DispatchRays(&desc);

                    D3D12_RESOURCE_BARRIER barrier = {};
                    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                    barrier.UAV.pResource = resources.RTAORaw;

                    // Wait for the ray trace to complete
                    d3d.cmdList->ResourceBarrier(1, &barrier);

                    // --- Run the filter compute shader ---------------------------------

                    // Set the PSO and dispatch threads
                    d3d.cmdList->SetPipelineState(resources.filterPSO);

                    uint32_t groupsX = DivRoundUp(d3d.width, RTAO_FILTER_BLOCK_SIZE);
                    uint32_t groupsY = DivRoundUp(d3d.height, RTAO_FILTER_BLOCK_SIZE);
                    d3d.cmdList->Dispatch(groupsX, groupsY, 1);

                    // Wait for the compute pass to finish
                    barrier.UAV.pResource = resources.RTAOOutput;
                    d3d.cmdList->ResourceBarrier(1, &barrier);
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
                SAFE_RELEASE(resources.RTAOOutput);
                SAFE_RELEASE(resources.RTAORaw);

                SAFE_RELEASE(resources.shaderTable);
                SAFE_RELEASE(resources.shaderTableUpload);
                resources.filterCS.Release();
                resources.rtShaders.Release();

                SAFE_RELEASE(resources.rtpso);
                SAFE_RELEASE(resources.rtpsoInfo);
                SAFE_RELEASE(resources.filterPSO);

                resources.shaderTableSize = 0;
                resources.shaderTableRecordSize = 0;
                resources.shaderTableMissTableSize = 0;
                resources.shaderTableHitGroupTableSize = 0;

                resources.shaderTableRGSStartAddress = 0;
                resources.shaderTableMissTableStartAddress = 0;
                resources.shaderTableHitGroupTableStartAddress = 0;
            }

            /**
             * Write the RTAO texture resources to disk.
             */
            bool WriteRTAOBuffersToDisk(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::string directory)
            {
                CoInitialize(NULL);
                bool success = WriteResourceToDisk(d3d, directory + "/R-RTAO_Raw", resources.RTAORaw, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                success &= WriteResourceToDisk(d3d, directory + "/R-RTAO_Filtered", resources.RTAOOutput, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                return success;
            }

        } // namespace Graphics::D3D12::RTAO

    } // namespace Graphics::D3D12

    namespace RTAO
    {

        bool Initialize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
        {
            return Graphics::D3D12::RTAO::Initialize(d3d, d3dResources, resources, perf, log);
        }

        bool Reload(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::D3D12::RTAO::Reload(d3d, d3dResources, resources, log);
        }

        bool Resize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::D3D12::RTAO::Resize(d3d, d3dResources, resources, log);
        }

        void Update(Globals& d3d, GlobalResources& d3dResources, Resources& resources, const Configs::Config& config)
        {
            return Graphics::D3D12::RTAO::Update(d3d, d3dResources, resources, config);
        }

        void Execute(Globals& d3d, GlobalResources& d3dResources, Resources& resources)
        {
            return Graphics::D3D12::RTAO::Execute(d3d, d3dResources, resources);
        }

        void Cleanup(Globals& d3d, Resources& resources)
        {
            Graphics::D3D12::RTAO::Cleanup(resources);
        }

        bool WriteRTAOBuffersToDisk(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::string directory)
        {
            return Graphics::D3D12::RTAO::WriteRTAOBuffersToDisk(d3d, d3dResources, resources, directory);
        }

    } // namespace Graphics::RTAO
}
