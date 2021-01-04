/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include "Harness.h"

#if RTXGI_PERF_MARKERS
#define USE_PIX
#include <pix3.h>
#endif

#include "Config.h"
#include "GLTF.h"
#include "ImGui.h"
#include "Shaders.h"
#include "Textures.h"
#include "UI.h"

#include <sstream>

/**
 * Returns integer x/y, but if there is a remainder, rounds up.
 */
inline static UINT DivRoundUp(UINT x, UINT y)
{
    if (x % y) return 1 + x / y;
    else return x / y;
}

namespace Harness
{

/**
* Performs initialization tasks for the test harness.
*/
bool Initialize(ConfigInfo &config, Scene &scene, D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, ShaderCompiler &shaderCompiler, HWND &window, std::ofstream &log)
{
    // Load the GLTF assets
    log << "Loading assets...";
    if (!GLTF::Load(config, scene))
    {
        log << "\nError: failed to load the GLTF assets!";
        return false;
    }
    log << "done.\n";

    // Initialize the DXC shader compiler
    log << "Initializing DXC...";
    if (!Shaders::InitCompiler(shaderCompiler))
    {
        log << "\nError: failed to initialize the DXC shader compiler!";
        return false;
    }
    log << "done.\n";

    // Initialize D3D12
    log << "Initializing D3D12...";
    if (!D3D12::Initialize(d3d, resources, shaderCompiler, scene, window))
    {
        log << "\nError: failed to initialize D3D12!";
        return false;
    }
    log << "done.\n";

    // Initialize scene resources
    log << "Creating GPU resources...";
    if (!D3DResources::Initialize(d3d, resources, scene))
    {
        log << "\nError: failed to create GPU resources for scene!";
        return false;
    }
    log << "done.\n";

    // Initialize DXR
    log << "Initializing DXR...";
    if (!DXR::Initialize(d3d, dxr, resources, shaderCompiler, scene))
    {
        log << "\nError: failed to initialize DXR!";
        return false;
    }
    log << "done.\n";

    // Load additional textures
    log << "Loading additional textures...";

    std::string filepath = config.root;
    filepath.append("data\\textures\\blue-noise-rgb-256.png");

    Texture blueNoise;
    blueNoise.name = "Blue Noise";
    blueNoise.filepath = filepath;

    // Load texture, create a D3D12 resource, and upload texels to the GPU
    if (!Textures::LoadAndCreateTexture(d3d, resources, blueNoise, resources.blueNoiseIndex))
    {
        log << "\nError: failed to load and create a GPU resource for blue noise texture!";
        return false;
    }
    log << "done.\n";

    // Initialize ImGui
    log << "Initializing ImGui...";
    UI::Initialize(d3d, resources, window);
    log << "done.\n";

    D3D12::SubmitCmdList(d3d);
    D3D12::WaitForGPU(d3d);
    D3D12::ResetCmdList(d3d);

    return true;
}

/**
 * Loads and compiles RTXGI SDK shaders.
 */
bool CompileVolumeShaders(std::vector<ShaderProgram> &shaders, ShaderCompiler &shaderCompiler, const rtxgi::DDGIVolumeDesc &volumeDesc, std::ofstream &log)
{
    log << "Loading and compiling shaders...";

    std::wstring numRays = std::to_wstring((int)volumeDesc.numRaysPerProbe);
    std::wstring numIrradianceTexels = std::to_wstring(volumeDesc.numIrradianceTexels);
    std::wstring numDistanceTexels = std::to_wstring(volumeDesc.numDistanceTexels);

    std::wstring path = std::wstring(shaderCompiler.rtxgi.begin(), shaderCompiler.rtxgi.end());
    std::wstring file;

    // RTXGI irradiance blending
    file = path + L"shaders/ddgi/ProbeBlendingCS.hlsl";

    shaders.push_back(ShaderProgram());
    shaders.back().filepath = file.c_str();
    shaders.back().entryPoint = L"DDGIProbeBlendingCS";
    shaders.back().targetProfile = L"cs_6_0";
    shaders.back().numDefines = 4;
    shaders.back().defines = new DxcDefine[shaders.back().numDefines];
    shaders.back().defines[0].Name = L"RTXGI_DDGI_BLEND_RADIANCE";
    shaders.back().defines[0].Value = L"1";
    shaders.back().defines[1].Name = L"RAYS_PER_PROBE";
    shaders.back().defines[1].Value = numRays.c_str();
    shaders.back().defines[2].Name = L"PROBE_NUM_TEXELS";
    shaders.back().defines[2].Value = numIrradianceTexels.c_str();
    shaders.back().defines[3].Name = L"PROBE_UAV_INDEX";
    shaders.back().defines[3].Value = L"0";

    if (!Shaders::Compile(shaderCompiler, shaders.back(), true))
    {
        log << "\nError: failed to load and compile the probe irradiance blending compute shader!\n";
        return false;
    }
    RTXGI_SAFE_DELETE_ARRAY(shaders.back().defines);

    // RTXGI distance blending
    shaders.push_back(ShaderProgram());
    shaders.back().filepath = file.c_str();
    shaders.back().entryPoint = L"DDGIProbeBlendingCS";
    shaders.back().targetProfile = L"cs_6_0";
    shaders.back().numDefines = 4;
    shaders.back().defines = new DxcDefine[shaders.back().numDefines];
    shaders.back().defines[0].Name = L"RTXGI_DDGI_BLEND_RADIANCE";
    shaders.back().defines[0].Value = L"0";
    shaders.back().defines[1].Name = L"RAYS_PER_PROBE";
    shaders.back().defines[1].Value = numRays.c_str();
    shaders.back().defines[2].Name = L"PROBE_NUM_TEXELS";
    shaders.back().defines[2].Value = numDistanceTexels.c_str();
    shaders.back().defines[3].Name = L"PROBE_UAV_INDEX";
    shaders.back().defines[3].Value = L"1";

    if (!Shaders::Compile(shaderCompiler, shaders.back(), true))
    {
        log << "\nError: failed to load and compile the probe distance blending compute shader!\n";
        return false;
    }
    RTXGI_SAFE_DELETE_ARRAY(shaders.back().defines);

    // RTXGI border rows update
    file = path + L"shaders/ddgi/ProbeBorderUpdateCS.hlsl";

    shaders.push_back(ShaderProgram());
    shaders.back().filepath = file.c_str();
    shaders.back().entryPoint = L"DDGIProbeBorderRowUpdateCS";
    shaders.back().targetProfile = L"cs_6_0";

    if (!Shaders::Compile(shaderCompiler, shaders.back(), true))
    {
        log << "\nError: failed to load and compile the probe border update compute shader!\n";
        return false;
    }

    // RTXGI border columns update
    shaders.push_back(ShaderProgram());
    shaders.back().filepath = file.c_str();
    shaders.back().entryPoint = L"DDGIProbeBorderColumnUpdateCS";
    shaders.back().targetProfile = L"cs_6_0";

    if (!Shaders::Compile(shaderCompiler, shaders.back(), true))
    {
        log << "\nError: failed to load and compile the probe border update compute shader!\n";
        return false;
    }

    shaders.push_back(ShaderProgram());   // ensures the classifier bytecode is always at the same array index
#if RTXGI_DDGI_PROBE_RELOCATION
    // RTXGI probe relocation
    file = path + L"shaders/ddgi/ProbeRelocationCS.hlsl";

    shaders.back().filepath = file.c_str();
    shaders.back().entryPoint = L"DDGIProbeRelocationCS";
    shaders.back().targetProfile = L"cs_6_0";
    if (!Shaders::Compile(shaderCompiler, shaders.back(), true))
    {
        log << "\nError: failed to load and compile the probe relocation compute shader!\n";
        return false;
    }
#endif

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    // RTXGI probe state classifier
    file = path + L"shaders/ddgi/ProbeStateClassifierCS.hlsl";

    shaders.push_back(ShaderProgram());
    shaders.back().filepath = file.c_str();
    shaders.back().entryPoint = L"DDGIProbeStateClassifierCS";
    shaders.back().targetProfile = L"cs_6_0";
    if (!Shaders::Compile(shaderCompiler, shaders.back(), true))
    {
        log << "\nError: failed to load and compile the probe state classifier compute shader!\n";
        return false;
    }

    // RTXGI probe state classifier, activate all probes
    shaders.push_back(ShaderProgram());
    shaders.back().filepath = file.c_str();
    shaders.back().entryPoint = L"DDGIProbeStateActivateAllCS";
    shaders.back().targetProfile = L"cs_6_0";
    if (!Shaders::Compile(shaderCompiler, shaders.back(), true))
    {
        log << "\nError: failed to load and compile the probe state classifier activate all compute shader!\n";
        return false;
    }
#endif

    log << "done.\n";
    return true;
}

bool CompileVolumeShadersMulti(std::vector<std::vector<ShaderProgram>>& shaders, ShaderCompiler& shaderCompiler, const std::vector<rtxgi::DDGIVolumeDesc>& volumeDescs, std::ofstream& log)
{
    shaders.resize(volumeDescs.size());
    for (int i = 0; i < volumeDescs.size(); i++)
    {
        if (!CompileVolumeShaders(shaders[i], shaderCompiler, volumeDescs[i], log))
        {
            return false;
        }
    }
    return true;
}

#if !RTXGI_DDGI_SDK_MANAGED_RESOURCES

/**
* Create resources used by the RTXGI DDGI Volume.
*/
bool CreateVolumeResources(
    D3D12Global &d3d,
    D3D12Resources &resources,
    std::vector<ShaderProgram> &shaders,
    rtxgi::DDGIVolume* &volume,
    const rtxgi::DDGIVolumeDesc &volumeDesc,
    rtxgi::DDGIVolumeResources &volumeResources,
    std::ofstream &log,
    size_t index)
{
    log << "\nCreating RTXGI DDGI Volume resources...";

    // Create the volume's constant buffer and textures
    {
        // Create the RT radiance texture
        UINT width = 0;
        UINT height = 0;
        rtxgi::GetDDGIVolumeTextureDimensions(volumeDesc, rtxgi::EDDGITextureType::RTRadiance, width, height);

        DXGI_FORMAT format = rtxgi::GetDDGIVolumeTextureFormat(rtxgi::EDDGITextureType::RTRadiance);

        if (!D3D12::CreateTexture(d3d, &volumeResources.probeRTRadiance, width, height, format, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)) return false;
#if RTXGI_NAME_D3D_OBJECTS
        std::wstring name = L"RTXGI DDGIVolume Probe RT Radiance " + index;
        volumeResources.probeRTRadiance->SetName(name.c_str());
#endif

        // Create the probe irradiance texture
        rtxgi::GetDDGIVolumeTextureDimensions(volumeDesc, rtxgi::EDDGITextureType::Irradiance, width, height);
        format = rtxgi::GetDDGIVolumeTextureFormat(rtxgi::EDDGITextureType::Irradiance);

        if (!D3D12::CreateTexture(d3d, &volumeResources.probeIrradiance, width, height, format, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)) return false;
#if RTXGI_NAME_D3D_OBJECTS
        name = L"RTXGI DDGIVolume Probe Irradiance " + index;
        volumeResources.probeIrradiance->SetName(name.c_str());
#endif

        // Create the probe distance texture
        rtxgi::GetDDGIVolumeTextureDimensions(volumeDesc, rtxgi::EDDGITextureType::Distance, width, height);
        format = rtxgi::GetDDGIVolumeTextureFormat(rtxgi::EDDGITextureType::Distance);

        if (!D3D12::CreateTexture(d3d, &volumeResources.probeDistance, width, height, format, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)) return false;
#if RTXGI_NAME_D3D_OBJECTS
        name = L"RTXGI DDGIVolume Probe Distance " + index;
        volumeResources.probeDistance->SetName(name.c_str());
#endif

#if RTXGI_DDGI_PROBE_RELOCATION
        // Create the probe offsets texture
        rtxgi::GetDDGIVolumeTextureDimensions(volumeDesc, rtxgi::EDDGITextureType::Offsets, width, height);
        if (width <= 0) return false;

        format = rtxgi::GetDDGIVolumeTextureFormat(rtxgi::EDDGITextureType::Offsets);

        if (!D3D12::CreateTexture(d3d, &volumeResources.probeOffsets, width, height, format, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)) return false;
#if RTXGI_NAME_D3D_OBJECTS
        volumeResources.probeOffsets->SetName(L"RTXGI DDGIVolume Probe Offsets");
#endif
#endif

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        // Create the probe offsets texture
        rtxgi::GetDDGIVolumeTextureDimensions(volumeDesc, rtxgi::EDDGITextureType::States, width, height);
        if (width <= 0) return false;

        format = rtxgi::GetDDGIVolumeTextureFormat(rtxgi::EDDGITextureType::States);

        if(!D3D12::CreateTexture(d3d, &volumeResources.probeStates, width, height, format, D3D12_RESOURCE_STATE_UNORDERED_ACCESS)) return false;
#if RTXGI_NAME_D3D_OBJECTS
        volumeResources.probeStates->SetName(L"RTXGI DDGIVolume Probe States");
#endif
#endif
    }

    // Create the volume's resource descriptors
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = volumeResources.descriptorHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += (volumeResources.descriptorHeapDescSize * volumeResources.descriptorHeapOffset);
        //log << std::hex;
        //log << "descriptors for volume " << index << " start " << handle.ptr << std::endl;

        // Create the RT radiance UAV 
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = rtxgi::GetDDGIVolumeTextureFormat(rtxgi::EDDGITextureType::RTRadiance);
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        d3d.device->CreateUnorderedAccessView(volumeResources.probeRTRadiance, nullptr, &uavDesc, handle);
        //log << "rt radiance " << handle.ptr << std::endl;
        handle.ptr += volumeResources.descriptorHeapDescSize;

        // Create the irradiance UAV
        uavDesc = {};
        uavDesc.Format = rtxgi::GetDDGIVolumeTextureFormat(rtxgi::EDDGITextureType::Irradiance);
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        d3d.device->CreateUnorderedAccessView(volumeResources.probeIrradiance, nullptr, &uavDesc, handle);
        //log << "rt irradiance " << handle.ptr << std::endl;
        handle.ptr += volumeResources.descriptorHeapDescSize;

        // Create the distance UAV
        uavDesc = {};
        uavDesc.Format = rtxgi::GetDDGIVolumeTextureFormat(rtxgi::EDDGITextureType::Distance);
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        d3d.device->CreateUnorderedAccessView(volumeResources.probeDistance, nullptr, &uavDesc, handle);
        //log << "rt distance " << handle.ptr << std::endl;
        //log << std::dec;
#if RTXGI_DDGI_PROBE_RELOCATION
        handle.ptr += volumeResources.descriptorHeapDescSize;

        // Create the probe offsets UAV
        uavDesc = {};
        uavDesc.Format = rtxgi::GetDDGIVolumeTextureFormat(rtxgi::EDDGITextureType::Offsets);
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        d3d.device->CreateUnorderedAccessView(volumeResources.probeOffsets, nullptr, &uavDesc, handle);
#else
        // Even if the probe offsets resource isn't created, we need to increment
        // to place the probe classifier in the correct descriptor heap slot.
        handle.ptr += volumeResources.descriptorHeapDescSize;
#endif

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        handle.ptr += volumeResources.descriptorHeapDescSize;

        // Create the probe offsets UAV
        uavDesc = {};
        uavDesc.Format = rtxgi::GetDDGIVolumeTextureFormat(rtxgi::EDDGITextureType::States);
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        d3d.device->CreateUnorderedAccessView(volumeResources.probeStates, nullptr, &uavDesc, handle);
#endif
    }

