/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "graphics/PathTracing.h"

namespace Graphics
{
    namespace D3D12
    {
        namespace PathTracing
        {
            //----------------------------------------------------------------------------------------------------------
            // Private Functions
            //----------------------------------------------------------------------------------------------------------

            bool CreateTextures(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
            {
                // Create the output (R8G8B8A8_UNORM) texture resource
                TextureDesc desc = { static_cast<UINT>(d3d.width), static_cast<UINT>(d3d.height), 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS };
                CHECK(CreateTexture(d3d, desc, &resources.PTOutput), "create path tracing output texture resource!\n", log);
            #ifdef GFX_NAME_OBJECTS
                resources.PTOutput->SetName(L"PT Output");
            #endif

                // Add the output texture UAV to the descriptor heap
                D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                uavDesc.Format = desc.format;

                D3D12_CPU_DESCRIPTOR_HANDLE handle;
                handle.ptr = d3dResources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::UAV_PT_OUTPUT * d3dResources.srvDescHeapEntrySize);
                d3d.device->CreateUnorderedAccessView(resources.PTOutput, nullptr, &uavDesc, handle);

                // Create the accumulation (R32G32B32A32_FLOAT) texture resource
                desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
                desc.state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                CHECK(CreateTexture(d3d, desc, &resources.PTAccumulation), "create path tracing accumulation texture resource!\n", log);
            #ifdef GFX_NAME_OBJECTS
                resources.PTAccumulation->SetName(L"PT Accumulation");
            #endif

                // Add the accumulation texture UAV to the descriptor heap
                uavDesc.Format = desc.format;
                handle.ptr = d3dResources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::UAV_PT_ACCUMULATION * d3dResources.srvDescHeapEntrySize);
                d3d.device->CreateUnorderedAccessView(resources.PTAccumulation, nullptr, &uavDesc, handle);

                return true;
            }

