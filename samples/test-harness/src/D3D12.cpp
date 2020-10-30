/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "Common.h"
#include "Shaders.h"

#include <iostream>
#include <fstream>
#include <wincodec.h>
#include <ScreenGrab12.h>
#include <rtxgi/Defines.h>

using namespace DirectX;

static const D3D12_HEAP_PROPERTIES defaultHeapProperties =
{
    D3D12_HEAP_TYPE_DEFAULT,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0, 0
};

static const D3D12_HEAP_PROPERTIES uploadHeapProperties =
{
    D3D12_HEAP_TYPE_UPLOAD,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0, 0
};


//----------------------------------------------------------------------------------------------------------
// Private Functions
//----------------------------------------------------------------------------------------------------------

/**
 * Device creation helper.
 */
bool CreateDeviceInternal(ID3D12Device5* &device, IDXGIFactory4* &factory)
{
#if _DEBUG
    {
        ID3D12Debug1* debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            // GPU based validation was causing unexpected TDRs, so be careful if you use it
            //debugController->SetEnableGPUBasedValidation(TRUE);
        }
    }
#endif
    // Create the device
    IDXGIAdapter1* adapter = nullptr;
    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 adapterDesc;
        adapter->GetDesc1(&adapterDesc);

        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            continue;   // Don't select the Basic Render Driver adapter
        }

        if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device5), (void**)&device)))
        {
            // Check if the device supports ray tracing
            D3D12_FEATURE_DATA_D3D12_OPTIONS5 features = {};
            HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features, sizeof(features));
            if (FAILED(hr) || features.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
            {
                RTXGI_SAFE_RELEASE(device);
                device = nullptr;
                continue;
            }

#if RTXGI_NAME_D3D_OBJECTS
            device->SetName(L"D3D12 Device");
#endif
            break;
        }

        if (device == nullptr)
        {
            // Didn't find a device that supports ray tracing
            return false;
        }
    }

    if(adapter)
    {
        RTXGI_SAFE_RELEASE(adapter);
    }

    return true;
}

/**
* Create a command queue.
*/
bool CreateCmdQueue(D3D12Global &d3d)
{
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    HRESULT hr = d3d.device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d.cmdQueue));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    d3d.cmdQueue->SetName(L"D3D12 Command Queue");
#endif
    return true;
}

/**
* Create a command allocator for each frame.
*/
bool CreateCmdAllocators(D3D12Global &d3d)
{
    for (UINT n = 0; n < 2; n++)
    {
        HRESULT hr = d3d.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d.cmdAlloc[n]));
        if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
        d3d.cmdAlloc[n]->SetName(L"D3D12 Command Allocator");
#endif
    }
    return true;
}

/**
* Create the command list.
*/
bool CreateCmdList(D3D12Global &d3d)
{
    HRESULT hr = d3d.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d.cmdAlloc[d3d.frameIndex], nullptr, IID_PPV_ARGS(&d3d.cmdList));
    hr = d3d.cmdList->Close();
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    d3d.cmdList->SetName(L"DXR Command List");
#endif
    return true;
}

/**
* Create a fence and event handle.
*/
bool CreateFence(D3D12Global &d3d)
{
    // Create the fence
    HRESULT hr = d3d.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&d3d.fence));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    d3d.fence->SetName(L"D3D12/DXR Fence");
#endif

    d3d.fenceValues[0] = d3d.fenceValues[1] = 0;
    d3d.fenceValues[d3d.frameIndex]++;

    // Create the event handle to use for frame synchronization
    d3d.fenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
    if (d3d.fenceEvent == nullptr)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        if (FAILED(hr)) return false;
    }
    return true;
}

/**
* Create a swap chain.
*/
bool CreateSwapChain(D3D12Global &d3d, HWND &window)
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

    IDXGISwapChain1* swapChain;

    // Create the swap chain
    HRESULT hr = d3d.factory->CreateSwapChainForHwnd(d3d.cmdQueue, window, &desc, nullptr, nullptr, &swapChain);
    if (FAILED(hr)) return false;

    // Associate the swap chain with a window
    hr = d3d.factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(hr)) return false;

    // Get the swap chain interface
    hr = swapChain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&d3d.swapChain));
    if (FAILED(hr)) return false;

    RTXGI_SAFE_RELEASE(swapChain);
    d3d.frameIndex = d3d.swapChain->GetCurrentBackBufferIndex();

    return true;
}

/**
* Create the RTV, CBV/SRV/UAV, and Sampler descriptor heaps.
*/
bool CreateDescriptorHeaps(D3D12Global &d3d, D3D12Resources &resources, const Scene &scene)
{
    // Describe the RTV heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    rtvDesc.NumDescriptors = 2;
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    // Create the RTV heap
    HRESULT hr = d3d.device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&resources.rtvHeap));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.rtvHeap->SetName(L"RTV Descriptor Heap");
#endif

    resources.rtvDescSize = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Describe the sampler heap
    // 1 bilinear sampler
    // 1 point sampler
    D3D12_DESCRIPTOR_HEAP_DESC samplerDesc = {};
    samplerDesc.NumDescriptors = 2;
    samplerDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    samplerDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    // Create the sampler heap
    hr = d3d.device->CreateDescriptorHeap(&samplerDesc, IID_PPV_ARGS(&resources.samplerHeap));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.samplerHeap->SetName(L"Sampler Descriptor Heap");
#endif

    // see Common.h DescriptorHeapConstants for detailed descriptor layout
    D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavDesc = {};
    cbvSrvUavDesc.NumDescriptors = DescriptorHeapConstants::SCENE_TEXTURE_OFFSET + (UINT)scene.textures.size();
    cbvSrvUavDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvSrvUavDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    
    // Create the CBV/SRV/UAV descriptor heap
    hr = d3d.device->CreateDescriptorHeap(&cbvSrvUavDesc, IID_PPV_ARGS(&resources.cbvSrvUavHeap));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.cbvSrvUavHeap->SetName(L"CBV/SRV/UAV Descriptor Heap");
