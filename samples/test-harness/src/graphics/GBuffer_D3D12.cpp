/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "graphics/GBuffer.h"

namespace Graphics
{
    namespace D3D12
    {
        namespace GBuffer
        {

            //----------------------------------------------------------------------------------------------------------
            // Private Functions
            //----------------------------------------------------------------------------------------------------------

            bool LoadAndCompileShaders(Globals& d3d, Resources& resources, std::ofstream& log)
            {
                // Release existing shaders
                resources.shaders.Release();

                std::wstring root = std::wstring(d3d.shaderCompiler.root.begin(), d3d.shaderCompiler.root.end());

                // Load and compile the ray generation shader
                resources.shaders.rgs.filepath = root + L"shaders/GBufferRGS.hlsl";
                resources.shaders.rgs.entryPoint = L"RayGen";
                resources.shaders.rgs.exportName = L"GBufferRGS";
                Shaders::AddDefine(resources.shaders.rgs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                CHECK(Shaders::Compile(d3d.shaderCompiler, resources.shaders.rgs, true), "compile GBuffer ray generation shader!\n", log);

                // Load and compile the miss shader
                resources.shaders.miss.filepath = root + L"shaders/Miss.hlsl";
                resources.shaders.miss.entryPoint = L"Miss";
                resources.shaders.miss.exportName = L"GBufferMiss";
                Shaders::AddDefine(resources.shaders.miss, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                CHECK(Shaders::Compile(d3d.shaderCompiler, resources.shaders.miss, true), "compile GBuffer miss shader!\n", log);

                // Add the hit group
                resources.shaders.hitGroups.emplace_back();

                Shaders::ShaderRTHitGroup& group = resources.shaders.hitGroups[0];
                group.exportName = L"GBufferHitGroup";

                // Load and compile the CHS
                group.chs.filepath = root + L"shaders/CHS.hlsl";
                group.chs.entryPoint = L"CHS_PRIMARY";
                group.chs.exportName = L"GBufferCHS";
                Shaders::AddDefine(group.chs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                CHECK(Shaders::Compile(d3d.shaderCompiler, group.chs, true), "compile GBuffer closest hit shader!\n", log);

                // Load and compile the AHS
                group.ahs.filepath = root + L"shaders/AHS.hlsl";
                group.ahs.entryPoint = L"AHS_PRIMARY";
                group.ahs.exportName = L"GBufferAHS";
                Shaders::AddDefine(group.ahs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                CHECK(Shaders::Compile(d3d.shaderCompiler, group.ahs, true), "compile GBuffer any hit shader!\n", log);

                // Set the payload size
                resources.shaders.payloadSizeInBytes = sizeof(PackedPayload);

                return true;
            }

            bool CreatePSOs(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
            {
                // Release existing PSOs
                SAFE_RELEASE(resources.rtpsoInfo);
                SAFE_RELEASE(resources.rtpso);

                // Create the RTPSO
                CHECK(CreateRayTracingPSO(
                    d3d.device,
                    d3dResources.rootSignature,
                    resources.shaders,
                    &resources.rtpso,
                    &resources.rtpsoInfo),
                    "create GBuffer RTPSO!\n", log);

            #ifdef GFX_NAME_OBJECTS
                resources.rtpso->SetName(L"GBuffer RTPSO");
            #endif

                return true;
            }

            bool CreateShaderTable(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
            {
                // The Shader Table layout is as follows:
                //    Entry 0:  Path Trace Ray Generation Shader
                //    Entry 1:  Path Trace Miss Shader
                //    Entry 2+: Path Trace HitGroups
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
                resources.shaderTableSize = (2 + static_cast<uint32_t>(resources.shaders.hitGroups.size())) * resources.shaderTableRecordSize;
                resources.shaderTableSize = ALIGN(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, resources.shaderTableSize);

                // Create the shader table upload buffer resource
                BufferDesc desc = { resources.shaderTableSize, 0, EHeapType::UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE };
                CHECK(CreateBuffer(d3d, desc, &resources.shaderTableUpload), "create GBuffer shader table upload buffer!", log);
            #ifdef GFX_NAME_OBJECTS
                resources.shaderTableUpload->SetName(L"GBuffer Shader Table Upload");
            #endif

                // Create the shader table buffer resource
                desc = { resources.shaderTableSize, 0, EHeapType::DEFAULT , D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE };
                CHECK(CreateBuffer(d3d, desc, &resources.shaderTable), "create GBuffer shader table!", log);
            #ifdef GFX_NAME_OBJECTS
                resources.shaderTable->SetName(L"GBuffer Shader Table");
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
                memcpy(pData, resources.rtpsoInfo->GetShaderIdentifier(resources.shaders.rgs.exportName.c_str()), shaderIdSize);
                *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = d3dResources.srvDescHeap->GetGPUDescriptorHandleForHeapStart();
                resources.shaderTableRGSStartAddress = resources.shaderTable->GetGPUVirtualAddress();

                // Entry 1: Miss Shader
                pData += resources.shaderTableRecordSize;
                memcpy(pData, resources.rtpsoInfo->GetShaderIdentifier(resources.shaders.miss.exportName.c_str()), shaderIdSize);
                resources.shaderTableMissTableStartAddress = resources.shaderTableRGSStartAddress + resources.shaderTableRecordSize;
                resources.shaderTableMissTableSize = resources.shaderTableRecordSize;

                // Entries 2+: Hit Groups and descriptor heap pointers
                for (uint32_t hitGroupIndex = 0; hitGroupIndex < static_cast<uint32_t>(resources.shaders.hitGroups.size()); hitGroupIndex++)
                {
                    pData += resources.shaderTableRecordSize;
                    memcpy(pData, resources.rtpsoInfo->GetShaderIdentifier(resources.shaders.hitGroups[hitGroupIndex].exportName), shaderIdSize);
                    *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = d3dResources.srvDescHeap->GetGPUDescriptorHandleForHeapStart();
                    *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize + 8) = d3dResources.samplerDescHeap->GetGPUDescriptorHandleForHeapStart();
                }
                resources.shaderTableHitGroupTableStartAddress = resources.shaderTableMissTableStartAddress + resources.shaderTableMissTableSize;
                resources.shaderTableHitGroupTableSize = static_cast<uint32_t>(resources.shaders.hitGroups.size()) * resources.shaderTableRecordSize;

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
             * Create resources, shaders, and PSOs.
             */
            bool Initialize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
            {
                if(!LoadAndCompileShaders(d3d, resources, log)) return false;
                if(!CreatePSOs(d3d, d3dResources, resources, log)) return false;
                if(!CreateShaderTable(d3d, d3dResources, resources, log)) return false;
                if(!UpdateShaderTable(d3d, d3dResources, resources, log)) return false;

                perf.AddStat("GBuffer", resources.cpuStat, resources.gpuStat);

                return true;
            }

            /**
             * Reload and compile shaders, recreate PSOs, and recreate the shader table.
             */
            bool Reload(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
            {
                log << "Reloading GBuffer shaders...";
                if (!LoadAndCompileShaders(d3d, resources, log)) return false;
                if (!CreatePSOs(d3d, d3dResources, resources, log)) return false;
                if (!UpdateShaderTable(d3d, d3dResources, resources, log)) return false;
                log << "done.\n";
                log << std::flush;

                return true;
            }

            /**
             * Update data before execute.
             */
            void Update(Globals& d3d, GlobalResources& d3dResources, Resources& resources, const Configs::Config& config)
            {
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);

                // Update bias constants
                d3dResources.constants.pt.rayNormalBias = config.pathTrace.rayNormalBias;
                d3dResources.constants.pt.rayViewBias = config.pathTrace.rayViewBias;

                CPU_TIMESTAMP_END(resources.cpuStat);
            }

            /**
             * Record the workload to the global command list.
             */
            void Execute(Globals& d3d, GlobalResources& d3dResources, Resources& resources)
            {
            #ifdef GFX_PERF_MARKERS
                PIXBeginEvent(d3d.cmdList, PIX_COLOR(GFX_PERF_MARKER_ORANGE), "GBuffer");
            #endif
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);

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
                GPU_TIMESTAMP_BEGIN(resources.gpuStat->GetGPUQueryBeginIndex());
                d3d.cmdList->DispatchRays(&desc);
                GPU_TIMESTAMP_END(resources.gpuStat->GetGPUQueryEndIndex());

                D3D12_RESOURCE_BARRIER barriers[4] = {};
                barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                barriers[0].UAV.pResource = d3dResources.rt.GBufferA;
                barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                barriers[1].UAV.pResource = d3dResources.rt.GBufferB;
                barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                barriers[2].UAV.pResource = d3dResources.rt.GBufferC;
                barriers[3].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                barriers[3].UAV.pResource = d3dResources.rt.GBufferD;

                // Wait for the ray trace to complete
                d3d.cmdList->ResourceBarrier(4, barriers);

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
                // Release shaders and shader table
                resources.shaders.Release();
                SAFE_RELEASE(resources.shaderTable);
                SAFE_RELEASE(resources.shaderTableUpload);

                // Release PSOs
                SAFE_RELEASE(resources.rtpsoInfo);
                SAFE_RELEASE(resources.rtpso);
            }

            /**
             * Write the GBuffer texture resources to disk.
             */
            bool WriteGBufferToDisk(Globals& d3d, GlobalResources& d3dResources, std::string directory)
            {
                CoInitialize(NULL);
                bool success = WriteResourceToDisk(d3d, directory + "/R-GBufferA", d3dResources.rt.GBufferA, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                success &= WriteResourceToDisk(d3d, directory + "/R-GBufferB", d3dResources.rt.GBufferB, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                success &= WriteResourceToDisk(d3d, directory + "/R-GBufferC", d3dResources.rt.GBufferC, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                success &= WriteResourceToDisk(d3d, directory + "/R-GBufferD", d3dResources.rt.GBufferD, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                return success;
            }

        } // namespace Graphics::D3D12::GBuffer

    } // namespace Graphics::D3D12

    namespace GBuffer
    {

        bool Initialize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
        {
            return Graphics::D3D12::GBuffer::Initialize(d3d, d3dResources, resources, perf, log);
        }

        bool Reload(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::D3D12::GBuffer::Reload(d3d, d3dResources, resources, log);
        }

        bool Resize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
        {
            return true; // nothing to do here in D3D12
        }

        void Update(Globals& d3d, GlobalResources& d3dResources, Resources& resources, const Configs::Config& config)
        {
            return Graphics::D3D12::GBuffer::Update(d3d, d3dResources, resources, config);
        }

        void Execute(Globals& d3d, GlobalResources& d3dResources, Resources& resources)
        {
            return Graphics::D3D12::GBuffer::Execute(d3d, d3dResources, resources);
        }

        bool WriteGBufferToDisk(Globals& d3d, GlobalResources& d3dResources, std::string directory)
        {
            return Graphics::D3D12::GBuffer::WriteGBufferToDisk(d3d, d3dResources, directory);
        }

        void Cleanup(Globals& d3d, Resources& resources)
        {
            Graphics::D3D12::GBuffer::Cleanup(resources);
        }

    } // namespace Graphics::GBuffer
}