            bool LoadAndCompileShaders(Globals& d3d, Resources& resources, std::ofstream& log)
            {
                // Release existing shaders
                resources.shaders.Release();

                std::wstring root = std::wstring(d3d.shaderCompiler.root.begin(), d3d.shaderCompiler.root.end());

                // Load and compile the ray generation shader
                resources.shaders.rgs.filepath = root + L"shaders/PathTraceRGS.hlsl";
                resources.shaders.rgs.entryPoint = L"RayGen";
                resources.shaders.rgs.exportName = L"PathTraceRGS";
                Shaders::AddDefine(resources.shaders.rgs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                Shaders::AddDefine(resources.shaders.rgs, L"GFX_NVAPI", std::to_wstring(1));
                CHECK(Shaders::Compile(d3d.shaderCompiler, resources.shaders.rgs, true), "compile path tracing ray generation shader!\n", log);

                // Load and compile the miss shader
                resources.shaders.miss.filepath = root + L"shaders/Miss.hlsl";
                resources.shaders.miss.entryPoint = L"Miss";
                resources.shaders.miss.exportName = L"PathTraceMiss";
                Shaders::AddDefine(resources.shaders.miss, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                CHECK(Shaders::Compile(d3d.shaderCompiler, resources.shaders.miss, true), "compile path tracing miss shader!\n", log);

                // Add the hit group
                resources.shaders.hitGroups.emplace_back();

                Shaders::ShaderRTHitGroup& group = resources.shaders.hitGroups[0];
                group.exportName = L"PathTraceHitGroup";

                // Load and compile the CHS
                group.chs.filepath = root + L"shaders/CHS.hlsl";
                group.chs.entryPoint = L"CHS_LOD0";
                group.chs.exportName = L"PathTraceCHS";
                Shaders::AddDefine(group.chs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                CHECK(Shaders::Compile(d3d.shaderCompiler, group.chs, true), "compile path tracing closest hit shader!\n", log);

                // Load and compile the AHS
                group.ahs.filepath = root + L"shaders/AHS.hlsl";
                group.ahs.entryPoint = L"AHS_LOD0";
                group.ahs.exportName = L"PathTraceAHS";
                Shaders::AddDefine(group.ahs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                CHECK(Shaders::Compile(d3d.shaderCompiler, group.ahs, true), "compile path tracing any hit shader!\n", log);

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
                    "create path tracing RTPSO!\n", log);

            #ifdef GFX_NAME_OBJECTS
                resources.rtpso->SetName(L"Path Tracing RTPSO");
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
                SAFE_RELEASE(resources.shaderTableUpload);

                UINT shaderIdSize = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;

                resources.shaderTableRecordSize = shaderIdSize;
                resources.shaderTableRecordSize += 8;              // descriptor table GPUVA
                resources.shaderTableRecordSize += 8;              // sampler descriptor table GPUVA
                resources.shaderTableRecordSize = ALIGN(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, resources.shaderTableRecordSize);

                // 2 + numHitGroups shader records in the table
                resources.shaderTableSize = (2 + static_cast<UINT>(resources.shaders.hitGroups.size())) * resources.shaderTableRecordSize;
                resources.shaderTableSize = ALIGN(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, resources.shaderTableSize);

                // Create the shader table upload buffer resource
                BufferDesc desc = { resources.shaderTableSize, 0, EHeapType::UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE };
                CHECK(CreateBuffer(d3d, desc, &resources.shaderTableUpload), "create path tracing shader table upload buffer!", log);
            #ifdef GFX_NAME_OBJECTS
                resources.shaderTableUpload->SetName(L"Path Tracing Shader Table Upload");
            #endif

                // Create the shader table device buffer resource
                desc = { resources.shaderTableSize, 0, EHeapType::DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE };
                CHECK(CreateBuffer(d3d, desc, &resources.shaderTable), "create path tracing shader table!", log);
            #ifdef GFX_NAME_OBJECTS
                resources.shaderTable->SetName(L"Path Tracing Shader Table");
            #endif

                return true;
            }

            bool UpdateShaderTable(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
            {
                UINT shaderIdSize = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT;

                // Write shader table records to the upload buffer
                UINT8* pData = nullptr;
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
                for (UINT hitGroupIndex = 0; hitGroupIndex < static_cast<UINT>(resources.shaders.hitGroups.size()); hitGroupIndex++)
                {
                    pData += resources.shaderTableRecordSize;
                    memcpy(pData, resources.rtpsoInfo->GetShaderIdentifier(resources.shaders.hitGroups[hitGroupIndex].exportName), shaderIdSize);
                    *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize) = d3dResources.srvDescHeap->GetGPUDescriptorHandleForHeapStart();
                    *reinterpret_cast<D3D12_GPU_DESCRIPTOR_HANDLE*>(pData + shaderIdSize + 8) = d3dResources.samplerDescHeap->GetGPUDescriptorHandleForHeapStart();
                }
                resources.shaderTableHitGroupTableStartAddress = resources.shaderTableMissTableStartAddress + resources.shaderTableMissTableSize;
                resources.shaderTableHitGroupTableSize = static_cast<UINT>(resources.shaders.hitGroups.size()) * resources.shaderTableRecordSize;

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
             * Create resources used by the path tracing pass.
             */
            bool Initialize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
            {
                if (!CreateTextures(d3d, d3dResources, resources, log)) return false;
                if (!LoadAndCompileShaders(d3d, resources, log)) return false;
                if (!CreatePSOs(d3d, d3dResources, resources, log)) return false;
                if (!CreateShaderTable(d3d, d3dResources, resources, log)) return false;
                if (!UpdateShaderTable(d3d, d3dResources, resources, log)) return false;

                perf.AddStat("Path Tracing", resources.cpuStat, resources.gpuStat);

                return true;
            }

            /**
             * Reload and compile shaders, recreate PSOs, and recreate the shader table.
             */
            bool Reload(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
            {
                log << "Reloading Path Tracing shaders...";
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
                SAFE_RELEASE(resources.PTOutput);
                SAFE_RELEASE(resources.PTAccumulation);

                if (!CreateTextures(d3d, d3dResources, resources, log)) return false;

                log << "Path Tracing resize, " << d3d.width << "x" << d3d.height << "\n";
                std::flush(log);
                return true;
            }

            /**
             * Update data before execute.
             */
            void Update(Globals& d3d, GlobalResources& d3dResources, Resources& resources, const Configs::Config& config)
            {
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);

                // Path Trace constants
                d3dResources.constants.pt.rayNormalBias = config.pathTrace.rayNormalBias;
                d3dResources.constants.pt.rayViewBias = config.pathTrace.rayViewBias;
                d3dResources.constants.pt.numBounces = config.pathTrace.numBounces;
                d3dResources.constants.pt.samplesPerPixel = config.pathTrace.samplesPerPixel;
                d3dResources.constants.pt.SetAntialiasing(config.pathTrace.antialiasing);
                d3dResources.constants.pt.SetShaderExecutionReordering(config.pathTrace.shaderExecutionReordering);

                // Post Process constants
                d3dResources.constants.post.useFlags = POSTPROCESS_FLAG_USE_NONE;
                if (config.postProcess.enabled)
                {
                    if (config.postProcess.exposure.enabled) d3dResources.constants.post.useFlags |= POSTPROCESS_FLAG_USE_EXPOSURE;
                    if (config.postProcess.tonemap.enabled) d3dResources.constants.post.useFlags |= POSTPROCESS_FLAG_USE_TONEMAPPING;
                    if (config.postProcess.dither.enabled) d3dResources.constants.post.useFlags |= POSTPROCESS_FLAG_USE_DITHER;
                    if (config.postProcess.gamma.enabled) d3dResources.constants.post.useFlags |= POSTPROCESS_FLAG_USE_GAMMA;
                    d3dResources.constants.post.exposure = pow(2.f, config.postProcess.exposure.fstops);
                }

                CPU_TIMESTAMP_END(resources.cpuStat);
            }

            /**
             * Record the workload to the global command list.
             */
            void Execute(Globals& d3d, GlobalResources& d3dResources, Resources& resources)
            {
            #ifdef GFX_PERF_MARKERS
                PIXBeginEvent(d3d.cmdList, PIX_COLOR(GFX_PERF_MARKER_YELLOW), "Path Tracing");
            #endif
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);

                D3D12_RESOURCE_BARRIER barriers[2] = {};

                // Transition the output buffer to UAV (from a copy source)
                barriers[0].Transition.pResource = resources.PTOutput;
                barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
                barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

                // Wait for the transitions to complete
                d3d.cmdList->ResourceBarrier(1, &barriers[0]);

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
                offset += LightingConsts::GetAlignedNum32BitValues();
                offset += RTAOConsts::GetAlignedNum32BitValues();
                offset += CompositeConsts::GetAlignedNum32BitValues();
                d3d.cmdList->SetComputeRoot32BitConstants(0, PostProcessConsts::GetNum32BitValues(), consts.post.GetData(), offset);

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

                // Transition the output buffer to a copy source (from UAV)
                barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

                // Transition the back buffer to a copy destination
                barriers[1].Transition.pResource = d3d.backBuffer[d3d.frameIndex];
                barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
                barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
                barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

                // Wait for the transitions to complete
                d3d.cmdList->ResourceBarrier(2, barriers);

                // Copy the output to the back buffer
                d3d.cmdList->CopyResource(d3d.backBuffer[d3d.frameIndex], resources.PTOutput);

                // Transition back buffer to present (from a copy destination)
                barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

                // Wait for the buffer transitions to complete
                d3d.cmdList->ResourceBarrier(1, &barriers[1]);

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
                SAFE_RELEASE(resources.PTOutput);
                SAFE_RELEASE(resources.PTAccumulation);

                SAFE_RELEASE(resources.shaderTable);
                SAFE_RELEASE(resources.shaderTableUpload);
                resources.shaders.Release();

                SAFE_RELEASE(resources.rtpsoInfo);
                SAFE_RELEASE(resources.rtpso);

                resources.shaderTableSize = 0;
                resources.shaderTableRecordSize = 0;
                resources.shaderTableMissTableSize = 0;
                resources.shaderTableHitGroupTableSize = 0;

                resources.shaderTableRGSStartAddress = 0;
                resources.shaderTableMissTableStartAddress = 0;
                resources.shaderTableHitGroupTableStartAddress = 0;
            }

        } // namespace Graphics::D3D12::PathTracing

    } // namespace Graphics::D3D12

    namespace PathTracing
    {

        bool Initialize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
        {
            return Graphics::D3D12::PathTracing::Initialize(d3d, d3dResources, resources, perf, log);
        }

        bool Reload(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::D3D12::PathTracing::Reload(d3d, d3dResources, resources, log);
        }

        bool Resize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::D3D12::PathTracing::Resize(d3d, d3dResources, resources, log);
        }

        void Update(Globals& d3d, GlobalResources& d3dResources, Resources& resources, const Configs::Config& config)
        {
            return Graphics::D3D12::PathTracing::Update(d3d, d3dResources, resources, config);
        }

        void Execute(Globals& d3d, GlobalResources& d3dResources, Resources& resources)
        {
            return Graphics::D3D12::PathTracing::Execute(d3d, d3dResources, resources);
        }

        void Cleanup(Globals& d3d, Resources& resources)
        {
            Graphics::D3D12::PathTracing::Cleanup(resources);
        }

    } // namespace Graphics::PathTracing
}