#endif

    resources.cbvSrvUavDescSize = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    return true;
}

/**
* Create the back buffer and RTV.
*/
bool CreateBackBuffer(D3D12Global &d3d, D3D12Resources &resources)
{
    HRESULT hr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;

    rtvHandle = resources.rtvHeap->GetCPUDescriptorHandleForHeapStart();

    // Create a RTV for each back buffer
    for (UINT n = 0; n < 2; n++)
    {
        hr = d3d.swapChain->GetBuffer(n, IID_PPV_ARGS(&d3d.backBuffer[n]));
        if (FAILED(hr)) return false;

        d3d.device->CreateRenderTargetView(d3d.backBuffer[n], nullptr, rtvHandle);

#if RTXGI_NAME_D3D_OBJECTS
        if (n == 0)
        {
            d3d.backBuffer[n]->SetName(L"Back Buffer 0");
        }
        else
        {
            d3d.backBuffer[n]->SetName(L"Back Buffer 1");
        }
#endif

        rtvHandle.ptr += (1 * resources.rtvDescSize);
    }
    return true;
}

/**
* Create the raster viewport.
*/
bool CreateViewport(D3D12Global &d3d)
{
    d3d.viewport.Width = (float)d3d.width;
    d3d.viewport.Height = (float)d3d.height;
    d3d.viewport.MinDepth = D3D12_MIN_DEPTH;
    d3d.viewport.MaxDepth = D3D12_MAX_DEPTH;
    d3d.viewport.TopLeftX = 0.f;
    d3d.viewport.TopLeftY = 0.f;
    return true;
}

/**
 * Create the raster scissor.
 */
bool CreateScissor(D3D12Global &d3d)
{
    d3d.scissor.left = 0;
    d3d.scissor.top = 0;
    d3d.scissor.right = d3d.width;
    d3d.scissor.bottom = d3d.height;
    return true;
}

/**
* Create the render targets.
*/
bool CreateRenderTargets(D3D12Global &d3d, D3D12Resources &resources)
{
    // Describe the render target resources
    D3D12_RESOURCE_DESC desc = {};
    desc.DepthOrArraySize = 1;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    desc.Width = d3d.width;
    desc.Height = d3d.height;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;

    // Create GBufferA
    HRESULT hr = d3d.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.GBufferA));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.GBufferA->SetName(L"GBufferA");
#endif

    // Create GBufferB (RGBA32_FLOAT)
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    hr = d3d.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.GBufferB));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.GBufferB->SetName(L"GBufferB");
#endif

    // Create GBufferC (RGBA32_FLOAT)
    hr = d3d.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.GBufferC));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.GBufferC->SetName(L"GBufferC");
#endif

    // Create GBufferD (RGBA32_FLOAT)
    hr = d3d.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.GBufferD));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.GBufferD->SetName(L"GBufferD");
#endif

    // Create the RTAO Raw resource (R8_UNORM)
    desc.Format = DXGI_FORMAT_R8_UNORM;
    hr = d3d.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.RTAORaw));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.RTAORaw->SetName(L"RTAO Raw");
#endif

    // Create the RTAO Filtered resource (R8_UNORM)
    hr = d3d.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.RTAOFiltered));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.RTAOFiltered->SetName(L"RTAO Filtered");
#endif

    // Create the path tracing render target (RGBA8_UNORM)
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    hr = d3d.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&resources.PTOutput));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.PTOutput->SetName(L"PT Output");
#endif

    // Create the PT accumulation render target (RGBA32_FLOAT)
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    hr = d3d.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&resources.PTAccumulation));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.PTAccumulation->SetName(L"PT Accumulation");
#endif

    // Create the render target UAVs on the descriptor heap
    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    UINT handleIncrement = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

    // GBuffer resources
    handle.ptr += (handleIncrement * 2);        // GBufferA is 3rd on the descriptor heap
    d3d.device->CreateUnorderedAccessView(resources.GBufferA, nullptr, &uavDesc, handle);

    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    handle.ptr += handleIncrement;              // GBufferB is 4th on the descriptor heap
    d3d.device->CreateUnorderedAccessView(resources.GBufferB, nullptr, &uavDesc, handle);

    handle.ptr += handleIncrement;              // GBufferC is 5th on the descriptor heap
    d3d.device->CreateUnorderedAccessView(resources.GBufferC, nullptr, &uavDesc, handle);

    handle.ptr += handleIncrement;              // GBufferD is 6th on the descriptor heap
    d3d.device->CreateUnorderedAccessView(resources.GBufferD, nullptr, &uavDesc, handle);

    // Ambient Occlusion resources
    uavDesc.Format = DXGI_FORMAT_R8_UNORM;

    handle.ptr += handleIncrement;              // RTAORaw is 7th on the descriptor heap
    d3d.device->CreateUnorderedAccessView(resources.RTAORaw, nullptr, &uavDesc, handle);

    handle.ptr += handleIncrement;              // RTAOFiltered is 8th on the descriptor heap
    d3d.device->CreateUnorderedAccessView(resources.RTAOFiltered, nullptr, &uavDesc, handle);

    // Path Tracing resources
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    handle.ptr += handleIncrement;              // PTOutput is 9th on the descriptor heap
    d3d.device->CreateUnorderedAccessView(resources.PTOutput, nullptr, &uavDesc, handle);

    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    handle.ptr += handleIncrement;              // PTAccumluation is 10th on the descriptor heap
    d3d.device->CreateUnorderedAccessView(resources.PTAccumulation, nullptr, &uavDesc, handle);

    return true;
}