    // Create the volume's root signature
    {
        ID3DBlob* signature;
        if (!rtxgi::GetDDGIVolumeRootSignatureDesc(volumeResources.descriptorHeapOffset, &signature)) return false;
        if (signature == nullptr) return false;

        HRESULT hr = d3d.device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&volumeResources.rootSignature));
        RTXGI_SAFE_RELEASE(signature);
        if (FAILED(hr)) return false;

#if RTXGI_NAME_D3D_OBJECTS
        volumeResources.rootSignature->SetName(L"RTXGI DDGIVolume Root Signature");
#endif
    }

    // Create the volume's PSOs
    {
        // Create the radiance blending PSO
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.CS.BytecodeLength = shaders[0].bytecode->GetBufferSize();
        psoDesc.CS.pShaderBytecode = shaders[0].bytecode->GetBufferPointer();
        psoDesc.pRootSignature = volumeResources.rootSignature;

        HRESULT hr = d3d.device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&volumeResources.probeRadianceBlendingPSO));
        if (FAILED(hr)) return false;

        // Create the distance blending PSO
        psoDesc.CS.BytecodeLength = shaders[1].bytecode->GetBufferSize();
        psoDesc.CS.pShaderBytecode = shaders[1].bytecode->GetBufferPointer();
        hr = d3d.device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&volumeResources.probeDistanceBlendingPSO));
        if (FAILED(hr)) return false;

        // Create the border row PSO
        psoDesc.CS.BytecodeLength = shaders[2].bytecode->GetBufferSize();
        psoDesc.CS.pShaderBytecode = shaders[2].bytecode->GetBufferPointer();
        hr = d3d.device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&volumeResources.probeBorderRowPSO));
        if (FAILED(hr)) return false;

        // Create the border column PSO
        psoDesc.CS.BytecodeLength = shaders[3].bytecode->GetBufferSize();
        psoDesc.CS.pShaderBytecode = shaders[3].bytecode->GetBufferPointer();
        hr = d3d.device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&volumeResources.probeBorderColumnPSO));
        if (FAILED(hr)) return false;

