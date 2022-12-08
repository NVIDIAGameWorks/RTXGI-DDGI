/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "graphics/DDGIVisualizations.h"

#include "Geometry.h"

using namespace rtxgi;
using namespace rtxgi::vulkan;

using namespace Graphics::DDGI::Visualizations;

namespace Graphics
{
    namespace Vulkan
    {
        namespace DDGI
        {
            namespace Visualizations
            {

                //----------------------------------------------------------------------------------------------------------
                // Private Functions
                //----------------------------------------------------------------------------------------------------------

                bool UpdateDescriptorSets(Globals& vk, GlobalResources& vkResources, Resources& resources)
                {
                    // Store the data to be written to the descriptor set
                    VkWriteDescriptorSet* descriptor = nullptr;
                    std::vector<VkWriteDescriptorSet> descriptors;

                    // 0: Samplers
                    VkDescriptorImageInfo samplers[] =
                    {
                        { vkResources.samplers[SamplerIndices::BILINEAR_WRAP], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED },
                        { vkResources.samplers[SamplerIndices::POINT_CLAMP], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED }
                    };

                    descriptor = &descriptors.emplace_back();
                    descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptor->dstSet = resources.descriptorSet;
                    descriptor->dstBinding = DescriptorLayoutBindings::SAMPLERS;
                    descriptor->dstArrayElement = SamplerIndices::BILINEAR_WRAP;
                    descriptor->descriptorCount = _countof(samplers);
                    descriptor->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                    descriptor->pImageInfo = samplers;

                    // 1: Camera Constant Buffer
                    VkDescriptorBufferInfo camera = { vkResources.cameraCB, 0, VK_WHOLE_SIZE };

                    descriptor = &descriptors.emplace_back();
                    descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptor->dstSet = resources.descriptorSet;
                    descriptor->dstBinding = DescriptorLayoutBindings::CB_CAMERA;
                    descriptor->dstArrayElement = 0;
                    descriptor->descriptorCount = 1;
                    descriptor->descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    descriptor->pBufferInfo = &camera;

                    // 5: DDGIVolume Constants StructuredBuffer
                    VkDescriptorBufferInfo volumeConstants = { resources.volumeConstantsSTB, 0, VK_WHOLE_SIZE };

                    descriptor = &descriptors.emplace_back();
                    descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptor->dstSet = resources.descriptorSet;
                    descriptor->dstBinding = DescriptorLayoutBindings::STB_DDGI_VOLUME_CONSTS;
                    descriptor->dstArrayElement = 0;
                    descriptor->descriptorCount = 1;
                    descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    descriptor->pBufferInfo = &volumeConstants;

                    // 6: DDGIVolume Resource Indices StructuredBuffer
                    VkDescriptorBufferInfo volumeResourceIndices = { resources.volumeResourceIndicesSTB, 0, VK_WHOLE_SIZE };

                    descriptor = &descriptors.emplace_back();
                    descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptor->dstSet = resources.descriptorSet;
                    descriptor->dstBinding = DescriptorLayoutBindings::STB_DDGI_VOLUME_RESOURCE_INDICES;
                    descriptor->dstArrayElement = 0;
                    descriptor->descriptorCount = 1;
                    descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    descriptor->pBufferInfo = &volumeResourceIndices;

                    // 7: Probe Vis TLAS Instances RWStructuredBuffer
                    VkDescriptorBufferInfo instances = { resources.tlas.instances, 0, VK_WHOLE_SIZE };

                    descriptor = &descriptors.emplace_back();
                    descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptor->dstSet = resources.descriptorSet;
                    descriptor->dstBinding = DescriptorLayoutBindings::UAV_STB_TLAS_INSTANCES;
                    descriptor->dstArrayElement = 0;
                    descriptor->descriptorCount = 1;
                    descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    descriptor->pBufferInfo = &instances;

                    // 8: Texture2D UAVs
                    VkDescriptorImageInfo rwTex2D[] =
                    {
                        { VK_NULL_HANDLE, vkResources.rt.GBufferAView, VK_IMAGE_LAYOUT_GENERAL },
                        { VK_NULL_HANDLE, vkResources.rt.GBufferBView, VK_IMAGE_LAYOUT_GENERAL },
                    };

                    descriptor = &descriptors.emplace_back();
                    descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptor->dstSet = resources.descriptorSet;
                    descriptor->dstBinding = DescriptorLayoutBindings::UAV_TEX2D;
                    descriptor->dstArrayElement = RWTex2DIndices::GBUFFERA;
                    descriptor->descriptorCount = _countof(rwTex2D);
                    descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    descriptor->pImageInfo = rwTex2D;

                    // 10: Probe Vis TLAS
                    VkWriteDescriptorSetAccelerationStructureKHR probeTLAS = {};
                    probeTLAS.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                    probeTLAS.accelerationStructureCount = 1;
                    probeTLAS.pAccelerationStructures = &resources.tlas.asKHR;

                    descriptor = &descriptors.emplace_back();
                    descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptor->dstSet = resources.descriptorSet;
                    descriptor->dstBinding = DescriptorLayoutBindings::SRV_TLAS;
                    descriptor->dstArrayElement = TLASIndices::DDGI_PROBE_VIS;
                    descriptor->descriptorCount = 1;
                    descriptor->descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
                    descriptor->pNext = &probeTLAS;

                    // 12: Texture2DArray SRVs
                    std::vector<VkDescriptorImageInfo> tex2DArray;
                    uint32_t numVolumes = static_cast<uint32_t>(resources.volumes->size());
                    if(numVolumes > 0)
                    {
                        for (uint32_t volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
                        {
                            // Add the DDGIVolume texture arrays
                            DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes->at(volumeIndex));
                            tex2DArray.push_back({ VK_NULL_HANDLE, volume->GetProbeRayDataView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
                            tex2DArray.push_back({ VK_NULL_HANDLE, volume->GetProbeIrradianceView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
                            tex2DArray.push_back({ VK_NULL_HANDLE, volume->GetProbeDistanceView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
                            tex2DArray.push_back({ VK_NULL_HANDLE, volume->GetProbeDataView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
                            tex2DArray.push_back({ VK_NULL_HANDLE, volume->GetProbeVariabilityView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
                            tex2DArray.push_back({ VK_NULL_HANDLE, volume->GetProbeVariabilityAverageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
                        }

                        descriptor = &descriptors.emplace_back();
                        descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                        descriptor->dstSet = resources.descriptorSet;
                        descriptor->dstBinding = DescriptorLayoutBindings::SRV_TEX2DARRAY;
                        descriptor->dstArrayElement = 0;
                        descriptor->descriptorCount = static_cast<uint32_t>(tex2DArray.size());
                        descriptor->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                        descriptor->pImageInfo = tex2DArray.data();
                    }

                    // 13: ByteAddressBuffer SRVs (sphere index & vertex buffer)
                    VkDescriptorBufferInfo byteAddressBuffers[] =
                    {
                        { resources.probeIB, 0, VK_WHOLE_SIZE },
                        { resources.probeVB, 0, VK_WHOLE_SIZE }
                    };

                    // ByteAddress Buffer SRVs (index / vertex buffers)
                    descriptor = &descriptors.emplace_back();
                    descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptor->dstSet = resources.descriptorSet;
                    descriptor->dstBinding = DescriptorLayoutBindings::SRV_BYTEADDRESS;
                    descriptor->dstArrayElement = ByteAddressIndices::SPHERE_INDICES;
                    descriptor->descriptorCount = _countof(byteAddressBuffers);
                    descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    descriptor->pBufferInfo = byteAddressBuffers;

                    // Update the descriptor set
                    vkUpdateDescriptorSets(vk.device, static_cast<uint32_t>(descriptors.size()), descriptors.data(), 0, nullptr);

                    return true;
                }

                bool UpdateShaderTable(Globals& vk, GlobalResources& vkResources, Resources& resources)
                {
                    uint32_t shaderGroupIdSize = vk.deviceRTPipelineProps.shaderGroupHandleSize;

                    // Write shader table records
                    uint8_t* pData = nullptr;
                    VKCHECK(vkMapMemory(vk.device, resources.shaderTableUploadMemory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(&pData)));

                    // Write shader table records for each shader
                    VkDeviceAddress address = GetBufferDeviceAddress(vk.device, resources.shaderTable);

                    // Get the shader group IDs from the default pipeline
                    std::vector<uint8_t> shaderGroupIdBuffer(shaderGroupIdSize * resources.rtShadersModule.numGroups);
                    VKCHECK(vkGetRayTracingShaderGroupHandlesKHR(vk.device, resources.rtPipeline, 0, resources.rtShadersModule.numGroups, (shaderGroupIdSize * resources.rtShadersModule.numGroups), shaderGroupIdBuffer.data()));

                    // Get the shader group ID for the alternate RGS from the alternate pipeline
                    std::vector<uint8_t> shaderGroupIdBuffer2(shaderGroupIdSize * resources.rtShadersModule2.numGroups);
                    VKCHECK(vkGetRayTracingShaderGroupHandlesKHR(vk.device, resources.rtPipeline2, 0, resources.rtShadersModule2.numGroups, (shaderGroupIdSize * resources.rtShadersModule2.numGroups), shaderGroupIdBuffer2.data()));

                    // Separate the shader group IDs into arrays
                    std::vector<uint8_t*> shaderGroupIds(resources.rtShadersModule.numGroups);
                    std::vector<uint8_t*> shaderGroup2Ids(resources.rtShadersModule2.numGroups);
                    for (uint32_t i = 0; i < resources.rtShadersModule.numGroups; ++i)
                    {
                        shaderGroupIds[i] = shaderGroupIdBuffer.data() + i * shaderGroupIdSize;
                        shaderGroup2Ids[i] = shaderGroupIdBuffer2.data() + i * shaderGroupIdSize;
                    }

                    uint32_t groupIndex = 0;

                    // Entry 0: Ray Generation Shader (Default)
                    memcpy(pData, shaderGroupIds[groupIndex++], shaderGroupIdSize);
                    resources.shaderTableRGSStartAddress = address;

                    address += resources.shaderTableRecordSize;

                    // Entry 2: Miss Shader
                    pData += resources.shaderTableRecordSize;
                    memcpy(pData, shaderGroupIds[groupIndex++], shaderGroupIdSize);
                    resources.shaderTableMissTableStartAddress = address;
                    resources.shaderTableMissTableSize = resources.shaderTableRecordSize;

                    address += resources.shaderTableMissTableSize;

                    // Entry 3: Hit Group (CHS only)
                    for (uint32_t hitGroupIndex = 0; hitGroupIndex < static_cast<uint32_t>(resources.rtShaders.hitGroups.size()); hitGroupIndex++)
                    {
                        pData += resources.shaderTableRecordSize;
                        memcpy(pData, shaderGroupIds[groupIndex++], shaderGroupIdSize);
                    }
                    resources.shaderTableHitGroupTableStartAddress = address;
                    resources.shaderTableHitGroupTableSize = static_cast<uint32_t>(resources.rtShaders.hitGroups.size()) * resources.shaderTableRecordSize;

                    // Reset group index for alternate pipeline
                    groupIndex = 0;
                    address += resources.shaderTableRecordSize;

                    // Entry 4: Ray Generation Shader (Alternate)
                    pData += resources.shaderTableRecordSize;
                    memcpy(pData, shaderGroup2Ids[groupIndex++], shaderGroupIdSize);
                    resources.shaderTableRGS2StartAddress = address;

                    address += resources.shaderTableRecordSize;

                    // Entry 5: Miss Shader (Alternate)
                    pData += resources.shaderTableRecordSize;
                    memcpy(pData, shaderGroup2Ids[groupIndex++], shaderGroupIdSize);
                    resources.shaderTableMissTable2StartAddress = address;

                    address += resources.shaderTableMissTableSize;

                    // Entry 6: Hit Group (CHS only)
                    for (uint32_t hitGroupIndex = 0; hitGroupIndex < static_cast<uint32_t>(resources.rtShaders2.hitGroups.size()); hitGroupIndex++)
                    {
                        pData += resources.shaderTableRecordSize;
                        memcpy(pData, shaderGroup2Ids[groupIndex++], shaderGroupIdSize);
                    }
                    resources.shaderTableHitGroupTable2StartAddress = address;

                    // Unmap
                    vkUnmapMemory(vk.device, resources.shaderTableUploadMemory);

                    // Schedule a copy of the shader table from the upload buffer to the device buffer
                    VkBufferCopy bufferCopy = {};
                    bufferCopy.size = resources.shaderTableSize;
                    vkCmdCopyBuffer(vk.cmdBuffer[vk.frameIndex], resources.shaderTableUpload, resources.shaderTable, 1, &bufferCopy);

                    return true;
                }

                bool UpdateInstances(Globals& vk, Resources& resources)
                {
                    // Clear the instances
                    resources.probeInstances.clear();

                    // Gather the probe instances from volumes
                    uint16_t instanceOffset = 0;
                    for (uint32_t volumeIndex = 0; volumeIndex < static_cast<uint32_t>(resources.volumes->size()); volumeIndex++)
                    {
                        // Get the volume
                        DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes->at(volumeIndex));

                        // Skip this volume if its "Show Probes" flag is disabled
                        if (!volume->GetShowProbes()) continue;

                        // Get the address of the probe blas
                        VkAccelerationStructureDeviceAddressInfoKHR asDeviceAddressInfo = {};
                        asDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
                        asDeviceAddressInfo.accelerationStructure = resources.blas.asKHR;
                        VkDeviceAddress blasAddress = vkGetAccelerationStructureDeviceAddressKHR(vk.device, &asDeviceAddressInfo);

                        // Add an instance for each probe
                        for (uint32_t probeIndex = 0; probeIndex < static_cast<uint32_t>(volume->GetNumProbes()); probeIndex++)
                        {
                            // Describe the probe instance
                            VkAccelerationStructureInstanceKHR desc = {};
                            desc.instanceCustomIndex  = instanceOffset;                     // instance offset in first 16 bits
                            desc.instanceCustomIndex |= (uint8_t)volume->GetIndex() << 16;  // volume index in last 8 bits

                            // Set the instance mask based on the visualization type
                            if(volume->GetProbeVisType() == EDDGIVolumeProbeVisType::Default) desc.mask = 0x01;
                            else if(volume->GetProbeVisType() == EDDGIVolumeProbeVisType::Hide_Inactive) desc.mask = 0x02;

                            desc.accelerationStructureReference = blasAddress;
                            desc.instanceShaderBindingTableRecordOffset = 0;     // A single hit group for all geometry
                        #if (COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT) || (COORDINATE_SYSTEM == COORDINATE_SYSTEM_LEFT_Z_UP)
                            desc.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR;
                        #endif

                            // Initialize transform to identity, instance transforms are updated on the GPU
                            desc.transform.matrix[0][0] = desc.transform.matrix[1][1] = desc.transform.matrix[2][2] = 1.f;

                            resources.probeInstances.push_back(desc);
                        }

                        // Increment the instance offset
                        instanceOffset += volume->GetNumProbes();
                    }

                    // Early out if no volumes want to visualize probes
                    if (resources.probeInstances.size() == 0) return true;

                    // Copy the instance data to the upload buffer
                    uint8_t* pData = nullptr;
                    uint32_t size = static_cast<uint32_t>(resources.probeInstances.size()) * sizeof(VkAccelerationStructureInstanceKHR);
                    VKCHECK(vkMapMemory(vk.device, resources.tlas.instancesUploadMemory, 0, size, 0, reinterpret_cast<void**>(&pData)));
                    memcpy(pData, resources.probeInstances.data(), size);
                    vkUnmapMemory(vk.device, resources.tlas.instancesUploadMemory);

                    // Schedule a copy of the upload buffer to the device buffer
                    VkBufferCopy bufferCopy = {};
                    bufferCopy.size = size;
                    vkCmdCopyBuffer(vk.cmdBuffer[vk.frameIndex], resources.tlas.instancesUpload, resources.tlas.instances, 1, &bufferCopy);

                    return true;
                }

                bool UpdateTLAS(Globals& vk, GlobalResources& vkResources, Resources& resources, const Configs::Config& config)
                {
                #ifdef GFX_PERF_MARKERS
                    AddPerfMarker(vk, GFX_PERF_MARKER_GREEN, "RTXGI: Visualization, Update Probe TLAS");
                #endif

                    // Update the instances and copy them to the GPU
                    UpdateInstances(vk, resources);

                    // Early out if no volumes want to visualize probes
                    if (resources.probeInstances.size() == 0) return true;

                    // Bind the descriptor set
                    vkCmdBindDescriptorSets(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_COMPUTE, vkResources.pipelineLayout, 0, 1, &resources.descriptorSet, 0, nullptr);

                    // Bind the update pipeline
                    vkCmdBindPipeline(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_COMPUTE, resources.updateTlasPipeline);

                    uint32_t instanceOffset = 0;
                    for (uint32_t volumeIndex = 0; volumeIndex < static_cast<uint32_t>(resources.volumes->size()); volumeIndex++)
                    {
                        DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes->at(volumeIndex));

                        // Skip this volume if the "Show Probes" flag is disabled
                        if (!volume->GetShowProbes()) continue;

                        // Update the constants
                        vkResources.constants.ddgivis.instanceOffset = instanceOffset;
                        vkResources.constants.ddgivis.probeRadius = config.ddgi.volumes[volumeIndex].probeRadius;

                        // Update the vis push constants
                        uint32_t offset = GlobalConstants::GetAlignedSizeInBytes() - DDGIVisConsts::GetAlignedSizeInBytes();
                        vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, DDGIVisConsts::GetSizeInBytes(), vkResources.constants.ddgivis.GetData());

                        // Update the DDGIRootConstants
                        offset = GlobalConstants::GetAlignedSizeInBytes();
                        vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, DDGIRootConstants::GetSizeInBytes(), volume->GetPushConstants().GetData());

                        // Dispatch the compute shader
                        float groupSize = 32.f;
                        uint32_t numProbes = static_cast<uint32_t>(volume->GetNumProbes());
                        uint32_t numGroups = (uint32_t)ceil((float)numProbes / groupSize);
                        vkCmdDispatch(vk.cmdBuffer[vk.frameIndex], numGroups, 1, 1);

                        // Increment the instance offset
                        instanceOffset += volume->GetNumProbes();
                    }

                    // Wait for the compute passes to finish
                    VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
                    barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
                    vkCmdPipelineBarrier(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);

                    VkBuildAccelerationStructureFlagBitsKHR buildFlags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

                    // Describe the TLAS geometry instances
                    VkAccelerationStructureGeometryInstancesDataKHR asInstanceData = {};
                    asInstanceData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
                    asInstanceData.arrayOfPointers = VK_FALSE;
                    asInstanceData.data = VkDeviceOrHostAddressConstKHR{ GetBufferDeviceAddress(vk.device, resources.tlas.instances) };

                    // Describe the mesh primitive geometry
                    VkAccelerationStructureGeometryDataKHR asGeometryData = {};
                    asGeometryData.instances = asInstanceData;

                    VkAccelerationStructureGeometryKHR asGeometry = {};
                    asGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
                    asGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
                    asGeometry.geometry = asGeometryData;

                    // Describe the top level acceleration structure inputs
                    VkAccelerationStructureBuildGeometryInfoKHR asInputs = {};
                    asInputs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
                    asInputs.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
                    asInputs.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
                    asInputs.geometryCount = 1;
                    asInputs.pGeometries = &asGeometry;
                    asInputs.flags = buildFlags;
                    asInputs.scratchData = VkDeviceOrHostAddressKHR{ GetBufferDeviceAddress(vk.device, resources.tlas.scratch) };
                    asInputs.dstAccelerationStructure = resources.tlas.asKHR;

                    // Describe and build the BLAS
                    std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos(1);
                    VkAccelerationStructureBuildRangeInfoKHR buildInfo = { static_cast<UINT>(resources.probeInstances.size()), 0, 0, 0 };
                    buildRangeInfos[0] = &buildInfo;

                    vkCmdBuildAccelerationStructuresKHR(vk.cmdBuffer[vk.frameIndex], 1, &asInputs, buildRangeInfos.data());

                    // Wait for the TLAS build to complete
                    barrier = {};
                    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
                    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
                    vkCmdPipelineBarrier(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);

                #ifdef GFX_PERF_MARKERS
                    vkCmdEndDebugUtilsLabelEXT(vk.cmdBuffer[vk.frameIndex]);
                #endif

                    return true;
                }

                // --- Create -----------------------------------------------------------------------------------------

                bool LoadAndCompileShaders(Globals& vk, Resources& resources, std::ofstream& log)
                {
                    // Release existing shaders
                    resources.rtShaders.Release();
                    resources.rtShaders2.rgs.Release();
                    resources.textureVisCS.Release();
                    resources.updateTlasCS.Release();

                    std::wstring root = std::wstring(vk.shaderCompiler.root.begin(), vk.shaderCompiler.root.end());

                    // Load and compile the ray generation shaders
                    {
                        resources.rtShaders.rgs.filepath = root + L"shaders/ddgi/visualizations/ProbesRGS.hlsl";
                        resources.rtShaders.rgs.entryPoint = L"RayGen";
                        resources.rtShaders.rgs.exportName = L"DDGIVisProbesRGS";
                        resources.rtShaders.rgs.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                        Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                        Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_COORDINATE_SYSTEM", std::to_wstring(RTXGI_COORDINATE_SYSTEM));
                        CHECK(Shaders::Compile(vk.shaderCompiler, resources.rtShaders.rgs, true), "compile DDGI Visualizations ray generation shader!\n", log);

                        // Load and compile alternate RGS
                        resources.rtShaders2.rgs.filepath = root + L"shaders/ddgi/visualizations/ProbesRGS.hlsl";
                        resources.rtShaders2.rgs.entryPoint = L"RayGenHideInactive";
                        resources.rtShaders2.rgs.exportName = L"DDGIVisProbesRGS";
                        resources.rtShaders2.rgs.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                        Shaders::AddDefine(resources.rtShaders2.rgs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                        Shaders::AddDefine(resources.rtShaders2.rgs, L"RTXGI_COORDINATE_SYSTEM", std::to_wstring(RTXGI_COORDINATE_SYSTEM));
                        CHECK(Shaders::Compile(vk.shaderCompiler, resources.rtShaders2.rgs, true), "compile DDGI Visualizations ray generation shader!\n", log);
                    }

                    // Load and compile the miss shader
                    {
                        resources.rtShaders.miss.filepath = root + L"shaders/ddgi/visualizations/ProbesMiss.hlsl";
                        resources.rtShaders.miss.entryPoint = L"Miss";
                        resources.rtShaders.miss.exportName = L"DDGIVisProbesMiss";
                        resources.rtShaders.miss.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                        Shaders::AddDefine(resources.rtShaders.miss, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                        CHECK(Shaders::Compile(vk.shaderCompiler, resources.rtShaders.miss, true), "compile DDGI Visualizations miss shader!\n", log);

                        // Copy to the alternate RT pipeline
                        resources.rtShaders2.miss = resources.rtShaders.miss;
                    }

                    // Add the hit group
                    {
                        resources.rtShaders.hitGroups.emplace_back();

                        Shaders::ShaderRTHitGroup& group = resources.rtShaders.hitGroups[0];
                        group.exportName = L"DDGIVisProbesHitGroup";

                        // Closest hit shader (don't need any-hit for probes)
                        group.chs.filepath = root + L"shaders/ddgi/visualizations/ProbesCHS.hlsl";
                        group.chs.entryPoint = L"CHS";
                        group.chs.exportName = L"DDGIVisProbesCHS";
                        group.chs.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };

                        // Load and compile
                        Shaders::AddDefine(group.chs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                        CHECK(Shaders::Compile(vk.shaderCompiler, group.chs, true), "compile DDGI Visualizations closest hit shader!\n", log);

                        // Set the payload size
                        resources.rtShaders.payloadSizeInBytes = sizeof(ProbeVisualizationPayload);

                        // Copy to the alternate RT pipeline
                        resources.rtShaders2.hitGroups = resources.rtShaders.hitGroups;
                        resources.rtShaders2.payloadSizeInBytes = resources.rtShaders.payloadSizeInBytes;
                    }

                    // Load and compile the volume texture shader
                    {
                        resources.textureVisCS.filepath = root + L"shaders/ddgi/visualizations/VolumeTexturesCS.hlsl";
                        resources.textureVisCS.entryPoint = L"CS";
                        resources.textureVisCS.targetProfile = L"cs_6_6";
                        resources.textureVisCS.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                        Shaders::AddDefine(resources.textureVisCS, L"RTXGI_PUSH_CONSTS_TYPE", L"2");                                                 // use the application's push constants layout
                        Shaders::AddDefine(resources.textureVisCS, L"RTXGI_PUSH_CONSTS_STRUCT_NAME", L"GlobalConstants");                            // specify the struct name of the application's push constants
                        Shaders::AddDefine(resources.textureVisCS, L"RTXGI_PUSH_CONSTS_VARIABLE_NAME", L"GlobalConst");                              // specify the variable name of the application's push constants
                        Shaders::AddDefine(resources.textureVisCS, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_VOLUME_INDEX_NAME", L"ddgi_volumeIndex");          // specify the name of the DDGIVolume index field in the application's push constants struct
                        Shaders::AddDefine(resources.textureVisCS, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_X_NAME", L"ddgi_reductionInputSizeX");  // specify the name of the DDGIVolume reduction pass input size fields the application's push constants struct
                        Shaders::AddDefine(resources.textureVisCS, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_Y_NAME", L"ddgi_reductionInputSizeY");
                        Shaders::AddDefine(resources.textureVisCS, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_Z_NAME", L"ddgi_reductionInputSizeZ");
                        Shaders::AddDefine(resources.textureVisCS, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                        Shaders::AddDefine(resources.textureVisCS, L"RTXGI_COORDINATE_SYSTEM", std::to_wstring(RTXGI_COORDINATE_SYSTEM));
                        Shaders::AddDefine(resources.textureVisCS, L"THGP_DIM_X", L"8");
                        Shaders::AddDefine(resources.textureVisCS, L"THGP_DIM_Y", L"4");
                        CHECK(Shaders::Compile(vk.shaderCompiler, resources.textureVisCS, true), "compile DDGI Visualizations volume textures compute shader!\n", log);
                    }

                    // Load and compile the TLAS update compute shader
                    {
                        resources.updateTlasCS.filepath = root + L"shaders/ddgi/visualizations/ProbesUpdateCS.hlsl";
                        resources.updateTlasCS.entryPoint = L"CS";
                        resources.updateTlasCS.targetProfile = L"cs_6_6";
                        resources.updateTlasCS.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                        Shaders::AddDefine(resources.updateTlasCS, L"RTXGI_PUSH_CONSTS_TYPE", L"2");                                                 // use the application's push constants layout
                        Shaders::AddDefine(resources.updateTlasCS, L"RTXGI_PUSH_CONSTS_STRUCT_NAME", L"GlobalConstants");                            // specify the struct name of the application's push constants
                        Shaders::AddDefine(resources.updateTlasCS, L"RTXGI_PUSH_CONSTS_VARIABLE_NAME", L"GlobalConst");                              // specify the variable name of the application's push constants
                        Shaders::AddDefine(resources.updateTlasCS, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_VOLUME_INDEX_NAME", L"ddgi_volumeIndex");          // specify the name of the DDGIVolume index field in the application's push constants struct
                        Shaders::AddDefine(resources.updateTlasCS, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_X_NAME", L"ddgi_reductionInputSizeX");  // specify the name of the DDGIVolume reduction pass input size fields the application's push constants struct
                        Shaders::AddDefine(resources.updateTlasCS, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_Y_NAME", L"ddgi_reductionInputSizeY");
                        Shaders::AddDefine(resources.updateTlasCS, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_Z_NAME", L"ddgi_reductionInputSizeZ");
                        Shaders::AddDefine(resources.updateTlasCS, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                        Shaders::AddDefine(resources.updateTlasCS, L"RTXGI_COORDINATE_SYSTEM", std::to_wstring(RTXGI_COORDINATE_SYSTEM));
                        CHECK(Shaders::Compile(vk.shaderCompiler, resources.updateTlasCS, true), "compile DDGI Visualizations probes update compute shader!\n", log);
                    }

                    return true;
                }

                bool CreateDescriptorSets(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
                {
                    // Describe the descriptor set allocation
                    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
                    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                    descriptorSetAllocateInfo.descriptorPool = vkResources.descriptorPool;
                    descriptorSetAllocateInfo.descriptorSetCount = 1;
                    descriptorSetAllocateInfo.pSetLayouts = &vkResources.descriptorSetLayout;

                    // Allocate the descriptor set
                    VKCHECK(vkAllocateDescriptorSets(vk.device, &descriptorSetAllocateInfo, &resources.descriptorSet));
                #ifdef GFX_NAME_OBJECTS
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.descriptorSet), "DDGI Visualizations Descriptor Set", VK_OBJECT_TYPE_DESCRIPTOR_SET);
                #endif

                    return true;
                }

                bool CreatePipelines(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
                {
                    // Release existing shader modules
                    resources.rtShadersModule.Release(vk.device);
                    resources.rtShadersModule2.Release(vk.device);
                    vkDestroyShaderModule(vk.device, resources.textureVisModule, nullptr);
                    vkDestroyShaderModule(vk.device, resources.updateTlasModule, nullptr);

                    // Release existing pipelines
                    vkDestroyPipeline(vk.device, resources.rtPipeline, nullptr);
                    vkDestroyPipeline(vk.device, resources.rtPipeline2, nullptr);
                    vkDestroyPipeline(vk.device, resources.textureVisPipeline, nullptr);
                    vkDestroyPipeline(vk.device, resources.updateTlasPipeline, nullptr);

                    // Create the shader modules
                    {
                        // Create the probe visualization RT shader module (default)
                        CHECK(CreateRayTracingShaderModules(
                            vk.device,
                            resources.rtShaders,
                            resources.rtShadersModule),
                            "create DDGI Visualization RT shader modules!\n", log);
                    #ifdef GFX_NAME_OBJECTS
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rtShadersModule.rgs), "DDGI Probe RT Visualization RGS Shader Module (Default)", VK_OBJECT_TYPE_SHADER_MODULE);
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rtShadersModule.miss), "DDGI Probe RT Visualization MS Shader Module (Default)", VK_OBJECT_TYPE_SHADER_MODULE);
                    #endif

                        // Create the probe visualization RT shader module (alternate)
                        CHECK(CreateRayTracingShaderModules(
                            vk.device,
                            resources.rtShaders2,
                            resources.rtShadersModule2),
                            "create DDGI Visualization RT shader modules!\n", log);
                    #ifdef GFX_NAME_OBJECTS
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rtShadersModule2.rgs), "DDGI Probe RT Visualization RGS Shader Module (Alternate)", VK_OBJECT_TYPE_SHADER_MODULE);
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rtShadersModule2.miss), "DDGI Probe RT Visualization MS Shader Module (Alternate)", VK_OBJECT_TYPE_SHADER_MODULE);
                    #endif

                        // Create the texture vis shader module
                        CHECK(CreateShaderModule(
                            vk.device,
                            resources.textureVisCS,
                            &resources.textureVisModule),
                            "create DDGI Volume Texture Visualization shader module!\n", log);
                    #ifdef GFX_NAME_OBJECTS
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.textureVisModule), "DDGI Volume Texture Visualization Shader Module", VK_OBJECT_TYPE_SHADER_MODULE);
                    #endif

                        // Create the probe update shader module
                        CHECK(CreateShaderModule(
                            vk.device,
                            resources.updateTlasCS,
                            &resources.updateTlasModule),
                            "create DDGI Visualization Probe Update shader module!\n", log);
                    #ifdef GFX_NAME_OBJECTS
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.updateTlasModule), "DDGI Visualization Probe Update Shader Module", VK_OBJECT_TYPE_SHADER_MODULE);
                    #endif
                    }

                    // Create the pipelines
                    {
                        // Create the probe visualization RT pipeline (default)
                        CHECK(CreateRayTracingPipeline(
                            vk.device,
                            vkResources.pipelineLayout,
                            resources.rtShaders,
                            resources.rtShadersModule,
                            &resources.rtPipeline),
                            "create DDGI Probe Visualization RT pipeline!\n", log);
                    #ifdef GFX_NAME_OBJECTS
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rtPipeline), "DDGI Probe Visualization RT Pipeline (Default)", VK_OBJECT_TYPE_PIPELINE);
                    #endif

                        // Create the probe visualization RT pipeline (alternate)
                        CHECK(CreateRayTracingPipeline(
                            vk.device,
                            vkResources.pipelineLayout,
                            resources.rtShaders2,
                            resources.rtShadersModule2,
                            &resources.rtPipeline2),
                            "create DDGI Probe Visualization RT pipeline!\n", log);
                    #ifdef GFX_NAME_OBJECTS
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rtPipeline2), "DDGI Probe Visualization RT Pipeline (Alternate)", VK_OBJECT_TYPE_PIPELINE);
                    #endif

                        // Create the volume texture visualization pipeline
                        CHECK(CreateComputePipeline(
                            vk.device,
                            vkResources.pipelineLayout,
                            resources.textureVisCS,
                            resources.textureVisModule,
                            &resources.textureVisPipeline),
                            "create DDGI Volume Texture Visualization Pipeline!\n", log);

                    #ifdef GFX_NAME_OBJECTS
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.textureVisPipeline), "DDGI Volume Texture Visualization Pipeline", VK_OBJECT_TYPE_PIPELINE);
                    #endif

                        // Create the probe update pipeline
                        CHECK(CreateComputePipeline(
                            vk.device,
                            vkResources.pipelineLayout,
                            resources.updateTlasCS,
                            resources.updateTlasModule,
                            &resources.updateTlasPipeline),
                            "create DDGI Visualization Probe Update Pipeline!\n", log);

                    #ifdef GFX_NAME_OBJECTS
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.updateTlasPipeline), "DDGI Visualization Probe Update Pipeline", VK_OBJECT_TYPE_PIPELINE);
                    #endif
                    }

                    return true;
                }