/*
* Create the scene geometry vertex buffers.
*/
bool CreateVertexBuffers(D3D12Global &d3d, D3D12Resources &resources, const Scene &scene)
{
    resources.sceneVBs.resize(scene.numGeometries);
    resources.sceneVBViews.resize(scene.numGeometries);
    for (size_t meshIndex = 0; meshIndex < scene.meshes.size(); meshIndex++)
    {
        // Get the mesh
        const Mesh mesh = scene.meshes[meshIndex];
        for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); primitiveIndex++)
        {
            // Get the mesh primitive
            const MeshPrimitive primitive = mesh.primitives[primitiveIndex];

            // Create the vertex buffer and copy the data to the GPU
            if (!D3D12::CreateVertexBuffer(d3d, &resources.sceneVBs[primitive.index], resources.sceneVBViews[primitive.index], primitive)) return false;
#if RTXGI_NAME_D3D_OBJECTS
            std::string name = "VB: ";
            name.append(mesh.name.c_str());
            name.append(", Primitive: ");
            name.append(std::to_string(primitiveIndex));
            std::wstring n = std::wstring(name.begin(), name.end());
            resources.sceneVBs[primitive.index]->SetName(n.c_str());
#endif
        }
    }
    return true;
}

/**
* Create the scene geometry index buffers.
*/
bool CreateIndexBuffers(D3D12Global &d3d, D3D12Resources &resources, const Scene &scene)
{
    resources.sceneIBs.resize(scene.numGeometries);
    resources.sceneIBViews.resize(scene.numGeometries);
    for (size_t meshIndex = 0; meshIndex < scene.meshes.size(); meshIndex++)
    {
        // Get the mesh
        const Mesh mesh = scene.meshes[meshIndex];
        for (size_t primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); primitiveIndex++)
        {
            // Get the mesh primitive
            const MeshPrimitive primitive = mesh.primitives[primitiveIndex];

            // Create the index buffer and copy the data to the GPU
            if (!D3D12::CreateIndexBuffer(d3d, &resources.sceneIBs[primitive.index], resources.sceneIBViews[primitive.index], primitive)) return false;
#if RTXGI_NAME_D3D_OBJECTS
            std::string name = "IB: ";
            name.append(mesh.name.c_str());
            name.append(", Primitive: ");
            name.append(std::to_string(primitiveIndex));
            std::wstring n = std::wstring(name.begin(), name.end());
            resources.sceneIBs[primitive.index]->SetName(n.c_str());
#endif
        }
    }
    return true;
}

/**
* Create the scene textures.
*/
bool CreateTextures(D3D12Global &d3d, D3D12Resources &resources, const Scene &scene)
{
    // Early out if there are no scene textures
    if (scene.textures.size() == 0) return true;

    UINT64 uploadBufferSize = 0;
    size_t textureIndex = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += (resources.cbvSrvUavDescSize * DescriptorHeapConstants::SCENE_TEXTURE_OFFSET);   // Scene textures start at the 44th slot on the descriptor heap

    // Create the texture resources
    resources.sceneTextures.resize(scene.textures.size());
    for (textureIndex = 0; textureIndex < scene.textures.size(); textureIndex++)
    {
        // Get the texture
        const Texture texture = scene.textures[textureIndex];

        // Create the texture resource
        if (!D3D12::CreateTexture(d3d, &resources.sceneTextures[textureIndex], texture.width, texture.height, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_STATE_COPY_DEST)) return false;
#if RTXGI_NAME_D3D_OBJECTS
        std::string str = "Texture: ";
        str.append(texture.name);
        std::wstring name = std::wstring(str.begin(), str.end());
        resources.sceneTextures[textureIndex]->SetName(name.c_str());
#endif

        // Add the size of this texture to the running total
        uploadBufferSize += (RTXGI_ALIGN(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, texture.width * texture.stride) * texture.height);

        // Create an SRV for the texture
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        d3d.device->CreateShaderResourceView(resources.sceneTextures[textureIndex], &srvDesc, handle);

        // Increment the slot on the descriptor heap
        handle.ptr += resources.cbvSrvUavDescSize;
    }

    // Describe the upload buffer resource
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = uploadBufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // Create the upload buffer
    HRESULT hr = d3d.device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resources.sceneTextureUploadBuffer));
    if (FAILED(hr)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.sceneTextureUploadBuffer->SetName(L"Scene Texture Upload Heap");
#endif

    // Copy the textures to the upload buffer
    UINT8* pData = nullptr;
    hr = resources.sceneTextureUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pData));
    for (textureIndex = 0; textureIndex < scene.textures.size(); textureIndex++)
    {
        const Texture texture = scene.textures[textureIndex];
        size_t rowSize = (texture.width * texture.stride);
        if (rowSize < D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)
        {
            // Copy each row of the image, padding the copies for the pitch alignment
            UINT8* source = texture.pixels;
            for (size_t rowIndex = 0; rowIndex < texture.height; rowIndex++)
            {
                memcpy(pData, source, rowSize);
                pData += D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
                source += rowSize;
            }
        }
        else
        {
            // RowSize is aligned, copy the entire image
            size_t size = (texture.width * texture.height * texture.stride);
            memcpy(pData, texture.pixels, size);
            pData += size;
        }
    }
    resources.sceneTextureUploadBuffer->Unmap(0, nullptr);

    // Schedule a copy of each texture from the upload heap resource to default heap resource
    UINT64 offset = 0;
    for (textureIndex = 0; textureIndex < scene.textures.size(); textureIndex++)
    {
        const Texture texture = scene.textures[textureIndex];

        // Describe the upload heap resource (source)
        D3D12_TEXTURE_COPY_LOCATION source = {};
        source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        source.pResource = resources.sceneTextureUploadBuffer;
        source.PlacedFootprint.Offset = offset;
        source.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        source.PlacedFootprint.Footprint.Width = texture.width;
        source.PlacedFootprint.Footprint.Height = texture.height;
        source.PlacedFootprint.Footprint.RowPitch = RTXGI_ALIGN(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, (texture.width * texture.stride));
        source.PlacedFootprint.Footprint.Depth = 1;

        // Describe the default heap resource (destination)
        D3D12_TEXTURE_COPY_LOCATION destination = {};
        destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        destination.pResource = resources.sceneTextures[textureIndex];
        destination.SubresourceIndex = 0;

        // Copy the texture from the upload heap to the default heap resource
        d3d.cmdList->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);

        // Transition the default heap texture resource to a shader resource
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resources.sceneTextures[textureIndex];
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        d3d.cmdList->ResourceBarrier(1, &barrier);

        offset += (RTXGI_ALIGN(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, texture.width * texture.stride) * texture.height);
    }

    return true;
}

