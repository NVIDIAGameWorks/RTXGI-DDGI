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

#include "Common.h"
#include "Input.h"

#include <rtxgi/Types.h>
#include <rtxgi/ddgi/DDGIVolume.h>

namespace Harness
{
    bool Initialize(ConfigInfo &config, Scene &scene, D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, ShaderCompiler &shaderCompiler, HWND& window, std::ofstream &log);
    bool CompileVolumeShaders(std::vector<ShaderProgram> &shaders, ShaderCompiler &shaderCompiler, const rtxgi::DDGIVolumeDesc &volumeDesc, std::ofstream &log);
    bool CompileVolumeShadersMulti(std::vector<std::vector<ShaderProgram>> &shaders, ShaderCompiler &shaderCompiler, const std::vector<rtxgi::DDGIVolumeDesc> &volumeDescs, std::ofstream &log);

#if !RTXGI_DDGI_SDK_MANAGED_RESOURCES
    bool CreateVolumeResources(
        D3D12Global &d3d,
        D3D12Resources &resources,
        std::vector<ShaderProgram> &shaders,
        rtxgi::DDGIVolume* &volume,
        const rtxgi::DDGIVolumeDesc &volumeDesc,
        rtxgi::DDGIVolumeResources &volumeResources,
        std::ofstream &log,
        size_t index = 0);

    void DestroyVolumeResources(rtxgi::DDGIVolumeResources &volumeResources);
#endif

    bool CreateVolume(
        D3D12Global &d3d,
        D3D12Resources &resources,
        std::vector<ShaderProgram> &shaders,
        rtxgi::DDGIVolume* &volume,
        rtxgi::DDGIVolumeDesc &volumeDesc,
        rtxgi::DDGIVolumeResources &volumeResources,
        std::ofstream &log,
        size_t index = 0);

    bool CreateVolumeMulti(
        D3D12Global &d3d,
        D3D12Resources &resources,
        std::vector<std::vector<ShaderProgram>> &shaders,
        std::vector<rtxgi::DDGIVolume*> &volumes,
        std::vector<rtxgi::DDGIVolumeDesc> &volumeDescs,
        std::vector<rtxgi::DDGIVolumeResources> &volumeResources,
        std::ofstream &log);

    bool CreateDescriptors(D3D12Global &d3d, D3D12Resources &resources, rtxgi::DDGIVolume* &volume, std::ofstream &log, size_t index = 0);
    bool CreateDescriptorsMulti(D3D12Global &d3d, D3D12Resources &resources, std::vector<rtxgi::DDGIVolume*> &volumes, std::ofstream &log);
    bool CreateProbeVisResources(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, rtxgi::DDGIVolume* volume, std::ofstream &log, size_t index = 0);
    bool CreateProbeVisResourcesMulti(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, std::vector<rtxgi::DDGIVolume*> volumes, std::ofstream &log);

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
        std::ofstream &log);

    void RayTraceProbes(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, ID3D12Resource* probeRTRadiance, RTOptions &rtOptions, int numRaysPerProbe, int numProbes, int cbIndex=0);
    void RayTracePrimary(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, RTOptions &rtOptions);
    void RayTraceAO(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, PostProcessOptions &postOptions);
    void FilterAO(D3D12Global &d3d, D3D12Resources &resources, PostProcessOptions &postOptions);
    void RenderIndirect(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, PostProcessOptions &postOptions);

    void PathTrace(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, RTOptions &rtOptions, PostProcessOptions &postOptions);
}