#if RTXGI_DDGI_PROBE_RELOCATION
        // Create the probe relocation PSO
        psoDesc.CS.BytecodeLength = shaders[4].bytecode->GetBufferSize();
        psoDesc.CS.pShaderBytecode = shaders[4].bytecode->GetBufferPointer();
        hr = d3d.device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&volumeResources.probeRelocationPSO));
        if (FAILED(hr)) return false;
#endif

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        // Create the probe classifier PSO
        psoDesc.CS.BytecodeLength = shaders[5].bytecode->GetBufferSize();
        psoDesc.CS.pShaderBytecode = shaders[5].bytecode->GetBufferPointer();
        hr = d3d.device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&volumeResources.probeStateClassifierPSO));
        if (FAILED(hr)) return false;

        // Create the probe classifier activate all PSO
        psoDesc.CS.BytecodeLength = shaders[6].bytecode->GetBufferSize();
        psoDesc.CS.pShaderBytecode = shaders[6].bytecode->GetBufferPointer();
        hr = d3d.device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&volumeResources.probeStateClassifierActivateAllPSO));
        if (FAILED(hr)) return false;
#endif
    }

    log << "done.\n";
    return true;
}

/**
* Destroy the resources created for the RTXGI DDGI Volume.
*/
void DestroyVolumeResources(rtxgi::DDGIVolumeResources &volumeResources)
{
    RTXGI_SAFE_RELEASE(volumeResources.rootSignature);
    RTXGI_SAFE_RELEASE(volumeResources.probeRTRadiance);
    RTXGI_SAFE_RELEASE(volumeResources.probeIrradiance);
    RTXGI_SAFE_RELEASE(volumeResources.probeDistance);
#if RTXGI_DDGI_PROBE_RELOCATION
    RTXGI_SAFE_RELEASE(volumeResources.probeOffsets);
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    RTXGI_SAFE_RELEASE(volumeResources.probeStates);
#endif
    RTXGI_SAFE_RELEASE(volumeResources.probeRadianceBlendingPSO);
    RTXGI_SAFE_RELEASE(volumeResources.probeDistanceBlendingPSO);
    RTXGI_SAFE_RELEASE(volumeResources.probeBorderRowPSO);
    RTXGI_SAFE_RELEASE(volumeResources.probeBorderColumnPSO);
#if RTXGI_DDGI_PROBE_RELOCATION
    RTXGI_SAFE_RELEASE(volumeResources.probeRelocationPSO);
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    RTXGI_SAFE_RELEASE(volumeResources.probeStateClassifierPSO);
    RTXGI_SAFE_RELEASE(volumeResources.probeStateClassifierActivateAllPSO);
#endif
}

