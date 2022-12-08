/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Graphics.h"
#include "UI.h"
#include "ImageCapture.h"

#if GFX_NVAPI
#include "nvapi.h"
#include "nvShaderExtnEnums.h"

#define NV_SHADER_EXTN_SLOT           999999
#define NV_SHADER_EXTN_REGISTER_SPACE 999999
#endif

namespace Graphics
{
    using namespace DirectX;

    namespace D3D12
    {
        //----------------------------------------------------------------------------------------------------------
        // Private Functions
        //----------------------------------------------------------------------------------------------------------

        bool Check(HRESULT hr, std::string fileName, uint32_t lineNumber)
        {
            if (hr != S_OK)
            {
                std::string msg = "D3D call failed in:\n" + fileName + " at line " + std::to_string(lineNumber);
                Graphics::UI::MessageBox(msg);
                return false;
            }
            return true;
        }

        /**
         * Check whether tearing support is available for fullscreen borderless windows.
         */
        bool CheckTearingSupport(Globals& d3d)
        {
            BOOL allowTearing = FALSE;
            D3DCHECK(d3d.factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing)));

            d3d.allowTearing = (bool)allowTearing;
            return true;
        }

        /**
         * Convert wide strings to narrow strings.
         */
        void ConvertWideStringToNarrow(std::wstring& wide, std::string& narrow)
        {
            narrow.resize(wide.size());
        #if defined(_WIN32) || defined(WIN32)
            size_t converted = 0;
            wcstombs_s(&converted, narrow.data(), (narrow.size() + 1), wide.c_str(), wide.size());
        #else
            wcstombs(narrow.data(), wide.c_str(), narrow.size() + 1);
        #endif
        }

        /**
         * Device creation helper.
         */
        bool CreateDeviceInternal(Globals& d3d, Configs::Config& config)
        {
            // Create the device
            IDXGIAdapter1* adapter = nullptr;
            for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != d3d.factory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
            {
                DXGI_ADAPTER_DESC1 adapterDesc;
                adapter->GetDesc1(&adapterDesc);
                if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    continue;   // Don't select the Basic Render Driver adapter
                }

                if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, _uuidof(ID3D12Device6), (void**)&d3d.device)))
                {
                    // Check if the device supports ray tracing
                    D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5 = {};
                    HRESULT hr = d3d.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
                    if (FAILED(hr) || features5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
                    {
                        SAFE_RELEASE(d3d.device);
                        d3d.device = nullptr;
                        continue;
                    }

                    // Check if the device supports SM6.6
                    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = {};
                    shaderModel.HighestShaderModel = D3D_SHADER_MODEL_6_6;
                    hr = d3d.device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(D3D12_FEATURE_DATA_SHADER_MODEL));
                    if (FAILED(hr))
                    {
                        SAFE_RELEASE(d3d.device);
                        d3d.device = nullptr;
                        continue;
                    }

                    // Resource binding tier 3 is required for SM6.6 dynamic resources
                    D3D12_FEATURE_DATA_D3D12_OPTIONS features = {};
                    hr = d3d.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &features, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS));
                    if (FAILED(hr) || features.ResourceBindingTier < D3D12_RESOURCE_BINDING_TIER_3)
                    {
                        SAFE_RELEASE(d3d.device);
                        d3d.device = nullptr;
                        continue;
                    }

                #if GFX_NVAPI
                    // Check for SER HLSL extension support
                    NvAPI_Status status = NvAPI_D3D12_IsNvShaderExtnOpCodeSupported(
                        d3d.device,
                        NV_EXTN_OP_HIT_OBJECT_REORDER_THREAD,
                        &d3d.supportsShaderExecutionReordering);

                    if (status == NVAPI_OK && d3d.supportsShaderExecutionReordering)
                    {
                        // Check for SER device support
                        NVAPI_D3D12_RAYTRACING_THREAD_REORDERING_CAPS ReorderCaps = NVAPI_D3D12_RAYTRACING_THREAD_REORDERING_CAP_NONE;
                        status = NvAPI_D3D12_GetRaytracingCaps(
                            d3d.device,
                            NVAPI_D3D12_RAYTRACING_CAPS_TYPE_THREAD_REORDERING,
                            &ReorderCaps,
                            sizeof(ReorderCaps));

                        if (status != NVAPI_OK || ReorderCaps == NVAPI_D3D12_RAYTRACING_THREAD_REORDERING_CAP_NONE)
                        {
                            d3d.supportsShaderExecutionReordering = false;
                        }
                    }
                #endif

                    D3D12_FEATURE_DATA_D3D12_OPTIONS1 waveFeatures = {};
                    hr = d3d.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &waveFeatures, sizeof(waveFeatures));
                    if (SUCCEEDED(hr))
                    {
                        d3d.features.waveLaneCount = waveFeatures.WaveLaneCountMin;
                    }

                    // Set the graphics API name
                    config.app.api = "Direct3D 12";

                    // Save the GPU name
                    std::wstring name(adapterDesc.Description);
                    ConvertWideStringToNarrow(name, config.app.gpuName);
                #ifdef GFX_NAME_OBJECTS
                    d3d.device->SetName(name.c_str());
                #endif
                    break;
                }

                if (d3d.device == nullptr)
                {
                    return false; // Didn't find a device that supports ray tracing
                }
            }

            if (adapter) { SAFE_RELEASE(adapter); }
            return true;
        }

        /**
         * Create the command queue.
         */
        bool CreateCmdQueue(Globals& d3d)
        {
            D3D12_COMMAND_QUEUE_DESC desc = {};
            desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

            D3DCHECK(d3d.device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d.cmdQueue)));
        #ifdef GFX_NAME_OBJECTS
            d3d.cmdQueue->SetName(L"Command Queue");
        #endif
            return true;
        }

        /**
         * Create a command allocator for each frame.
         */
        bool CreateCmdAllocators(Globals& d3d)
        {
            for (UINT i = 0; i < 2; i++)
            {
                D3DCHECK(d3d.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d.cmdAlloc[i])));
            #ifdef GFX_NAME_OBJECTS
                std::wstring name = L"Command Allocator " + std::to_wstring(i);
                d3d.cmdAlloc[i]->SetName(name.c_str());
            #endif
            }
            return true;
        }

        /**
         * Create the command list.
         */
        bool CreateCmdList(Globals& d3d)
        {
            D3DCHECK(d3d.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d.cmdAlloc[d3d.frameIndex], nullptr, IID_PPV_ARGS(&d3d.cmdList)));
            D3DCHECK(d3d.cmdList->Close());
        #ifdef GFX_NAME_OBJECTS
            d3d.cmdList->SetName(L"Command List");
        #endif
            return true;
        }

        /**
         * Create the fence and event handle.
         */
        bool CreateFence(Globals& d3d)
        {
            // Create the fence
            D3DCHECK(d3d.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d.fence)));
        #ifdef GFX_NAME_OBJECTS
            d3d.fence->SetName(L"Fence");
        #endif

            // Initialize the fence
            d3d.fence->Signal(d3d.fenceValue);

            // Create the event handle to use for frame synchronization
            d3d.fenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
            if (d3d.fenceEvent == nullptr)
            {
                if (FAILED(HRESULT_FROM_WIN32(GetLastError()))) return false;
            }
            return true;
        }

        /**
         * Create a swap chain.
         */
        bool CreateSwapChain(Globals& d3d)
        {
            // Describe the swap chain
            DXGI_SWAP_CHAIN_DESC1 desc = {};
            desc.BufferCount = 2;
            desc.Width = d3d.width;
            desc.Height = d3d.height;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            desc.SampleDesc.Count = 1;
            desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
            if(d3d.allowTearing) desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

            // Get the native window from GLFW
            HWND window = glfwGetWin32Window(d3d.window);

            // Create the swap chain
            IDXGISwapChain1* swapChain = nullptr;
            D3DCHECK(d3d.factory->CreateSwapChainForHwnd(d3d.cmdQueue, window, &desc, nullptr, nullptr, &swapChain));

            // Associate the swap chain with a window, disable ALT+Enter DXGI shortcut
            D3DCHECK(d3d.factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));

            // Get the swap chain interface
            D3DCHECK(swapChain->QueryInterface(__uuidof(IDXGISwapChain4), reinterpret_cast<void**>(&d3d.swapChain)));

            SAFE_RELEASE(swapChain);
            d3d.frameIndex = d3d.swapChain->GetCurrentBackBufferIndex();

            return true;
        }

        /**
         * Create the RTV, Resource, and Sampler descriptor heaps.
         */
        bool CreateDescriptorHeaps(Globals& d3d, Resources& resources, const Scenes::Scene& scene)
        {
            // Describe the RTV heap
            D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
            rtvDesc.NumDescriptors = 2;
            rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

            // Create the RTV heap
            D3DCHECK(d3d.device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&resources.rtvDescHeap)));
        #ifdef GFX_NAME_OBJECTS
            resources.rtvDescHeap->SetName(L"RTV Descriptor Heap");
        #endif

            resources.rtvDescHeapStart = resources.rtvDescHeap->GetCPUDescriptorHandleForHeapStart();
            resources.rtvDescHeapEntrySize = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

            // Describe the sampler heap
            // 1 bilinear wrap sampler
            // 1 point clamp sampler
            // 1 aniso wrap sampler
            D3D12_DESCRIPTOR_HEAP_DESC samplerDesc = {};
            samplerDesc.NumDescriptors = 3;
            samplerDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            samplerDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            // Create the sampler heap
            D3DCHECK(d3d.device->CreateDescriptorHeap(&samplerDesc, IID_PPV_ARGS(&resources.samplerDescHeap)));
        #ifdef GFX_NAME_OBJECTS
            resources.samplerDescHeap->SetName(L"Sampler Descriptor Heap");
        #endif

            resources.samplerDescHeapStart = resources.samplerDescHeap->GetCPUDescriptorHandleForHeapStart();
            resources.samplerDescHeapEntrySize = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

            // See Graphics.h::DescriptorHeapOffsets for the descriptor heap layout
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.NumDescriptors = DescriptorHeapOffsets::SRV_INDICES + (2 * scene.numMeshPrimitives);
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            // Create the resources descriptor heap
            D3DCHECK(d3d.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&resources.srvDescHeap)));
        #ifdef GFX_NAME_OBJECTS
            resources.srvDescHeap->SetName(L"Descriptor Heap");
        #endif

            resources.srvDescHeapStart = resources.srvDescHeap->GetCPUDescriptorHandleForHeapStart();
            resources.srvDescHeapEntrySize = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            return true;
        }

        /**
         * Create the timestamp query heap.
         */
        bool CreateQueryHeaps(Globals& d3d, Resources& resources)
        {
            // Describe the timestamp query heap
            D3D12_QUERY_HEAP_DESC desc = {};
            desc.Count = MAX_TIMESTAMPS;
            desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;

            // Create the timestamp query heap
            D3DCHECK(d3d.device->CreateQueryHeap(&desc, IID_PPV_ARGS(&resources.timestampHeap)));

            // Describe the timestamps resource (read-back)
            D3D12_RESOURCE_DESC resourceDesc = {};
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            resourceDesc.Width = MAX_TIMESTAMPS * sizeof(UINT64);
            resourceDesc.Height = 1;
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            resourceDesc.MipLevels = 1;
            resourceDesc.SampleDesc.Count = 1;

            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_READBACK;

            // Create the timestamps resource
            D3DCHECK(d3d.device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resources.timestamps)));

            // Get the frequency of timestamp ticks
            D3DCHECK(d3d.cmdQueue->GetTimestampFrequency(&resources.timestampFrequency));

            return true;
        }

        /**
         * Create the back buffer and RTV.
         */
        bool CreateBackBuffer(Globals& d3d, Resources& resources)
        {
            // Create a RTV for each back buffer
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = resources.rtvDescHeapStart;
            for (UINT bufferIndex = 0; bufferIndex < 2; bufferIndex++)
            {
                D3DCHECK(d3d.swapChain->GetBuffer(bufferIndex, IID_PPV_ARGS(&d3d.backBuffer[bufferIndex])));

                d3d.device->CreateRenderTargetView(d3d.backBuffer[bufferIndex], nullptr, rtvHandle);

            #ifdef GFX_NAME_OBJECTS
                if (bufferIndex == 0)
                {
                    d3d.backBuffer[bufferIndex]->SetName(L"Back Buffer 0");
                }
                else
                {
                    d3d.backBuffer[bufferIndex]->SetName(L"Back Buffer 1");
                }
            #endif

                rtvHandle.ptr += resources.rtvDescHeapEntrySize;
            }
            return true;
        }

        /**
         * Create the raster viewport.
         */
        bool CreateViewport(Globals& d3d)
        {
            d3d.viewport.Width = static_cast<float>(d3d.width);
            d3d.viewport.Height = static_cast<float>(d3d.height);
            d3d.viewport.MinDepth = D3D12_MIN_DEPTH;
            d3d.viewport.MaxDepth = D3D12_MAX_DEPTH;
            d3d.viewport.TopLeftX = 0.f;
            d3d.viewport.TopLeftY = 0.f;
            return true;
        }

        /**
         * Create the raster scissor.
         */
        bool CreateScissor(Globals& d3d)
        {
            d3d.scissor.left = 0;
            d3d.scissor.top = 0;
            d3d.scissor.right = d3d.width;
            d3d.scissor.bottom = d3d.height;
            return true;
        }

        /**
         * Create the samplers.
         */
        bool CreateSamplers(Globals& d3d, Resources& resources)
        {
            // Describe a bilinear sampler
            D3D12_SAMPLER_DESC desc = {};
            memset(&desc, 0, sizeof(desc));
            desc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            desc.MipLODBias = 0.f;
            desc.MinLOD = 0.f;
            desc.MaxLOD = D3D12_FLOAT32_MAX;
            desc.MaxAnisotropy = 1;

            D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.samplerDescHeapStart;
            handle.ptr = resources.samplerDescHeapStart.ptr + (resources.samplerDescHeapEntrySize * SamplerHeapOffsets::BILINEAR_WRAP);

            // Create the bilinear wrap sampler
            d3d.device->CreateSampler(&desc, resources.samplerDescHeapStart);

            // Describe and create a point sampler
            desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
            desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

            handle.ptr = resources.samplerDescHeapStart.ptr + (resources.samplerDescHeapEntrySize * SamplerHeapOffsets::POINT_CLAMP);

            d3d.device->CreateSampler(&desc, handle);

            // Describe and create an anisotropic sampler
            desc.Filter = D3D12_FILTER_ANISOTROPIC;
            desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            desc.MaxAnisotropy = 16;

            handle.ptr = resources.samplerDescHeapStart.ptr + (resources.samplerDescHeapEntrySize * SamplerHeapOffsets::ANISO);

            d3d.device->CreateSampler(&desc, handle);

            return true;
        }

        /**
         * Create the index buffer for a mesh.
         * Copy the index data to the upload buffer and schedule a copy to the device buffer.
         */
        bool CreateIndexBuffer(Globals& d3d, const Scenes::Mesh& mesh, ID3D12Resource** device, ID3D12Resource** upload, D3D12_INDEX_BUFFER_VIEW& view)
        {
            // Create the index buffer upload resource
            UINT sizeInBytes = mesh.numIndices * sizeof(UINT);
            BufferDesc desc = { sizeInBytes, 0, EHeapType::UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE };
            if (!CreateBuffer(d3d, desc, upload)) return false;

            // Create the index buffer device resource
            desc = { sizeInBytes, 0, EHeapType::DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE };
            if (!CreateBuffer(d3d, desc, device)) return false;

            // Initialize the index buffer view
            view.Format = DXGI_FORMAT_R32_UINT;
            view.SizeInBytes = sizeInBytes;
            view.BufferLocation = (*device)->GetGPUVirtualAddress();

            // Copy the index data of each mesh primitive to the upload buffer
            UINT8* pData = nullptr;
            D3D12_RANGE readRange = {};
            D3DCHECK((*upload)->Map(0, &readRange, reinterpret_cast<void**>(&pData)));

            for (UINT primitiveIndex = 0; primitiveIndex < static_cast<UINT>(mesh.primitives.size()); primitiveIndex++)
            {
                // Get the mesh primitive and copy its indices to the upload buffer
                const Scenes::MeshPrimitive& primitive = mesh.primitives[primitiveIndex];

                UINT size = static_cast<UINT>(primitive.indices.size()) * sizeof(UINT);
                memcpy(pData + primitive.indexByteOffset, primitive.indices.data(), size);
            }
            (*upload)->Unmap(0, nullptr);

            // Schedule a copy of the upload buffer to the device buffer
            d3d.cmdList->CopyBufferRegion(*device, 0, *upload, 0, sizeInBytes);

            // Transition the default heap resource to generic read after the copy is complete
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = *device;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            d3d.cmdList->ResourceBarrier(1, &barrier);

            return true;
        }

        /**
         * Create the vertex buffer for a mesh.
         * Copy the vertex data to the upload buffer and schedule a copy to the device buffer.
         */
        bool CreateVertexBuffer(Globals& d3d, const Scenes::Mesh& mesh, ID3D12Resource** device, ID3D12Resource** upload, D3D12_VERTEX_BUFFER_VIEW& view)
        {
            // Create the vertex buffer upload resource
            UINT stride = sizeof(Vertex);
            UINT sizeInBytes = mesh.numVertices * stride;
            BufferDesc desc = { sizeInBytes, 0, EHeapType::UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE };
            if (!CreateBuffer(d3d, desc, upload)) return false;

            // Create the vertex buffer device resource
            desc = { sizeInBytes, 0, EHeapType::DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE };
            if (!CreateBuffer(d3d, desc, device)) return false;

            // Initialize the vertex buffer view
            view.StrideInBytes = stride;
            view.SizeInBytes = sizeInBytes;
            view.BufferLocation = (*device)->GetGPUVirtualAddress();

            // Copy the vertex data of each mesh primitive to the upload buffer
            UINT8* pData = nullptr;
            D3D12_RANGE readRange = {};
            D3DCHECK((*upload)->Map(0, &readRange, reinterpret_cast<void**>(&pData)));

            for (UINT primitiveIndex = 0; primitiveIndex < static_cast<UINT>(mesh.primitives.size()); primitiveIndex++)
            {
                // Get the mesh primitive and copy its vertices to the upload buffer
                const Scenes::MeshPrimitive& primitive = mesh.primitives[primitiveIndex];

                UINT size = static_cast<UINT>(primitive.vertices.size()) * stride;
                memcpy(pData + primitive.vertexByteOffset, primitive.vertices.data(), size);
            }
            (*upload)->Unmap(0, nullptr);

            // Schedule a copy of the upload buffer to the device buffer
            d3d.cmdList->CopyBufferRegion(*device, 0, *upload, 0, sizeInBytes);

            // Transition the default heap resource to generic read after the copy is complete
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = *device;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            d3d.cmdList->ResourceBarrier(1, &barrier);

            return true;
        }

        /**
         * Create a bottom level acceleration structure for a mesh.
         */
        bool CreateBLAS(Globals& d3d, Resources& resources, const Scenes::Mesh& mesh)
        {
            // Describe the mesh primitives
            std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> primitives;

            D3D12_RAYTRACING_GEOMETRY_DESC desc = {};
            desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            for (UINT primitiveIndex = 0; primitiveIndex < static_cast<UINT>(mesh.primitives.size()); primitiveIndex++)
            {
                // Get the mesh primitive
                const Scenes::MeshPrimitive& primitive = mesh.primitives[primitiveIndex];

                desc.Triangles.VertexBuffer.StartAddress = resources.sceneVBs[mesh.index]->GetGPUVirtualAddress() + primitive.vertexByteOffset;
                desc.Triangles.VertexBuffer.StrideInBytes = resources.sceneVBViews[mesh.index].StrideInBytes;
                desc.Triangles.VertexCount = static_cast<UINT>(primitive.vertices.size());
                desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
                desc.Triangles.IndexBuffer = resources.sceneIBs[mesh.index]->GetGPUVirtualAddress() + primitive.indexByteOffset;
                desc.Triangles.IndexFormat = resources.sceneIBViews[mesh.index].Format;
                desc.Triangles.IndexCount = static_cast<UINT>(primitive.indices.size());
                desc.Flags = primitive.opaque ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;

                primitives.push_back(desc);
            }

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

            // Describe the bottom level acceleration structure inputs
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS asInputs = {};
            asInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
            asInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            asInputs.NumDescs = static_cast<UINT>(primitives.size());
            asInputs.pGeometryDescs = primitives.data();
            asInputs.Flags = buildFlags;

            // Get the size requirements for the BLAS buffer
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO asPreBuildInfo = {};
            d3d.device->GetRaytracingAccelerationStructurePrebuildInfo(&asInputs, &asPreBuildInfo);
            asPreBuildInfo.ScratchDataSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, asPreBuildInfo.ScratchDataSizeInBytes);
            asPreBuildInfo.ResultDataMaxSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, asPreBuildInfo.ResultDataMaxSizeInBytes);

            // Create the BLAS scratch buffer
            BufferDesc blasScratchDesc =
            {
                asPreBuildInfo.ScratchDataSizeInBytes,
                D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT,
                EHeapType::DEFAULT,
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            };
            if (!CreateBuffer(d3d, blasScratchDesc, &resources.blas[mesh.index].scratch)) return false;
        #ifdef GFX_NAME_OBJECTS
            std::wstring name = L"BLAS: " + std::wstring(mesh.name.begin(), mesh.name.end()) + L" (scratch)";
            resources.blas[mesh.index].scratch->SetName(name.c_str());
        #endif

            // Create the BLAS buffer
            BufferDesc blasDesc =
            {
                asPreBuildInfo.ResultDataMaxSizeInBytes,
                D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT,
                EHeapType::DEFAULT,
                D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            };
            if (!CreateBuffer(d3d, blasDesc, &resources.blas[mesh.index].as)) return false;
        #ifdef GFX_NAME_OBJECTS
            name = L"BLAS: " + std::wstring(mesh.name.begin(), mesh.name.end());
            resources.blas[mesh.index].as->SetName(name.c_str());
        #endif

            // Describe and build the BLAS
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
            buildDesc.Inputs = asInputs;
            buildDesc.ScratchAccelerationStructureData = resources.blas[mesh.index].scratch->GetGPUVirtualAddress();
            buildDesc.DestAccelerationStructureData = resources.blas[mesh.index].as->GetGPUVirtualAddress();

            d3d.cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

            return true;
        }

        /**
         * Create a top level acceleration structure for a scene.
         */
        bool CreateTLAS(Globals& d3d, Resources& resources, const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instances, const std::string debugName = "")
        {
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

            // Get the size requirements for the TLAS buffers
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS ASInputs = {};
            ASInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
            ASInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            ASInputs.InstanceDescs = resources.tlas.instances->GetGPUVirtualAddress();
            ASInputs.NumDescs = static_cast<UINT>(instances.size());
            ASInputs.Flags = buildFlags;

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO ASPreBuildInfo = {};
            d3d.device->GetRaytracingAccelerationStructurePrebuildInfo(&ASInputs, &ASPreBuildInfo);
            ASPreBuildInfo.ResultDataMaxSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ResultDataMaxSizeInBytes);
            ASPreBuildInfo.ScratchDataSizeInBytes = ALIGN(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT, ASPreBuildInfo.ScratchDataSizeInBytes);

            // Create TLAS scratch buffer resource
            BufferDesc desc =
            {
                ASPreBuildInfo.ScratchDataSizeInBytes,
                D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT,
                EHeapType::DEFAULT,
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
            };
            if (!CreateBuffer(d3d, desc, &resources.tlas.scratch)) return false;
        #ifdef GFX_NAME_OBJECTS
            resources.tlas.scratch->SetName(L"Scene TLAS Scratch");
        #endif

            // Create the TLAS buffer resource
            desc.size = ASPreBuildInfo.ResultDataMaxSizeInBytes;
            desc.state = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
            if (!CreateBuffer(d3d, desc, &resources.tlas.as)) return false;
        #ifdef GFX_NAME_OBJECTS
            resources.tlas.as->SetName(L"Scene TLAS");
        #endif

            // Describe and build the TLAS
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
            buildDesc.Inputs = ASInputs;
            buildDesc.ScratchAccelerationStructureData = resources.tlas.scratch->GetGPUVirtualAddress();
            buildDesc.DestAccelerationStructureData = resources.tlas.as->GetGPUVirtualAddress();

            d3d.cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

            // Wait for the TLAS build to complete
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            barrier.UAV.pResource = resources.tlas.as;

            d3d.cmdList->ResourceBarrier(1, &barrier);

            return true;
        }

        /**
         * Create GPU heap resources, upload the texture, and schedule a copy from the GPU upload to default heap.
         */
        bool CreateAndUploadTexture(Globals& d3d, Resources& resources, const Textures::Texture& texture, std::ofstream& log)
        {
            std::vector<ID3D12Resource*>* tex;
            std::vector<ID3D12Resource*>* upload;
            if (texture.type == Textures::ETextureType::SCENE)
            {
                tex = &resources.sceneTextures;
                upload = &resources.sceneTextureUploadBuffers;
            }
            else if (texture.type == Textures::ETextureType::ENGINE)
            {
                tex = &resources.textures;
                upload = &resources.textureUploadBuffers;
            }

            tex->emplace_back();
            upload->emplace_back();

            ID3D12Resource*& resource = tex->back();
            ID3D12Resource*& uploadBuffer = upload->back();

            // Create the default heap texture resource
            {
                TextureDesc desc = { texture.width, texture.height, 1, texture.mips, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE };
                if(texture.format == Textures::ETextureFormat::BC7) desc.format = DXGI_FORMAT_BC7_TYPELESS;
                CHECK(CreateTexture(d3d, desc, &resource), "create the texture default heap resource!", log);
            #ifdef GFX_NAME_OBJECTS
                std::string name = "Texture: " + texture.name;
                std::wstring wname = std::wstring(name.begin(), name.end());
                resource->SetName(wname.c_str());
            #endif
            }

            // Create the upload heap buffer resource
            {
                BufferDesc desc = { texture.texelBytes, 1, EHeapType::UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ , D3D12_RESOURCE_FLAG_NONE };
                CHECK(CreateBuffer(d3d, desc, &uploadBuffer), "create the texture upload heap buffer!", log);
            #ifdef GFX_NAME_OBJECTS
                std::string name = " Texture Upload Buffer: " + texture.name;
                std::wstring wname = std::wstring(name.begin(), name.end());
                uploadBuffer->SetName(wname.c_str());
            #endif
            }

            // Copy the texel data to the upload buffer resource
            {
                UINT8* pData = nullptr;
                D3D12_RANGE range = { 0, 0 };
                if (FAILED(uploadBuffer->Map(0, &range, reinterpret_cast<void**>(&pData)))) return false;

                if (texture.format == Textures::ETextureFormat::BC7)
                {
                    // Aligned, copy all the image pixels
                    memcpy(pData, texture.texels, texture.texelBytes);
                }
                else if(texture.format == Textures::ETextureFormat::UNCOMPRESSED)
                {
                    UINT rowSize = texture.width * texture.stride;
                    UINT rowPitch = ALIGN(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, rowSize);
                    if(rowSize == rowPitch)
                    {
                        // Aligned, copy the all image pixels
                        memcpy(pData, texture.texels, texture.texelBytes);
                    }
                    else
                    {
                        // RowSize is *not* aligned to D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
                        // Copy each row of the image and add padding to match the row pitch alignment
                        UINT8* pSource = texture.texels;
                        for (UINT rowIndex = 0; rowIndex < texture.height; rowIndex++)
                        {
                            memcpy(pData, texture.texels, rowSize);
                            pData += rowPitch;
                            pSource += rowSize;
                        }
                    }
                }

                uploadBuffer->Unmap(0, &range);
            }

            // Schedule a copy the of the upload heap resource to the default heap resource, then transition it to a shader resource
            {
                // Describe the texture
                D3D12_RESOURCE_DESC texDesc = {};
                texDesc.Width = texture.width;
                texDesc.Height = texture.height;
                texDesc.MipLevels = texture.mips;
                texDesc.DepthOrArraySize = 1;
                texDesc.SampleDesc.Count = 1;
                texDesc.SampleDesc.Quality = 0;
                texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
                texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
                if (texture.format == Textures::ETextureFormat::BC7) texDesc.Format = DXGI_FORMAT_BC7_TYPELESS;

                // Get the resource footprints and total upload buffer size
                UINT64 uploadSize = 0;
                std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints;
                std::vector<UINT> numRows;
                std::vector<UINT64> rowSizes;
                footprints.resize(texture.mips);
                numRows.resize(texture.mips);
                rowSizes.resize(texture.mips);
                d3d.device->GetCopyableFootprints(&texDesc, 0, texture.mips, 0, footprints.data(), numRows.data(), rowSizes.data(), &uploadSize);
                assert(uploadSize <= texture.texelBytes);

                // Describe the upload buffer resource (source)
                D3D12_TEXTURE_COPY_LOCATION source = {};
                source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                source.pResource = uploadBuffer;

                // Describe the default heap resource (destination)
                D3D12_TEXTURE_COPY_LOCATION destination = {};
                destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                destination.pResource = resource;

                // Copy each texture mip level from the upload heap to default heap
                for(UINT mipIndex = 0; mipIndex < texture.mips; mipIndex++)
                {
                    // Update the mip level footprint
                    source.PlacedFootprint = footprints[mipIndex];

                    // Update the destination mip level
                    destination.SubresourceIndex = mipIndex;

                    // Copy the texture from the upload heap to the default heap
                    d3d.cmdList->CopyTextureRegion(&destination, 0, 0, 0, &source, NULL);
                }

                // Transition the default heap texture resource to a shader resource
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = resource;
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

                d3d.cmdList->ResourceBarrier(1, &barrier);
            }

            return true;
        }

        /**
         * Create the global root signature.
         */
        bool CreateGlobalRootSignature(Globals& d3d, Resources& resources)
        {
            std::vector<D3D12_ROOT_PARAMETER> rootParameters;

        #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS
            // Descriptor Heap Ranges
            std::vector<D3D12_DESCRIPTOR_RANGE> ranges;

            // Cameras Constant Buffer CBV (b1, space0)
            {
                D3D12_DESCRIPTOR_RANGE range = {};
                range.BaseShaderRegister = 1;
                range.NumDescriptors = 1;
                range.RegisterSpace = 0;
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                range.OffsetInDescriptorsFromTableStart = DescriptorHeapOffsets::CBV_CAMERA;
                ranges.push_back(range);
            }

            // Lights StructuredBuffer SRV (t2, space0)
            {
                D3D12_DESCRIPTOR_RANGE range = {};
                range.BaseShaderRegister = 2;
                range.NumDescriptors = 1;
                range.RegisterSpace = 0;
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                range.OffsetInDescriptorsFromTableStart = DescriptorHeapOffsets::STB_LIGHTS;
                ranges.push_back(range);
            }

            // Materials StructuredBuffer SRV (t3, space0)
            {
                D3D12_DESCRIPTOR_RANGE range = {};
                range.BaseShaderRegister = 3;
                range.NumDescriptors = 1;
                range.RegisterSpace = 0;
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                range.OffsetInDescriptorsFromTableStart = DescriptorHeapOffsets::STB_MATERIALS;
                ranges.push_back(range);
            }

            // TLAS Instances StructuredBuffer SRV (t4, space0)
            {
                D3D12_DESCRIPTOR_RANGE range = {};
                range.BaseShaderRegister = 4;
                range.NumDescriptors = 1;
                range.RegisterSpace = 0;
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                range.OffsetInDescriptorsFromTableStart = DescriptorHeapOffsets::STB_TLAS_INSTANCES;
                ranges.push_back(range);
            }

            // DDGIVolume Constants StructuredBuffer SRV (t5, space0)
            {
                D3D12_DESCRIPTOR_RANGE range = {};
                range.BaseShaderRegister = 5;
                range.NumDescriptors = 1;
                range.RegisterSpace = 0;
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                range.OffsetInDescriptorsFromTableStart = DescriptorHeapOffsets::STB_DDGI_VOLUME_CONSTS;
                ranges.push_back(range);
            }

            // DDGIVolume Bindless Resource Indices StructuredBuffer SRV (t6, space0)
            {
                D3D12_DESCRIPTOR_RANGE range = {};
                range.BaseShaderRegister = 6;
                range.NumDescriptors = 1;
                range.RegisterSpace = 0;
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                range.OffsetInDescriptorsFromTableStart = DescriptorHeapOffsets::STB_DDGI_VOLUME_RESOURCE_INDICES;
                ranges.push_back(range);
            }

            // TLAS Instances RWStructuredBuffer UAV (u5, space0)
            {
                D3D12_DESCRIPTOR_RANGE range = {};
                range.BaseShaderRegister = 5;
                range.NumDescriptors = 1;
                range.RegisterSpace = 0;
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                range.OffsetInDescriptorsFromTableStart = DescriptorHeapOffsets::UAV_STB_TLAS_INSTANCES;
                ranges.push_back(range);
            }

            // Bindless UAVs, RWTexture2D (u6, space0)
            {
                D3D12_DESCRIPTOR_RANGE range = {};
                range.BaseShaderRegister = 6;
                range.NumDescriptors = UINT_MAX;
                range.RegisterSpace = 0;
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                range.OffsetInDescriptorsFromTableStart = DescriptorHeapOffsets::UAV_TEX2D_START;
                ranges.push_back(range);
            }

            // Bindless UAVs, RWTexture2DArray (u6, space1)
            {
                D3D12_DESCRIPTOR_RANGE range = {};
                range.BaseShaderRegister = 6;
                range.NumDescriptors = UINT_MAX;
                range.RegisterSpace = 1;
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                range.OffsetInDescriptorsFromTableStart = DescriptorHeapOffsets::UAV_TEX2DARRAY_START;
                ranges.push_back(range);
            }

            // Bindless SRVs, RaytracingAccelerationStructure (t7, space0)
            {
                D3D12_DESCRIPTOR_RANGE range = {};
                range.BaseShaderRegister = 7;
                range.NumDescriptors = UINT_MAX;
                range.RegisterSpace = 0;
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                range.OffsetInDescriptorsFromTableStart = DescriptorHeapOffsets::SRV_TLAS_START;
                ranges.push_back(range);
            }

            // Bindless SRVs, Texture2D (t7, space1)
            {
                D3D12_DESCRIPTOR_RANGE range = {};
                range.BaseShaderRegister = 7;
                range.NumDescriptors = UINT_MAX;
                range.RegisterSpace = 1;
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                range.OffsetInDescriptorsFromTableStart = DescriptorHeapOffsets::SRV_TEX2D_START;
                ranges.push_back(range);
            }

            // Bindless SRVs, Texture2DArray (t7, space2)
            {
                D3D12_DESCRIPTOR_RANGE range = {};
                range.BaseShaderRegister = 7;
                range.NumDescriptors = UINT_MAX;
                range.RegisterSpace = 2;
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                range.OffsetInDescriptorsFromTableStart = DescriptorHeapOffsets::SRV_TEX2DARRAY_START;
                ranges.push_back(range);
            }

            // Bindless SRVs, ByteAddressBuffers (t7, space3)
            {
                D3D12_DESCRIPTOR_RANGE range = {};
                range.BaseShaderRegister = 7;
                range.NumDescriptors = UINT_MAX;
                range.RegisterSpace = 3;
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                range.OffsetInDescriptorsFromTableStart = DescriptorHeapOffsets::SRV_BYTEADDRESS_START;
                ranges.push_back(range);
            }
        #endif

            // Root Parameter 0: Global Root Constants (b0, space0)
            {
                D3D12_ROOT_PARAMETER param = {};
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
                param.Constants.ShaderRegister = 0;
                param.Constants.RegisterSpace = 0;
                param.Constants.Num32BitValues = GlobalConstants::GetAlignedNum32BitValues();
                rootParameters.push_back(param);
            }

            // Root Parameter 1: DDGI Root Constants (b0, space1)
            {
                D3D12_ROOT_PARAMETER param = {};
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
                param.Constants.ShaderRegister = 0;
                param.Constants.RegisterSpace = 1;
                param.Constants.Num32BitValues = DDGIRootConstants::GetAlignedNum32BitValues();
                rootParameters.push_back(param);
            }

        #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS
            // Root Parameter 2: Sampler Descriptor Table
            {
                // Bindless Samplers (s0, space0)
                D3D12_DESCRIPTOR_RANGE range;
                range.BaseShaderRegister = 0;
                range.NumDescriptors = UINT_MAX;
                range.RegisterSpace = 0;
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                range.OffsetInDescriptorsFromTableStart = 0;

                // Sampler Descriptor Table
                D3D12_ROOT_PARAMETER param = {};
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
                param.DescriptorTable.NumDescriptorRanges = 1;
                param.DescriptorTable.pDescriptorRanges = &range;
                rootParameters.push_back(param);
            }

            // Root Parameter 3: Resource Descriptor Table
            {
                D3D12_ROOT_PARAMETER param = {};
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
                param.DescriptorTable.NumDescriptorRanges = (UINT)ranges.size();
                param.DescriptorTable.pDescriptorRanges = ranges.data();
                rootParameters.push_back(param);
            }
        #endif

        #if GFX_NVAPI
            // Fake UAV for NVAPI
            D3D12_DESCRIPTOR_RANGE nvapiRange = {};
            nvapiRange.BaseShaderRegister = NV_SHADER_EXTN_SLOT;
            nvapiRange.NumDescriptors = 1;
            nvapiRange.RegisterSpace = NV_SHADER_EXTN_REGISTER_SPACE;
            nvapiRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            nvapiRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            // Root Parameter 2 (or 4): NVAPI
            D3D12_ROOT_PARAMETER param = {};
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            param.DescriptorTable.NumDescriptorRanges = 1;
            param.DescriptorTable.pDescriptorRanges = &nvapiRange;
            rootParameters.push_back(param);
        #endif

            // Describe the root signature
            D3D12_ROOT_SIGNATURE_DESC desc = {};
            desc.NumParameters = static_cast<UINT>(rootParameters.size());
            desc.pParameters = rootParameters.data();
            desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP
            desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
            desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;
        #endif

            // Create the global root signature
            resources.rootSignature = CreateRootSignature(d3d, desc);
            if (!resources.rootSignature) return false;
        #ifdef GFX_NAME_OBJECTS
            resources.rootSignature->SetName(L"Global Root Signature");
        #endif

            return true;
        }

        /**
         * Create the shared render targets.
         */
        bool CreateRenderTargets(Globals& d3d, Resources& resources)
        {
            // Create the GBufferA (R8G8B8A8_UNORM) texture resource
            TextureDesc desc = { static_cast<UINT>(d3d.width), static_cast<UINT>(d3d.height), 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS };
            if(!CreateTexture(d3d, desc, &resources.rt.GBufferA)) return false;
        #ifdef GFX_NAME_OBJECTS
            resources.rt.GBufferA->SetName(L"GBufferA");
        #endif

            // Add the GBufferA UAV to the descriptor heap
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Format = desc.format;

            D3D12_CPU_DESCRIPTOR_HANDLE handle;
            handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::UAV_GBUFFERA * resources.srvDescHeapEntrySize);
            d3d.device->CreateUnorderedAccessView(resources.rt.GBufferA, nullptr, &uavDesc, handle);

            // Create the GBufferB (R32G32B32A32_FLOAT) texture resource
            desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            if(!CreateTexture(d3d, desc, &resources.rt.GBufferB)) return false;
        #ifdef GFX_NAME_OBJECTS
            resources.rt.GBufferB->SetName(L"GBufferB");
        #endif

            // Add the GBufferB UAV to the descriptor heap
            uavDesc.Format = desc.format;
            handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::UAV_GBUFFERB * resources.srvDescHeapEntrySize);
            d3d.device->CreateUnorderedAccessView(resources.rt.GBufferB, nullptr, &uavDesc, handle);

            // Create the GBufferC (R32G32B32A32_FLOAT) texture resource
            if (!CreateTexture(d3d, desc, &resources.rt.GBufferC)) return false;
        #ifdef GFX_NAME_OBJECTS
            resources.rt.GBufferC->SetName(L"GBufferC");
        #endif

            // Add the GBufferC UAV to the descriptor heap
            handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::UAV_GBUFFERC * resources.srvDescHeapEntrySize);
            d3d.device->CreateUnorderedAccessView(resources.rt.GBufferC, nullptr, &uavDesc, handle);

            // Create GBufferD (R32G32B32A32_FLOAT)
            if (!CreateTexture(d3d, desc, &resources.rt.GBufferD)) return false;
        #ifdef GFX_NAME_OBJECTS
            resources.rt.GBufferD->SetName(L"GBufferD");
        #endif

            // Add the GBufferD UAV to the descriptor heap
            handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::UAV_GBUFFERD * resources.srvDescHeapEntrySize);
            d3d.device->CreateUnorderedAccessView(resources.rt.GBufferD, nullptr, &uavDesc, handle);

            return true;
        }

        /**
         * Release D3D12 resources.
         */
        void Cleanup(Resources& resources)
        {
            // Release descriptor heaps
            SAFE_RELEASE(resources.rtvDescHeap);
            SAFE_RELEASE(resources.srvDescHeap);
            SAFE_RELEASE(resources.samplerDescHeap);
            resources.rtvDescHeapStart = { 0 };
            resources.srvDescHeapStart = { 0 };
            resources.samplerDescHeapStart = { 0 };
            resources.rtvDescHeapEntrySize = 0;
            resources.srvDescHeapEntrySize = 0;
            resources.samplerDescHeapEntrySize = 0;

            // Release query heaps
            SAFE_RELEASE(resources.timestampHeap);
            SAFE_RELEASE(resources.timestamps);

            // Release Root Signature
            SAFE_RELEASE(resources.rootSignature);

            // Buffers
            if (resources.cameraCB) resources.cameraCB->Unmap(0, nullptr);
            if (resources.lightsSTBUpload) resources.lightsSTBUpload->Unmap(0, nullptr);
            SAFE_RELEASE(resources.cameraCB);
            SAFE_RELEASE(resources.lightsSTB);
            SAFE_RELEASE(resources.lightsSTBUpload);
            SAFE_RELEASE(resources.materialsSTB);
            SAFE_RELEASE(resources.meshOffsetsRB);
            SAFE_RELEASE(resources.geometryDataRB);
            resources.cameraCBPtr = nullptr;
            resources.lightsSTBPtr = nullptr;
            resources.materialsSTBPtr = nullptr;
            resources.meshOffsetsRBPtr = nullptr;
            resources.geometryDataRBPtr = nullptr;

            // Render Targets
            SAFE_RELEASE(resources.rt.GBufferA);
            SAFE_RELEASE(resources.rt.GBufferB);
            SAFE_RELEASE(resources.rt.GBufferC);
            SAFE_RELEASE(resources.rt.GBufferD);

            // Release Scene geometry
            size_t resourceIndex;
            assert(resources.sceneIBs.size() == resources.sceneVBs.size());
            for (resourceIndex = 0; resourceIndex < resources.sceneIBs.size(); resourceIndex++)
            {
                SAFE_RELEASE(resources.sceneIBs[resourceIndex]);
                SAFE_RELEASE(resources.sceneVBs[resourceIndex]);
            }
            resources.sceneIBs.clear();
            resources.sceneVBs.clear();
            resources.sceneVBViews.clear();
            resources.sceneIBViews.clear();

            // Release Scene acceleration structures
            for (resourceIndex = 0; resourceIndex < resources.blas.size(); resourceIndex++)
            {
                resources.blas[resourceIndex].Release();
            }
            resources.tlas.Release();

            // Release Scene textures
            for (resourceIndex = 0; resourceIndex < resources.sceneTextures.size(); resourceIndex++)
            {
                SAFE_RELEASE(resources.sceneTextures[resourceIndex]);
            }

            // Release default textures
            for (resourceIndex = 0; resourceIndex < resources.textures.size(); resourceIndex++)
            {
                SAFE_RELEASE(resources.textures[resourceIndex]);
            }
        }

        /**
         * Release core D3D12 resources.
         */
        void Cleanup(Globals& d3d)
        {
            // Leave fullscreen mode if necessary
            if (d3d.fullscreen) d3d.swapChain->SetFullscreenState(FALSE, nullptr);

            CloseHandle(d3d.fenceEvent);
            Shaders::Cleanup(d3d.shaderCompiler);

            // Release core D3D12 objects
            SAFE_RELEASE(d3d.backBuffer[0]);
            SAFE_RELEASE(d3d.backBuffer[1]);
            SAFE_RELEASE(d3d.swapChain);
            SAFE_RELEASE(d3d.fence);
            SAFE_RELEASE(d3d.cmdList);
            SAFE_RELEASE(d3d.cmdAlloc[0]);
            SAFE_RELEASE(d3d.cmdAlloc[1]);
            SAFE_RELEASE(d3d.cmdQueue);
            SAFE_RELEASE(d3d.device);
            SAFE_RELEASE(d3d.factory);

        #if GFX_NVAPI
            NvAPI_Unload();
        #endif
        }

        //----------------------------------------------------------------------------------------------------------
        // Private Scene Functions
        //----------------------------------------------------------------------------------------------------------

        /**
         * Create the scene camera constant buffer.
         */
        bool CreateSceneCameraConstantBuffer(Globals& d3d, Resources& resources, const Scenes::Scene& scene)
        {
            // Create the camera buffer resource
            UINT size = ALIGN(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, Scenes::Camera::GetGPUDataSize());
            BufferDesc desc = { size, 0, EHeapType::UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE };
            if (!CreateBuffer(d3d, desc, &resources.cameraCB)) return false;
        #ifdef GFX_NAME_OBJECTS
            resources.cameraCB->SetName(L"Camera Constant Buffer");
        #endif

            // Add the camera CBV to the descriptor heap
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.SizeInBytes = size;
            cbvDesc.BufferLocation = resources.cameraCB->GetGPUVirtualAddress();

            D3D12_CPU_DESCRIPTOR_HANDLE handle;
            handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::CBV_CAMERA * resources.srvDescHeapEntrySize);
            d3d.device->CreateConstantBufferView(&cbvDesc, handle);

            // Map the buffer for updates
            D3D12_RANGE readRange = {};
            D3DCHECK(resources.cameraCB->Map(0, &readRange, reinterpret_cast<void**>(&resources.cameraCBPtr)));

            return true;
        }

        /**
         * Create the scene lights structured buffer.
         */
        bool CreateSceneLightsBuffer(Globals& d3d, Resources& resources, const Scenes::Scene& scene)
        {
            UINT size = ALIGN(D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT, Scenes::Light::GetGPUDataSize() * static_cast<UINT>(scene.lights.size()));
            if (size == 0) return true; // scenes with no lights are valid

            // Create the lights upload buffer resource
            BufferDesc desc = { size, 0, EHeapType::UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE };
            if (!CreateBuffer(d3d, desc, &resources.lightsSTBUpload)) return false;
        #ifdef GFX_NAME_OBJECTS
            resources.lightsSTBUpload->SetName(L"Lights Upload Structured Buffer");
        #endif

            // Create the lights device buffer resource
            desc = { size, 0, EHeapType::DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE };
            if (!CreateBuffer(d3d, desc, &resources.lightsSTB)) return false;
        #ifdef GFX_NAME_OBJECTS
            resources.lightsSTB->SetName(L"Lights Structured Buffer");
        #endif

            // Copy the lights to the upload buffer. Leave the buffer mapped for updates.
            UINT offset = 0;
            D3D12_RANGE readRange = {};
            D3DCHECK(resources.lightsSTBUpload->Map(0, &readRange, reinterpret_cast<void**>(&resources.lightsSTBPtr)));
            for (UINT lightIndex = 0; lightIndex < static_cast<UINT>(scene.lights.size()); lightIndex++)
            {
                const Scenes::Light& light = scene.lights[lightIndex];
                memcpy(resources.lightsSTBPtr + offset, light.GetGPUData(), Scenes::Light::GetGPUDataSize());
                offset += Scenes::Light::GetGPUDataSize();
            }

            // Schedule a copy of the upload buffer to the device buffer
            d3d.cmdList->CopyBufferRegion(resources.lightsSTB, 0, resources.lightsSTBUpload, 0, size);

            // Transition the default heap resource to generic read after the copy is complete
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = resources.lightsSTB;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            d3d.cmdList->ResourceBarrier(1, &barrier);

            // Add the lights structured buffer SRV to the descriptor heap
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Buffer.NumElements = static_cast<UINT>(scene.lights.size());
            srvDesc.Buffer.StructureByteStride = sizeof(Light);
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

            D3D12_CPU_DESCRIPTOR_HANDLE handle;
            handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::STB_LIGHTS * resources.srvDescHeapEntrySize);
            d3d.device->CreateShaderResourceView(resources.lightsSTB, &srvDesc, handle);

            return true;
        }

        /**
         * Create the scene materials buffer.
         */
        bool CreateSceneMaterialsBuffer(Globals& d3d, Resources& resources, const Scenes::Scene& scene)
        {
            // Create the materials buffer upload resource
            UINT size = ALIGN(D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT, Scenes::Material::GetGPUDataSize() * static_cast<UINT>(scene.materials.size()));
            BufferDesc desc = { size, 0, EHeapType::UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE };
            if (!CreateBuffer(d3d, desc, &resources.materialsSTBUpload)) return false;
        #ifdef GFX_NAME_OBJECTS
            resources.materialsSTBUpload->SetName(L"Materials Upload Structured Buffer");
        #endif

            // Create the materials buffer device resource
            desc = { size, 0, EHeapType::DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE };
            if (!CreateBuffer(d3d, desc, &resources.materialsSTB)) return false;
        #ifdef GFX_NAME_OBJECTS
            resources.materialsSTB->SetName(L"Materials Structured Buffer");
        #endif

            // Determine the texture index offset
        #if RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS
            const int SCENE_TEXTURES_INDEX = DescriptorHeapOffsets::SRV_SCENE_TEXTURES - DescriptorHeapOffsets::SRV_TEX2D_START;
        #elif RTXGI_BINDLESS_TYPE == RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP
            const int SCENE_TEXTURES_INDEX = DescriptorHeapOffsets::SRV_SCENE_TEXTURES;
        #endif

            // Copy the materials to the upload buffer
            UINT offset = 0;
            D3D12_RANGE readRange = {};
            D3DCHECK(resources.materialsSTBUpload->Map(0, &readRange, reinterpret_cast<void**>(&resources.materialsSTBPtr)));
            for (UINT materialIndex = 0; materialIndex < static_cast<UINT>(scene.materials.size()); materialIndex++)
            {
                // Get the material
                Scenes::Material material = scene.materials[materialIndex];

                // Add the offset to the textures (in resource arrays or on the descriptor heap)
                if (material.data.albedoTexIdx > -1) material.data.albedoTexIdx += SCENE_TEXTURES_INDEX;
                if (material.data.normalTexIdx > -1) material.data.normalTexIdx += SCENE_TEXTURES_INDEX;
                if (material.data.roughnessMetallicTexIdx > -1) material.data.roughnessMetallicTexIdx += SCENE_TEXTURES_INDEX;
                if (material.data.emissiveTexIdx > -1) material.data.emissiveTexIdx += SCENE_TEXTURES_INDEX;

                // Copy the material
                memcpy(resources.materialsSTBPtr + offset, material.GetGPUData(), Scenes::Material::GetGPUDataSize());

                // Move the destination pointer to the next material
                offset += Scenes::Material::GetGPUDataSize();
            }
            resources.materialsSTBUpload->Unmap(0, nullptr);

            // Schedule a copy of the upload buffer to the device buffer
            d3d.cmdList->CopyBufferRegion(resources.materialsSTB, 0, resources.materialsSTBUpload, 0, size);

            // Transition the default heap resource to generic read after the copy is complete
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = resources.materialsSTB;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            d3d.cmdList->ResourceBarrier(1, &barrier);

            // Add the materials structured buffer SRV to the descriptor heap
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Buffer.NumElements = static_cast<UINT>(scene.materials.size());
            srvDesc.Buffer.StructureByteStride = Scenes::Material::GetGPUDataSize();
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

            D3D12_CPU_DESCRIPTOR_HANDLE handle;
            handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::STB_MATERIALS * resources.srvDescHeapEntrySize);
            d3d.device->CreateShaderResourceView(resources.materialsSTB, &srvDesc, handle);

            return true;
        }

        /**
         * Create the scene material indexing buffers.
         */
        bool CreateSceneMaterialIndexingBuffers(Globals& d3d, Resources& resources, const Scenes::Scene& scene)
        {
            // Mesh Offsets

            // Create the mesh offsets upload buffer resource
            UINT meshOffsetsSize = ALIGN(D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT, sizeof(UINT) * static_cast<UINT>(scene.meshes.size()) );
            BufferDesc desc = { meshOffsetsSize, 0, EHeapType::UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE };
            if (!CreateBuffer(d3d, desc, &resources.meshOffsetsRBUpload)) return false;
        #ifdef GFX_NAME_OBJECTS
            resources.meshOffsetsRBUpload->SetName(L"Mesh Offsets Upload ByteAddressBuffer");
        #endif

            // Create the mesh offsets device buffer resource
            desc = { meshOffsetsSize, 0, EHeapType::DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE };
            if (!CreateBuffer(d3d, desc, &resources.meshOffsetsRB)) return false;
        #ifdef GFX_NAME_OBJECTS
            resources.meshOffsetsRB->SetName(L"Mesh Offsets ByteAddressBuffer");
        #endif

            // Geometry Data

            // Create the geometry (mesh primitive) data upload buffer resource
            UINT geometryDataSize = ALIGN(D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT, sizeof(GeometryData) * scene.numMeshPrimitives);
            desc = { geometryDataSize, 0, EHeapType::UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE };
            if (!CreateBuffer(d3d, desc, &resources.geometryDataRBUpload)) return false;
        #ifdef GFX_NAME_OBJECTS
            resources.geometryDataRBUpload->SetName(L"Geometry Data Upload ByteAddressBuffer");
        #endif

            // Create the geometry (mesh primitive) data device buffer resource
            desc = { geometryDataSize, 0, EHeapType::DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE };
            if (!CreateBuffer(d3d, desc, &resources.geometryDataRB)) return false;
        #ifdef GFX_NAME_OBJECTS
            resources.geometryDataRB->SetName(L"Geometry Data ByteAddressBuffer");
        #endif

            // Copy the mesh offsets and geometry data to the upload buffers
            UINT primitiveOffset = 0;
            D3D12_RANGE readRange = {};
            D3DCHECK(resources.meshOffsetsRBUpload->Map(0, &readRange, reinterpret_cast<void**>(&resources.meshOffsetsRBPtr)));
            D3DCHECK(resources.geometryDataRBUpload->Map(0, &readRange, reinterpret_cast<void**>(&resources.geometryDataRBPtr)));

            UINT8* meshOffsetsAddress = resources.meshOffsetsRBPtr;
            UINT8* geometryDataAddress = resources.geometryDataRBPtr;
            for (UINT meshIndex = 0; meshIndex < static_cast<UINT>(scene.meshes.size()); meshIndex++)
            {
                // Get the mesh
                const Scenes::Mesh& mesh = scene.meshes[meshIndex];

                // Copy the mesh offset to the upload buffer
                UINT meshOffset = primitiveOffset * sizeof(GeometryData);
                memcpy(meshOffsetsAddress, &meshOffset, sizeof(UINT));
                meshOffsetsAddress += sizeof(UINT);

                for (UINT primitiveIndex = 0; primitiveIndex < static_cast<UINT>(mesh.primitives.size()); primitiveIndex++)
                {
                    // Get the mesh primitive and copy its material index to the upload buffer
                    const Scenes::MeshPrimitive& primitive = mesh.primitives[primitiveIndex];

                    GeometryData data;
                    data.materialIndex = primitive.material;
                    data.indexByteAddress = primitive.indexByteOffset;
                    data.vertexByteAddress = primitive.vertexByteOffset;
                    memcpy(geometryDataAddress, &data, sizeof(GeometryData));

                    geometryDataAddress += sizeof(GeometryData);
                    primitiveOffset++;
                }
            }
            resources.meshOffsetsRBUpload->Unmap(0, nullptr);
            resources.geometryDataRBUpload->Unmap(0, nullptr);

            // Schedule a copy of the upload buffers to the device buffers
            d3d.cmdList->CopyBufferRegion(resources.meshOffsetsRB, 0, resources.meshOffsetsRBUpload, 0, meshOffsetsSize);
            d3d.cmdList->CopyBufferRegion(resources.geometryDataRB, 0, resources.geometryDataRBUpload, 0, geometryDataSize);

            // Transition the default heap resources to generic read after the copies are complete
            std::vector<D3D12_RESOURCE_BARRIER> barriers;

            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            barrier.Transition.pResource = resources.meshOffsetsRB;
            barriers.push_back(barrier);
            barrier.Transition.pResource = resources.geometryDataRB;
            barriers.push_back(barrier);

            d3d.cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

            // Add the mesh offsets ByteAddressBuffer SRV to the descriptor heap
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

            D3D12_CPU_DESCRIPTOR_HANDLE handle;
            srvDesc.Buffer.NumElements = static_cast<UINT>(scene.meshes.size());
            handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::SRV_MESH_OFFSETS * resources.srvDescHeapEntrySize);
            d3d.device->CreateShaderResourceView(resources.meshOffsetsRB, &srvDesc, handle);

            // Add the geometry (mesh primitive) data ByteAddressBuffer SRV to the descriptor heap
            srvDesc.Buffer.NumElements = scene.numMeshPrimitives * (sizeof(GeometryData) / sizeof(UINT));
            handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::SRV_GEOMETRY_DATA * resources.srvDescHeapEntrySize);
            d3d.device->CreateShaderResourceView(resources.geometryDataRB, &srvDesc, handle);

            return true;
        }

        /**
         * Create the scene TLAS instances buffers.
         */
        bool CreateSceneInstancesBuffer(Globals& d3d, Resources& resources, const std::vector<D3D12_RAYTRACING_INSTANCE_DESC>& instances)
        {
            // Create the TLAS instance upload buffer resource
            UINT size = ALIGN(D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT, static_cast<UINT>(instances.size()) * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
            BufferDesc desc = { size, 0, EHeapType::UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE };
            if (!CreateBuffer(d3d, desc, &resources.tlas.instancesUpload)) return false;
        #ifdef GFX_NAME_OBJECTS
            resources.tlas.instancesUpload->SetName(L"TLAS Instance Descriptors Upload Buffer");
        #endif

            // Create the TLAS instance device buffer resource
            desc = { size, 0, EHeapType::DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE };
            if (!CreateBuffer(d3d, desc, &resources.tlas.instances)) return false;
        #ifdef GFX_NAME_OBJECTS
            resources.tlas.instances->SetName(L"TLAS Instance Descriptors Buffer");
        #endif

            // Copy the instance data to the upload buffer
            UINT8* pData = nullptr;
            D3D12_RANGE readRange = {};
            D3DCHECK(resources.tlas.instancesUpload->Map(0, &readRange, reinterpret_cast<void**>(&pData)));
            memcpy(pData, instances.data(), desc.size);
            resources.tlas.instancesUpload->Unmap(0, nullptr);

            // Schedule a copy of the upload buffer to the device buffer
            d3d.cmdList->CopyBufferRegion(resources.tlas.instances, 0, resources.tlas.instancesUpload, 0, size);

            // Transition the default heap resource to generic read after the copy is complete
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = resources.tlas.instances;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            d3d.cmdList->ResourceBarrier(1, &barrier);

            // Add the TLAS instances structured buffer SRV to the descriptor heap
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Buffer.NumElements = static_cast<UINT>(instances.size());
            srvDesc.Buffer.StructureByteStride = sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

            D3D12_CPU_DESCRIPTOR_HANDLE handle;
            handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::STB_TLAS_INSTANCES * resources.srvDescHeapEntrySize);
            d3d.device->CreateShaderResourceView(resources.tlas.instances, &srvDesc, handle);

            return true;
        }

        /**
         * Create the scene mesh index buffers.
         */
        bool CreateSceneIndexBuffers(Globals& d3d, Resources& resources, const Scenes::Scene& scene)
        {
            UINT numMeshes = static_cast<UINT>(scene.meshes.size());

            resources.sceneIBs.resize(numMeshes);
            resources.sceneIBUploadBuffers.resize(numMeshes);
            resources.sceneIBViews.resize(numMeshes);
            for (UINT meshIndex = 0; meshIndex < numMeshes; meshIndex++)
            {
                // Get the mesh
                const Scenes::Mesh& mesh = scene.meshes[meshIndex];

                // Create the index buffer and copy the index data to the GPU
                if (!CreateIndexBuffer(d3d, mesh,
                    &resources.sceneIBs[meshIndex],
                    &resources.sceneIBUploadBuffers[meshIndex],
                    resources.sceneIBViews[meshIndex])) return false;
            #ifdef GFX_NAME_OBJECTS
                std::string name = "IB: " + mesh.name;
                std::wstring n = std::wstring(name.begin(), name.end());
                resources.sceneIBs[meshIndex]->SetName(n.c_str());
            #endif

                // Add the index buffer SRV to the descriptor heap
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
                srvDesc.Buffer.NumElements = mesh.numIndices;
                srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

                D3D12_CPU_DESCRIPTOR_HANDLE handle;
                handle.ptr = resources.srvDescHeapStart.ptr + ((DescriptorHeapOffsets::SRV_INDICES + (meshIndex * 2)) * resources.srvDescHeapEntrySize);
                d3d.device->CreateShaderResourceView(resources.sceneIBs[meshIndex], &srvDesc, handle);
            }
            return true;
        }

        /**
         * Create the scene mesh vertex buffers.
         */
        bool CreateSceneVertexBuffers(Globals& d3d, Resources& resources, const Scenes::Scene& scene)
        {
            UINT numMeshes = static_cast<UINT>(scene.meshes.size());

            resources.sceneVBs.resize(numMeshes);
            resources.sceneVBUploadBuffers.resize(numMeshes);
            resources.sceneVBViews.resize(numMeshes);
            for (UINT meshIndex = 0; meshIndex < numMeshes; meshIndex++)
            {
                // Get the mesh
                const Scenes::Mesh& mesh = scene.meshes[meshIndex];

                // Create the vertex buffer and copy the data to the GPU
                if (!CreateVertexBuffer(d3d, mesh,
                    &resources.sceneVBs[meshIndex],
                    &resources.sceneVBUploadBuffers[meshIndex],
                    resources.sceneVBViews[meshIndex])) return false;
            #ifdef GFX_NAME_OBJECTS
                std::string name = "VB: " + mesh.name;
                std::wstring n = std::wstring(name.begin(), name.end());
                resources.sceneVBs[meshIndex]->SetName(n.c_str());
            #endif

                // Add the vertex buffer SRV to the descriptor heap
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
                srvDesc.Buffer.NumElements = (sizeof(Vertex) * mesh.numVertices) / 4;
                srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

                D3D12_CPU_DESCRIPTOR_HANDLE handle;
                handle.ptr = resources.srvDescHeapStart.ptr + ((DescriptorHeapOffsets::SRV_VERTICES + (meshIndex * 2)) * resources.srvDescHeapEntrySize);
                d3d.device->CreateShaderResourceView(resources.sceneVBs[meshIndex], &srvDesc, handle);
            }
            return true;
        }

        /**
         * Create the scene's bottom level acceleration structure(s).
         */
        bool CreateSceneBLAS(Globals& d3d, Resources& resources, const Scenes::Scene& scene)
        {
            // Build a BLAS for each mesh
            resources.blas.resize(scene.meshes.size());
            for (UINT meshIndex = 0; meshIndex < static_cast<UINT>(scene.meshes.size()); meshIndex++)
            {
                // Get the mesh and create its BLAS
                const Scenes::Mesh mesh = scene.meshes[meshIndex];
                if (!CreateBLAS(d3d, resources, mesh)) return false;
            }

            // Wait for the BLAS builds to complete
            D3D12_RESOURCE_BARRIER uavBarrier = {};
            uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            uavBarrier.UAV.pResource = nullptr;
            d3d.cmdList->ResourceBarrier(1, &uavBarrier);

            return true;
        }

        /**
         * Create the scene's top level acceleration structure.
         */
        bool CreateSceneTLAS(Globals& d3d, Resources& resources, const Scenes::Scene& scene)
        {
            // Describe the scene TLAS instances
            std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instances;
            for (size_t instanceIndex = 0; instanceIndex < scene.instances.size(); instanceIndex++)
            {
                // Get the mesh instance
                const Scenes::MeshInstance& instance = scene.instances[instanceIndex];

                // Describe the mesh instance
                D3D12_RAYTRACING_INSTANCE_DESC desc = {};
                desc.InstanceID = instance.meshIndex; // quantized to 24-bits
                desc.InstanceMask = 0xFF;
                desc.AccelerationStructure = resources.blas[instance.meshIndex].as->GetGPUVirtualAddress();
            #if COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT || COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP
                desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
            #endif
                desc.Flags |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;

                // Write the instance transform
                memcpy(desc.Transform, instance.transform, sizeof(XMFLOAT4) * 3);

                instances.push_back(desc);
            }

            // Create the TLAS instances buffer
            if (!CreateSceneInstancesBuffer(d3d, resources, instances)) return false;

            // Build the TLAS
            if (!CreateTLAS(d3d, resources, instances, "TLAS")) return false;

            // Add the TLAS SRV to the descriptor heap
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
            srvDesc.RaytracingAccelerationStructure.Location = resources.tlas.as->GetGPUVirtualAddress();
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

            D3D12_CPU_DESCRIPTOR_HANDLE handle;
            handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::SRV_SCENE_TLAS * resources.srvDescHeapEntrySize);
            d3d.device->CreateShaderResourceView(nullptr, &srvDesc, handle);

            return true;
        }

        /**
         * Create the scene textures.
         */
        bool CreateSceneTextures(Globals& d3d, Resources& resources, const Scenes::Scene& scene, std::ofstream& log)
        {
            // Early out if there are no scene textures
            if (scene.textures.size() == 0) return true;

            D3D12_CPU_DESCRIPTOR_HANDLE handle;
            handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::SRV_SCENE_TEXTURES * resources.srvDescHeapEntrySize);

            // Create the default and upload heap texture resources
            for (UINT textureIndex = 0; textureIndex < static_cast<UINT>(scene.textures.size()); textureIndex++)
            {
                // Get the texture
                const Textures::Texture texture = scene.textures[textureIndex];

                // Create the GPU texture resources, upload the texture data, and schedule a copy
                CHECK(CreateAndUploadTexture(d3d, resources, texture, log), "create and upload scene texture!\n", log);

                // Add the texture SRV to the descriptor heap
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = texture.mips;
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                if (texture.format == Textures::ETextureFormat::UNCOMPRESSED) srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                else if(texture.format == Textures::ETextureFormat::BC7) srvDesc.Format = DXGI_FORMAT_BC7_UNORM;

                d3d.device->CreateShaderResourceView(resources.sceneTextures[textureIndex], &srvDesc, handle);

                // Move to the next slot on the descriptor heap
                handle.ptr += resources.srvDescHeapEntrySize;
            }

            return true;
        }

        //----------------------------------------------------------------------------------------------------------
        // Private Functions
        //----------------------------------------------------------------------------------------------------------

        /**
         * Load texture data, create GPU heap resources, upload the texture to the GPU heap,
         * unload the CPU side texture, and schedule a copy from the GPU upload to default heap.
         */
        bool CreateDefaultTexture(Globals& d3d, Resources& resources, Textures::Texture& texture, std::ofstream& log)
        {
            // Load the texture from disk
            if(!Textures::Load(texture)) return false;

            // Create and upload the texture data
            if(!CreateAndUploadTexture(d3d, resources, texture, log)) return false;

            // Free the texels on the CPU now that the texture data is copied to the upload heap resource
            Textures::Unload(texture);

            return true;
        }

        /**
         * Load and create the default texture resources.
         */
        bool LoadAndCreateDefaultTextures(Globals& d3d, Resources& resources, const Configs::Config& config, std::ofstream& log)
        {
            Textures::Texture blueNoise;
            blueNoise.name = "Blue Noise";
            blueNoise.filepath = config.app.root + "data\\textures\\blue-noise-rgb-256.png";
            blueNoise.type = Textures::ETextureType::ENGINE;

            // Load the texture data, create the texture, copy it to the upload heap, and schedule a copy to the default heap
            CHECK(CreateDefaultTexture(d3d, resources, blueNoise, log), "create the blue noise texture!", log);

            // Add the blue noise texture SRV to the descriptor heap
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvDesc.Texture2D.MipLevels = 1;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

            D3D12_CPU_DESCRIPTOR_HANDLE handle;
            handle.ptr = resources.srvDescHeapStart.ptr + (DescriptorHeapOffsets::SRV_BLUE_NOISE * resources.srvDescHeapEntrySize);
            d3d.device->CreateShaderResourceView(resources.textures.back(), &srvDesc, handle);

            return true;
        }

        //----------------------------------------------------------------------------------------------------------
        // Debug Functions
        //----------------------------------------------------------------------------------------------------------

        /**
         * Write an image (or images) to disk from the given D3D12 resource.
         */
        bool WriteResourceToDisk(Globals& d3d, std::string file, ID3D12Resource* pResource, D3D12_RESOURCE_STATES state)
        {
            // Get the heap properties
            D3D12_HEAP_PROPERTIES sourceHeapProperties = {};
            D3D12_HEAP_FLAGS sourceHeapFlags = {};
            D3DCHECK(pResource->GetHeapProperties(&sourceHeapProperties, &sourceHeapFlags));

            // Create a command allocator
            ID3D12CommandAllocator* commandAlloc = nullptr;
            D3DCHECK(d3d.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAlloc)));
        #ifdef GFX_NAME_OBJECTS
            d3d.cmdList->SetName(L"WriteResourceToDisk Command Allocator");
        #endif

            // Create a command list
            ID3D12GraphicsCommandList* commandList = nullptr;
            D3DCHECK(d3d.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAlloc, nullptr, IID_PPV_ARGS(&commandList)));
        #ifdef GFX_NAME_OBJECTS
            d3d.cmdList->SetName(L"WriteResourceToDisk Command List");
        #endif

            // Create fence
            ID3D12Fence* fence = nullptr;
            D3DCHECK(d3d.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
        #ifdef GFX_NAME_OBJECTS
            d3d.cmdList->SetName(L"WriteResourceToDisk Fence");
        #endif

            // Get the resource descriptor
            const D3D12_RESOURCE_DESC desc = pResource->GetDesc();

            // Get the row count, row size in bytes, and size in bytes of the (sub)resource
            UINT numRows;
            UINT64 rowSizeInBytes;
            UINT64 sizeInBytes;
            d3d.device->GetCopyableFootprints(
                &desc,
                0,
                1,
                0,
                nullptr,
                &numRows,
                &rowSizeInBytes,
                &sizeInBytes);

            // Round up the source row size (pitch) to multiples of 256B
            UINT64 dstRowPitch = (rowSizeInBytes + 255) & ~0xFF;

            // Describe the staging (read-back) buffer resource
            D3D12_RESOURCE_DESC bufferDesc = {};
            bufferDesc.Alignment = desc.Alignment;
            bufferDesc.DepthOrArraySize = 1;
            bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
            bufferDesc.Height = 1;
            bufferDesc.Width = dstRowPitch * desc.Height;
            bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            bufferDesc.MipLevels = 1;
            bufferDesc.SampleDesc.Count = 1;
            bufferDesc.SampleDesc.Quality = 0;

            // Transition the source texture resource to a copy source
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = pResource;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = state;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

            commandList->ResourceBarrier(1, &barrier);

            // Loop over the subresources (array slices), copying them from the GPU
            std::vector<ID3D12Resource*> stagingResources;
            for(UINT subresourceIndex = 0; subresourceIndex < desc.DepthOrArraySize; subresourceIndex++)
            {
                // Create a staging texture resource
                stagingResources.emplace_back(nullptr);
                D3DCHECK(d3d.device->CreateCommittedResource(
                    &readbackHeapProps,
                    D3D12_HEAP_FLAG_NONE,
                    &bufferDesc,
                    D3D12_RESOURCE_STATE_COPY_DEST,
                    nullptr,
                    IID_PPV_ARGS(&stagingResources[subresourceIndex])));

                // Describe the copy footprint of the resource
                D3D12_PLACED_SUBRESOURCE_FOOTPRINT subresource = {};
                subresource.Footprint.Width = static_cast<UINT>(desc.Width);
                subresource.Footprint.Height = desc.Height;
                subresource.Footprint.Depth = 1;
                subresource.Footprint.RowPitch = static_cast<UINT>(dstRowPitch);
                subresource.Footprint.Format = desc.Format;

                // Describe the copy source resource
                D3D12_TEXTURE_COPY_LOCATION copySrc = {};
                copySrc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                copySrc.pResource = pResource;
                copySrc.SubresourceIndex = subresourceIndex;

                // Describe the copy destination resource
                D3D12_TEXTURE_COPY_LOCATION copyDest = {};
                copyDest.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                copyDest.pResource = stagingResources[subresourceIndex];
                copyDest.PlacedFootprint = subresource;

                // Schedule the texture copy
                commandList->CopyTextureRegion(&copyDest, 0, 0, 0, &copySrc, nullptr);
            }

            // Transition the source texture resource to the specified state
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            barrier.Transition.StateAfter = state;
            commandList->ResourceBarrier(1, &barrier);

            // Close the command list
            D3DCHECK(commandList->Close());

            // Execute the command list
            d3d.cmdQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList**>(&commandList));

            // Signal the fence
            D3DCHECK(d3d.cmdQueue->Signal(fence, 1));

            // Block until the copy is complete
            while (fence->GetCompletedValue() < 1)
                SwitchToThread();

            // Copy the staging resources and write them to disk
            bool result = true;
            for (UINT subresourceIndex = 0; subresourceIndex < desc.DepthOrArraySize; subresourceIndex++)
            {
                // Map the staging texture resource
                UINT8* pData = nullptr;
                UINT64 imageSize = dstRowPitch * numRows;
                D3D12_RANGE readRange = { 0, static_cast<size_t>(imageSize) };
                D3DCHECK(stagingResources[subresourceIndex]->Map(0, &readRange, (void**)&pData));

                // Convert the resource to RGBA8 UNORM (using WIC)
                std::vector<unsigned char> converted(desc.Width * desc.Height * ImageCapture::NumChannels);
                D3DCHECK(ImageCapture::ConvertTextureResource(desc, imageSize, dstRowPitch, pData, converted));

                // Write the resource to disk as a PNG file (using STB)
                std::string filename = file;
                if(desc.DepthOrArraySize > 1) filename += "-Layer-" + std::to_string(subresourceIndex);
                filename.append(".png");
                result &= ImageCapture::CapturePng(filename, static_cast<uint32_t>(desc.Width), static_cast<uint32_t>(desc.Height), converted.data());

                // Unmap the staging texture
                stagingResources[subresourceIndex]->Unmap(0, nullptr);

                // Release the staging texture
                SAFE_RELEASE(stagingResources[subresourceIndex]);
            }

            // Clean up
            SAFE_RELEASE(fence);
            SAFE_RELEASE(commandList);
            SAFE_RELEASE(commandAlloc);

            return result;
        }

        //----------------------------------------------------------------------------------------------------------
        // Public Functions
        //----------------------------------------------------------------------------------------------------------

        /**
         * Toggle between windowed and fullscreen borderless modes.
         */
        bool ToggleFullscreen(Globals& d3d)
        {
            GLFWmonitor* monitor = NULL;
            if (!d3d.fullscreen)
            {
                glfwGetWindowPos(d3d.window, &d3d.x, &d3d.y);
                glfwGetWindowSize(d3d.window, &d3d.windowWidth, &d3d.windowHeight);
                monitor = glfwGetPrimaryMonitor();

                // "Borderless" fullscreen mode
                const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                glfwSetWindowMonitor(d3d.window, monitor, d3d.x, d3d.y, mode->width, mode->height, mode->refreshRate);
            }
            else
            {
                glfwSetWindowMonitor(d3d.window, monitor, d3d.x, d3d.y, d3d.windowWidth, d3d.windowHeight, d3d.vsync ? 60 : GLFW_DONT_CARE);
            }

            d3d.fullscreen = ~d3d.fullscreen;
            d3d.fullscreenChanged = false;
            return true;
        }

        /**
         * Create a D3D12 device.
         */
        bool CreateDevice(Globals& d3d, Configs::Config& config)
        {
        #if _DEBUG
            {
                // Enable the D3D12 debug layer.
                ID3D12Debug* debug = nullptr;
                if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
                {
                    debug->EnableDebugLayer();
                }
            }
        #endif

        #if GFX_NVAPI
            NvAPI_Initialize();
        #endif

            // Create a DXGI factory
            if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&d3d.factory)))) return false;

            // Check for tearing support
            if (!CheckTearingSupport(d3d)) return false;

            // Create the device
            return CreateDeviceInternal(d3d, config);
        }

        /**
         * Create a D3D12 root signature.
         */
        ID3D12RootSignature* CreateRootSignature(Globals& d3d, const D3D12_ROOT_SIGNATURE_DESC& desc)
        {
            ID3DBlob* sig = nullptr;
            ID3DBlob* error = nullptr;
            if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &error)))
            {
                if (error)
                {
                    const char* errorMsg = (const char*)error->GetBufferPointer();
                    OutputDebugString(errorMsg);
                }
                return nullptr;
            }

            ID3D12RootSignature* pRootSig = nullptr;
            HRESULT hr = d3d.device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&pRootSig));
            if (FAILED(hr)) return nullptr;

            SAFE_RELEASE(sig);
            SAFE_RELEASE(error);
            return pRootSig;
        }

        /**
         * Create a buffer resource.
         */
        bool CreateBuffer(Globals& d3d, const BufferDesc& info, ID3D12Resource** ppResource)
        {
            // Describe the buffer resource
            D3D12_RESOURCE_DESC desc = {};
            desc.Alignment = 0;
            desc.Height = 1;
            desc.Width = info.size;
            desc.MipLevels = 1;
            desc.DepthOrArraySize = 1;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            desc.Flags = info.flags;

            // Select the heap
            D3D12_HEAP_PROPERTIES heapProps;
            if (info.heap == EHeapType::DEFAULT)
            {
                heapProps = defaultHeapProps;
            }
            else if (info.heap == EHeapType::UPLOAD)
            {
                heapProps = uploadHeapProps;
            }
            else if (info.heap == EHeapType::READBACK)
            {
                heapProps = readbackHeapProps;
            }

            // Create the buffer resource
            D3DCHECK(d3d.device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, info.state, nullptr, IID_PPV_ARGS(ppResource)));
            return true;
        }

        /**
         * Create a texture resource on the default heap.
         */
        bool CreateTexture(Globals& d3d, const TextureDesc& info, ID3D12Resource** resource)
        {
            // Describe the texture resource
            D3D12_RESOURCE_DESC desc = {};
            desc.Width = info.width;
            desc.Height = info.height;
            desc.MipLevels = info.mips;
            desc.Format = info.format;
            desc.DepthOrArraySize = (UINT16)info.arraySize;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Flags = info.flags;

            // Setup the optimized clear value
            D3D12_CLEAR_VALUE clear = {};
            clear.Color[3] = 1.f;
            clear.Format = info.format;

            // Create the texture resource
            bool useClear = (info.flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
            D3DCHECK(d3d.device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &desc, info.state, useClear ? &clear : nullptr, IID_PPV_ARGS(resource)));

            return true;
        }

        /**
         * Create a compute pipeline state object.
         */
        bool CreateComputePSO(ID3D12Device* device, ID3D12RootSignature* rootSignature, const Shaders::ShaderProgram& shader, ID3D12PipelineState** pso)
        {
            if (!rootSignature) return false;
            if (!shader.bytecode) return false;

            // Describe the compute pipeline
            D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
            desc.pRootSignature = rootSignature;
            desc.CS.BytecodeLength = shader.bytecode->GetBufferSize();
            desc.CS.pShaderBytecode = shader.bytecode->GetBufferPointer();

            D3DCHECK(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(pso)));
            return true;
        }

        /**
         * Create a raster graphics pipeline state object.
         */
        bool CreateRasterPSO(
            ID3D12Device* device,
            ID3D12RootSignature* rootSignature,
            const Shaders::ShaderPipeline& shaders,
            const RasterDesc& info,
            ID3D12PipelineState** pso)
        {
            D3D12_BLEND_DESC blendDescs = {};
            blendDescs.RenderTarget[0] = info.blendDesc.RenderTarget[0];

            D3D12_SHADER_BYTECODE vs = {};
            vs.BytecodeLength = shaders.vs.bytecode->GetBufferSize();
            vs.pShaderBytecode = shaders.vs.bytecode->GetBufferPointer();

            D3D12_SHADER_BYTECODE ps = {};
            ps.BytecodeLength = shaders.ps.bytecode->GetBufferSize();
            ps.pShaderBytecode = shaders.ps.bytecode->GetBufferPointer();

            // Describe and create the PSO
            D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
            desc.InputLayout = { info.inputLayoutDescs, info.numInputLayouts };
            desc.pRootSignature = rootSignature;
            desc.VS = vs;
            desc.PS = ps;
            desc.RasterizerState = info.rasterDesc;
            desc.BlendState = info.blendDesc;
            desc.SampleMask = UINT_MAX;
            desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            desc.NumRenderTargets = 1;
            desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.SampleDesc.Count = 1;

            // Create the raster pipeline state object
            D3DCHECK(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(pso)));
            return true;
        }

        /**
         * Create a ray tracing graphics pipeline state object.
         */
        bool CreateRayTracingPSO(
            ID3D12Device5* device,
            ID3D12RootSignature* rootSignature,
            const Shaders::ShaderRTPipeline& shaders,
            ID3D12StateObject** rtpso,
            ID3D12StateObjectProperties** rtpsoProps)
        {
            UINT index = 0;
            std::vector<LPCWCHAR> exportNames;

            // Find the number of required subobjects
            // 1: ray generation shader
            // 1: miss shader
            // x: hit groups (and hit shaders)
            // 1: shader config
            // 1: shader config association
            // 1: global root signature
            // 1: ray tracing pipeline config
            // 6 + x
            UINT numSubobjects = 6;
            for (size_t hitGroupIndex = 0; hitGroupIndex < shaders.hitGroups.size(); hitGroupIndex++)
            {
                numSubobjects += shaders.hitGroups[hitGroupIndex].numSubobjects();
            }

            std::vector<D3D12_STATE_SUBOBJECT> subobjects;
            subobjects.resize(numSubobjects);

            // Add a state subobject for the ray generation shader
            D3D12_EXPORT_DESC rgsExportDesc = {};
            rgsExportDesc.ExportToRename = shaders.rgs.entryPoint.c_str();
            rgsExportDesc.Name = shaders.rgs.exportName.c_str();

            D3D12_DXIL_LIBRARY_DESC rgsLibDesc = {};
            rgsLibDesc.DXILLibrary.BytecodeLength = shaders.rgs.bytecode->GetBufferSize();
            rgsLibDesc.DXILLibrary.pShaderBytecode = shaders.rgs.bytecode->GetBufferPointer();
            rgsLibDesc.NumExports = 1;
            rgsLibDesc.pExports = &rgsExportDesc;

            D3D12_STATE_SUBOBJECT rgs = {};
            rgs.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
            rgs.pDesc = &rgsLibDesc;

            subobjects[index++] = rgs;
            exportNames.push_back(shaders.rgs.exportName.c_str());

            // Add a state subobject for the miss shader
            D3D12_EXPORT_DESC msExportDesc = {};
            msExportDesc.Name = shaders.miss.exportName.c_str();
            msExportDesc.ExportToRename = shaders.miss.entryPoint.c_str();

            D3D12_DXIL_LIBRARY_DESC msLibDesc = {};
            msLibDesc.DXILLibrary.BytecodeLength = shaders.miss.bytecode->GetBufferSize();
            msLibDesc.DXILLibrary.pShaderBytecode = shaders.miss.bytecode->GetBufferPointer();
            msLibDesc.NumExports = 1;
            msLibDesc.pExports = &msExportDesc;

            D3D12_STATE_SUBOBJECT miss = {};
            miss.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
            miss.pDesc = &msLibDesc;

            subobjects[index++] = miss;
            exportNames.push_back(shaders.miss.exportName.c_str());

            // Add a state subobject for each hit group's hit shaders
            std::vector<D3D12_EXPORT_DESC> exportDescs;
            std::vector<D3D12_DXIL_LIBRARY_DESC> libDescs;
            std::vector<D3D12_STATE_SUBOBJECT> stateObjects;
            std::vector<D3D12_HIT_GROUP_DESC> hitGroupDescs;
            UINT size = static_cast<UINT>(shaders.hitGroups.size()) * 4;  // CHS|AHS|IS|HG
            exportDescs.resize(size);
            libDescs.resize(size);
            stateObjects.resize(size);
            hitGroupDescs.resize(shaders.hitGroups.size());
            for (UINT hitGroupIndex = 0; hitGroupIndex < static_cast<UINT>(shaders.hitGroups.size()); hitGroupIndex++)
            {
                const Shaders::ShaderRTHitGroup& hitGroup = shaders.hitGroups[hitGroupIndex];
                hitGroupDescs[hitGroupIndex].HitGroupExport = hitGroup.exportName;

                if (hitGroup.hasCHS())
                {
                    UINT chsIndex = (hitGroupIndex * 4);
                    exportDescs[chsIndex].Name = hitGroup.chs.exportName.c_str();
                    exportDescs[chsIndex].ExportToRename = hitGroup.chs.entryPoint.c_str();

                    libDescs[chsIndex].DXILLibrary.BytecodeLength = hitGroup.chs.bytecode->GetBufferSize();
                    libDescs[chsIndex].DXILLibrary.pShaderBytecode = hitGroup.chs.bytecode->GetBufferPointer();
                    libDescs[chsIndex].NumExports = 1;
                    libDescs[chsIndex].pExports = &exportDescs[chsIndex];

                    stateObjects[chsIndex].Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
                    stateObjects[chsIndex].pDesc = &libDescs[chsIndex];

                    subobjects[index++] = stateObjects[chsIndex];

                    hitGroupDescs[hitGroupIndex].ClosestHitShaderImport = hitGroup.chs.exportName.c_str();
                }

                if (hitGroup.hasAHS())
                {
                    UINT ahsIndex = (hitGroupIndex * 4) + 1;
                    exportDescs[ahsIndex].Name = hitGroup.ahs.exportName.c_str();
                    exportDescs[ahsIndex].ExportToRename = hitGroup.ahs.entryPoint.c_str();

                    libDescs[ahsIndex].DXILLibrary.BytecodeLength = hitGroup.ahs.bytecode->GetBufferSize();
                    libDescs[ahsIndex].DXILLibrary.pShaderBytecode = hitGroup.ahs.bytecode->GetBufferPointer();
                    libDescs[ahsIndex].NumExports = 1;
                    libDescs[ahsIndex].pExports = &exportDescs[ahsIndex];

                    stateObjects[ahsIndex].Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
                    stateObjects[ahsIndex].pDesc = &libDescs[ahsIndex];

                    subobjects[index++] = stateObjects[ahsIndex];

                    hitGroupDescs[hitGroupIndex].AnyHitShaderImport = hitGroup.ahs.exportName.c_str();
                }

                if (hitGroup.hasIS())
                {
                    UINT isIndex = (hitGroupIndex * 4) + 2;
                    exportDescs[isIndex].Name = hitGroup.is.exportName.c_str();
                    exportDescs[isIndex].ExportToRename = hitGroup.is.entryPoint.c_str();

                    libDescs[isIndex].DXILLibrary.BytecodeLength = hitGroup.is.bytecode->GetBufferSize();
                    libDescs[isIndex].DXILLibrary.pShaderBytecode = hitGroup.is.bytecode->GetBufferPointer();
                    libDescs[isIndex].NumExports = 1;
                    libDescs[isIndex].pExports = &exportDescs[isIndex];

                    stateObjects[isIndex].Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
                    stateObjects[isIndex].pDesc = &libDescs[isIndex];

                    subobjects[index++] = stateObjects[isIndex];

                    hitGroupDescs[hitGroupIndex].IntersectionShaderImport = hitGroup.is.exportName.c_str();
                }

                // Add a state subobject for the hit group
                UINT hgIndex = (hitGroupIndex * 4) + 3;
                stateObjects[hgIndex].Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
                stateObjects[hgIndex].pDesc = &hitGroupDescs[hitGroupIndex];

                subobjects[index++] = stateObjects[hgIndex];
                exportNames.push_back(hitGroup.exportName);
            }

            // Add a state subobject for the shader payload configuration
            D3D12_RAYTRACING_SHADER_CONFIG shaderDesc = {};
            shaderDesc.MaxPayloadSizeInBytes = shaders.payloadSizeInBytes;
            shaderDesc.MaxAttributeSizeInBytes = D3D12_RAYTRACING_MAX_ATTRIBUTE_SIZE_IN_BYTES;

            D3D12_STATE_SUBOBJECT shaderConfigObject = {};
            shaderConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
            shaderConfigObject.pDesc = &shaderDesc;

            subobjects[index++] = shaderConfigObject;

            // Add a state subobject for the association between shaders and the payload
            D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderPayloadAssociation = {};
            shaderPayloadAssociation.NumExports = static_cast<UINT>(exportNames.size());
            shaderPayloadAssociation.pExports = exportNames.data();
            shaderPayloadAssociation.pSubobjectToAssociate = &subobjects[(index - 1)];

            D3D12_STATE_SUBOBJECT shaderPayloadAssociationObject = {};
            shaderPayloadAssociationObject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
            shaderPayloadAssociationObject.pDesc = &shaderPayloadAssociation;

            subobjects[index++] = shaderPayloadAssociationObject;

            // Add a state subobject for the global root signature
            D3D12_STATE_SUBOBJECT globalRootSig;
            globalRootSig.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
            globalRootSig.pDesc = &rootSignature;

            subobjects[index++] = globalRootSig;

            // Add a state subobject for the ray tracing pipeline config
            D3D12_RAYTRACING_PIPELINE_CONFIG1 pipelineConfig = {};
            pipelineConfig.MaxTraceRecursionDepth = 1;
            pipelineConfig.Flags = D3D12_RAYTRACING_PIPELINE_FLAG_SKIP_PROCEDURAL_PRIMITIVES;

            D3D12_STATE_SUBOBJECT pipelineConfigObject = {};
            pipelineConfigObject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
            pipelineConfigObject.pDesc = &pipelineConfig;

            subobjects[index++] = pipelineConfigObject;

            // Describe the Ray Tracing Pipeline State Object
            D3D12_STATE_OBJECT_DESC pipelineDesc = {};
            pipelineDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
            pipelineDesc.NumSubobjects = static_cast<UINT>(subobjects.size());
            pipelineDesc.pSubobjects = subobjects.data();

        #if GFX_NVAPI
            // Enable NVAPI extension shader slot
            NvAPI_Status status = NvAPI_D3D12_SetNvShaderExtnSlotSpace(device, NV_SHADER_EXTN_SLOT, NV_SHADER_EXTN_REGISTER_SPACE);
            assert(status == NVAPI_OK);
        #endif

            // Create the RT Pipeline State Object (RTPSO)
            D3DCHECK(device->CreateStateObject(&pipelineDesc, IID_PPV_ARGS(rtpso)));

            // Get the RT Pipeline State Object properties
            D3DCHECK((*rtpso)->QueryInterface(IID_PPV_ARGS(rtpsoProps)));

        #if GFX_NVAPI
            // Disable NVAPI extension shader slot after the state object is created
            status = NvAPI_D3D12_SetNvShaderExtnSlotSpace(device, ~0u, 0);
            assert(status == NVAPI_OK);
        #endif

            return true;
        }

        /*
         * Initialize D3D12.
         */
        bool Initialize(const Configs::Config& config, Scenes::Scene& scene, Globals& d3d, Resources& resources, std::ofstream& log)
        {
            // Set config variables
            d3d.width = config.app.width;
            d3d.height = config.app.height;
            d3d.vsync = config.app.vsync;

            // Lighting constants
            resources.constants.lights.hasDirectionalLight = scene.hasDirectionalLight;
            resources.constants.lights.numSpotLights = scene.numSpotLights;
            resources.constants.lights.numPointLights = scene.numPointLights;

            // Initialize the shader compiler
            CHECK(Shaders::Initialize(config, d3d.shaderCompiler), "initialize the shader compiler!", log);

            // Create core D3D12 objects
            CHECK(CreateCmdQueue(d3d), "create command queue!", log);
            CHECK(CreateCmdAllocators(d3d), "create command allocators!", log);
            CHECK(CreateFence(d3d), "create fence!", log);
            CHECK(CreateSwapChain(d3d), " create swap chain!", log);
            CHECK(CreateCmdList(d3d), "create command list!", log);
            CHECK(ResetCmdList(d3d), " reset command list!", log);
            CHECK(CreateDescriptorHeaps(d3d, resources, scene), "create descriptor heaps!", log);
            CHECK(CreateQueryHeaps(d3d, resources), "create query heaps!", log);
            CHECK(CreateGlobalRootSignature(d3d, resources), "create global root signature!", log);
            CHECK(CreateBackBuffer(d3d, resources), "create back buffers!", log);
            CHECK(CreateRenderTargets(d3d, resources), "create render targets!", log);
            CHECK(CreateSamplers(d3d, resources), "create samplers!", log);
            CHECK(CreateViewport(d3d), "create viewport!", log);
            CHECK(CreateScissor(d3d), "create scissor!", log);

            // Create default graphics resources
            CHECK(LoadAndCreateDefaultTextures(d3d, resources, config, log), "load and create default textures!", log);

            // Create scene specific resources
            CHECK(CreateSceneCameraConstantBuffer(d3d, resources, scene), "create scene camera constant buffer!", log);
            CHECK(CreateSceneLightsBuffer(d3d, resources, scene), "create scene lights structured buffer!", log);
            CHECK(CreateSceneMaterialsBuffer(d3d, resources, scene), "create scene materials buffer!", log);
            CHECK(CreateSceneMaterialIndexingBuffers(d3d, resources, scene), "create scene material indexing buffers!", log);
            CHECK(CreateSceneIndexBuffers(d3d, resources, scene), "create scene index buffers!", log);
            CHECK(CreateSceneVertexBuffers(d3d, resources, scene), "create scene vertex buffers!", log);
            CHECK(CreateSceneBLAS(d3d, resources, scene), "create scene bottom level acceleration structures!", log);
            CHECK(CreateSceneTLAS(d3d, resources, scene), "create scene top level acceleration structure!", log);
            CHECK(CreateSceneTextures(d3d, resources, scene, log), "create scene textures!", log);

            // Execute GPU work to finish initialization
            SubmitCmdList(d3d);
            WaitForGPU(d3d);
            ResetCmdList(d3d);

            // Release upload buffers
            SAFE_RELEASE(resources.materialsSTBUpload);
            SAFE_RELEASE(resources.meshOffsetsRBUpload);
            SAFE_RELEASE(resources.geometryDataRBUpload);
            SAFE_RELEASE(resources.tlas.instancesUpload);

            // Release scene geometry upload buffers
            UINT resourceIndex;
            assert(resources.sceneIBs.size() == resources.sceneVBs.size());
            for (resourceIndex = 0; resourceIndex < static_cast<UINT>(resources.sceneIBs.size()); resourceIndex++)
            {
                SAFE_RELEASE(resources.sceneIBUploadBuffers[resourceIndex]);
                SAFE_RELEASE(resources.sceneVBUploadBuffers[resourceIndex]);
            }
            resources.sceneIBUploadBuffers.clear();
            resources.sceneVBUploadBuffers.clear();

            // Release scene texture upload buffers
            for (UINT resourceIndex = 0; resourceIndex < resources.sceneTextures.size(); resourceIndex++)
            {
                SAFE_RELEASE(resources.sceneTextureUploadBuffers[resourceIndex]);
            }
            resources.sceneTextureUploadBuffers.clear();

            // Release default texture upload buffers
            for (UINT resourceIndex = 0; resourceIndex < resources.textures.size(); resourceIndex++)
            {
                SAFE_RELEASE(resources.textureUploadBuffers[resourceIndex]);
            }
            resources.sceneTextureUploadBuffers.clear();

            // Unload the CPU-side textures
            Scenes::Cleanup(scene);

            return true;
        }

        /**
         * Update constant buffers.
         */
        void Update(Globals& d3d, Resources& resources, const Configs::Config& config, Scenes::Scene& scene)
        {
            // Update application constants
            resources.constants.app.frameNumber = d3d.frameNumber;
            resources.constants.app.skyRadiance = { config.scene.skyColor.x * config.scene.skyIntensity, config.scene.skyColor.y  * config.scene.skyIntensity, config.scene.skyColor.z  * config.scene.skyIntensity };

            // Update the camera constant buffer
            Scenes::Camera& camera = scene.GetActiveCamera();
            camera.data.resolution.x = (float)d3d.width;
            camera.data.resolution.y = (float)d3d.height;
            camera.data.aspect = camera.data.resolution.x / camera.data.resolution.y;
            memcpy(resources.cameraCBPtr, camera.GetGPUData(), camera.GetGPUDataSize());

            // Update the lights buffer for lights that have been modified
            UINT lastDirtyLight = 0;
            for (UINT lightIndex = 0; lightIndex < static_cast<UINT>(scene.lights.size()); lightIndex++)
            {
                Scenes::Light& light = scene.lights[lightIndex];
                if (light.dirty)
                {
                    UINT offset = lightIndex * Scenes::Light::GetGPUDataSize();
                    memcpy(resources.lightsSTBPtr + offset, light.GetGPUData(), Scenes::Light::GetGPUDataSize());
                    light.dirty = false;
                    lastDirtyLight = lightIndex + 1;
                }
            }

            if (lastDirtyLight > 0)
            {
                // Transition the lights device buffer to a copy destination
                D3D12_RESOURCE_BARRIER barrier = {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Transition.pResource = resources.lightsSTB;
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

                d3d.cmdList->ResourceBarrier(1, &barrier);

                // Schedule a copy of the upload buffer to the device buffer
                UINT size = Scenes::Light::GetGPUDataSize() * lastDirtyLight;
                d3d.cmdList->CopyBufferRegion(resources.lightsSTB, 0, resources.lightsSTBUpload, 0, size);

                // Transition the lights device buffer to generic read after the copy is complete
                barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;

                d3d.cmdList->ResourceBarrier(1, &barrier);
            }
        }

        /**
         * Update the swap chain.
         */
        bool Resize(Globals& d3d, GlobalResources& resources, int width, int height, std::ofstream& log)
        {
            d3d.width = width;
            d3d.height = height;

            d3d.viewport.Width = static_cast<float>(d3d.width);
            d3d.viewport.Height = static_cast<float>(d3d.height);
            d3d.scissor.right = d3d.width;
            d3d.scissor.bottom = d3d.height;

            // Release back buffer and GBuffer
            SAFE_RELEASE(d3d.backBuffer[0]);
            SAFE_RELEASE(d3d.backBuffer[1]);
            SAFE_RELEASE(resources.rt.GBufferA);
            SAFE_RELEASE(resources.rt.GBufferB);
            SAFE_RELEASE(resources.rt.GBufferC);
            SAFE_RELEASE(resources.rt.GBufferD);

            // Resize the swap chain
            DXGI_SWAP_CHAIN_DESC desc = {};
            d3d.swapChain->GetDesc(&desc);
            D3DCHECK(d3d.swapChain->ResizeBuffers(2, d3d.width, d3d.height, desc.BufferDesc.Format, desc.Flags));

            // Recreate the swapchain, back buffer, and GBuffer
            if (!CreateBackBuffer(d3d, resources)) return false;
            if (!CreateRenderTargets(d3d, resources)) return false;

            // Reset the frame index
            d3d.frameIndex = d3d.swapChain->GetCurrentBackBufferIndex();

            // Reset the frame number
            d3d.frameNumber = 1;

            log << "Back buffer resize, " << d3d.width << "x" << d3d.height << "\n";
            log << "GBuffer resize, " << d3d.width << "x" << d3d.height << "\n";
            std::flush(log);

            return true;
        }

        /**
         * Reset the command list.
         */
        bool ResetCmdList(Globals& d3d)
        {
            // Reset the command allocator for the current frame
            D3DCHECK(d3d.cmdAlloc[d3d.frameIndex]->Reset());

            // Reset the command list for the current frame
            D3DCHECK(d3d.cmdList->Reset(d3d.cmdAlloc[d3d.frameIndex], nullptr));

            return true;
        }

        /**
         * Submit the command list.
         */
        bool SubmitCmdList(Globals& d3d)
        {
            // Close the command list
            D3DCHECK(d3d.cmdList->Close());

            // Submit the command list
            ID3D12CommandList* pGraphicsList = { d3d.cmdList };
            d3d.cmdQueue->ExecuteCommandLists(1, &pGraphicsList);

            return true;
        }

        /**
         * Swap the back buffers.
         */
        bool Present(Globals& d3d)
        {
            HRESULT hr;
            if (!d3d.vsync && d3d.allowTearing) hr = d3d.swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
            else hr = d3d.swapChain->Present(d3d.vsync, 0);
            d3d.frameNumber++;
            return !FAILED(hr);
        }

        /**
         * Wait for pending GPU work to complete.
         */
        bool WaitForGPU(Globals& d3d)
        {
            // Increment the fence value
            d3d.fenceValue++;

            // Schedule a fence update in the queue (from the GPU)
            D3DCHECK(d3d.cmdQueue->Signal(d3d.fence, d3d.fenceValue));

            // Wait (on the CPU) until the fence has been processed on the GPU
            UINT64 fence = d3d.fence->GetCompletedValue();
            if (fence < d3d.fenceValue)
            {
                D3DCHECK(d3d.fence->SetEventOnCompletion(d3d.fenceValue, d3d.fenceEvent));
                WaitForSingleObjectEx(d3d.fenceEvent, INFINITE, FALSE);
            }

            return true;
        }

        /**
         * Prepare to render the next frame.
         */
        bool MoveToNextFrame(Globals& d3d)
        {
            // Set the frame index for the next frame
            d3d.frameIndex = d3d.swapChain->GetCurrentBackBufferIndex();
            return true;
        }

        /**
         * Resolve the timestamp queries.
         */
    #ifdef GFX_PERF_INSTRUMENTATION
        void BeginFrame(Globals& d3d, GlobalResources& resources, Instrumentation::Performance& performance)
        {
            d3d.cmdList->EndQuery(resources.timestampHeap, D3D12_QUERY_TYPE_TIMESTAMP, performance.gpuTimes[0]->GetGPUQueryBeginIndex());
        }

        void EndFrame(Globals& d3d, GlobalResources& resources, Instrumentation::Performance& performance)
        {
            d3d.cmdList->EndQuery(resources.timestampHeap, D3D12_QUERY_TYPE_TIMESTAMP, performance.gpuTimes[0]->GetGPUQueryEndIndex());
        }

        void ResolveTimestamps(Globals& d3d, GlobalResources& resources, Instrumentation::Performance& performance)
        {
            d3d.cmdList->ResolveQueryData(resources.timestampHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0, performance.GetNumActiveGPUQueries(), resources.timestamps, 0);
        }

        bool UpdateTimestamps(Globals& d3d, GlobalResources& resources, Instrumentation::Performance& performance)
        {
            std::vector<UINT64> queries;
            queries.resize(performance.GetNumActiveGPUQueries());

            // Copy the timestamps from the read-back buffer
            UINT64* pData = nullptr;
            D3D12_RANGE readRange = {};
            D3DCHECK(resources.timestamps->Map(0, &readRange, reinterpret_cast<void**>(&pData)));
            memcpy(queries.data(), pData, sizeof(UINT64) * performance.GetNumActiveGPUQueries());
            resources.timestamps->Unmap(0, nullptr);

            // Update the GPU performance stats for the active GPU timestamp queries
            UINT64 elapsedTicks = 0;
            for (UINT statIndex = 0; statIndex < static_cast<UINT>(performance.gpuTimes.size()); statIndex++)
            {
                // Get the stat
                Instrumentation::Stat*& s = performance.gpuTimes[statIndex];

                // Skip the stat if it wasn't active this frame
                if(s->gpuQueryStartIndex == -1) continue;

                // Compute the elapsed GPU time in milliseconds
                elapsedTicks = queries[s->gpuQueryEndIndex] - queries[s->gpuQueryStartIndex];
                s->elapsed = (1000 * static_cast<double>(elapsedTicks)) / static_cast<double>(resources.timestampFrequency);
                Instrumentation::Resolve(s);

                // Reset the GPU query indices for a new frame
                s->ResetGPUQueryIndices();
            }
            Instrumentation::Stat::ResetGPUQueryCount();

            return true;
        }
    #endif

        /**
         * Release D3D12 resources.
         */
        void Cleanup(Globals& d3d, GlobalResources& resources)
        {
            Cleanup(resources);
            Cleanup(d3d);
        }

        /**
         * Write the back buffer texture resources to disk.
         */
        bool WriteBackBufferToDisk(Globals& d3d, std::string directory)
        {
            return WriteResourceToDisk(d3d, directory + "/R-BackBuffer", d3d.backBuffer[d3d.frameIndex], D3D12_RESOURCE_STATE_PRESENT);
        }
    }

    /**
     * Create a graphics device.
     */
    bool CreateDevice(Globals& gfx, Configs::Config& config)
    {
        return Graphics::D3D12::CreateDevice(gfx, config);
    }

    /**
     * Create a graphics device.
     */
    bool Initialize(const Configs::Config& config, Scenes::Scene& scene, Globals& gfx, GlobalResources& resources, std::ofstream& log)
    {
        return Graphics::D3D12::Initialize(config, scene, gfx, resources, log);
    }

    /**
     * Update root constants and constant buffers.
     */
    void Update(Globals& gfx, GlobalResources& gfxResources, const Configs::Config& config, Scenes::Scene& scene)
    {
        Graphics::D3D12::Update(gfx, gfxResources, config, scene);
    }

    /**
     * Resize the swapchain.
     */
    bool Resize(Globals& gfx, GlobalResources& gfxResources, int width, int height, std::ofstream& log)
    {
        return Graphics::D3D12::Resize(gfx, gfxResources, width, height, log);
    }

    /**
     * Toggle between windowed and fullscreen borderless modes.
     */
    bool ToggleFullscreen(Globals& gfx)
    {
        return Graphics::D3D12::ToggleFullscreen(gfx);
    }

    /**
     * Reset the graphics command list.
     */
    bool ResetCmdList(Globals& gfx)
    {
        return Graphics::D3D12::ResetCmdList(gfx);
    }

    /**
     * Submit the graphics command list.
     */
    bool SubmitCmdList(Globals& gfx)
    {
        return Graphics::D3D12::SubmitCmdList(gfx);
    }

    /**
     * Present the current frame.
     */
    bool Present(Globals& gfx)
    {
        return Graphics::D3D12::Present(gfx);
    }

    /**
     * Wait for the graphics device to idle.
     */
    bool WaitForGPU(Globals& gfx)
    {
        return Graphics::D3D12::WaitForGPU(gfx);
    }

    /**
     * Move to the next the next frame.
     */
    bool MoveToNextFrame(Globals& gfx)
    {
        return Graphics::D3D12::MoveToNextFrame(gfx);
    }

