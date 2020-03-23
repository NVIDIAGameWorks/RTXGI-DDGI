/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include "Common.h"
#include "Input.h"

#include <rtxgi/Types.h>
#include <rtxgi/ddgi/DDGIVolume.h>

namespace Harness
{
    bool Initialize(ConfigInfo &config, D3D12Info &d3d, DXRInfo &dxr, D3D12Resources &resources, D3D12ShaderCompiler &shaderCompiler, HWND &window, ofstream &log);
    bool CompileShaders(vector<D3D12ShaderInfo> &shaders, D3D12ShaderCompiler &shaderCompiler, const rtxgi::DDGIVolumeDesc &volumeDesc, ofstream &log);

#if !RTXGI_DDGI_SDK_MANAGED_RESOURCES
    bool CreateVolumeResources(
        D3D12Info &d3d,
        D3D12Resources &resources,
        vector<D3D12ShaderInfo> &shaders,
        rtxgi::DDGIVolume* &volume,
        const rtxgi::DDGIVolumeDesc &volumeDesc,
        rtxgi::DDGIVolumeResources &volumeResources,
        ofstream &log);

    void DestroyVolumeResources(rtxgi::DDGIVolumeResources &volumeResources);
#endif

    bool CreateVolume(
        D3D12Info &d3d,
        D3D12Resources &resources,
        vector<D3D12ShaderInfo> &shaders,
        rtxgi::DDGIVolume* &volume,
        rtxgi::DDGIVolumeDesc &volumeDesc,
        rtxgi::DDGIVolumeResources &volumeResources,
        ofstream &log);

    bool CreateDescriptors(D3D12Info &d3d, D3D12Resources &resources, rtxgi::DDGIVolume* &volume, ofstream &log);
    bool CreateProbeVisResources(D3D12Info &d3d, DXRInfo &dxr, D3D12Resources &resources, rtxgi::DDGIVolume* volume, ofstream &log);
    
    bool HotReload(
        ConfigInfo &config,
        LightInfo &lights,
        CameraInfo &camera,
        D3D12Info &d3d,
        DXRInfo &dxr,
        D3D12Resources &resources,
        vector<D3D12ShaderInfo> &shaders,
        rtxgi::DDGIVolume* &volume,
        rtxgi::DDGIVolumeDesc &volumeDesc,
        rtxgi::DDGIVolumeResources &volumeResources,
        InputInfo &inputInfo,
        InputOptions &inputOptions,
        RTOptions &rtOptions,
        PostProcessOptions &postOptions,
        VizOptions &vizOptions,
        ofstream &log);

    void RayTraceProbes(D3D12Info &d3d, DXRInfo &dxr, D3D12Resources &resources, ID3D12Resource* probeRTRadiance, RTOptions &rtOptions, int numRaysPerProbe, int numProbes);
    void RayTracePrimary(D3D12Info &d3d, DXRInfo &dxr, D3D12Resources &resources, RTOptions &rtOptions);  
    void RayTraceAO(D3D12Info& d3d, DXRInfo& dxr, D3D12Resources& resources, PostProcessOptions& postOptions);
    void FilterAO(D3D12Info& d3d, D3D12Resources& resources, PostProcessOptions &postOptions);
    void RenderIndirect(D3D12Info &d3d, DXRInfo &dxr, D3D12Resources &resources, PostProcessOptions &postOptions);
    
    void PathTrace(D3D12Info &d3d, DXRInfo &dxr, D3D12Resources &resources, RTOptions &rtOptions, PostProcessOptions &postOptions);
}
