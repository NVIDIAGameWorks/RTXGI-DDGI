/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "graphics/Composite.h"

namespace Graphics
{
    namespace D3D12
    {
        namespace Composite
        {

            //----------------------------------------------------------------------------------------------------------
            // Private Functions
            //----------------------------------------------------------------------------------------------------------

            bool LoadAndCompileShaders(Globals& d3d, Resources& resources, std::ofstream& log)
            {
                // Release existing shaders
                resources.shaders.Release();

                std::wstring root = std::wstring(d3d.shaderCompiler.root.begin(), d3d.shaderCompiler.root.end());

                // Load and compile the vertex shader
                resources.shaders.vs.filepath = root + L"shaders/Composite.hlsl";
                resources.shaders.vs.entryPoint = L"VS";
                resources.shaders.vs.targetProfile = L"vs_6_6";
                Shaders::AddDefine(resources.shaders.vs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                CHECK(Shaders::Compile(d3d.shaderCompiler, resources.shaders.vs, true), "compile composition vertex shader!\n", log);

                // Load and compile the pixel shader
                resources.shaders.ps.filepath = root + L"shaders/Composite.hlsl";
                resources.shaders.ps.entryPoint = L"PS";
                resources.shaders.ps.targetProfile = L"ps_6_6";
                Shaders::AddDefine(resources.shaders.ps, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE));
                CHECK(Shaders::Compile(d3d.shaderCompiler, resources.shaders.ps, true), "compile composition pixel shader!\n", log);

                return true;
            }

