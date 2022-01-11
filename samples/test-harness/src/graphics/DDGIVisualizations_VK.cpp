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
                namespace DescriptorLayoutBindings
                {
                    const int SAMPLERS = 0;                                 //   0: Samplers
                    const int CBV_CAMERA = SAMPLERS + 1;                    //   1: Camera constant buffer
                    const int SRV_BVH = CBV_CAMERA + 1;                     //   2: RT acceleration structure
                    const int STB_DDGI_VOLUMES = SRV_BVH + 1;               //   3: DDGIVolume constants structured buffer
                    const int STB_INSTANCES = STB_DDGI_VOLUMES + 1;         //   4: TLAS instance descriptors structured buffer
                    const int UAV_GBUFFERA = STB_INSTANCES + 1;             //   5: GBufferA RWTexture
                    const int UAV_GBUFFERB = UAV_GBUFFERA + 1;              //   6: GBufferB RWTexture
                    const int SRV_RAW = UAV_GBUFFERB + 1;                   //   7: Raw Buffers (Probe Sphere Mesh Index/Vertex Buffers)
                    const int SRV_TEX2D = SRV_RAW + 1;                      //   8: DDGIVolume Textures, 4 SRV per DDGIVolume (Ray Data, Irradiance, Distance, Probe Data)
                };

                //----------------------------------------------------------------------------------------------------------
                // Private Functions
                //----------------------------------------------------------------------------------------------------------

                bool UpdateDescriptorSets(Globals& vk, GlobalResources& vkResources, Resources& resources)
                {
                    // Store the data to be written to the descriptor set
                    std::vector<VkWriteDescriptorSet> writeDescriptorSets;

                    // Samplers
                    VkDescriptorImageInfo samplersInfo[] =
                    {
                        { vkResources.samplers[0], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED }, // bilinear wrap sampler
                        { vkResources.samplers[1], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED }  // point clamp sampler
                    };

                    VkWriteDescriptorSet samplerSet = {};
                    samplerSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    samplerSet.dstSet = resources.descriptorSet;
                    samplerSet.dstBinding = DescriptorLayoutBindings::SAMPLERS;
                    samplerSet.dstArrayElement = SamplerIndices::BILINEAR_WRAP;
                    samplerSet.descriptorCount = _countof(samplersInfo);
                    samplerSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                    samplerSet.pImageInfo = samplersInfo;

                    writeDescriptorSets.push_back(samplerSet);

                    // Camera constant buffer
                    VkDescriptorBufferInfo cameraCBInfo = { vkResources.cameraCB, 0, VK_WHOLE_SIZE };
                    VkWriteDescriptorSet cameraCBSet = {};
                    cameraCBSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    cameraCBSet.dstSet = resources.descriptorSet;
                    cameraCBSet.dstBinding = DescriptorLayoutBindings::CBV_CAMERA;
                    cameraCBSet.dstArrayElement = 0;
                    cameraCBSet.descriptorCount = 1;
                    cameraCBSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    cameraCBSet.pBufferInfo = &cameraCBInfo;

                    writeDescriptorSets.push_back(cameraCBSet);

                    // Ray Tracing TLAS
                    VkWriteDescriptorSetAccelerationStructureKHR tlasInfo = {};
                    tlasInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                    tlasInfo.accelerationStructureCount = 1;
                    tlasInfo.pAccelerationStructures = &resources.tlas.asKHR;

                    VkWriteDescriptorSet tlasSet = {};
                    tlasSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    tlasSet.pNext = &tlasInfo;
                    tlasSet.dstSet = resources.descriptorSet;
                    tlasSet.dstBinding = DescriptorLayoutBindings::SRV_BVH;
                    tlasSet.dstArrayElement = 0;
                    tlasSet.descriptorCount = 1;
                    tlasSet.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

                    writeDescriptorSets.push_back(tlasSet);

                    // DDGIVolume constants structured buffer
                    VkDescriptorBufferInfo constantsSTBInfo = { resources.constantsSTB, 0, VK_WHOLE_SIZE };

                    VkWriteDescriptorSet constantsSTBSet = {};
                    constantsSTBSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    constantsSTBSet.dstSet = resources.descriptorSet;
                    constantsSTBSet.dstBinding = DescriptorLayoutBindings::STB_DDGI_VOLUMES;
                    constantsSTBSet.dstArrayElement = 0;
                    constantsSTBSet.descriptorCount = 1;
                    constantsSTBSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    constantsSTBSet.pBufferInfo = &constantsSTBInfo;

                    writeDescriptorSets.push_back(constantsSTBSet);

                    // Instances structured buffer
                    VkDescriptorBufferInfo instancesSTBInfo = { resources.tlas.instances, 0, VK_WHOLE_SIZE };

                    VkWriteDescriptorSet instancesSTBSet = {};
                    instancesSTBSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    instancesSTBSet.dstSet = resources.descriptorSet;
                    instancesSTBSet.dstBinding = DescriptorLayoutBindings::STB_INSTANCES;
                    instancesSTBSet.dstArrayElement = 0;
                    instancesSTBSet.descriptorCount = 1;
                    instancesSTBSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    instancesSTBSet.pBufferInfo = &instancesSTBInfo;

                    writeDescriptorSets.push_back(instancesSTBSet);

                    VkDescriptorImageInfo gBufferAInfo = { VK_NULL_HANDLE, vkResources.rt.GBufferAView, VK_IMAGE_LAYOUT_GENERAL };
                    VkDescriptorImageInfo gBufferBInfo = { VK_NULL_HANDLE, vkResources.rt.GBufferBView, VK_IMAGE_LAYOUT_GENERAL };

                    // GBufferA UAV
                    VkWriteDescriptorSet gBufferASet = {};
                    gBufferASet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    gBufferASet.dstSet = resources.descriptorSet;
                    gBufferASet.dstBinding = DescriptorLayoutBindings::UAV_GBUFFERA;
                    gBufferASet.dstArrayElement = 0;
                    gBufferASet.descriptorCount = 1;
                    gBufferASet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    gBufferASet.pImageInfo = &gBufferAInfo;

                    writeDescriptorSets.push_back(gBufferASet);

                    // GBufferB UAV
                    VkWriteDescriptorSet gBufferBSet = {};
                    gBufferBSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    gBufferBSet.dstSet = resources.descriptorSet;
                    gBufferBSet.dstBinding = DescriptorLayoutBindings::UAV_GBUFFERB;
                    gBufferBSet.dstArrayElement = 0;
                    gBufferBSet.descriptorCount = 1;
                    gBufferBSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    gBufferBSet.pImageInfo = &gBufferBInfo;

                    writeDescriptorSets.push_back(gBufferBSet);

                    VkDescriptorBufferInfo rawBuffersInfo[] =
                    {
                        { resources.probeIB, 0, VK_WHOLE_SIZE },
                        { resources.probeVB, 0, VK_WHOLE_SIZE }
                    };

                    // ByteAddress Buffer SRVs (index / vertex buffers)
                    VkWriteDescriptorSet rawBuffersSet = {};
                    rawBuffersSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    rawBuffersSet.dstSet = resources.descriptorSet;
                    rawBuffersSet.dstBinding = DescriptorLayoutBindings::SRV_RAW;
                    rawBuffersSet.dstArrayElement = 0;
                    rawBuffersSet.descriptorCount = _countof(rawBuffersInfo);
                    rawBuffersSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    rawBuffersSet.pBufferInfo = rawBuffersInfo;

                    writeDescriptorSets.push_back(rawBuffersSet);

                    std::vector<VkDescriptorImageInfo> tex2DInfo;
                    for (uint32_t volumeIndex = 0; volumeIndex < static_cast<uint32_t>(resources.volumes->size()); volumeIndex++)
                    {
                        DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes->at(volumeIndex));

                        tex2DInfo.push_back({ VK_NULL_HANDLE, volume->GetProbeRayDataView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
                        tex2DInfo.push_back({ VK_NULL_HANDLE, volume->GetProbeIrradianceView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
                        tex2DInfo.push_back({ VK_NULL_HANDLE, volume->GetProbeDistanceView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
                        tex2DInfo.push_back({ VK_NULL_HANDLE, volume->GetProbeDataView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
                    }

                    // Tex2D DDGIVolume storage images
                    VkWriteDescriptorSet tex2DSet = {};
                    tex2DSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    tex2DSet.dstSet = resources.descriptorSet;
                    tex2DSet.dstBinding = DescriptorLayoutBindings::SRV_TEX2D;
                    tex2DSet.dstArrayElement = 0;
                    tex2DSet.descriptorCount = static_cast<uint32_t>(tex2DInfo.size());
                    tex2DSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                    tex2DSet.pImageInfo = tex2DInfo.data();

                    writeDescriptorSets.push_back(tex2DSet);

                    // Update the descriptor set
                    vkUpdateDescriptorSets(vk.device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

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

                    // Get the shader group IDs from the pipeline
                    std::vector<uint8_t> shaderGroupIdBuffer(shaderGroupIdSize * resources.rtShadersModule.numGroups);
                    VKCHECK(vkGetRayTracingShaderGroupHandlesKHR(vk.device, resources.rtPipeline, 0, resources.rtShadersModule.numGroups, (shaderGroupIdSize * resources.rtShadersModule.numGroups), shaderGroupIdBuffer.data()));

                    // Separate the shader group IDs into an array
                    std::vector<uint8_t*> shaderGroupIds(resources.rtShadersModule.numGroups);
                    for (uint32_t i = 0; i < resources.rtShadersModule.numGroups; ++i)
                    {
                        shaderGroupIds[i] = shaderGroupIdBuffer.data() + i * shaderGroupIdSize;
                    }

                    uint32_t groupIndex = 0;

                    // Entry 0: Ray Generation Shader
                    memcpy(pData, shaderGroupIds[groupIndex++], shaderGroupIdSize);
                    resources.shaderTableRGSStartAddress = address;

                    address += resources.shaderTableRecordSize;

                    // Entry 1: Miss Shader
                    pData += resources.shaderTableRecordSize;
                    memcpy(pData, shaderGroupIds[groupIndex++], shaderGroupIdSize);
                    resources.shaderTableMissTableStartAddress = address;
                    resources.shaderTableMissTableSize = resources.shaderTableRecordSize;

                    address += resources.shaderTableMissTableSize;

                    // Entries 2+: Hit Groups
                    for (uint32_t hitGroupIndex = 0; hitGroupIndex < static_cast<uint32_t>(resources.rtShaders.hitGroups.size()); hitGroupIndex++)
                    {
                        pData += resources.shaderTableRecordSize;
                        memcpy(pData, shaderGroupIds[groupIndex++], shaderGroupIdSize);
                    }
                    resources.shaderTableHitGroupTableStartAddress = address;
                    resources.shaderTableHitGroupTableSize = static_cast<uint32_t>(resources.rtShaders.hitGroups.size()) * resources.shaderTableRecordSize;

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

                        // Add an instance for each probe
                        for (uint32_t probeIndex = 0; probeIndex < static_cast<uint32_t>(volume->GetNumProbes()); probeIndex++)
                        {
                            VkAccelerationStructureDeviceAddressInfoKHR asDeviceAddressInfo = {};
                            asDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
                            asDeviceAddressInfo.accelerationStructure = resources.blas.asKHR;
                            VkDeviceAddress blasAddress = vkGetAccelerationStructureDeviceAddressKHR(vk.device, &asDeviceAddressInfo);

                            // Describe the probe instance
                            VkAccelerationStructureInstanceKHR desc = {};
                            desc.instanceCustomIndex  = instanceOffset;                     // instance offset in first 16 bits
                            desc.instanceCustomIndex |= (uint8_t)volume->GetIndex() << 16;  // volume index in last 8 bits
                            desc.mask = 0xFF;
                            desc.instanceShaderBindingTableRecordOffset = 0;     // A single hit group for all geometry
                            desc.accelerationStructureReference = blasAddress;
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

                bool UpdateTLAS(Globals& vk, Resources& resources, const Configs::Config& config)
                {
                #ifdef GFX_PERF_MARKERS
                    AddPerfMarker(vk, GFX_PERF_MARKER_GREEN, "Update DDGI Visualizations TLAS");
                #endif

                    // Update the instances and copy them to the GPU
                    UpdateInstances(vk, resources);

                    // Early out if no volumes want to visualize probes
                    if (resources.probeInstances.size() == 0) return true;

                    // Bind the descriptor set
                    vkCmdBindDescriptorSets(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_COMPUTE, resources.pipelineLayout, 0, 1, &resources.descriptorSet, 0, nullptr);

                    // Bind the update pipeline
                    vkCmdBindPipeline(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_COMPUTE, resources.updateTlasPipeline);

                    uint32_t instanceOffset = 0;
                    for (uint32_t volumeIndex = 0; volumeIndex < static_cast<uint32_t>(resources.volumes->size()); volumeIndex++)
                    {
                        DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes->at(volumeIndex));

                        // Skip this volume if the "Show Probes" flag is disabled
                        if (!volume->GetShowProbes()) continue;

                        // Update constants
                        resources.constants.volumeIndex = volume->GetIndex();
                        resources.constants.instanceOffset = instanceOffset;
                        resources.constants.probeRadius = config.ddgi.volumes[volumeIndex].probeRadius;

                        // Set the push constants
                        vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], resources.pipelineLayout, VK_SHADER_STAGE_ALL, 0, DDGIVisConstants::GetSizeInBytes(), resources.constants.GetData());

                        // Dispatch the compute shader
                        uint32_t numProbes = static_cast<uint32_t>(volume->GetNumProbes());
                        vkCmdDispatch(vk.cmdBuffer[vk.frameIndex], numProbes, 1, 1);

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

                bool CreatePipelineLayout(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
                {
                    // Describe the descriptor set layout bindings (aligns with DDGI/Visualizations/Descriptors.hlsl)
                    std::vector<VkDescriptorSetLayoutBinding> bindings;

                    // 0: Samplers
                    VkDescriptorSetLayoutBinding samplersBinding = {};
                    samplersBinding.binding = DescriptorLayoutBindings::SAMPLERS;
                    samplersBinding.descriptorCount = 10;
                    samplersBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                    samplersBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

                    bindings.push_back(samplersBinding);

                    // 1: Camera constant buffer
                    VkDescriptorSetLayoutBinding cameraCBBinding = {};
                    cameraCBBinding.binding = 1;
                    cameraCBBinding.descriptorCount = 1;
                    cameraCBBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    cameraCBBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

                    bindings.push_back(cameraCBBinding);

                    // 2: Ray Tracing Acceleration Structure SRV
                    VkDescriptorSetLayoutBinding bvhBinding = {};
                    bvhBinding.binding = 2;
                    bvhBinding.descriptorCount = 1;
                    bvhBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
                    bvhBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;    // we do not allow tracing in hit shaders (i.e. recursive tracing)

                    bindings.push_back(bvhBinding);

                    // 3: DDGIVolume constants structured buffer SRV
                    VkDescriptorSetLayoutBinding constantsSTBBinding = {};
                    constantsSTBBinding.binding = 3;
                    constantsSTBBinding.descriptorCount = 1;
                    constantsSTBBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    constantsSTBBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

                    bindings.push_back(constantsSTBBinding);

                    // 4: TLAS Instances UAV (u0)
                    VkDescriptorSetLayoutBinding tlasBinding = {};
                    tlasBinding.binding = 4;
                    tlasBinding.descriptorCount = 1;
                    tlasBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    tlasBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

                    bindings.push_back(tlasBinding);

                    // 5: GBufferA UAV (u1)
                    VkDescriptorSetLayoutBinding gbufferABinding = {};
                    gbufferABinding.binding = 5;
                    gbufferABinding.descriptorCount = 1;
                    gbufferABinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    gbufferABinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

                    bindings.push_back(gbufferABinding);

                    // 6: GBufferB UAV (u2)
                    VkDescriptorSetLayoutBinding gbufferBBinding = {};
                    gbufferBBinding.binding = 6;
                    gbufferBBinding.descriptorCount = 1;
                    gbufferBBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    gbufferBBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

                    bindings.push_back(gbufferBBinding);

                    // 7: ByteAddress Buffers
                    VkDescriptorSetLayoutBinding rawBufferBinding = {};
                    rawBufferBinding.binding = 7;
                    rawBufferBinding.descriptorCount = 500;
                    rawBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    rawBufferBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

                    bindings.push_back(rawBufferBinding);

                    // 8: Textures (SRVs)
                    VkDescriptorSetLayoutBinding tex2DBinding = {};
                    tex2DBinding.binding = 8;
                    tex2DBinding.descriptorCount = 500;
                    tex2DBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                    tex2DBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

                    bindings.push_back(tex2DBinding);

                    // Describe the push constants
                    VkPushConstantRange pushConstantRange = {};
                    pushConstantRange.stageFlags = VK_SHADER_STAGE_ALL;
                    pushConstantRange.offset = 0;
                    pushConstantRange.size = DDGIVisConstants::GetAlignedSizeInBytes();

                    VkDescriptorBindingFlags bindingFlags[] =
                    {
                        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, // 0: Samplers[]
                        0, // 1: Camera Constant Buffer
                        0, // 2: RT Acceleration Structure
                        0, // 3: DDGIVolumes Structured Buffer
                        0, // 4: Instances Structured Buffer
                        0, // 5: GBufferA SRV
                        0, // 6: GBufferB SRV
                        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, // 7: ByteAddressBuffer[]
                        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, // 8: Tex2D[]
                    };
                    assert(_countof(bindingFlags) == bindings.size()); // must have 1 binding flag per binding slot

                    // Describe the descriptor bindings
                    VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingsCreateInfo = {};
                    descriptorSetLayoutBindingsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
                    descriptorSetLayoutBindingsCreateInfo.pBindingFlags = bindingFlags;
                    descriptorSetLayoutBindingsCreateInfo.bindingCount = _countof(bindingFlags);

                    // Describe the global descriptor set layout
                    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
                    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                    descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingsCreateInfo;
                    descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
                    descriptorSetLayoutCreateInfo.pBindings = bindings.data();

                    // Create the descriptor set layout
                    VKCHECK(vkCreateDescriptorSetLayout(vk.device, &descriptorSetLayoutCreateInfo, nullptr, &resources.descriptorSetLayout));
                #ifdef GFX_NAME_OBJECTS
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.descriptorSetLayout), "DDGI Visualizations Descriptor Set Layout", VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);
                #endif

                    // Describe the pipeline layout
                    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
                    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
                    pipelineLayoutCreateInfo.setLayoutCount = 1;
                    pipelineLayoutCreateInfo.pSetLayouts = &resources.descriptorSetLayout;
                    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
                    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

                    // Create the pipeline layout
                    VKCHECK(vkCreatePipelineLayout(vk.device, &pipelineLayoutCreateInfo, nullptr, &resources.pipelineLayout));
                #ifdef GFX_NAME_OBJECTS
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.pipelineLayout), "DDGI Visualizations Pipeline Layout", VK_OBJECT_TYPE_PIPELINE_LAYOUT);
                #endif

                    return true;
                }

                bool LoadAndCompileShaders(Globals& vk, Resources& resources, std::ofstream& log)
                {
                    // Release existing shaders
                    resources.rtShaders.Release();
                    resources.textureVisCS.Release();
                    resources.updateTlasCS.Release();

                    std::wstring root = std::wstring(vk.shaderCompiler.root.begin(), vk.shaderCompiler.root.end());

                    // Load and compile the ray generation shader
                    {
                        resources.rtShaders.rgs.filepath = root + L"shaders/ddgi/visualizations/ProbesRGS.hlsl";
                        resources.rtShaders.rgs.entryPoint = L"RayGen";
                        resources.rtShaders.rgs.exportName = L"DDGIVisProbesRGS";
                        resources.rtShaders.rgs.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };

                        Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_COORDINATE_SYSTEM", std::to_wstring(RTXGI_COORDINATE_SYSTEM));

                        // Load and compile RGS
                        CHECK(Shaders::Compile(vk.shaderCompiler, resources.rtShaders.rgs, true), "compile DDGI Visualizations ray generation shader!\n", log);
                    }

                    // Load and compile the miss shader
                    {
                        resources.rtShaders.miss.filepath = root + L"shaders/ddgi/visualizations/ProbesMiss.hlsl";
                        resources.rtShaders.miss.entryPoint = L"Miss";
                        resources.rtShaders.miss.exportName = L"DDGIVisProbesMiss";
                        resources.rtShaders.miss.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };

                        // Load and compile
                        CHECK(Shaders::Compile(vk.shaderCompiler, resources.rtShaders.miss, true), "compile DDGI Visualizations miss shader!\n", log);
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
                        group.chs.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };

                        // Load and compile
                        CHECK(Shaders::Compile(vk.shaderCompiler, group.chs, true), "compile DDGI Visualizations closest hit shader!\n", log);

                        // Set the payload size
                        resources.rtShaders.payloadSizeInBytes = sizeof(ProbesPayload);
                    }

                    // Load and compile the volume texture shader
                    {
                        resources.textureVisCS.filepath = root + L"shaders/ddgi/visualizations/VolumeTexturesCS.hlsl";
                        resources.textureVisCS.entryPoint = L"CS";
                        resources.textureVisCS.targetProfile = L"cs_6_0";
                        resources.textureVisCS.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };

                        Shaders::AddDefine(resources.textureVisCS, L"RTXGI_COORDINATE_SYSTEM", std::to_wstring(RTXGI_COORDINATE_SYSTEM));
                        Shaders::AddDefine(resources.textureVisCS, L"THGP_DIM_X", L"8");
                        Shaders::AddDefine(resources.textureVisCS, L"THGP_DIM_Y", L"4");

                        CHECK(Shaders::Compile(vk.shaderCompiler, resources.textureVisCS, true), "compile DDGI Visualizations volume textures compute shader!\n", log);
                    }

                    // Load and compile the TLAS update compute shader
                    {
                        resources.updateTlasCS.filepath = root + L"shaders/ddgi/visualizations/ProbesUpdateCS.hlsl";
                        resources.updateTlasCS.entryPoint = L"CS";
                        resources.updateTlasCS.targetProfile = L"cs_6_0";
                        resources.updateTlasCS.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };

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
                    descriptorSetAllocateInfo.pSetLayouts = &resources.descriptorSetLayout;

                    // Allocate the descriptor set
                    VKCHECK(vkAllocateDescriptorSets(vk.device, &descriptorSetAllocateInfo, &resources.descriptorSet));
                #ifdef GFX_NAME_OBJECTS
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.descriptorSet), "DDGI Visualizations Descriptor Set", VK_OBJECT_TYPE_DESCRIPTOR_SET);
                #endif

                    return true;
                }

                bool CreatePipelines(Globals& vk, Resources& resources, std::ofstream& log)
                {
                    // Release existing shader modules
                    resources.rtShadersModule.Release(vk.device);
                    vkDestroyShaderModule(vk.device, resources.textureVisModule, nullptr);
                    vkDestroyShaderModule(vk.device, resources.updateTlasModule, nullptr);

                    // Release existing pipelines
                    vkDestroyPipeline(vk.device, resources.rtPipeline, nullptr);
                    vkDestroyPipeline(vk.device, resources.textureVisPipeline, nullptr);
                    vkDestroyPipeline(vk.device, resources.updateTlasPipeline, nullptr);

                    // Create the shader modules
                    {
                        // Create the RT pipeline shader module
                        CHECK(CreateRayTracingShaderModules(
                            vk.device,
                            resources.rtShaders,
                            resources.rtShadersModule),
                            "create DDGI Visualization RT shader modules!\n", log);
                    #ifdef GFX_NAME_OBJECTS
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rtShadersModule.rgs), "DDGI Probe RT Visualization RGS Shader Module", VK_OBJECT_TYPE_SHADER_MODULE);
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rtShadersModule.miss), "DDGI Probe RT Visualization MS Shader Module", VK_OBJECT_TYPE_SHADER_MODULE);
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
                        // Create the probe RT pipeline
                        CHECK(CreateRayTracingPipeline(
                            vk.device,
                            resources.pipelineLayout,
                            resources.rtShaders,
                            resources.rtShadersModule,
                            &resources.rtPipeline),
                            "create DDGI Probe Visualization RT pipeline!\n", log);

                    #ifdef GFX_NAME_OBJECTS
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rtPipeline), "DDGI Probe Visualization RT Pipeline", VK_OBJECT_TYPE_PIPELINE);
                    #endif

                        // Create the volume texture visualization pipeline
                        CHECK(CreateComputePipeline(
                            vk.device,
                            resources.pipelineLayout,
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
                            resources.pipelineLayout,
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
                    //    Entry 0:  Probe Vis Ray Generation Shader
                    //    Entry 1:  Probe Vis Miss Shader
                    //    Entry 2+: Probe Vis HitGroups
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
                    VkBuildAccelerationStructureFlagBitsKHR buildFlags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;

                    uint32_t primitiveCount = static_cast<uint32_t>(resources.probe.indices.size()) / 3;

                    // Describe the BLAS geometries
                    VkAccelerationStructureGeometryTrianglesDataKHR asTriangleData = {};
                    asTriangleData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
                    asTriangleData.vertexData = VkDeviceOrHostAddressConstKHR{ GetBufferDeviceAddress(vk.device, resources.probeVB) };
                    asTriangleData.vertexStride = sizeof(Vertex);
                    asTriangleData.maxVertex = static_cast<uint32_t>(resources.probe.vertices.size());
                    asTriangleData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
                    asTriangleData.indexData = VkDeviceOrHostAddressConstKHR{ GetBufferDeviceAddress(vk.device, resources.probeIB) };
                    asTriangleData.indexType = VK_INDEX_TYPE_UINT32;

                    // Describe the mesh primitive geometry
                    VkAccelerationStructureGeometryDataKHR asGeometryData = {};
                    asGeometryData.triangles = asTriangleData;

                    VkAccelerationStructureGeometryKHR asGeometry = {};
                    asGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
                    asGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
                    asGeometry.geometry = asGeometryData;
                    if (resources.probe.opaque) asGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

                    // Describe the bottom level acceleration structure inputs
                    VkAccelerationStructureBuildGeometryInfoKHR asInputs = {};
                    asInputs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
                    asInputs.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
                    asInputs.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
                    asInputs.geometryCount = 1;
                    asInputs.pGeometries = &asGeometry;
                    asInputs.flags = buildFlags;

                    // Get the size requirements for the BLAS buffer
                    VkAccelerationStructureBuildSizesInfoKHR asPreBuildInfo = {};
                    asPreBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
                    vkGetAccelerationStructureBuildSizesKHR(vk.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &asInputs, &primitiveCount, &asPreBuildInfo);

                    // Create the acceleration structure buffer, allocate and bind device memory
                    BufferDesc desc = { asPreBuildInfo.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
                    if (!CreateBuffer(vk, desc, &resources.blas.asBuffer, &resources.blas.asMemory)) return false;
                #ifdef GFX_NAME_OBJECTS
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.blas.asBuffer), "BLAS: Probe Sphere, Primitive 0", VK_OBJECT_TYPE_BUFFER);
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.blas.asMemory), "BLAS Memory: Probe Sphere, Primitive 0", VK_OBJECT_TYPE_DEVICE_MEMORY);
                #endif

                    // Create the scratch buffer, allocate and bind device memory
                    desc = { asPreBuildInfo.buildScratchSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
                    if (!CreateBuffer(vk, desc, &resources.blas.scratch, &resources.blas.scratchMemory)) return false;
                    asInputs.scratchData = VkDeviceOrHostAddressKHR{ GetBufferDeviceAddress(vk.device, resources.blas.scratch) };
                #ifdef GFX_NAME_OBJECTS
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.blas.scratch), "BLAS Scratch: Probe Sphere, Primitive 0", VK_OBJECT_TYPE_BUFFER);
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.blas.scratchMemory), "BLAS Scratch Memory: Probe Sphere, Primitive 0", VK_OBJECT_TYPE_DEVICE_MEMORY);
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
                        DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes->at(volumeIndex));
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
                    resources.constantsSTB = ddgiResources.constantsSTB;

                    // Reset the command list before initialization
                    CHECK(ResetCmdList(vk), "reset command list!", log);

                    if (!CreatePipelineLayout(vk, vkResources, resources, log)) return false;
                    if (!LoadAndCompileShaders(vk, resources, log)) return false;
                    if (!CreateDescriptorSets(vk, vkResources, resources, log)) return false;
                    if (!CreatePipelines(vk, resources, log)) return false;
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
                    resources.constantsSTB = ddgiResources.constantsSTB;

                    log << "Reloading DDGI Visualization shaders...";
                    if (!LoadAndCompileShaders(vk, resources, log)) return false;
                    if (!CreatePipelines(vk, resources, log)) return false;
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
                        Configs::DDGIVolume volume = config.ddgi.volumes[config.ddgi.selectedVolume];

                        if (resources.flags & VIS_FLAG_SHOW_PROBES)
                        {
                            // Update constants
                            resources.constants.probeType = volume.probeType;
                            resources.constants.probeRadius = volume.probeRadius;
                            resources.constants.probeAlpha = volume.probeAlpha;
                            resources.constants.distanceDivisor = volume.probeDistanceDivisor;

                            // Update the TLAS instances and rebuild
                            UpdateTLAS(vk, resources, config);
                        }

                        if (resources.flags & VIS_FLAG_SHOW_TEXTURES)
                        {
                            // Update constants
                            resources.constants.volumeIndex = config.ddgi.selectedVolume;
                            resources.constants.distanceDivisor = volume.probeDistanceDivisor;

                            resources.constants.rayDataTextureScale = volume.probeRayDataScale;
                            resources.constants.irradianceTextureScale = volume.probeIrradianceScale;
                            resources.constants.distanceTextureScale = volume.probeDistanceScale;
                            resources.constants.relocationOffsetTextureScale = volume.probeRelocationOffsetScale;
                            resources.constants.classificationStateTextureScale = volume.probeClassificationStateScale;
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

                                // Set the push constants
                                DDGIVisConstants consts = resources.constants;
                                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], resources.pipelineLayout, VK_SHADER_STAGE_ALL, 0, DDGIConstants::GetSizeInBytes(), consts.GetData());

                                // Bind the pipeline
                                vkCmdBindPipeline(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, resources.rtPipeline);

                                // Bind the descriptor set
                                vkCmdBindDescriptorSets(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, resources.pipelineLayout, 0, 1, &resources.descriptorSet, 0, nullptr);

                                // Describe the shader table
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

                                // Dispatch rays
                                GPU_TIMESTAMP_BEGIN(resources.gpuProbeStat->GetQueryBeginIndex());
                                vkCmdTraceRaysKHR(
                                    vk.cmdBuffer[vk.frameIndex],
                                    &raygenRegion,
                                    &missRegion,
                                    &hitRegion,
                                    &callableRegion,
                                    vk.width,
                                    vk.height,
                                    1);
                                GPU_TIMESTAMP_END(resources.gpuProbeStat->GetQueryEndIndex());

                                // Wait for the ray trace to finish
                                VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                                ImageBarrierDesc barrier = { VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
                                SetImageMemoryBarrier(vk.cmdBuffer[vk.frameIndex], vkResources.rt.GBufferA, barrier);
                                SetImageMemoryBarrier(vk.cmdBuffer[vk.frameIndex], vkResources.rt.GBufferB, barrier);

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

                            // Set the push constants
                            DDGIVisConstants consts = resources.constants;
                            vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], resources.pipelineLayout, VK_SHADER_STAGE_ALL, 0, DDGIVisConstants::GetSizeInBytes(), consts.GetData());

                            // Bind the pipeline
                            vkCmdBindPipeline(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_COMPUTE, resources.textureVisPipeline);

                            // Bind the descriptor set
                            vkCmdBindDescriptorSets(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_COMPUTE, resources.pipelineLayout, 0, 1, &resources.descriptorSet, 0, nullptr);

                            // Dispatch threads
                            uint32_t groupsX = DivRoundUp(vk.width, 8);
                            uint32_t groupsY = DivRoundUp(vk.height, 4);

                            GPU_TIMESTAMP_BEGIN(resources.gpuTextureStat->GetQueryBeginIndex());
                            vkCmdDispatch(vk.cmdBuffer[vk.frameIndex], groupsX, groupsY, 1);
                            GPU_TIMESTAMP_END(resources.gpuTextureStat->GetQueryEndIndex());

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
                    resources.textureVisCS.Release();
                    resources.updateTlasCS.Release();

                    // Shader Modules
                    resources.rtShadersModule.Release(device);
                    vkDestroyShaderModule(device, resources.textureVisModule, nullptr);
                    vkDestroyShaderModule(device, resources.updateTlasModule, nullptr);

                    // Pipelines
                    vkDestroyPipeline(device, resources.rtPipeline, nullptr);
                    vkDestroyPipeline(device, resources.textureVisPipeline, nullptr);
                    vkDestroyPipeline(device, resources.updateTlasPipeline, nullptr);

                    // Descriptor Set Layout and Pipeline Layout
                    vkDestroyDescriptorSetLayout(device, resources.descriptorSetLayout, nullptr);
                    vkDestroyPipelineLayout(device, resources.pipelineLayout, nullptr);

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

        bool Initialize(Globals& vk, GlobalResources& vkResources, DDGI::Resources& ddgiResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
        {
            return Graphics::Vulkan::DDGI::Visualizations::Initialize(vk, vkResources, ddgiResources, resources, perf, log);
        }

        bool Reload(Globals& vk, GlobalResources& vkResources, DDGI::Resources& ddgiResources, Resources& resources, std::ofstream& log)
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