/**
 * Create the samplers.
 */
bool CreateSamplers(D3D12Global &d3d, D3D12Resources &resources)
{
    // Get the sampler descriptor heap handle
    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.samplerHeap->GetCPUDescriptorHandleForHeapStart();

    // Describe a bilinear sampler
    D3D12_SAMPLER_DESC desc = {};
    memset(&desc, 0, sizeof(desc));
    desc.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    desc.MipLODBias = 0.f;
    desc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    desc.MinLOD = 0.f;
    desc.MaxLOD = D3D12_FLOAT32_MAX;
    desc.MaxAnisotropy = 1;

    // Create the sampler
    d3d.device->CreateSampler(&desc, handle);

    UINT handleIncrement = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    // Describe a point sampler
    desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    
    handle.ptr += handleIncrement;
    d3d.device->CreateSampler(&desc, handle);

    return true;
}

/**
 * Create the camera constant buffer.
 */
bool CreateCameraConstantBuffer(D3D12Global &d3d, D3D12Resources &resources)
{
    UINT size = RTXGI_ALIGN(256, sizeof(Camera));
    D3D12BufferInfo bufferInfo(size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &resources.cameraCB)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.cameraCB->SetName(L"Camera Constant Buffer");
#endif

    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();

    // Create the CBV on the descriptor heap
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.SizeInBytes = size;
    cbvDesc.BufferLocation = resources.cameraCB->GetGPUVirtualAddress();    // camera constant buffer is 1st on the descriptor heap
        
    d3d.device->CreateConstantBufferView(&cbvDesc, handle);
     
    HRESULT hr = resources.cameraCB->Map(0, nullptr, reinterpret_cast<void**>(&resources.cameraCBStart));
    if (FAILED(hr)) return false;

    return true;
}

/**
 * Create the lights constant buffer.
 */
bool CreateLightsConstantBuffer(D3D12Global &d3d, D3D12Resources &resources)
{
    UINT size = RTXGI_ALIGN(256, sizeof(LightInfo));
    D3D12BufferInfo bufferInfo(size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &resources.lightsCB)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.lightsCB->SetName(L"Lights Constant Buffer");
#endif

    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();

    // Create the CBV on the descriptor heap
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.SizeInBytes = size;

    handle.ptr += resources.cbvSrvUavDescSize * 1;          // lights constant buffer is 2nd on the descriptor heap
    cbvDesc.BufferLocation = resources.lightsCB->GetGPUVirtualAddress();

    d3d.device->CreateConstantBufferView(&cbvDesc, handle);

    HRESULT hr = resources.lightsCB->Map(0, nullptr, reinterpret_cast<void**>(&resources.lightsCBStart));
    if (FAILED(hr)) return false;

    return true;
}

/**
 * Create the root signature used for compute shaders.
 */
bool CreateComputeRootSignature(D3D12Global &d3d, D3D12Resources &resources)
{
    // Describe the root signature
    D3D12_DESCRIPTOR_RANGE ranges[1];
    UINT rangeIndex = 0;

    // GBufferA, GBufferB, GBufferC, GBufferD, RTAORaw, RTAOFiltered (u0, u1, u2, u3, u4, u5)
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = 6;
    ranges[rangeIndex].RegisterSpace = 0;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapConstants::RT_GBUFFER_OFFSET;
    rangeIndex++;

    // CBV/SRV/UAV descriptor table
    D3D12_ROOT_PARAMETER param0 = {};
    param0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param0.DescriptorTable.NumDescriptorRanges = _countof(ranges);
    param0.DescriptorTable.pDescriptorRanges = ranges;

    // Root constants (b0)
    D3D12_ROOT_PARAMETER param1 = {};
    param1.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    param1.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param1.Constants.Num32BitValues = 12;
    param1.Constants.RegisterSpace = 0;
    param1.Constants.ShaderRegister = 0;

    D3D12_ROOT_PARAMETER rootParams[2] = { param0, param1 };

    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.NumParameters = _countof(rootParams);
    rootDesc.pParameters = rootParams;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    // Create the root signature
    resources.computeRootSig = D3D12::CreateRootSignature(d3d, rootDesc);
    if (resources.computeRootSig == nullptr) return false;

#if RTXGI_NAME_D3D_OBJECTS
    resources.computeRootSig->SetName(L"Compute Root Signature");
#endif
    return true;
}

/**
 * Create the root signature used for raster passes.
 */