#endif /* !RTXGI_DDGI_SDK_MANAGED_RESOURCES */

/**
* Creates an RTXGI DDGI Volume.
*/
bool CreateVolume(
    D3D12Global &d3d,
    D3D12Resources &resources,
    std::vector<ShaderProgram> &shaders,
    rtxgi::DDGIVolume* &volume,
    rtxgi::DDGIVolumeDesc &volumeDesc,
    rtxgi::DDGIVolumeResources &volumeResources,
    std::ofstream &log,
    size_t index)
{
    log << "Creating RTXGI DDGI Volume...";

    assert(std::strcmp(RTXGI_VERSION::getVersionString(), "1.1.23") == 0);

    rtxgi::ERTXGIStatus status = rtxgi::ERTXGIStatus::OK;

    volume = new rtxgi::DDGIVolume();

    // Specify the volume resources
    volumeResources.descriptorHeap = resources.cbvSrvUavHeap;
    volumeResources.descriptorHeapDescSize = d3d.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    volumeResources.descriptorHeapOffset = DescriptorHeapConstants::VOLUME_OFFSET + DescriptorHeapConstants::DESCRIPTORS_PER_VOLUME*static_cast<int>(index);

#if RTXGI_DDGI_SDK_MANAGED_RESOURCES
    volumeResources.device = d3d.device;
    volumeResources.probeRadianceBlendingCS = shaders[0].bytecode;
    volumeResources.probeDistanceBlendingCS = shaders[1].bytecode;
    volumeResources.probeBorderRowCS = shaders[2].bytecode;
    volumeResources.probeBorderColumnCS = shaders[3].bytecode;
#if RTXGI_DDGI_PROBE_RELOCATION
    volumeResources.probeRelocationCS = shaders[4].bytecode;
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
    volumeResources.probeStateClassifierCS = shaders[5].bytecode;
    volumeResources.probeStateClassifierActivateAllCS = shaders[6].bytecode;
#endif
#else
    if (!CreateVolumeResources(d3d, resources, shaders, volume, volumeDesc, volumeResources, log, index))
    {
        log << "\nError: failed to create volume resources!\n";
        return false;
    }
#endif

    // Create the DDGIVolume
    status = volume->Create(volumeDesc, volumeResources);
    if (status != rtxgi::ERTXGIStatus::OK)
    {
        log << "\nError: failed to create the DDGIVolume!\n";
        return false;
    }

    log << "Creating RTXGI DDGI Volume...done.\n";
    return true;
}