                bool CreateShaderTable(Globals& vk, Resources& resources, std::ofstream& log)
                {
                    // The Shader Table layout is as follows:
                    //    Entry 0:  Probe Vis Ray Generation Shader (default)
                    //    Entry 2:  Probe Vis Miss Shader
                    //    Entry 3:  Probe Vis HitGroup (CHS only)
                    //    Entry 4:  Probe Vis Ray Generation Shader (alternate)
                    //    Entry 5:  Probe Vis Miss Shader (alternate)
                    //    Entry 6:  Probe Vis HitGroup (CHS only) (alternate)

                    // All shader records in the Shader Table must have the same size, so shader record size will be based on the largest required entry.
                    // The entries must be aligned to VkPhysicalDeviceRayTracingPipelinePropertiesKHR::shaderGroupBaseAlignment.

                    // Release the existing shader table
                    resources.shaderTableSize = 0;
                    resources.shaderTableRecordSize = 0;
                    vkDestroyBuffer(vk.device, resources.shaderTableUpload, nullptr);
                    vkFreeMemory(vk.device, resources.shaderTableUploadMemory, nullptr);
                    vkDestroyBuffer(vk.device, resources.shaderTable, nullptr);
                    vkFreeMemory(vk.device, resources.shaderTableMemory, nullptr);

                    uint32_t shaderGroupIdSize = vk.deviceRTPipelineProps.shaderGroupHandleSize;

                    // Configure the shader record size (no shader record data)
                    resources.shaderTableRecordSize = shaderGroupIdSize;
                    resources.shaderTableRecordSize = ALIGN(vk.deviceRTPipelineProps.shaderGroupBaseAlignment, resources.shaderTableRecordSize);

                    // Find the shader table size
                    resources.shaderTableSize = (2 + static_cast<uint32_t>(resources.rtShaders.hitGroups.size())) * resources.shaderTableRecordSize;
                    resources.shaderTableSize *= 2; // for alternate pipeline
                    resources.shaderTableSize = ALIGN(vk.deviceRTPipelineProps.shaderGroupBaseAlignment, resources.shaderTableSize);

                    // Create the shader table upload buffer resource
                    BufferDesc desc = { resources.shaderTableSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
                    CHECK(CreateBuffer(vk, desc, &resources.shaderTableUpload, &resources.shaderTableUploadMemory), "create DDGI Visualizations shader table upload buffer!", log);
                #ifdef GFX_NAME_OBJECTS
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTableUpload), "DDGI Probe Vis Shader Table Upload", VK_OBJECT_TYPE_BUFFER);
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTableUploadMemory), "DDGI Probe Vis Shader Table Upload Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
                #endif