bool CreateRasterRootSignature(D3D12Global &d3d, D3D12Resources &resources)
{
    // Describe the root signature
    D3D12_DESCRIPTOR_RANGE ranges[6];
    UINT rangeIndex = 0;

    // Camera and lights constant buffers (b1, b2)
    ranges[rangeIndex].BaseShaderRegister = 1;
    ranges[rangeIndex].NumDescriptors = 2;
    ranges[rangeIndex].RegisterSpace = 0;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapConstants::CAMERA_OFFSET;
    rangeIndex++;

    // RTGBufferA, RTGBufferB, RTGBufferC, RTGBufferD, RTAORaw, RTAOFiltered (u0, u1, u2, u3, u4, u5)
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = 6;
    ranges[rangeIndex].RegisterSpace = 0;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapConstants::RT_GBUFFER_OFFSET;
    rangeIndex++;

    // --- RTXGI DDGIVolume Entries -------------------------------------------
    // SRV array (t0, space1)
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = DescriptorHeapConstants::DESCRIPTORS_PER_VOLUME * NUM_MAX_VOLUMES;
    ranges[rangeIndex].RegisterSpace = 1;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapConstants::VOLUME_OFFSET;
    rangeIndex++;

    // UAV float array (u0, space1)
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = DescriptorHeapConstants::DESCRIPTORS_PER_VOLUME * NUM_MAX_VOLUMES;
    ranges[rangeIndex].RegisterSpace = 1;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapConstants::VOLUME_OFFSET;
    rangeIndex++;
    
    // UAV uint array (u0, space2)
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = DescriptorHeapConstants::DESCRIPTORS_PER_VOLUME * NUM_MAX_VOLUMES;
    ranges[rangeIndex].RegisterSpace = 2;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapConstants::VOLUME_OFFSET;
    rangeIndex++;

    // Blue Noise RGB SRV (t5)
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = 1;
    ranges[rangeIndex].RegisterSpace = 0;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = DescriptorHeapConstants::BLUE_NOISE_OFFSET;
    rangeIndex++;

    // Samplers (s0, s1)
    D3D12_DESCRIPTOR_RANGE samplerRange;
    samplerRange.BaseShaderRegister = 0;
    samplerRange.NumDescriptors = 2;
    samplerRange.RegisterSpace = 0;
    samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    samplerRange.OffsetInDescriptorsFromTableStart = 0;

    // Volume Constant Buffer (b1, space1)
    D3D12_ROOT_PARAMETER param0;
    param0.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    param0.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    param0.Descriptor.RegisterSpace = 1;
    param0.Descriptor.ShaderRegister = 1;

    // CBV/SRV/UAV descriptor table
    D3D12_ROOT_PARAMETER param1 = {};
    param1.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param1.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    param1.DescriptorTable.NumDescriptorRanges = _countof(ranges);
    param1.DescriptorTable.pDescriptorRanges = ranges;

    // Sampler descriptor table
    D3D12_ROOT_PARAMETER param2 = {};
    param2.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param2.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    param2.DescriptorTable.NumDescriptorRanges = 1;
    param2.DescriptorTable.pDescriptorRanges = &samplerRange;

    // Noise root constants (b4)
    D3D12_ROOT_PARAMETER param3 = {};
    param3.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    param3.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    param3.Constants.Num32BitValues = 12;
    param3.Constants.RegisterSpace = 0;
    param3.Constants.ShaderRegister = 4;

    // Raster root constants (b5)
    D3D12_ROOT_PARAMETER param4 = {};
    param4.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    param4.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    param4.Constants.Num32BitValues = 8;
    param4.Constants.RegisterSpace = 0;
    param4.Constants.ShaderRegister = 5;

    // volume root constant (b0, space1)
    D3D12_ROOT_PARAMETER param5 = {};
    param5.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    param5.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    param5.Constants.Num32BitValues = 1;
    param5.Constants.RegisterSpace = 1;
    param5.Constants.ShaderRegister = 0;

    D3D12_ROOT_PARAMETER rootParams[6] = { param0, param1, param2, param3, param4, param5 };

    D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.NumParameters = _countof(rootParams);
    rootDesc.pParameters = rootParams;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // Create the root signature
    resources.rasterRootSig = D3D12::CreateRootSignature(d3d, rootDesc);
    if (resources.rasterRootSig == nullptr) return false;

#if RTXGI_NAME_D3D_OBJECTS
    resources.rasterRootSig->SetName(L"Fullscreen Raster Root Signature");
#endif
    return true;
}

/**
 * Create a graphics pipeline state object for full screen passes.
 */
bool CreatePSO(D3D12Global &d3d, D3D12_SHADER_BYTECODE &vs, D3D12_SHADER_BYTECODE &ps, ID3D12RootSignature* rs, ID3D12PipelineState** pso)
{
    const D3D12_RENDER_TARGET_BLEND_DESC defaultBlendDesc =
    {
        FALSE,FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL,
    };

    D3D12_INPUT_ELEMENT_DESC defaultInputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // Describe the rasterizer states
    D3D12_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterDesc.CullMode = D3D12_CULL_MODE_NONE;

    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0] = defaultBlendDesc;

    // Describe and create the PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
    desc.InputLayout = { defaultInputElementDescs, _countof(defaultInputElementDescs) };
    desc.pRootSignature = rs;
    desc.VS = vs;
    desc.PS = ps;
    desc.RasterizerState = rasterDesc;
    desc.BlendState = blendDesc;
    desc.SampleMask = UINT_MAX;
    desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;

    // Create the PSO
    HRESULT hr = d3d.device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(pso));
    if (FAILED(hr)) return false;
    return true;
}

/**
 * Load shaders and create a compute pipeline state object for the AO filtering.
 */
