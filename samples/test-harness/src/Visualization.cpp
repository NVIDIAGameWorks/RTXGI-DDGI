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

#include <rtxgi/ddgi/DDGIVolume.h>

#if RTXGI_PERF_MARKERS
#define USE_PIX
#include <pix3.h>
#endif

//----------------------------------------------------------------------------------------------------------
// Public Functions
//----------------------------------------------------------------------------------------------------------

namespace Visualization
{

/**
* Builds the command list to render a debug visualization of the DDGIVolume buffers.
*/
void RenderBuffers(D3D12Global &d3d, D3D12Resources &resources, const VizOptions &options, size_t index)
{
#if RTXGI_PERF_MARKERS
    PIXScopedEvent(d3d.cmdList, PIX_COLOR(118, 185, 0), "VIZ: RTXGI Buffers");
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

    // Set root signature and pipeline state
    d3d.cmdList->SetGraphicsRootSignature(resources.rasterRootSig);
    d3d.cmdList->SetPipelineState(resources.visBuffersPSO);

    // Set the CBV/SRV/UAV and sampler descriptor heaps, descriptor tables, and root parameters
    ID3D12DescriptorHeap* ppHeaps[] = { resources.cbvSrvUavHeap, resources.samplerHeap };
    d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Set the constant buffer
    // always the first volume for now
    UINT64 groupOffset = (UINT64(d3d.frameIndex) * resources.numVolumes) * GetDDGIVolumeConstantBufferSize();
    d3d.cmdList->SetGraphicsRootConstantBufferView(0, resources.volumeGroupCB->GetGPUVirtualAddress() + groupOffset);

    // Set descriptor tables
    d3d.cmdList->SetGraphicsRootDescriptorTable(1, resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
    d3d.cmdList->SetGraphicsRootDescriptorTable(2, resources.samplerHeap->GetGPUDescriptorHandleForHeapStart());
    
    // Set constants
    UINT rasterConstants[7] = 
    {
        *(UINT*)&options.probeRadius,
        *(UINT*)&options.irradianceScale,
        *(UINT*)&options.distanceScale,
        *(UINT*)&options.radianceScale,
        *(UINT*)&options.offsetScale,
        *(UINT*)&options.stateScale,
        *(UINT*)&options.distanceDivisor
    };
    d3d.cmdList->SetGraphicsRoot32BitConstants(4, 7, &options, 1);
    d3d.cmdList->SetGraphicsRoot32BitConstant(5, static_cast<UINT>(index), 0);

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
 * Builds the command list to render a debug visualization of the DDGIVolume probes.
 */
void RenderProbes(D3D12Global &d3d, DXRGlobal &dxr, D3D12Resources &resources, size_t index)
{
#if RTXGI_PERF_MARKERS
    PIXScopedEvent(d3d.cmdList, PIX_COLOR(118, 185, 0), "VIZ: RTXGI Probes");
#endif

    // Set the CBV/SRV/UAV and sampler descriptor heaps
    ID3D12DescriptorHeap* ppHeaps[] = { resources.cbvSrvUavHeap, resources.samplerHeap };
    d3d.cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Set the RT global root signature
    d3d.cmdList->SetComputeRootSignature(dxr.globalRootSig);

    // Set constant buffer and TLAS SRV
    UINT64 groupOffset = (UINT64(d3d.frameIndex) * resources.numVolumes) * GetDDGIVolumeConstantBufferSize();
    d3d.cmdList->SetComputeRootConstantBufferView(0, resources.volumeGroupCB->GetGPUVirtualAddress() + groupOffset);

    d3d.cmdList->SetComputeRootShaderResourceView(1, dxr.visTLASes[index].pResult->GetGPUVirtualAddress());

    // Set descriptor heaps
    d3d.cmdList->SetComputeRootDescriptorTable(2, resources.cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart());
    d3d.cmdList->SetComputeRootDescriptorTable(3, resources.samplerHeap->GetGPUDescriptorHandleForHeapStart());

    // set root constants
    d3d.cmdList->SetComputeRoot32BitConstant(7, static_cast<UINT>(index), 0);

    // Dispatch rays
    D3D12_DISPATCH_RAYS_DESC desc = {};
    desc.RayGenerationShaderRecord.StartAddress = dxr.shaderTable->GetGPUVirtualAddress() + (dxr.shaderTableRecordSize * 3);
    desc.RayGenerationShaderRecord.SizeInBytes = dxr.shaderTableRecordSize;

    desc.MissShaderTable.StartAddress = dxr.shaderTable->GetGPUVirtualAddress() + (dxr.shaderTableRecordSize * 5);
    desc.MissShaderTable.SizeInBytes = dxr.shaderTableRecordSize;
    desc.MissShaderTable.StrideInBytes = dxr.shaderTableRecordSize;

    desc.HitGroupTable.StartAddress = dxr.shaderTable->GetGPUVirtualAddress() + (dxr.shaderTableRecordSize * 6);
    desc.HitGroupTable.SizeInBytes = dxr.shaderTableRecordSize;
    desc.HitGroupTable.StrideInBytes = dxr.shaderTableRecordSize;

    desc.Width = d3d.width;
    desc.Height = d3d.height;
    desc.Depth = 1;

    d3d.cmdList->SetPipelineState1(dxr.rtpso);
    d3d.cmdList->DispatchRays(&desc);

    // Wait for the ray trace to complete
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = resources.GBufferA;

    d3d.cmdList->ResourceBarrier(1, &uavBarrier);
}

}
