/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include <iostream>
#include <rtxgi/Defines.h>

#include "Common.h"
#include "Shaders.h"

//----------------------------------------------------------------------------------------------------------
// Private Functions
//----------------------------------------------------------------------------------------------------------

/**
 * Device creation helper.
 */
bool CreateDeviceInternal(ID3D12Device5* &device, IDXGIFactory4* &factory)
{
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
            // Check if the device supports ray tracing.
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
bool CreateCmdQueue(D3D12Info &d3d)
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
bool CreateCmdAllocators(D3D12Info &d3d)
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
bool CreateCmdList(D3D12Info &d3d)
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
bool CreateFence(D3D12Info &d3d)
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
bool CreateSwapChain(D3D12Info &d3d, HWND &window)
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
bool CreateDescriptorHeaps(D3D12Info &d3d, D3D12Resources &resources)
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
    // 1 trilinear sampler
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

    // Describe the CBV/SRV/UAV descriptor heap
    // 0:  1 CBV for the camera constants (b1)
    // 1:  1 CBV for the material constants (b2)
    // 2:  1 CBV for the lights constants (b3)
    // 3:  1 UAV for the RT GBufferA (u0)
    // 4:  1 UAV for the RT GBufferB (u1)
    // 5:  1 UAV for the RT GBufferC (u2)
    // 6:  1 UAV for the RT GBufferD (u3)
    // 7:  1 UAV for the RT AO Raw (u4)
    // 8:  1 UAV for the RT AO Filtered (u5)
    // 9:  1 UAV for the Vis TLAS instance data (u6)
    // 10: 1 UAV for the PT output (u7)
    // 11: 1 UAV for the PT accumulation (u8)
    // --- Entries added by the SDK for a DDGIVolume -----------
    // 12: 1 UAV for the probe RT radiance (u0, space1)
    // 13: 1 UAV for the probe irradiance (u1, space1)
    // 14: 1 UAV for the probe distance (u2, space1)
    // 15: 1 UAV for the probe offsets (optional) (u3, space1)
    // 16: 1 UAV for the probe states (optional) (u4, space1)
    // ---------------------------------------------------------
    // Entries used for sampling the DDGIVolume:
    // 17: 1 SRV for the probe irradiance (t0)
    // 18: 1 SRV for the probe distance (t1)
    // ---------------------------------------------------------
    // Loaded Textures:
    // 19: 1 SRV for 256x256 RGB blue noise texture
    // ---------------------------------------------------------
    // ImGui:
    // 20: ImGui font texture
    // ---------------------------------------------------------
    D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavDesc = {};
    cbvSrvUavDesc.NumDescriptors = 21;
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
bool CreateBackBuffer(D3D12Info &d3d, D3D12Resources &resources)
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
* Create the viewport.
*/
bool CreateViewport(D3D12Info &d3d)
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
 * Create the scissor.
 */
bool CreateScissor(D3D12Info &d3d)
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
bool CreateSamplers(D3D12Info &d3d, D3D12Resources &resources)
{
    // Get the sampler descriptor heap handle
    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.samplerHeap->GetCPUDescriptorHandleForHeapStart();

    // Describe a trilinear sampler
    D3D12_SAMPLER_DESC desc = {};
    memset(&desc, 0, sizeof(desc));
    desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
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
bool CreateCameraConstantBuffer(D3D12Info &d3d, D3D12Resources &resources)
{
    UINT size = RTXGI_ALIGN(256, sizeof(CameraInfo));
    D3D12BufferCreateInfo bufferInfo(size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
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
 * Create the material constant buffer.
 */
bool CreateMaterialConstantBuffer(D3D12Info &d3d, D3D12Resources &resources)
{
    const int numFaces = 18;
    UINT size = RTXGI_ALIGN(256, (sizeof(XMFLOAT4) * numFaces));
    D3D12BufferCreateInfo bufferInfo(size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &resources.materialCB)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.materialCB->SetName(L"Material Constant Buffer");
#endif

    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();

    // Create the CBV on the descriptor heap
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.SizeInBytes = size;
    cbvDesc.BufferLocation = resources.materialCB->GetGPUVirtualAddress();

    handle.ptr += resources.cbvSrvUavDescSize;      // material constant buffer is 2nd on the descriptor heap
    d3d.device->CreateConstantBufferView(&cbvDesc, handle);

    HRESULT hr = resources.materialCB->Map(0, nullptr, reinterpret_cast<void**>(&resources.materialCBStart));
    if (FAILED(hr)) return false;

    XMFLOAT4 red = { 0.63f, 0.065f, 0.05f, 1.f };
    XMFLOAT4 green = { 0.14f, 0.45f, 0.091f, 1.f };
    XMFLOAT4 white = { 0.725f, 0.71f, 0.68f, 1.f };

#if RTXGI_DDGI_DEBUG_COLORS
    XMFLOAT4 yellow = { 1.f, 1.f, 0.f, 1.f };
    XMFLOAT4 blue = { 0.f, 0.f, 1.f, 1.f };
    XMFLOAT4 cyan = { 1.f, 0.f, 1.f, 1.f };
    XMFLOAT4 magenta = { 0.f, 1.f, 1.f, 1.f };
    XMFLOAT4 orange = { 1.f, 0.56f, 0.f, 1.f };

    XMFLOAT4 colors[numFaces] = 
    {
        yellow, red, magenta, green, cyan, blue,
        orange, orange, orange, orange, orange, orange,
        white, white, white, white, white, white
    };
#else
    XMFLOAT4 colors[numFaces] =
    { 
        white, red, white, green, white, white,
        white, white, white, white, white, white,
        white, white, white, white, white, white 
    };
#endif

    memcpy(resources.materialCBStart, colors, (sizeof(XMFLOAT4) * numFaces));
    resources.materialCB->Unmap(0, nullptr);

    return true;
}

/**
 * Create the lights constant buffer.
 */
bool CreateLightsConstantBuffer(D3D12Info &d3d, D3D12Resources &resources)
{
    UINT size = RTXGI_ALIGN(256, sizeof(LightInfo));
    D3D12BufferCreateInfo bufferInfo(size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &resources.lightsCB)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.lightsCB->SetName(L"Lights Constant Buffer");
#endif

    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();

    // Create the CBV on the descriptor heap
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.SizeInBytes = size;

    handle.ptr += resources.cbvSrvUavDescSize * 2;          // lights constant buffer is 3rd on the descriptor heap
    cbvDesc.BufferLocation = resources.lightsCB->GetGPUVirtualAddress();

    d3d.device->CreateConstantBufferView(&cbvDesc, handle);

    HRESULT hr = resources.lightsCB->Map(0, nullptr, reinterpret_cast<void**>(&resources.lightsCBStart));
    if (FAILED(hr)) return false;

    return true;
}

/**
 * Create the root signature used for compute shaders.
 */
bool CreateComputeRootSignature(D3D12Info &d3d, D3D12Resources &resources)
{
    // Describe the root signature
    D3D12_DESCRIPTOR_RANGE ranges[1];
    UINT rangeIndex = 0;

    // RTGBufferA, RTGBufferB, RTGBufferC, RTGBufferD, RTAORaw, RTAOFiltered (u0, u1, u2, u3, u4, u5)
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = 6;
    ranges[rangeIndex].RegisterSpace = 0;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = 3;
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
bool CreateRasterRootSignature(D3D12Info &d3d, D3D12Resources &resources)
{
    // Describe the root signature
    D3D12_DESCRIPTOR_RANGE ranges[7];
    UINT rangeIndex = 0;

    // Camera constant buffer (b1)
    ranges[rangeIndex].BaseShaderRegister = 1;
    ranges[rangeIndex].NumDescriptors = 1;
    ranges[rangeIndex].RegisterSpace = 0;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = 0;
    rangeIndex++;

    // Lights constant buffer (b3)
    ranges[rangeIndex].BaseShaderRegister = 3;
    ranges[rangeIndex].NumDescriptors = 1;
    ranges[rangeIndex].RegisterSpace = 0;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = 2;
    rangeIndex++;

    // RTGBufferA, RTGBufferB, RTGBufferC, RTGBufferD, RTAORaw, RTAOFiltered (u0, u1, u2, u3, u4, u5)
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = 6;
    ranges[rangeIndex].RegisterSpace = 0;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = 3;
    rangeIndex++;

    // --- RTXGI DDGIVolume Entries -------------------------------------------

    // RTXGI DDGIVolume RT probe radiance (u0, space1)
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = 1;
    ranges[rangeIndex].RegisterSpace = 1;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = 12;
    rangeIndex++;

    // RTXGI DDGIVolume probe offsets, probe states (u3, u4, space1)
    ranges[rangeIndex].BaseShaderRegister = 3;
    ranges[rangeIndex].NumDescriptors = 2;
    ranges[rangeIndex].RegisterSpace = 1;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = 15;
    rangeIndex++;

    // --- RTXGI DDGIVolume Entries -------------------------------------------

    // RTXGI DDGIVolume probe irradiance and distance SRV (t0, t1)
    ranges[rangeIndex].BaseShaderRegister = 0;
    ranges[rangeIndex].NumDescriptors = 2;
    ranges[rangeIndex].RegisterSpace = 0;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = 17;
    rangeIndex++;

    // Blue Noise RGB SRV (t5)
    ranges[rangeIndex].BaseShaderRegister = 5;
    ranges[rangeIndex].NumDescriptors = 1;
    ranges[rangeIndex].RegisterSpace = 0;
    ranges[rangeIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[rangeIndex].OffsetInDescriptorsFromTableStart = 19;

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
    param3.Constants.Num32BitValues = 8;
    param3.Constants.RegisterSpace = 0;
    param3.Constants.ShaderRegister = 4;

    // Raster root constants (b5)
    D3D12_ROOT_PARAMETER param4 = {};
    param4.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    param4.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    param4.Constants.Num32BitValues = 8;
    param4.Constants.RegisterSpace = 0;
    param4.Constants.ShaderRegister = 5;

    D3D12_ROOT_PARAMETER rootParams[5] = { param0, param1, param2, param3, param4 };

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
 * Create a compute PSO
 */
bool CreateComputePSO(D3D12_SHADER_BYTECODE &cs, ID3D12RootSignature* rs, ID3D12PipelineState** pso, D3D12Info &d3d)
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = rs;
    desc.CS = cs;

    HRESULT hr = d3d.device->CreateComputePipelineState(&desc, IID_PPV_ARGS(pso));
    if (FAILED(hr)) return false;
    return true;
}

/**
 * Create a graphics PSO for full screen passes.
 */
bool CreatePSO(D3D12_SHADER_BYTECODE &vs, D3D12_SHADER_BYTECODE &ps, ID3D12RootSignature* rs, ID3D12PipelineState** pso, D3D12Info &d3d)
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
 * Load shaders and create the compute PSO for the AO filtering.
 */
bool CreateAOFilterPSO(D3D12Info &d3d, D3D12Resources &resources, D3D12ShaderCompiler &shaderCompiler)
{
    std::wstring file = std::wstring(shaderCompiler.root.begin(), shaderCompiler.root.end());
    file.append(L"shaders\\AOFilterCS.hlsl");

    // Load the shader
    D3D12ShaderInfo csInfo;
    csInfo.filename = file.c_str();
    csInfo.entryPoint = L"CS";
    csInfo.targetProfile = L"cs_6_0";

    wstring blockSize = to_wstring((int)AO_FILTER_BLOCK_SIZE);
    DxcDefine defines[] =
    {
        L"BLOCK_SIZE", blockSize.c_str(),
    };

    csInfo.numDefines = _countof(defines);
    csInfo.defines = defines;

    if (!Shaders::Compile(shaderCompiler, csInfo, true)) return false;

    D3D12_SHADER_BYTECODE cs;
    cs.BytecodeLength = csInfo.bytecode->GetBufferSize();
    cs.pShaderBytecode = csInfo.bytecode->GetBufferPointer();

    // Create the PSO
    if (!CreateComputePSO(cs, resources.computeRootSig, &resources.AOFilterPSO, d3d)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.AOFilterPSO->SetName(L"AO Filter");
#endif

    return true;
}

/**
 * Load shaders and create the graphics PSO for the indirect fullscreen pass.
 */
bool CreateIndirectPSO(D3D12Info &d3d, D3D12Resources &resources, D3D12ShaderCompiler &shaderCompiler)
{
    std::wstring file = std::wstring(shaderCompiler.root.begin(), shaderCompiler.root.end());
    file.append(L"shaders\\Indirect.hlsl");

    // Load the shaders
    D3D12ShaderInfo vsInfo;
    vsInfo.filename = file.c_str();
    vsInfo.entryPoint = L"VS";
    vsInfo.targetProfile = L"vs_6_0";
    if (!Shaders::Compile(shaderCompiler, vsInfo, true)) return false;

    D3D12_SHADER_BYTECODE vs;
    vs.BytecodeLength = vsInfo.bytecode->GetBufferSize();
    vs.pShaderBytecode = vsInfo.bytecode->GetBufferPointer();

    D3D12ShaderInfo psInfo;
    psInfo.filename = file.c_str();
    psInfo.entryPoint = L"PS";
    psInfo.targetProfile = L"ps_6_0";
    if (!Shaders::Compile(shaderCompiler, psInfo, true)) return false;

    D3D12_SHADER_BYTECODE ps;
    ps.BytecodeLength = psInfo.bytecode->GetBufferSize();
    ps.pShaderBytecode = psInfo.bytecode->GetBufferPointer();

    // Create the PSO
    if (!CreatePSO(vs, ps, resources.rasterRootSig, &resources.indirectPSO, d3d)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.indirectPSO->SetName(L"Indirect PSO");
#endif

    return true;
}

/**
 * Load shaders and create the graphics PSO for the DDGIVolume buffer visualization fullscreen pass.
 */
bool CreateVisPSO(D3D12Info &d3d, D3D12Resources &resources, D3D12ShaderCompiler &shaderCompiler)
{
    std::wstring file = std::wstring(shaderCompiler.root.begin(), shaderCompiler.root.end());
    file.append(L"shaders\\VisDDGIBuffers.hlsl");

    // Load the shaders
    D3D12ShaderInfo vsInfo;
    vsInfo.filename = file.c_str();
    vsInfo.entryPoint = L"VS";
    vsInfo.targetProfile = L"vs_6_0";
    if (!Shaders::Compile(shaderCompiler, vsInfo, true)) return false;

    D3D12_SHADER_BYTECODE vs;
    vs.BytecodeLength = vsInfo.bytecode->GetBufferSize();
    vs.pShaderBytecode = vsInfo.bytecode->GetBufferPointer();

    D3D12ShaderInfo psInfo;
    psInfo.filename = file.c_str();
    psInfo.entryPoint = L"PS";
    psInfo.targetProfile = L"ps_6_0";
    if (!Shaders::Compile(shaderCompiler, psInfo, true)) return false;

    D3D12_SHADER_BYTECODE ps;
    ps.BytecodeLength = psInfo.bytecode->GetBufferSize();
    ps.pShaderBytecode = psInfo.bytecode->GetBufferPointer();

    // Create the PSO
    if (!CreatePSO(vs, ps, resources.rasterRootSig, &resources.visBuffersPSO, d3d)) return false;
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

/*
* Initialize D3D12.
*/
bool Initialize(D3D12Info &d3d, D3D12Resources &resources, D3D12ShaderCompiler &shaderCompiler, HWND &window)
{
    if (!CreateCmdQueue(d3d)) return false;
    if (!CreateCmdAllocators(d3d)) return false;
    if (!CreateFence(d3d)) return false;
    if (!CreateSwapChain(d3d, window)) return false;
    if (!CreateCmdList(d3d)) return false;
    if (!ResetCmdList(d3d)) return false;
    if (!CreateDescriptorHeaps(d3d, resources)) return false;
    if (!CreateBackBuffer(d3d, resources)) return false;
    if (!CreateSamplers(d3d, resources)) return false;
    if (!CreateViewport(d3d)) return false;
    if (!CreateScissor(d3d)) return false;

    if (!CreateCameraConstantBuffer(d3d, resources)) return false;
    if (!CreateMaterialConstantBuffer(d3d, resources)) return false;
    if (!CreateLightsConstantBuffer(d3d, resources)) return false;

    if (!CreateComputeRootSignature(d3d, resources)) return false;
    if (!CreateAOFilterPSO(d3d, resources, shaderCompiler)) return false;

    if (!CreateRasterRootSignature(d3d, resources)) return false;
    if (!CreateIndirectPSO(d3d, resources, shaderCompiler)) return false;
    if (!CreateVisPSO(d3d, resources, shaderCompiler)) return false;

    return true;
}

/**
* Create a root signature.
*/
ID3D12RootSignature* CreateRootSignature(D3D12Info &d3d, const D3D12_ROOT_SIGNATURE_DESC &desc)
{
    ID3DBlob* sig = nullptr;
    ID3DBlob* error = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &error);
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
bool CreateBuffer(D3D12Info &d3d, D3D12BufferCreateInfo &info, ID3D12Resource** ppResource)
{
    D3D12_HEAP_PROPERTIES heapDesc = {};
    heapDesc.Type = info.heapType;
    heapDesc.CreationNodeMask = 1;
    heapDesc.VisibleNodeMask = 1;

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
* Create a GPU texture resource.
*/
bool CreateTexture(UINT64 width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_STATES state, ID3D12Resource** resource, ID3D12Device* device)
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

/**
* Create a D3D12 device.
*/
bool CreateDevice(ID3D12Device5* &device)
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
    IDXGIFactory4* factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return false;

    return CreateDeviceInternal(device, factory);
}

/**
* Create a D3D12 device.
*/
bool CreateDevice(D3D12Info &d3d)
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

    return CreateDeviceInternal(d3d.device, d3d.factory);
}

/**
* Reset the command list.
*/
bool ResetCmdList(D3D12Info &d3d)
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
void SubmitCmdList(D3D12Info &d3d)
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
void Present(D3D12Info &d3d)
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
bool WaitForGPU(D3D12Info &d3d)
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
bool MoveToNextFrame(D3D12Info &d3d)
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
* Free D3D12 resources.
*/
void Cleanup(D3D12Info &d3d, D3D12Resources &resources)
{
    if (resources.cameraCB) resources.cameraCB->Unmap(0, nullptr);
    if (resources.lightsCB) resources.lightsCB->Unmap(0, nullptr);
    resources.cameraCBStart = 0;
    resources.materialCBStart = 0;
    resources.lightsCBStart = 0;

    RTXGI_SAFE_RELEASE(resources.rtvHeap);
    RTXGI_SAFE_RELEASE(resources.cbvSrvUavHeap);
    RTXGI_SAFE_RELEASE(resources.samplerHeap);

    resources.geometry.Release();
    for (UINT resourceIndex = 0; resourceIndex < resources.vertexBuffers.size(); resourceIndex++)
    {
        RTXGI_SAFE_RELEASE(resources.vertexBuffers[resourceIndex]);
        RTXGI_SAFE_RELEASE(resources.indexBuffers[resourceIndex]);
    }

    for (UINT resourceIndex = 0; resourceIndex < resources.textures.size(); resourceIndex++)
    {
        RTXGI_SAFE_RELEASE(resources.textures[resourceIndex].texture);
        RTXGI_SAFE_RELEASE(resources.textures[resourceIndex].uploadBuffer);
    }

    RTXGI_SAFE_RELEASE(resources.sphereVertexBuffer);
    RTXGI_SAFE_RELEASE(resources.sphereIndexBuffer);

    RTXGI_SAFE_RELEASE(resources.computeRootSig);
    RTXGI_SAFE_RELEASE(resources.AOFilterPSO);

    RTXGI_SAFE_RELEASE(resources.rasterRootSig);
    RTXGI_SAFE_RELEASE(resources.indirectPSO);
    RTXGI_SAFE_RELEASE(resources.visBuffersPSO);

    RTXGI_SAFE_RELEASE(resources.cameraCB);
    RTXGI_SAFE_RELEASE(resources.materialCB);
    RTXGI_SAFE_RELEASE(resources.lightsCB);

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