bool CreateAOFilterPSO(D3D12Global &d3d, D3D12Resources &resources, ShaderCompiler &shaderCompiler)
{
    std::wstring file = std::wstring(shaderCompiler.root.begin(), shaderCompiler.root.end());
    file.append(L"shaders\\AOFilterCS.hlsl");

    // Load and compile the compute shader
    ShaderProgram csInfo;
    csInfo.filepath = file.c_str();
    csInfo.entryPoint = L"CS";
    csInfo.targetProfile = L"cs_6_0";

    std::wstring blockSize = std::to_wstring((int)AO_FILTER_BLOCK_SIZE);
    DxcDefine defines[] =
    {
        L"BLOCK_SIZE", blockSize.c_str(),
    };

    csInfo.numDefines = _countof(defines);
    csInfo.defines = defines;

    if (!Shaders::Compile(shaderCompiler, csInfo, true)) return false;

    // Create the PSO
    if (!D3D12::CreateComputePSO(d3d.device, resources.computeRootSig, csInfo.bytecode, &resources.AOFilterPSO)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.AOFilterPSO->SetName(L"AO Filter PSO");
#endif

    return true;
}

/**
 * Load shaders and create the graphics pipeline state object for the indirect fullscreen pass.
 */
bool CreateIndirectPSO(D3D12Global &d3d, D3D12Resources &resources, ShaderCompiler &shaderCompiler)
{
    std::wstring file = std::wstring(shaderCompiler.root.begin(), shaderCompiler.root.end());
    file.append(L"shaders\\Indirect.hlsl");

    // Load and compile the vertex shader
    ShaderProgram vsInfo;
    vsInfo.filepath = file.c_str();
    vsInfo.entryPoint = L"VS";
    vsInfo.targetProfile = L"vs_6_0";
    if (!Shaders::Compile(shaderCompiler, vsInfo, true)) return false;

    D3D12_SHADER_BYTECODE vs;
    vs.BytecodeLength = vsInfo.bytecode->GetBufferSize();
    vs.pShaderBytecode = vsInfo.bytecode->GetBufferPointer();

    // Load and compile the pixel shader
    ShaderProgram psInfo;
    psInfo.filepath = file.c_str();
    psInfo.entryPoint = L"PS";
    psInfo.targetProfile = L"ps_6_0";
    if (!Shaders::Compile(shaderCompiler, psInfo, true)) return false;

    D3D12_SHADER_BYTECODE ps;
    ps.BytecodeLength = psInfo.bytecode->GetBufferSize();
    ps.pShaderBytecode = psInfo.bytecode->GetBufferPointer();

    // Create the PSO
    if (!CreatePSO(d3d, vs, ps, resources.rasterRootSig, &resources.indirectPSO)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.indirectPSO->SetName(L"Indirect Lighting PSO");
#endif

    return true;
}

/**
 * Load shaders and create the graphics pipeline state object for the DDGIVolume buffer visualization fullscreen pass.
 */
bool CreateVisPSO(D3D12Global &d3d, D3D12Resources &resources, ShaderCompiler &shaderCompiler)
{
    std::wstring file = std::wstring(shaderCompiler.root.begin(), shaderCompiler.root.end());
    file.append(L"shaders\\VisDDGIBuffers.hlsl");

    // Load and compile the vertex shader
    ShaderProgram vsInfo;
    vsInfo.filepath = file.c_str();
    vsInfo.entryPoint = L"VS";
    vsInfo.targetProfile = L"vs_6_0";
    if (!Shaders::Compile(shaderCompiler, vsInfo, true)) return false;

    D3D12_SHADER_BYTECODE vs;
    vs.BytecodeLength = vsInfo.bytecode->GetBufferSize();
    vs.pShaderBytecode = vsInfo.bytecode->GetBufferPointer();

    // Load and compile the vertex shader
    ShaderProgram psInfo;
    psInfo.filepath = file.c_str();
    psInfo.entryPoint = L"PS";
    psInfo.targetProfile = L"ps_6_0";
    if (!Shaders::Compile(shaderCompiler, psInfo, true)) return false;

    D3D12_SHADER_BYTECODE ps;
    ps.BytecodeLength = psInfo.bytecode->GetBufferSize();
    ps.pShaderBytecode = psInfo.bytecode->GetBufferPointer();

    // Create the PSO
    if (!CreatePSO(d3d, vs, ps, resources.rasterRootSig, &resources.visBuffersPSO)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.visBuffersPSO->SetName(L"DDGIVolume Buffer Visualization PSO");
#endif

    return true;
}

//----------------------------------------------------------------------------------------------------------
// Public Functions
//----------------------------------------------------------------------------------------------------------

namespace D3D12
{

/**
* Create a D3D12 device.
*/
bool CreateDevice(D3D12Global &d3d)
{
#if defined(_DEBUG)
    // Enable the D3D12 debug layer.
    {
        ID3D12Debug* debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
        }
    }
#endif

    // Create a DXGI Factory
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&d3d.factory));
    if (FAILED(hr)) return false;

    // Create the device
    return CreateDeviceInternal(d3d.device, d3d.factory);
}

/*
* Initialize D3D12.
*/
bool Initialize(D3D12Global &d3d, D3D12Resources &resources, ShaderCompiler &shaderCompiler, Scene &scene, HWND &window)
{
    // Create core D3D12 objects
    if (!CreateCmdQueue(d3d)) return false;
    if (!CreateCmdAllocators(d3d)) return false;
    if (!CreateFence(d3d)) return false;
    if (!CreateSwapChain(d3d, window)) return false;
    if (!CreateCmdList(d3d)) return false;
    if (!ResetCmdList(d3d)) return false;
    if (!CreateDescriptorHeaps(d3d, resources, scene)) return false;
    if (!CreateBackBuffer(d3d, resources)) return false;
    if (!CreateRenderTargets(d3d, resources)) return false;
    if (!CreateSamplers(d3d, resources)) return false;
    if (!CreateViewport(d3d)) return false;
    if (!CreateScissor(d3d)) return false;

    // Create Root Signatures
    if (!CreateComputeRootSignature(d3d, resources)) return false;
    if (!CreateRasterRootSignature(d3d, resources)) return false;

    // Create Pipeline State Objects
    if (!CreateAOFilterPSO(d3d, resources, shaderCompiler)) return false;
    if (!CreateIndirectPSO(d3d, resources, shaderCompiler)) return false;
    if (!CreateVisPSO(d3d, resources, shaderCompiler)) return false;

    // Create constant buffers
    if (!CreateCameraConstantBuffer(d3d, resources)) return false;
    if (!CreateLightsConstantBuffer(d3d, resources)) return false;

    return true;
}

/**
* Create a root signature.
*/
ID3D12RootSignature* CreateRootSignature(D3D12Global &d3d, const D3D12_ROOT_SIGNATURE_DESC &desc)
{
    ID3DBlob* sig = nullptr;
    ID3DBlob* error = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &error);
    if (FAILED(hr) && error)
    {
        const char* errorMsg = (const char*)error->GetBufferPointer();
        OutputDebugString(errorMsg);
    }
    if (FAILED(hr)) return nullptr;

    ID3D12RootSignature* pRootSig;
    hr = d3d.device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&pRootSig));
    if (FAILED(hr)) return nullptr;

    RTXGI_SAFE_RELEASE(sig);
    RTXGI_SAFE_RELEASE(error);
    return pRootSig;
}