/**
* Create all the RTXGI DDGI Volumes.
*/
bool CreateVolumeMulti(
    D3D12Global &d3d,
    D3D12Resources &resources,
    std::vector<std::vector<ShaderProgram>> &shaders,
    std::vector<rtxgi::DDGIVolume*> &volumes,
    std::vector<rtxgi::DDGIVolumeDesc> &volumeDescs,
    std::vector<rtxgi::DDGIVolumeResources> &volumeResources,
    std::ofstream &log)
{
    const size_t numVolumes = min(volumeDescs.size(), NUM_MAX_VOLUMES);
    volumes.resize(numVolumes);
    volumeResources.resize(numVolumes);
    resources.numVolumes = static_cast<UINT>(numVolumes);

    // Create the volumes
    for (size_t volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
    {
        if (!CreateVolume(d3d, resources, shaders[volumeIndex], volumes[volumeIndex], volumeDescs[volumeIndex], volumeResources[volumeIndex], log, volumeIndex))
        {
            return false;
        }
    }

    // Create the combined constant buffer
    UINT size = rtxgi::GetDDGIVolumeConstantBufferSize() * 2 * static_cast<UINT>(numVolumes);   // sized to double buffer the data
    D3D12BufferInfo bufferInfo(size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (!D3D12::CreateBuffer(d3d, bufferInfo, &resources.volumeGroupCB)) return false;
#if RTXGI_NAME_D3D_OBJECTS
    resources.volumeGroupCB->SetName(L"RTXGI DDGIVolume Group Constant Buffer");
#endif

    return true;
}

/**
* Creates descriptors of the RTXGI DDGIVolume textures needed to compute indirect lighting.
*/
bool CreateDescriptors(D3D12Global &d3d, D3D12Resources &resources, rtxgi::DDGIVolume* &volume, std::ofstream &log, size_t index)
{
    log << "Creating descriptors...";

    D3D12_CPU_DESCRIPTOR_HANDLE handle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    //handle.ptr += resources.cbvSrvUavDescSize * (VOLUME_DESCRIPTOR_HEAP_START + rtxgi::GetDDGIVolumeNumDescriptors());
    // TODO: something about below calculation seems off
    handle.ptr += resources.cbvSrvUavDescSize * (DescriptorHeapConstants::VOLUME_OFFSET + (DescriptorHeapConstants::DESCRIPTORS_PER_VOLUME* index) + rtxgi::GetDDGIVolumeNumDescriptors());

    // Create the probe irradiance SRV 
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = rtxgi::GetDDGIVolumeTextureFormat(rtxgi::EDDGITextureType::Irradiance);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    d3d.device->CreateShaderResourceView(volume->GetProbeIrradianceTexture(), &srvDesc, handle);

    handle.ptr += resources.cbvSrvUavDescSize;

    // Create the probe distance SRV
    srvDesc = {};
    srvDesc.Format = rtxgi::GetDDGIVolumeTextureFormat(rtxgi::EDDGITextureType::Distance);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    d3d.device->CreateShaderResourceView(volume->GetProbeDistanceTexture(), &srvDesc, handle);

    handle.ptr += resources.cbvSrvUavDescSize;

    if (index == 0)
    {
        handle = resources.cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += resources.cbvSrvUavDescSize * (DescriptorHeapConstants::VOLUME_OFFSET + (DescriptorHeapConstants::DESCRIPTORS_PER_VOLUME * 4));

        // Create the blue noise RGB texture SRV
        srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        d3d.device->CreateShaderResourceView(resources.textures[resources.blueNoiseIndex], &srvDesc, handle);
    }
    log << "done.\n";
    return true;
}

/**
 * Create descriptors for all of the RTXGI DDGIVolumes.
 */
bool CreateDescriptorsMulti(D3D12Global &d3d, D3D12Resources &resources, std::vector<rtxgi::DDGIVolume*> &volumes, std::ofstream &log)
{
    for (size_t volumeIndex = 0; volumeIndex < volumes.size(); volumeIndex++)
    {
        if (!CreateDescriptors(d3d, resources, volumes[volumeIndex], log, volumeIndex))
        {
            return false;
        }
    }
    return true;
}

/**
* Creates the resources used to visualize the RTXGI DDGIVolume's probes.
*/
bool CreateProbeVisResources(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, rtxgi::DDGIVolume* volume, std::ofstream &log, size_t index)
{
    // Create a separate TLAS to visualize the volume's probes
    log << "Create Visualization TLAS...";
    if (!DXR::CreateVisTLAS(d3d, dxr, resources, volume->GetNumProbes(), index))
    {
        log << "\nError: failed to create Vis TLAS!\n";
        return false;
    }
    log << "done.\n";

    D3D12::SubmitCmdList(d3d);
    D3D12::WaitForGPU(d3d);
    D3D12::ResetCmdList(d3d);

    return true;
}

/**
* Create the visualization resources for all volumes.
*/
bool CreateProbeVisResourcesMulti(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, std::vector<rtxgi::DDGIVolume*> volumes, std::ofstream &log)
{
    dxr.visTLASes.resize(volumes.size());
    dxr.visTlasSizes.resize(volumes.size());
    for (size_t volumeIndex = 0; volumeIndex < volumes.size(); volumeIndex++)
    {
        if (!CreateProbeVisResources(d3d, dxr, resources, volumes[volumeIndex], log, volumeIndex))
        {
            return false;
        }
    }
    return true;
}

/**
 * Destroys and reallocates volume and visualization resources.
 */
bool HotReload(
    ConfigInfo &config,
    LightInfo &lights,
    Camera &camera,
    D3D12Global &d3d,
    DXRGlobal &dxr,
    D3D12Resources &resources,
    std::vector<std::vector<ShaderProgram>> &shaders,
    std::vector<rtxgi::DDGIVolume*> &volumes,
    std::vector<rtxgi::DDGIVolumeDesc> &volumeDescs,
    std::vector<rtxgi::DDGIVolumeResources> &volumeResources,
    InputInfo &inputInfo,
    InputOptions &inputOptions,
    RTOptions &rtOptions,
    PostProcessOptions &postOptions,
    VizOptions &vizOptions,
    std::ofstream &log)
{
    // Clear the volume descriptors
    volumeDescs.clear();

    // Reload the config file
    Camera cam = camera;
    if (!Config::Load(config, lights, camera, volumeDescs, inputInfo, inputOptions, rtOptions, postOptions, vizOptions, log))
    {
        log.close();
        return false;
    }
    
    // Keep the current camera origin and direction
    camera.position = cam.position;
    camera.forward = cam.forward;
    camera.up = cam.up;
    camera.right = cam.right;

    D3D12::WaitForGPU(d3d);

    // Release the volumes and their resources
    for (size_t volumeIndex = 0; volumeIndex < volumes.size(); volumeIndex++)
    {
        volumes[volumeIndex]->Destroy();
        delete volumes[volumeIndex];
        volumes[volumeIndex] = nullptr;
#if !RTXGI_DDGI_SDK_MANAGED_RESOURCES
        DestroyVolumeResources(volumeResources[volumeIndex]);
#endif
    }
    RTXGI_SAFE_RELEASE(resources.volumeGroupCB);

    // Create a RTXGI DDGIVolume
    if (!CreateVolumeMulti(d3d, resources, shaders, volumes, volumeDescs, volumeResources, log))
    {
        log.close();
        return false;
    }

    // Create descriptors for the DDGIVolume probe textures
    if (!CreateDescriptorsMulti(d3d, resources, volumes, log))
    {
        log.close();
        return false;
    }

    // Create resources used to visualize the volume's probes
    if (!CreateProbeVisResourcesMulti(d3d, dxr, resources, volumes, log))
    {
        log.close();
        return false;
    }

    return true;
}

/**
* Builds the command list to ray trace RTXGI DDGIVolume probes.
*/
void RayTraceProbes(
    D3D12Global &d3d,
    DXRGlobal &dxr,
    D3D12Resources &resources,
    ID3D12Resource* probeRTRadiance,
    RTOptions &rtOptions,
    int numRaysPerProbe,
    int numProbes,
    int cbIndex)
{
#if RTXGI_PERF_MARKERS
    PIXScopedEvent(d3d.cmdList, PIX_COLOR(227, 220, 18), "RTXGI: RT Probes");
#endif

    // Set the CBV/SRV/UAV and sampler descriptor heaps
    ID3D12DescriptorHeap* ppHeaps[] = { resources.cbvSrvUavHeap, resources.samplerHeap };
    d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Set the RT global root signature
    d3d.cmdList->SetComputeRootSignature(dxr.globalRootSig);

    // Set constant buffer and TLAS SRV
    UINT64 groupOffset = (d3d.frameIndex * resources.numVolumes) * GetDDGIVolumeConstantBufferSize();
    d3d.cmdList->SetComputeRootConstantBufferView(0, resources.volumeGroupCB->GetGPUVirtualAddress() + groupOffset);
    d3d.cmdList->SetComputeRootShaderResourceView(1, dxr.TLAS.pResult->GetGPUVirtualAddress());

    // Set descriptor heaps
    d3d.cmdList->SetComputeRootDescriptorTable(2, resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
    d3d.cmdList->SetComputeRootDescriptorTable(3, resources.samplerHeap->GetGPUDescriptorHandleForHeapStart());

    // Set ray tracing root constants
    UINT rtConstants[4] = { *(UINT*)&rtOptions.normalBias, *(UINT*)&rtOptions.viewBias, 0, *(UINT*)&rtOptions.skyIntensity };
    d3d.cmdList->SetComputeRoot32BitConstants(6, 4, &rtConstants, 0);

    // set volume select root constant
    d3d.cmdList->SetComputeRoot32BitConstant(7, cbIndex, 0);

    // Dispatch rays
    D3D12_DISPATCH_RAYS_DESC desc = {};
    desc.RayGenerationShaderRecord.StartAddress = dxr.shaderTable->GetGPUVirtualAddress();
    desc.RayGenerationShaderRecord.SizeInBytes = dxr.shaderTableRecordSize;

    desc.MissShaderTable.StartAddress = dxr.shaderTable->GetGPUVirtualAddress() + (dxr.shaderTableRecordSize * 5);
    desc.MissShaderTable.SizeInBytes = dxr.shaderTableRecordSize;
    desc.MissShaderTable.StrideInBytes = dxr.shaderTableRecordSize;

    desc.HitGroupTable.StartAddress = dxr.shaderTable->GetGPUVirtualAddress() + (dxr.shaderTableRecordSize * 7);
    desc.HitGroupTable.SizeInBytes = dxr.shaderTableRecordSize * resources.sceneVBs.size();
    desc.HitGroupTable.StrideInBytes = dxr.shaderTableRecordSize;

    desc.Width = numRaysPerProbe;
    desc.Height = numProbes;
    desc.Depth = 1;

    // Set the RTPSO and dispatch rays
    d3d.cmdList->SetPipelineState1(dxr.rtpso);
    d3d.cmdList->DispatchRays(&desc);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = probeRTRadiance;

    // Wait for the ray trace to complete
    d3d.cmdList->ResourceBarrier(1, &barrier);
}

/**
* Builds the command list to ray trace primary (camera) rays.
*/
void RayTracePrimary(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, RTOptions &rtOptions)
{
#if RTXGI_PERF_MARKERS
    PIXScopedEvent(d3d.cmdList, PIX_COLOR(227, 220, 18), "RT: Primary");
#endif

    // Set the CBV/SRV/UAV and sampler descriptor heaps
    ID3D12DescriptorHeap* ppHeaps[] = { resources.cbvSrvUavHeap, resources.samplerHeap };
    d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Set the RT global root signature
    d3d.cmdList->SetComputeRootSignature(dxr.globalRootSig);

    // Set TLAS SRV and descriptor heaps
    d3d.cmdList->SetComputeRootShaderResourceView(1, dxr.TLAS.pResult->GetGPUVirtualAddress());
    d3d.cmdList->SetComputeRootDescriptorTable(2, resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
    d3d.cmdList->SetComputeRootDescriptorTable(3, resources.samplerHeap->GetGPUDescriptorHandleForHeapStart());

    // Set ray tracing root constants
    UINT rtConstants[4] = { *(UINT*)&rtOptions.normalBias, *(UINT*)&rtOptions.viewBias, 0, *(UINT*)&rtOptions.skyIntensity };
    d3d.cmdList->SetComputeRoot32BitConstants(6, 4, &rtConstants, 0);

    // Dispatch rays
    D3D12_DISPATCH_RAYS_DESC desc = {};
    desc.RayGenerationShaderRecord.StartAddress = dxr.shaderTable->GetGPUVirtualAddress() + dxr.shaderTableRecordSize;
    desc.RayGenerationShaderRecord.SizeInBytes = dxr.shaderTableRecordSize;

    desc.MissShaderTable.StartAddress = dxr.shaderTable->GetGPUVirtualAddress() + (dxr.shaderTableRecordSize * 5);
    desc.MissShaderTable.SizeInBytes = dxr.shaderTableRecordSize;
    desc.MissShaderTable.StrideInBytes = dxr.shaderTableRecordSize;

    desc.HitGroupTable.StartAddress = dxr.shaderTable->GetGPUVirtualAddress() + (dxr.shaderTableRecordSize * 7);
    desc.HitGroupTable.SizeInBytes = (dxr.shaderTableRecordSize * resources.sceneVBs.size());
    desc.HitGroupTable.StrideInBytes = dxr.shaderTableRecordSize;

    desc.Width = d3d.width;
    desc.Height = d3d.height;
    desc.Depth = 1;

    // Set the RTPSO and dispatch rays
    d3d.cmdList->SetPipelineState1(dxr.rtpso);
    d3d.cmdList->DispatchRays(&desc);

    D3D12_RESOURCE_BARRIER barriers[4] = {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barriers[0].UAV.pResource = resources.GBufferA;
    barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barriers[1].UAV.pResource = resources.GBufferB;
    barriers[2].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barriers[2].UAV.pResource = resources.GBufferC;
    barriers[3].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barriers[3].UAV.pResource = resources.GBufferD;

    // Wait for the ray trace to complete
    d3d.cmdList->ResourceBarrier(4, barriers);
}

/**
* Builds the command list to ray trace ambient occlusion rays.
*/
void RayTraceAO(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, PostProcessOptions &postOptions)
{
#if RTXGI_PERF_MARKERS
    PIXScopedEvent(d3d.cmdList, PIX_COLOR(227, 220, 18), "RT: AO");
#endif

    // Set the CBV/SRV/UAV and sampler descriptor heaps
    ID3D12DescriptorHeap* ppHeaps[] = { resources.cbvSrvUavHeap, resources.samplerHeap };
    d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Set the RT global root signature
    d3d.cmdList->SetComputeRootSignature(dxr.globalRootSig);

    // Set TLAS SRV and descriptor heaps
    d3d.cmdList->SetComputeRootShaderResourceView(1, dxr.TLAS.pResult->GetGPUVirtualAddress());
    d3d.cmdList->SetComputeRootDescriptorTable(2, resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
    d3d.cmdList->SetComputeRootDescriptorTable(3, resources.samplerHeap->GetGPUDescriptorHandleForHeapStart());

    // Set the root constants
    float AOPower = pow(2.f, postOptions.AOPowerLog);
    UINT  viewAO = postOptions.viewAO ? 1 : 0;
    UINT  useRTAO = postOptions.useRTAO ? 1 : 0;
    float exposure = pow(2.f, postOptions.exposureFStops);

    UINT noiseConstants[7] =
    {
        (UINT)d3d.width,
        d3d.frameNumber,
        useRTAO,
        viewAO,
        *(UINT*)&postOptions.AORadius,
        *(UINT*)&AOPower,
        *(UINT*)&postOptions.AOBias
    };
    d3d.cmdList->SetComputeRoot32BitConstants(4, 7, &noiseConstants, 0);

    // Dispatch rays
    D3D12_DISPATCH_RAYS_DESC desc = {};
    desc.RayGenerationShaderRecord.StartAddress = dxr.shaderTable->GetGPUVirtualAddress() + (dxr.shaderTableRecordSize * 2);
    desc.RayGenerationShaderRecord.SizeInBytes = dxr.shaderTableRecordSize;

    desc.MissShaderTable.StartAddress = dxr.shaderTable->GetGPUVirtualAddress() + (dxr.shaderTableRecordSize * 5);
    desc.MissShaderTable.SizeInBytes = dxr.shaderTableRecordSize;
    desc.MissShaderTable.StrideInBytes = dxr.shaderTableRecordSize;

    desc.HitGroupTable.StartAddress = dxr.shaderTable->GetGPUVirtualAddress() + (dxr.shaderTableRecordSize * 7);
    desc.HitGroupTable.SizeInBytes = dxr.shaderTableRecordSize * resources.sceneVBs.size();
    desc.HitGroupTable.StrideInBytes = dxr.shaderTableRecordSize;

    desc.Width = d3d.width;
    desc.Height = d3d.height;
    desc.Depth = 1;

    d3d.cmdList->SetPipelineState1(dxr.rtpso);
    d3d.cmdList->DispatchRays(&desc);

    D3D12_RESOURCE_BARRIER barriers[1] = {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barriers[0].UAV.pResource = resources.RTAORaw;

    // Wait for the ray trace to complete
    d3d.cmdList->ResourceBarrier(1, barriers);
}

/**
* Builds the command list to filter the ambient occlusion data.
*/
void FilterAO(D3D12Global &d3d, D3D12Resources &resources, PostProcessOptions &options)
{
#if RTXGI_PERF_MARKERS
    PIXScopedEvent(d3d.cmdList, PIX_COLOR(252, 148, 3), "CS: Filter AO");
#endif

    // Set the CBV/SRV/UAV and sampler descriptor heaps
    ID3D12DescriptorHeap* ppHeaps[] = { resources.cbvSrvUavHeap };
    d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Set the root signature and root parameters
    d3d.cmdList->SetComputeRootSignature(resources.computeRootSig);
    d3d.cmdList->SetComputeRootDescriptorTable(0, resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());

    float distanceKernel[6];
    for (int i = 0; i < 6; ++i)
    {
        distanceKernel[i] = (float)exp(-float(i * i) / (2.f * options.AOFilterDistanceSigma * options.AOFilterDistanceSigma));
    }

    UINT computeConstants[12] = 
    {
        *(UINT*)&options.AOFilterDistanceSigma, 
        *(UINT*)&options.AOFilterDepthSigma, 
        (UINT)d3d.width, (UINT)d3d.height,
        *(UINT*)&distanceKernel[0], *(UINT*)&distanceKernel[1], 
        *(UINT*)&distanceKernel[2], *(UINT*)&distanceKernel[3], 
        *(UINT*)&distanceKernel[4], *(UINT*)&distanceKernel[5], 
        0, 0};

    d3d.cmdList->SetComputeRoot32BitConstants(1, 12, &computeConstants, 0);

    // Set the PSO and dispatch
    d3d.cmdList->SetPipelineState(resources.AOFilterPSO);
    UINT32 groupsX = DivRoundUp(d3d.width, AO_FILTER_BLOCK_SIZE);
    UINT32 groupsY = DivRoundUp(d3d.height, AO_FILTER_BLOCK_SIZE);
    d3d.cmdList->Dispatch(groupsX, groupsY, 1);

    D3D12_RESOURCE_BARRIER barriers[1] = {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barriers[0].UAV.pResource = resources.RTAOFiltered;

    // Wait for the compute filter pass to complete
    d3d.cmdList->ResourceBarrier(1, barriers);
}

/**
 * Builds the command list to compute indirect lighting from the RTXGI DDGIVolume.
 */
void RenderIndirect(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, PostProcessOptions &postOptions)
{
#if RTXGI_PERF_MARKERS
    PIXScopedEvent(d3d.cmdList, PIX_COLOR(227, 66, 18), "Post: Indirect Lighting");
#endif

    // Transition the back buffer to a render target
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Transition.pResource = d3d.backBuffer[d3d.frameIndex];
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    // Wait for the transition to complete
    d3d.cmdList->ResourceBarrier(1, &barrier);

    // Set the render target
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = resources.rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += (resources.rtvDescSize * d3d.frameIndex);
    d3d.cmdList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

    // Set raster root signature and pipeline state
    d3d.cmdList->SetGraphicsRootSignature(resources.rasterRootSig);
    d3d.cmdList->SetPipelineState(resources.indirectPSO);

    // Set the CBV/SRV/UAV and sampler descriptor heaps
    ID3D12DescriptorHeap* ppHeaps[] = { resources.cbvSrvUavHeap, resources.samplerHeap };
    d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Set the constant buffer
    {
        UINT64 groupOffset = (d3d.frameIndex * resources.numVolumes) * GetDDGIVolumeConstantBufferSize();
        d3d.cmdList->SetGraphicsRootConstantBufferView(0, resources.volumeGroupCB->GetGPUVirtualAddress() + groupOffset);
    }
    
    // Set the descriptor tables
    d3d.cmdList->SetGraphicsRootDescriptorTable(1, resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
    d3d.cmdList->SetGraphicsRootDescriptorTable(2, resources.samplerHeap->GetGPUDescriptorHandleForHeapStart());

    // Set the root constants
    float AOPower = pow(2.f, postOptions.AOPowerLog);
    UINT  viewAO = postOptions.viewAO ? 1 : 0;
    UINT  useRTAO = postOptions.useRTAO ? 1 : 0;
    float exposure = pow(2.f, postOptions.exposureFStops);

    UINT noiseConstants[11] =
    {
        (UINT)d3d.width,
        d3d.frameNumber,
        useRTAO,
        viewAO,
        *(UINT*)&postOptions.AORadius,
        *(UINT*)&AOPower,
        *(UINT*)&postOptions.AOBias,
        (UINT)postOptions.useTonemapping,
        (UINT)postOptions.useDithering,
        (UINT)postOptions.useExposure,
        *(UINT*)&exposure
    };
    d3d.cmdList->SetGraphicsRoot32BitConstants(3, 11, noiseConstants, 0);

    UINT rasterConstants[1] = { (UINT)postOptions.useDDGI };
    d3d.cmdList->SetGraphicsRoot32BitConstants(4, 1, rasterConstants, 0);

    // volume count root constant
    d3d.cmdList->SetGraphicsRoot32BitConstant(5, resources.numVolumes, 0);

    // Set necessary state
    d3d.cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    d3d.cmdList->RSSetViewports(1, &d3d.viewport);
    d3d.cmdList->RSSetScissorRects(1, &d3d.scissor);

    // Draw
    d3d.cmdList->DrawInstanced(3, 1, 0, 0);

    // Transition the back buffer to present
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

    // Wait for the transition to complete
    d3d.cmdList->ResourceBarrier(1, &barrier);
}

/**
 * Builds the command list for path tracing.
 */
void PathTrace(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, RTOptions &rtOptions, PostProcessOptions &postOptions)
{
#if RTXGI_PERF_MARKERS
    PIXScopedEvent(d3d.cmdList, PIX_COLOR(227, 220, 18), "Path Tracing");
#endif

    D3D12_RESOURCE_BARRIER OutputBarriers[2] = {};

    // Transition the PT output buffer to UAV (from a copy source)
    OutputBarriers[0].Transition.pResource = resources.PTOutput;
    OutputBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    OutputBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    OutputBarriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    
    // Wait for the transitions to complete
    d3d.cmdList->ResourceBarrier(1, &OutputBarriers[0]);

    // Set the CBV/SRV/UAV and sampler descriptor heaps
    ID3D12DescriptorHeap* ppHeaps[] = { resources.cbvSrvUavHeap, resources.samplerHeap };
    d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Set the RT global root signature
    d3d.cmdList->SetComputeRootSignature(dxr.globalRootSig);

    // Set TLAS SRV and descriptor heaps
    d3d.cmdList->SetComputeRootShaderResourceView(1, dxr.TLAS.pResult->GetGPUVirtualAddress());
    d3d.cmdList->SetComputeRootDescriptorTable(2, resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
    d3d.cmdList->SetComputeRootDescriptorTable(3, resources.samplerHeap->GetGPUDescriptorHandleForHeapStart());

    // Set root constants
    float exposure = pow(2.f, postOptions.exposureFStops);
    UINT noiseConstants[11] =
    {
        (UINT)d3d.width,
        d3d.frameNumber,
        0, 0, 0, 0, 0,
        (UINT)postOptions.useTonemapping,
        (UINT)postOptions.useDithering,
        (UINT)postOptions.useExposure,
        *(UINT*)&exposure
    };
    d3d.cmdList->SetComputeRoot32BitConstants(4, 11, noiseConstants, 0);

    UINT rtConstants[4] = { *(UINT*)&rtOptions.normalBias, *(UINT*)&rtOptions.viewBias, rtOptions.numBounces, *(UINT*)&rtOptions.skyIntensity };
    d3d.cmdList->SetComputeRoot32BitConstants(6, 4, &rtConstants, 0);

    // Dispatch rays
    D3D12_DISPATCH_RAYS_DESC desc = {};
    desc.RayGenerationShaderRecord.StartAddress = dxr.shaderTable->GetGPUVirtualAddress() + (dxr.shaderTableRecordSize * 4);
    desc.RayGenerationShaderRecord.SizeInBytes = dxr.shaderTableRecordSize;

    desc.MissShaderTable.StartAddress = dxr.shaderTable->GetGPUVirtualAddress() + (dxr.shaderTableRecordSize * 5);
    desc.MissShaderTable.SizeInBytes = dxr.shaderTableRecordSize;
    desc.MissShaderTable.StrideInBytes = dxr.shaderTableRecordSize;

    desc.HitGroupTable.StartAddress = dxr.shaderTable->GetGPUVirtualAddress() + (dxr.shaderTableRecordSize * 7);
    desc.HitGroupTable.SizeInBytes = (dxr.shaderTableRecordSize * resources.sceneVBs.size());
    desc.HitGroupTable.StrideInBytes = dxr.shaderTableRecordSize;

    desc.Width = d3d.width;
    desc.Height = d3d.height;
    desc.Depth = 1;

    // Set the RTPSO and dispatch rays
    d3d.cmdList->SetPipelineState1(dxr.rtpso);
    d3d.cmdList->DispatchRays(&desc);

    // Transition PTOutput to a copy source (from UAV)
    OutputBarriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    OutputBarriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    // Transition the back buffer to a copy destination
    OutputBarriers[1].Transition.pResource = d3d.backBuffer[d3d.frameIndex];
    OutputBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    OutputBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    OutputBarriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    // Wait for the transitions to complete
    d3d.cmdList->ResourceBarrier(2, OutputBarriers);

    // Copy the PT output to the back buffer
    d3d.cmdList->CopyResource(d3d.backBuffer[d3d.frameIndex], resources.PTOutput);

    // Transition back buffer to present
    OutputBarriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    OutputBarriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

    // Wait for the buffer transitions to complete
    d3d.cmdList->ResourceBarrier(1, &OutputBarriers[1]);
}

}