            bool CreatePSOs(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
            {
                // Release existing PSOs
                SAFE_RELEASE(resources.pso);

                // Describe the rasterizer properties
                RasterDesc desc = {};

                // Describe the vertex input layout
                D3D12_INPUT_ELEMENT_DESC inputLayoutDescs[] =
                {
                    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                };
                desc.numInputLayouts = _countof(inputLayoutDescs);
                desc.inputLayoutDescs = inputLayoutDescs;

                // Describe raster blending
                desc.blendDesc.RenderTarget[0] =
                {
                    FALSE,FALSE,
                    D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                    D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
                    D3D12_LOGIC_OP_NOOP,
                    D3D12_COLOR_WRITE_ENABLE_ALL,
                };

                // Describe the rasterizer state
                desc.rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
                desc.rasterDesc.CullMode = D3D12_CULL_MODE_NONE;

                // Create the PSO
                CHECK(CreateRasterPSO(
                    d3d.device,
                    d3dResources.rootSignature,
                    resources.shaders,
                    desc,
                    &resources.pso),
                    "create composition raster PSO!\n", log);

            #ifdef GFX_NAME_OBJECTS
                resources.pso->SetName(L"Composition PSO");
            #endif

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
                if(!LoadAndCompileShaders(d3d, resources, log)) return false;
                if(!CreatePSOs(d3d, d3dResources, resources, log)) return false;

                perf.AddStat("Composite", resources.cpuStat, resources.gpuStat);

                return true;
            }

            /**
             * Reload and compile shaders and recreate PSOs.
             */
            bool Reload(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
            {
                log << "Reloading Composition shaders...";
                if (!LoadAndCompileShaders(d3d, resources, log)) return false;
                if (!CreatePSOs(d3d, d3dResources, resources, log)) return false;
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

                // Composite constants
                d3dResources.constants.composite.useFlags = COMPOSITE_FLAG_USE_NONE;
                if (config.rtao.enabled) d3dResources.constants.composite.useFlags |= COMPOSITE_FLAG_USE_RTAO;
                if (config.ddgi.enabled) d3dResources.constants.composite.useFlags |= COMPOSITE_FLAG_USE_DDGI;

                d3dResources.constants.composite.showFlags = COMPOSITE_FLAG_SHOW_NONE;
                if(config.rtao.visualize) d3dResources.constants.composite.showFlags |=  COMPOSITE_FLAG_SHOW_RTAO;
                if(config.ddgi.showIndirect) d3dResources.constants.composite.showFlags |=  COMPOSITE_FLAG_SHOW_DDGI_INDIRECT;

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
                PIXBeginEvent(d3d.cmdList, PIX_COLOR(GFX_PERF_MARKER_BLUE), "Composite");
            #endif
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);

                // Transition the back buffer to a render target
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Transition.pResource = d3d.backBuffer[d3d.frameIndex];
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

                // Wait for the transition to complete
                d3d.cmdList->ResourceBarrier(1, &barrier);

                // Set the CBV/SRV/UAV and sampler descriptor heaps
                ID3D12DescriptorHeap* ppHeaps[] = { d3dResources.srvDescHeap, d3dResources.samplerDescHeap };
                d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

                // Set the root signature
                d3d.cmdList->SetGraphicsRootSignature(d3dResources.rootSignature);

                // Update the root constants
                UINT offset = 0;
                GlobalConstants consts = d3dResources.constants;
                d3d.cmdList->SetGraphicsRoot32BitConstants(0, AppConsts::GetNum32BitValues(), consts.app.GetData(), offset);
                offset += AppConsts::GetAlignedNum32BitValues();
                offset += PathTraceConsts::GetAlignedNum32BitValues();
                offset += LightingConsts::GetAlignedNum32BitValues();
                offset += RTAOConsts::GetAlignedNum32BitValues();
                d3d.cmdList->SetGraphicsRoot32BitConstants(0, CompositeConsts::GetNum32BitValues(), consts.composite.GetData(), offset);
                offset += CompositeConsts::GetAlignedNum32BitValues();
                d3d.cmdList->SetGraphicsRoot32BitConstants(0, PostProcessConsts::GetNum32BitValues(), consts.post.GetData(), offset);

                // Set the render target
                D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = d3dResources.rtvDescHeap->GetCPUDescriptorHandleForHeapStart();
                rtvHandle.ptr += (d3dResources.rtvDescHeapEntrySize * d3d.frameIndex);
                d3d.cmdList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

                // Set the root parameter descriptor tables
            #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS
                d3d.cmdList->SetGraphicsRootDescriptorTable(2, d3dResources.samplerDescHeap->GetGPUDescriptorHandleForHeapStart());
                d3d.cmdList->SetGraphicsRootDescriptorTable(3, d3dResources.srvDescHeap->GetGPUDescriptorHandleForHeapStart());
            #endif

                // Set raster state
                d3d.cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                d3d.cmdList->RSSetViewports(1, &d3d.viewport);
                d3d.cmdList->RSSetScissorRects(1, &d3d.scissor);

                // Set the pipeline state object
                d3d.cmdList->SetPipelineState(resources.pso);

                // Draw
                GPU_TIMESTAMP_BEGIN(resources.gpuStat->GetGPUQueryBeginIndex());
                d3d.cmdList->DrawInstanced(3, 1, 0, 0);
                GPU_TIMESTAMP_END(resources.gpuStat->GetGPUQueryEndIndex());

                // Transition the back buffer to present
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

                // Wait for the transition to complete
                d3d.cmdList->ResourceBarrier(1, &barrier);

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
                resources.shaders.Release();
                SAFE_RELEASE(resources.pso);
            }

        } // namespace Graphics::D3D12::Composite

    } // namespace Graphics::D3D12

    namespace Composite
    {

        bool Initialize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
        {
            return Graphics::D3D12::Composite::Initialize(d3d, d3dResources, resources, perf, log);
        }

        bool Reload(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::D3D12::Composite::Reload(d3d, d3dResources, resources, log);
        }

        bool Resize(Globals& d3d, GlobalResources& d3dResources, Resources& resources, std::ofstream& log)
        {
            return true; // nothing to do here in D3D12
        }

        void Update(Globals& d3d, GlobalResources& d3dResources, Resources& resources, const Configs::Config& config)
        {
            return Graphics::D3D12::Composite::Update(d3d, d3dResources, resources, config);
        }

        void Execute(Globals& d3d, GlobalResources& d3dResources, Resources& resources)
        {
            return Graphics::D3D12::Composite::Execute(d3d, d3dResources, resources);
        }

        void Cleanup(Globals& d3d, Resources& resources)
        {
            Graphics::D3D12::Composite::Cleanup(resources);
        }

    } // namespace Graphics::Composite
}