                    // Create the shader table buffer resource
                    desc = { resources.shaderTableSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
                    CHECK(CreateBuffer(vk, desc, &resources.shaderTable, &resources.shaderTableMemory), "create DDGI Visualizations shader table!", log);
                #ifdef GFX_NAME_OBJECTS
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTable), "DDGI Visualizations Shader Table", VK_OBJECT_TYPE_BUFFER);
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTableMemory), "DDGI Visualizations Shader Table Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
                #endif

                    return true;
                }

                bool CreateGeometry(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
                {
                    // Generate the sphere geometry
                    Geometry::CreateSphere(30, 30, resources.probe);

                    // Create the probe sphere's index buffer
                    CHECK(CreateIndexBuffer(
                        vk,
                        resources.probe,
                        &resources.probeIB,
                        &resources.probeIBMemory,
                        &resources.probeIBUpload,
                        &resources.probeIBUploadMemory),
                        "create probe index buffer!", log);
                #ifdef GFX_NAME_OBJECTS
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.probeIB), "IB: Probe Sphere, Primitive 0", VK_OBJECT_TYPE_BUFFER);
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.probeIBMemory), "IB: Probe Sphere, Primitive 0 Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
                #endif

                    // Create the probe sphere's vertex buffer
                    CHECK(CreateVertexBuffer(
                        vk,
                        resources.probe,
                        &resources.probeVB,
                        &resources.probeVBMemory,
                        &resources.probeVBUpload,
                        &resources.probeVBUploadMemory),
                        "create probe vertex buffer!", log);
                #ifdef GFX_NAME_OBJECTS
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.probeVB), "VB: Probe Sphere, Primitive 0", VK_OBJECT_TYPE_BUFFER);
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.probeVBMemory), "VB: Probe Sphere, Primitive 0 Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
                #endif

                    return true;
                }

                bool CreateBLAS(Globals& vk, Resources& resources)
                {
                    // Describe the BLAS geometries
                    VkAccelerationStructureGeometryKHR geometryDesc = {};
                    geometryDesc.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
                    geometryDesc.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
                    geometryDesc.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
                    geometryDesc.geometry.triangles.vertexData = VkDeviceOrHostAddressConstKHR{ GetBufferDeviceAddress(vk.device, resources.probeVB) };
                    geometryDesc.geometry.triangles.vertexStride = sizeof(Vertex);
                    geometryDesc.geometry.triangles.maxVertex = resources.probe.numVertices;
                    geometryDesc.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
                    geometryDesc.geometry.triangles.indexData = VkDeviceOrHostAddressConstKHR{ GetBufferDeviceAddress(vk.device, resources.probeIB) };
                    geometryDesc.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
                    geometryDesc.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

                    VkBuildAccelerationStructureFlagBitsKHR buildFlags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;

                    // Describe the bottom level acceleration structure inputs
                    VkAccelerationStructureBuildGeometryInfoKHR asInputs = {};
                    asInputs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
                    asInputs.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                    asInputs.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
                    asInputs.geometryCount = 1;
                    asInputs.pGeometries = &geometryDesc;
                    asInputs.flags = buildFlags;

                    // Get the size requirements for the BLAS buffer
                    uint32_t primitiveCount = resources.probe.numIndices / 3;
                    VkAccelerationStructureBuildSizesInfoKHR asPreBuildInfo = {};
                    asPreBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
                    vkGetAccelerationStructureBuildSizesKHR(vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &asInputs, &primitiveCount, &asPreBuildInfo);

                    // Create the BLAS scratch buffer, allocate and bind device memory
                    BufferDesc blasScratchDesc = { asPreBuildInfo.buildScratchSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
                    if (!CreateBuffer(vk, blasScratchDesc, &resources.blas.scratch, &resources.blas.scratchMemory)) return false;
                    asInputs.scratchData = VkDeviceOrHostAddressKHR{ GetBufferDeviceAddress(vk.device, resources.blas.scratch) };
                #ifdef GFX_NAME_OBJECTS
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.blas.scratch), "BLAS Scratch: Probe Sphere, Primitive 0", VK_OBJECT_TYPE_BUFFER);
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.blas.scratchMemory), "BLAS Scratch Memory: Probe Sphere, Primitive 0", VK_OBJECT_TYPE_DEVICE_MEMORY);
                #endif

                    // Create the BLAS buffer, allocate and bind device memory
                    BufferDesc blasDesc = { asPreBuildInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
                    if (!CreateBuffer(vk, blasDesc, &resources.blas.asBuffer, &resources.blas.asMemory)) return false;
                #ifdef GFX_NAME_OBJECTS
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.blas.asBuffer), "BLAS: Probe Sphere, Primitive 0", VK_OBJECT_TYPE_BUFFER);
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.blas.asMemory), "BLAS Memory: Probe Sphere, Primitive 0", VK_OBJECT_TYPE_DEVICE_MEMORY);
                #endif

                    // Describe the BLAS acceleration structure
                    VkAccelerationStructureCreateInfoKHR asCreateInfo = {};
                    asCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
                    asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                    asCreateInfo.size = asPreBuildInfo.accelerationStructureSize;
                    asCreateInfo.buffer = resources.blas.asBuffer;

                    // Create the BLAS acceleration structure
                    VKCHECK(vkCreateAccelerationStructureKHR(vk.device, &asCreateInfo, nullptr, &resources.blas.asKHR));
                #ifdef GFX_NAME_OBJECTS
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.blas.asKHR), "BLAS: Probe Sphere, Primitive 0", VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR);
                #endif

                    // Set the location of the final acceleration structure
                    asInputs.dstAccelerationStructure = resources.blas.asKHR;

                    // Describe and build the BLAS
                    std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> buildRangeInfos(1);
                    VkAccelerationStructureBuildRangeInfoKHR buildInfo = { primitiveCount, 0, 0, 0 };
                    buildRangeInfos[0] = &buildInfo;

                    vkCmdBuildAccelerationStructuresKHR(vk.cmdBuffer[vk.frameIndex], 1, &asInputs, buildRangeInfos.data());

                    // Wait for the BLAS build to complete
                    VkMemoryBarrier barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER };
                    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
                    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
                    vkCmdPipelineBarrier(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);

                    return true;
                }

                bool CreateInstances(Globals& vk, Resources& resources)
                {
                    // Release the existing TLAS
                    resources.tlas.Release(vk.device);

                    // Get the maximum number of probe instances from all volumes
                    for (uint32_t volumeIndex = 0; volumeIndex < static_cast<uint32_t>(resources.volumes->size()); volumeIndex++)
                    {
                        const DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes->at(volumeIndex));
                        resources.maxProbeInstances += volume->GetNumProbes();
                    }

                    // Early out if no volumes or probes exist
                    if (resources.maxProbeInstances == 0) return true;

                    // Create the TLAS instance upload buffer resource
                    uint32_t size = resources.maxProbeInstances * sizeof(VkAccelerationStructureInstanceKHR);
                    BufferDesc desc = { size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
                    if (!CreateBuffer(vk, desc, &resources.tlas.instancesUpload, &resources.tlas.instancesUploadMemory)) return false;
                #ifdef GFX_NAME_OBJECTS
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.tlas.instancesUpload), "TLAS Instance Descriptors Upload", VK_OBJECT_TYPE_BUFFER);
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.tlas.instancesUploadMemory), "TLAS Instance Descriptors Upload", VK_OBJECT_TYPE_DEVICE_MEMORY);
                #endif

                    // Create the TLAS instance device buffer resource
                    desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                    desc.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                    if (!CreateBuffer(vk, desc, &resources.tlas.instances, &resources.tlas.instancesMemory)) return false;
                #ifdef GFX_NAME_OBJECTS
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.tlas.instances), "TLAS Instance Descriptors", VK_OBJECT_TYPE_BUFFER);
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.tlas.instancesMemory), "TLAS Instance Descriptors", VK_OBJECT_TYPE_DEVICE_MEMORY);
                #endif

                    return true;
                }

                bool CreateTLAS(Globals& vk, Resources& resources)
                {
                    if (!CreateInstances(vk, resources)) return false;

                    VkBuildAccelerationStructureFlagBitsKHR buildFlags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;

                    uint32_t primitiveCount = resources.maxProbeInstances;

                    // Describe the TLAS geometry instances
                    VkAccelerationStructureGeometryInstancesDataKHR asInstanceData = {};
                    asInstanceData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
                    asInstanceData.arrayOfPointers = VK_FALSE;
                    asInstanceData.data = VkDeviceOrHostAddressConstKHR{ GetBufferDeviceAddress(vk.device, resources.tlas.instances) };

                    // Describe the mesh primitive geometry
                    VkAccelerationStructureGeometryDataKHR asGeometryData = {};
                    asGeometryData.instances = asInstanceData;

                    VkAccelerationStructureGeometryKHR asGeometry = {};
                    asGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
                    asGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
                    asGeometry.geometry = asGeometryData;

                    // Describe the top level acceleration structure inputs
                    VkAccelerationStructureBuildGeometryInfoKHR asInputs = {};
                    asInputs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
                    asInputs.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
                    asInputs.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
                    asInputs.geometryCount = 1;
                    asInputs.pGeometries = &asGeometry;
                    asInputs.flags = buildFlags;

                    // Get the size requirements for the TLAS buffer
                    VkAccelerationStructureBuildSizesInfoKHR asPreBuildInfo = {};
                    asPreBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
                    vkGetAccelerationStructureBuildSizesKHR(vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &asInputs, &primitiveCount, &asPreBuildInfo);

                    // Create the acceleration structure buffer, allocate and bind device memory
                    BufferDesc desc = { asPreBuildInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
                    if (!CreateBuffer(vk, desc, &resources.tlas.asBuffer, &resources.tlas.asMemory)) return false;
                #ifdef GFX_NAME_OBJECTS
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.tlas.asBuffer), "DDGI Probe Visualization TLAS", VK_OBJECT_TYPE_BUFFER);
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.tlas.asMemory), "DDGI Probe Visualization TLAS Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
                #endif

                    // Create the scratch buffer, allocate and bind device memory
                    desc = { asPreBuildInfo.buildScratchSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
                    if (!CreateBuffer(vk, desc, &resources.tlas.scratch, &resources.tlas.scratchMemory)) return false;
                    asInputs.scratchData = VkDeviceOrHostAddressKHR{ GetBufferDeviceAddress(vk.device, resources.tlas.scratch) };
                #ifdef GFX_NAME_OBJECTS
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.tlas.scratch), "DDGI Probe Visualization TLAS Scratch", VK_OBJECT_TYPE_BUFFER);
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.tlas.scratchMemory), "DDGI Probe Visualization TLAS Scratch Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
                #endif

                    // Describe the TLAS
                    VkAccelerationStructureCreateInfoKHR asCreateInfo = {};
                    asCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
                    asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
                    asCreateInfo.size = asPreBuildInfo.accelerationStructureSize;
                    asCreateInfo.buffer = resources.tlas.asBuffer;

                    // Create the TLAS
                    VKCHECK(vkCreateAccelerationStructureKHR(vk.device, &asCreateInfo, nullptr, &resources.tlas.asKHR));
                #ifdef GFX_NAME_OBJECTS
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.tlas.asKHR), "TLAS: DDGI Probe Visualization", VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR);
                #endif

                    return true;
                }

                //----------------------------------------------------------------------------------------------------------
                // Public Functions
                //----------------------------------------------------------------------------------------------------------

                /**
                 * Create resources used by the DDGI passes.
                 */
                bool Initialize(Globals& vk, GlobalResources& vkResources, DDGI::Resources& ddgiResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
                {
                    resources.volumes = &ddgiResources.volumes;
                    resources.volumeConstantsSTB = ddgiResources.volumeConstantsSTB;
                    resources.volumeResourceIndicesSTB = ddgiResources.volumeResourceIndicesSTB;

                    // Reset the command list before initialization
                    CHECK(ResetCmdList(vk), "reset command list!", log);

                    if (!LoadAndCompileShaders(vk, resources, log)) return false;
                    if (!CreateDescriptorSets(vk, vkResources, resources, log)) return false;
                    if (!CreatePipelines(vk, vkResources, resources, log)) return false;
                    if (!CreateShaderTable(vk, resources, log)) return false;
                    if (!CreateGeometry(vk, vkResources, resources, log)) return false;
                    if (!CreateBLAS(vk, resources)) return false;
                    if (!CreateTLAS(vk, resources)) return false;

                    if (!UpdateShaderTable(vk, vkResources, resources)) return false;
                    if (!UpdateDescriptorSets(vk, vkResources, resources)) return false;

                    resources.cpuStat = perf.AddCPUStat("DDGIVis");
                    resources.gpuProbeStat = perf.AddGPUStat("DDGI Probe Vis");
                    resources.gpuTextureStat = perf.AddGPUStat("DDGI Texture Vis");

                    // Execute GPU work to finish initialization
                    VKCHECK(vkEndCommandBuffer(vk.cmdBuffer[vk.frameIndex]));

                    VkSubmitInfo submitInfo = {};
                    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                    submitInfo.commandBufferCount = 1;
                    submitInfo.pCommandBuffers = &vk.cmdBuffer[vk.frameIndex];

                    VKCHECK(vkQueueSubmit(vk.queue, 1, &submitInfo, VK_NULL_HANDLE));
                    VKCHECK(vkQueueWaitIdle(vk.queue));

                    WaitForGPU(vk);

                    return true;
                }

                /**
                 * Reload and compile shaders, recreate PSOs, and recreate the shader table.
                 */
                bool Reload(Globals& vk, GlobalResources& vkResources, DDGI::Resources& ddgiResources, Resources& resources, std::ofstream& log)
                {
                    resources.volumes = &ddgiResources.volumes;
                    resources.volumeConstantsSTB = ddgiResources.volumeConstantsSTB;
                    resources.volumeResourceIndicesSTB = ddgiResources.volumeResourceIndicesSTB;

                    log << "Reloading DDGI Visualization shaders...";
                    if (!LoadAndCompileShaders(vk, resources, log)) return false;
                    if (!CreatePipelines(vk, vkResources, resources, log)) return false;
                    if (!UpdateShaderTable(vk, vkResources, resources)) return false;
                    if (!UpdateDescriptorSets(vk, vkResources, resources)) return false;

                    log << "done.\n";
                    log << std::flush;

                    return true;
                }

                /**
                 * Resize, update descriptor sets. DDGI output texture is resized in DDGI_VK.cpp
                 */
                bool Resize(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
                {
                    log << "Updating DDGI Visualization descriptor sets...";
                    if (!UpdateDescriptorSets(vk, vkResources, resources)) return false;
                    log << "done.\n";
                    log << std::flush;
                    return true;
                }

                /**
                 * Update data before execute.
                 */
                void Update(Globals& vk, GlobalResources& vkResources, Resources& resources, const Configs::Config& config)
                {
                    CPU_TIMESTAMP_BEGIN(resources.cpuStat);

                    // Update the show flags
                    resources.flags = VIS_FLAG_SHOW_NONE;
                    if (config.ddgi.showProbes) resources.flags |= VIS_FLAG_SHOW_PROBES;
                    if (config.ddgi.showTextures) resources.flags |= VIS_FLAG_SHOW_TEXTURES;

                    resources.enabled = config.ddgi.enabled;
                    if (resources.enabled)
                    {
                        // Get the currently selected volume
                        Configs::DDGIVolume volume = config.ddgi.volumes[config.ddgi.selectedVolume];

                        // Set the selected volume's index
                        resources.selectedVolume = config.ddgi.selectedVolume;

                        if (resources.flags & VIS_FLAG_SHOW_PROBES)
                        {
                            // Update probe visualization constants
                            vkResources.constants.ddgivis.probeType = volume.probeType;
                            vkResources.constants.ddgivis.probeRadius = volume.probeRadius;
                            vkResources.constants.ddgivis.distanceDivisor = volume.probeDistanceDivisor;

                            // Update the TLAS instances and rebuild
                            UpdateTLAS(vk, vkResources, resources, config);
                        }

                        if (resources.flags & VIS_FLAG_SHOW_TEXTURES)
                        {
                            // Update texture visualization constants
                            vkResources.constants.ddgivis.distanceDivisor = volume.probeDistanceDivisor;
                            vkResources.constants.ddgivis.rayDataTextureScale = volume.probeRayDataScale;
                            vkResources.constants.ddgivis.irradianceTextureScale = volume.probeIrradianceScale;
                            vkResources.constants.ddgivis.distanceTextureScale = volume.probeDistanceScale;
                            vkResources.constants.ddgivis.probeDataTextureScale = volume.probeDataScale;
                            vkResources.constants.ddgivis.probeVariabilityTextureScale = volume.probeVariabilityScale;
                            vkResources.constants.ddgivis.probeVariabilityTextureThreshold = volume.probeVariabilityThreshold;
                        }
                    }
                    CPU_TIMESTAMP_END(resources.cpuStat);
                }

                /**
                 * Record the graphics workload to the global command list.
                 */
                void Execute(Globals& vk, GlobalResources& vkResources, Resources& resources)
                {
                    CPU_TIMESTAMP_BEGIN(resources.cpuStat);
                    if (resources.enabled)
                    {
                        // Render probes
                        if (resources.flags & VIS_FLAG_SHOW_PROBES)
                        {
                            if (resources.probeInstances.size() > 0)
                            {
                            #ifdef GFX_PERF_MARKERS
                                AddPerfMarker(vk, GFX_PERF_MARKER_GREEN, "Vis: DDGIVolume Probes");
                            #endif

                                // Update the vis push constants
                                GlobalConstants consts = vkResources.constants;
                                uint32_t offset = GlobalConstants::GetAlignedSizeInBytes() - DDGIVisConsts::GetAlignedSizeInBytes();
                                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, DDGIVisConsts::GetSizeInBytes(), consts.ddgivis.GetData());

                                // Bind the descriptor set
                                vkCmdBindDescriptorSets(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vkResources.pipelineLayout, 0, 1, &resources.descriptorSet, 0, nullptr);

                                // Describe the shaders and dispatch (EDDGIVolumeProbeVisType::Default)
                                {
                                    VkStridedDeviceAddressRegionKHR raygenRegion = {};
                                    raygenRegion.deviceAddress = resources.shaderTableRGSStartAddress;
                                    raygenRegion.size = resources.shaderTableRecordSize;
                                    raygenRegion.stride = resources.shaderTableRecordSize;

                                    VkStridedDeviceAddressRegionKHR missRegion = {};
                                    missRegion.deviceAddress = resources.shaderTableMissTableStartAddress;
                                    missRegion.size = resources.shaderTableMissTableSize;
                                    missRegion.stride = resources.shaderTableRecordSize;

                                    VkStridedDeviceAddressRegionKHR hitRegion = {};
                                    hitRegion.deviceAddress = resources.shaderTableHitGroupTableStartAddress;
                                    hitRegion.size = resources.shaderTableHitGroupTableSize;
                                    hitRegion.stride = resources.shaderTableRecordSize;

                                    VkStridedDeviceAddressRegionKHR callableRegion = {};

                                    // Bind the pipeline
                                    vkCmdBindPipeline(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, resources.rtPipeline);

                                    // Dispatch rays
                                    GPU_TIMESTAMP_BEGIN(resources.gpuProbeStat->GetGPUQueryBeginIndex());
                                    vkCmdTraceRaysKHR(
                                        vk.cmdBuffer[vk.frameIndex],
                                        &raygenRegion,
                                        &missRegion,
                                        &hitRegion,
                                        &callableRegion,
                                        vk.width,
                                        vk.height,
                                        1);
                                    GPU_TIMESTAMP_END(resources.gpuProbeStat->GetGPUQueryEndIndex());

                                    // Wait for the ray trace to finish
                                    VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                                    ImageBarrierDesc barrier = { VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
                                    SetImageMemoryBarrier(vk.cmdBuffer[vk.frameIndex], vkResources.rt.GBufferA, barrier);
                                    SetImageMemoryBarrier(vk.cmdBuffer[vk.frameIndex], vkResources.rt.GBufferB, barrier);
                                }

                                // Describe the shaders and dispatch (EDDGIVolumeProbeVisType::Hide_Inactive)
                                {
                                    VkStridedDeviceAddressRegionKHR raygenRegion = {};
                                    raygenRegion.deviceAddress = resources.shaderTableRGS2StartAddress;
                                    raygenRegion.size = resources.shaderTableRecordSize;
                                    raygenRegion.stride = resources.shaderTableRecordSize;

                                    VkStridedDeviceAddressRegionKHR missRegion = {};
                                    missRegion.deviceAddress = resources.shaderTableMissTable2StartAddress;
                                    missRegion.size = resources.shaderTableMissTableSize;
                                    missRegion.stride = resources.shaderTableRecordSize;

                                    VkStridedDeviceAddressRegionKHR hitRegion = {};
                                    hitRegion.deviceAddress = resources.shaderTableHitGroupTable2StartAddress;
                                    hitRegion.size = resources.shaderTableHitGroupTableSize;
                                    hitRegion.stride = resources.shaderTableRecordSize;

                                    VkStridedDeviceAddressRegionKHR callableRegion = {};

                                    // Bind the pipeline
                                    vkCmdBindPipeline(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, resources.rtPipeline2);

                                    // Dispatch rays
                                    GPU_TIMESTAMP_BEGIN(resources.gpuProbeStat->GetGPUQueryBeginIndex());
                                    vkCmdTraceRaysKHR(
                                        vk.cmdBuffer[vk.frameIndex],
                                        &raygenRegion,
                                        &missRegion,
                                        &hitRegion,
                                        &callableRegion,
                                        vk.width,
                                        vk.height,
                                        1);
                                    GPU_TIMESTAMP_END(resources.gpuProbeStat->GetGPUQueryEndIndex());

                                    // Wait for the ray trace to finish
                                    VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                                    ImageBarrierDesc barrier = { VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
                                    SetImageMemoryBarrier(vk.cmdBuffer[vk.frameIndex], vkResources.rt.GBufferA, barrier);
                                    SetImageMemoryBarrier(vk.cmdBuffer[vk.frameIndex], vkResources.rt.GBufferB, barrier);
                                }

                            #ifdef GFX_PERF_MARKERS
                                vkCmdEndDebugUtilsLabelEXT(vk.cmdBuffer[vk.frameIndex]);
                            #endif
                            }
                        }

                        // Render volume textures
                        if (resources.flags & VIS_FLAG_SHOW_TEXTURES)
                        {
                        #ifdef GFX_PERF_MARKERS
                            AddPerfMarker(vk, GFX_PERF_MARKER_GREEN, "Vis: DDGIVolume Textures");
                        #endif

                            // Update the vis push constants
                            GlobalConstants consts = vkResources.constants;
                            uint32_t offset = GlobalConstants::GetAlignedSizeInBytes() - DDGIVisConsts::GetAlignedSizeInBytes();
                            vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, DDGIVisConsts::GetSizeInBytes(), consts.ddgivis.GetData());

                            // Update the DDGI push constants
                            DDGIRootConstants pushConsts = { resources.selectedVolume, 0, 0 };
                            vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, GlobalConstants::GetAlignedSizeInBytes(), DDGIRootConstants::GetSizeInBytes(), pushConsts.GetData());

                            // Bind the pipeline
                            vkCmdBindPipeline(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_COMPUTE, resources.textureVisPipeline);

                            // Bind the descriptor set
                            vkCmdBindDescriptorSets(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_COMPUTE, vkResources.pipelineLayout, 0, 1, &resources.descriptorSet, 0, nullptr);

                            // Dispatch threads
                            uint32_t groupsX = DivRoundUp(vk.width, 8);
                            uint32_t groupsY = DivRoundUp(vk.height, 4);

                            GPU_TIMESTAMP_BEGIN(resources.gpuTextureStat->GetGPUQueryBeginIndex());
                            vkCmdDispatch(vk.cmdBuffer[vk.frameIndex], groupsX, groupsY, 1);
                            GPU_TIMESTAMP_END(resources.gpuTextureStat->GetGPUQueryEndIndex());

                            // Wait for the ray trace to finish
                            VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                            ImageBarrierDesc barrier = { VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
                            SetImageMemoryBarrier(vk.cmdBuffer[vk.frameIndex], vkResources.rt.GBufferA, barrier);

                        #ifdef GFX_PERF_MARKERS
                            vkCmdEndDebugUtilsLabelEXT(vk.cmdBuffer[vk.frameIndex]);
                        #endif
                        }
                    }
                    CPU_TIMESTAMP_ENDANDRESOLVE(resources.cpuStat);
                }

                /**
                 * Release resources.
                 */
                void Cleanup(VkDevice device, Resources& resources)
                {
                    // Geometry
                    vkFreeMemory(device, resources.probeIBMemory, nullptr);
                    vkDestroyBuffer(device, resources.probeIB, nullptr);
                    vkFreeMemory(device, resources.probeIBUploadMemory, nullptr);
                    vkDestroyBuffer(device, resources.probeIBUpload, nullptr);

                    vkFreeMemory(device, resources.probeVBMemory, nullptr);
                    vkDestroyBuffer(device, resources.probeVB, nullptr);
                    vkFreeMemory(device, resources.probeVBUploadMemory, nullptr);
                    vkDestroyBuffer(device, resources.probeVBUpload, nullptr);

                    resources.blas.Release(device);
                    resources.tlas.Release(device);

                    // Shader Table
                    vkDestroyBuffer(device, resources.shaderTableUpload, nullptr);
                    vkFreeMemory(device, resources.shaderTableUploadMemory, nullptr);
                    vkDestroyBuffer(device, resources.shaderTable, nullptr);
                    vkFreeMemory(device, resources.shaderTableMemory, nullptr);

                    // Shaders
                    resources.rtShaders.Release();
                    resources.rtShaders2.rgs.Release();
                    resources.textureVisCS.Release();
                    resources.updateTlasCS.Release();

                    // Shader Modules
                    resources.rtShadersModule.Release(device);
                    resources.rtShadersModule2.Release(device);
                    vkDestroyShaderModule(device, resources.textureVisModule, nullptr);
                    vkDestroyShaderModule(device, resources.updateTlasModule, nullptr);

                    // Pipelines
                    vkDestroyPipeline(device, resources.rtPipeline, nullptr);
                    vkDestroyPipeline(device, resources.rtPipeline2, nullptr);
                    vkDestroyPipeline(device, resources.textureVisPipeline, nullptr);
                    vkDestroyPipeline(device, resources.updateTlasPipeline, nullptr);

                    resources.shaderTableSize = 0;
                    resources.shaderTableRecordSize = 0;
                    resources.shaderTableMissTableSize = 0;
                    resources.shaderTableHitGroupTableSize = 0;
                }

            } // namespace Graphics::Vulkan::DDGI::Visualizations

        } // namespace Graphics::Vulkan::DDGI

    } // namespace Graphics::Vulkan

    namespace DDGI::Visualizations
    {

        bool Initialize(Globals& vk, GlobalResources& vkResources, DDGI::Resources& ddgiResources, Resources& resources, Instrumentation::Performance& perf, Configs::Config& config, std::ofstream& log)
        {
            return Graphics::Vulkan::DDGI::Visualizations::Initialize(vk, vkResources, ddgiResources, resources, perf, log);
        }

        bool Reload(Globals& vk, GlobalResources& vkResources, DDGI::Resources& ddgiResources, Resources& resources, Configs::Config& config, std::ofstream& log)
        {
            return Graphics::Vulkan::DDGI::Visualizations::Reload(vk, vkResources, ddgiResources, resources, log);
        }

        bool Resize(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::Vulkan::DDGI::Visualizations::Resize(vk, vkResources, resources, log);
        }

        void Update(Globals& vk, GlobalResources& vkResources, Resources& resources, const Configs::Config& config)
        {
            return Graphics::Vulkan::DDGI::Visualizations::Update(vk, vkResources, resources, config);
        }

        void Execute(Globals& vk, GlobalResources& vkResources, Resources& resources)
        {
            return Graphics::Vulkan::DDGI::Visualizations::Execute(vk, vkResources, resources);
        }

        void Cleanup(Globals& vk, Resources& resources)
        {
            Graphics::Vulkan::DDGI::Visualizations::Cleanup(vk.device, resources);
        }

    } // namespace Graphics::DDGIVis
}