/**
* Create a compute pipeline state object.
*/
bool CreateComputePSO(ID3D12Device* device, ID3D12RootSignature* rootSignature, ID3DBlob* shader, ID3D12PipelineState** pipeline)
{
    if (shader == nullptr) return false;
    if (rootSignature == nullptr) return false;

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeDesc = {};
    pipeDesc.CS.BytecodeLength = shader->GetBufferSize();
    pipeDesc.CS.pShaderBytecode = shader->GetBufferPointer();
    pipeDesc.pRootSignature = rootSignature;

    HRESULT hr = device->CreateComputePipelineState(&pipeDesc, IID_PPV_ARGS(pipeline));
    if (FAILED(hr)) return false;

    return true;
}

/**
* Create a GPU buffer resource.
*/
bool CreateBuffer(D3D12Global &d3d, D3D12BufferInfo &info, ID3D12Resource** ppResource)
{
    // Describe the heap the GPU buffer resource uses
    D3D12_HEAP_PROPERTIES heapDesc = {};
    heapDesc.Type = info.heapType;
    heapDesc.CreationNodeMask = 1;
    heapDesc.VisibleNodeMask = 1;

    // Describe the GPU buffer resource
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = info.alignment;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Width = info.size;
    resourceDesc.Flags = info.flags;

    // Create the GPU resource
    HRESULT hr = d3d.device->CreateCommittedResource(&heapDesc, D3D12_HEAP_FLAG_NONE, &resourceDesc, info.state, nullptr, IID_PPV_ARGS(ppResource));
    if (FAILED(hr)) return false;
    return true;
}

/**
* Create a GPU texture resource on the default heap.
*/
bool CreateTexture(D3D12Global &d3d, ID3D12Resource** resource, UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_STATES state)
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
    HRESULT hr = d3d.device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(resource));
    if (hr != S_OK) return false;
    return true;
}

/**
 * Create the vertex buffer for a mesh primitive.
 */
bool CreateVertexBuffer(D3D12Global &d3d, ID3D12Resource** vb, D3D12_VERTEX_BUFFER_VIEW &view, const MeshPrimitive &primitive)
{
    UINT stride = sizeof(Vertex);

    // Create the vertex buffer
    D3D12BufferInfo info(primitive.vertices.size() * stride, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!D3D12::CreateBuffer(d3d, info, vb)) return false;

    // Copy the vertex data to the GPU
    UINT8* pDataBegin = nullptr;
    D3D12_RANGE readRange = {};
    HRESULT hr = (*vb)->Map(0, &readRange, reinterpret_cast<void**>(&pDataBegin));
    if (FAILED(hr)) return false;

    memcpy(pDataBegin, primitive.vertices.data(), info.size);
    (*vb)->Unmap(0, nullptr);

    // Initialize the vertex buffer view
    view.BufferLocation = (*vb)->GetGPUVirtualAddress();
    view.StrideInBytes = stride;
    view.SizeInBytes = static_cast<UINT>(info.size);

    return true;
}

/**
 * Create the index buffer for a mesh primitive.
 */
bool CreateIndexBuffer(D3D12Global &d3d, ID3D12Resource** ib, D3D12_INDEX_BUFFER_VIEW &view, const MeshPrimitive &primitive)
{
    UINT stride = sizeof(UINT);

    // Create the index buffer
    D3D12BufferInfo info(primitive.indices.size() * stride, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!D3D12::CreateBuffer(d3d, info, ib)) return false;

    // Copy the index data to the GPU
    UINT8* pDataBegin = nullptr;
    D3D12_RANGE readRange = {};
    HRESULT hr = (*ib)->Map(0, &readRange, reinterpret_cast<void**>(&pDataBegin));
    if (FAILED(hr)) return false;

    memcpy(pDataBegin, primitive.indices.data(), info.size);
    (*ib)->Unmap(0, nullptr);

    // Initialize the index buffer view
    view.BufferLocation = (*ib)->GetGPUVirtualAddress();
    view.SizeInBytes = static_cast<UINT>(info.size);
    view.Format = DXGI_FORMAT_R32_UINT;

    return true;
}

/**
* Reset the command list.
*/
bool ResetCmdList(D3D12Global &d3d)
{
    // Reset the command allocator for the current frame
    HRESULT hr = d3d.cmdAlloc[d3d.frameIndex]->Reset();
    if (FAILED(hr)) return false;

    // Reset the command list for the current frame
    hr = d3d.cmdList->Reset(d3d.cmdAlloc[d3d.frameIndex], nullptr);
    if (FAILED(hr)) return false;

    return true;
}

/*
* Submit the command list.
*/
void SubmitCmdList(D3D12Global &d3d)
{
    d3d.cmdList->Close();

    ID3D12CommandList* pGraphicsList = { d3d.cmdList };
    d3d.cmdQueue->ExecuteCommandLists(1, &pGraphicsList);
    d3d.fenceValues[d3d.frameIndex]++;
    d3d.cmdQueue->Signal(d3d.fence, d3d.fenceValues[d3d.frameIndex]);
}

/**
* Swap the back buffers.
*/
void Present(D3D12Global &d3d)
{
    HRESULT hr = d3d.swapChain->Present(d3d.vsync, 0);
    if (FAILED(hr))
    {
        hr = d3d.device->GetDeviceRemovedReason();
        throw std::runtime_error("Error: failed to present!");
    }
}

/*
* Wait for pending GPU work to complete.
*/
bool WaitForGPU(D3D12Global &d3d)
{
    // Schedule a signal command in the queue
    HRESULT hr = d3d.cmdQueue->Signal(d3d.fence, d3d.fenceValues[d3d.frameIndex]);
    if (FAILED(hr)) return false;

    // Wait until the fence has been processed
    hr = d3d.fence->SetEventOnCompletion(d3d.fenceValues[d3d.frameIndex], d3d.fenceEvent);
    if (FAILED(hr)) return false;

    WaitForSingleObjectEx(d3d.fenceEvent, INFINITE, FALSE);

    // Increment the fence value for the current frame
    d3d.fenceValues[d3d.frameIndex]++;
    return true;
}