#ifdef GFX_PERF_INSTRUMENTATION
    void BeginFrame(Globals& d3d, GlobalResources& d3dResources, Instrumentation::Performance& performance)
    {
        return Graphics::D3D12::BeginFrame(d3d, d3dResources, performance);
    }

    void EndFrame(Globals& d3d, GlobalResources& d3dResources, Instrumentation::Performance& performance)
    {
        return Graphics::D3D12::EndFrame(d3d, d3dResources, performance);
    }

    void ResolveTimestamps(Globals& d3d, GlobalResources& d3dResources, Instrumentation::Performance& performance)
    {
        return Graphics::D3D12::ResolveTimestamps(d3d, d3dResources, performance);
    }

    bool UpdateTimestamps(Globals& d3d, GlobalResources& d3dResources, Instrumentation::Performance& performance)
    {
        return Graphics::D3D12::UpdateTimestamps(d3d, d3dResources, performance);
    }
#endif

    /**
     * Cleanup global graphics resources.
     */
    void Cleanup(Globals& gfx, GlobalResources& gfxResources)
    {
        Graphics::D3D12::Cleanup(gfx, gfxResources);
    }

    /**
     * Write the back buffer texture resources to disk.
     */
    bool WriteBackBufferToDisk(Globals& d3d, std::string directory)
    {
        return Graphics::D3D12::WriteBackBufferToDisk(d3d, directory);
    }
}