/**
* Prepare to render the next frame.
*/
bool MoveToNextFrame(D3D12Global &d3d)
{
    // Schedule a Signal command in the queue
    const UINT64 currentFenceValue = d3d.fenceValues[d3d.frameIndex];
    HRESULT hr = d3d.cmdQueue->Signal(d3d.fence, currentFenceValue);
    if (FAILED(hr)) return false;

    // Update the frame index
    d3d.frameIndex = d3d.swapChain->GetCurrentBackBufferIndex();

    // If the next frame is not ready to be rendered yet, wait until it is
    if (d3d.fence->GetCompletedValue() < d3d.fenceValues[d3d.frameIndex])
    {
        hr = d3d.fence->SetEventOnCompletion(d3d.fenceValues[d3d.frameIndex], d3d.fenceEvent);
        if (FAILED(hr)) return false;

        WaitForSingleObjectEx(d3d.fenceEvent, INFINITE, FALSE);
    }

    // Set the fence value for the next frame
    d3d.fenceValues[d3d.frameIndex] = currentFenceValue + 1;

    d3d.frameNumber++;
    return true;
}

/*
* Save the back buffer to disk.
*/
bool ScreenCapture(D3D12Global &d3d, std::string filename)
{
    CoInitialize(NULL);
    std::wstring f = std::wstring(filename.begin(), filename.end());
    f.append(L".jpg");
    HRESULT hr = SaveWICTextureToFile(d3d.cmdQueue, d3d.backBuffer[d3d.frameIndex], GUID_ContainerFormatJpeg, f.c_str(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT);
    if(FAILED(hr)) return false;
    return true;
}

/*
* Release GPU resources.
*/
void Cleanup(D3D12Global &d3d)
{
    // Release core D3D12 objects
    RTXGI_SAFE_RELEASE(d3d.backBuffer[0]);
    RTXGI_SAFE_RELEASE(d3d.backBuffer[1]);
    RTXGI_SAFE_RELEASE(d3d.swapChain);
    RTXGI_SAFE_RELEASE(d3d.fence);
    RTXGI_SAFE_RELEASE(d3d.cmdList);
    RTXGI_SAFE_RELEASE(d3d.cmdAlloc[0]);
    RTXGI_SAFE_RELEASE(d3d.cmdAlloc[1]);
    RTXGI_SAFE_RELEASE(d3d.cmdQueue);
    RTXGI_SAFE_RELEASE(d3d.device);
    RTXGI_SAFE_RELEASE(d3d.factory);
}

}

namespace D3DResources
{

/*
* Initialize D3D assets for the scene.
*/
bool Initialize(D3D12Global &d3d, D3D12Resources &resources, Scene &scene)
{
    if (!CreateVertexBuffers(d3d, resources, scene)) return false;
    if (!CreateIndexBuffers(d3d, resources, scene)) return false;
    if (!CreateTextures(d3d, resources, scene)) return false;
    return true;
}

/*
* Release GPU resources.
*/
void Cleanup(D3D12Resources &resources)
{
    // Unmap constant buffers (if they exist)
    if (resources.cameraCB) resources.cameraCB->Unmap(0, nullptr);
    if (resources.lightsCB) resources.lightsCB->Unmap(0, nullptr);
    resources.cameraCBStart = nullptr;
    resources.materialCBStart = nullptr;
    resources.lightsCBStart = nullptr;

    // Release render targets
    RTXGI_SAFE_RELEASE(resources.GBufferA);
    RTXGI_SAFE_RELEASE(resources.GBufferB);
    RTXGI_SAFE_RELEASE(resources.GBufferC);
    RTXGI_SAFE_RELEASE(resources.GBufferD);
    RTXGI_SAFE_RELEASE(resources.RTAORaw);
    RTXGI_SAFE_RELEASE(resources.RTAOFiltered);
    RTXGI_SAFE_RELEASE(resources.PTOutput);
    RTXGI_SAFE_RELEASE(resources.PTAccumulation);

    // Release descriptor heaps
    RTXGI_SAFE_RELEASE(resources.rtvHeap);
    RTXGI_SAFE_RELEASE(resources.cbvSrvUavHeap);
    RTXGI_SAFE_RELEASE(resources.samplerHeap);

    // Release the root signatures
    RTXGI_SAFE_RELEASE(resources.computeRootSig);
    RTXGI_SAFE_RELEASE(resources.rasterRootSig);

    // Release the pipeline state objects
    RTXGI_SAFE_RELEASE(resources.AOFilterPSO);
    RTXGI_SAFE_RELEASE(resources.indirectPSO);
    RTXGI_SAFE_RELEASE(resources.visBuffersPSO);

    // Release the constant buffers
    RTXGI_SAFE_RELEASE(resources.cameraCB);
    RTXGI_SAFE_RELEASE(resources.materialCB);
    RTXGI_SAFE_RELEASE(resources.lightsCB);

    // Release scene geometry
    size_t resourceIndex;
    for (resourceIndex = 0; resourceIndex < resources.sceneVBs.size(); resourceIndex++)
    {
        RTXGI_SAFE_RELEASE(resources.sceneVBs[resourceIndex]);
        RTXGI_SAFE_RELEASE(resources.sceneIBs[resourceIndex]);
    }

    // Release visualization geometry
    RTXGI_SAFE_RELEASE(resources.sphereVB);
    RTXGI_SAFE_RELEASE(resources.sphereIB);

    // Release scene textures
    for (resourceIndex = 0; resourceIndex < resources.sceneTextures.size(); resourceIndex++)
    {
        RTXGI_SAFE_RELEASE(resources.sceneTextures[resourceIndex]);
    }
    RTXGI_SAFE_RELEASE(resources.sceneTextureUploadBuffer);

    // Release additional textures
    for (resourceIndex = 0; resourceIndex < resources.textures.size(); resourceIndex++)
    {
        RTXGI_SAFE_RELEASE(resources.textures[resourceIndex]);
        RTXGI_SAFE_RELEASE(resources.textureUploadBuffers[resourceIndex]);
    }
}

}


