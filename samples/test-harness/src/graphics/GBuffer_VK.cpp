/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "graphics/GBuffer.h"

namespace Graphics
{
    namespace Vulkan
    {
        namespace GBuffer
        {

            //----------------------------------------------------------------------------------------------------------
            // Private Functions
            //----------------------------------------------------------------------------------------------------------

            bool LoadAndCompileShaders(Globals& vk, Resources& resources, std::ofstream& log)
            {
                // Release existing shaders
                resources.shaders.Release();

                std::wstring root = std::wstring(vk.shaderCompiler.root.begin(), vk.shaderCompiler.root.end());

                // Load and compile the ray generation shader
                std::wstring shaderPath = root + L"shaders/GBufferRGS.hlsl";
                resources.shaders.rgs.filepath = shaderPath.c_str();
                resources.shaders.rgs.entryPoint = L"RayGen";
                resources.shaders.rgs.exportName = L"GBufferRGS";
                resources.shaders.rgs.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };
                CHECK(Shaders::Compile(vk.shaderCompiler, resources.shaders.rgs, true), "compile GBuffer ray generation shader!\n", log);

                // Load and compile the miss shader
                shaderPath = root + L"shaders/Miss.hlsl";
                resources.shaders.miss.filepath = shaderPath.c_str();
                resources.shaders.miss.entryPoint = L"Miss";
                resources.shaders.miss.exportName = L"GBufferMiss";
                resources.shaders.miss.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };
                CHECK(Shaders::Compile(vk.shaderCompiler, resources.shaders.miss, true), "compile GBuffer miss shader!\n", log);

                // Add the hit group
                resources.shaders.hitGroups.emplace_back();

                Shaders::ShaderRTHitGroup& group = resources.shaders.hitGroups[0];
                group.exportName = L"GBufferHitGroup";

                // Load and compile the CHS
                shaderPath = root + L"shaders/CHS.hlsl";
                group.chs.filepath = shaderPath.c_str();
                group.chs.entryPoint = L"CHS_PRIMARY";
                group.chs.exportName = L"GBufferCHS";
                group.chs.targetProfile = L"lib_6_4";
                group.chs.arguments = { L"-spirv", L"-T lib_6_4", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };
                CHECK(Shaders::Compile(vk.shaderCompiler, group.chs, true), "compile GBuffer closest hit shader!\n", log);

                // Load and compile the AHS
                shaderPath = root + L"shaders/AHS.hlsl";
                group.ahs.filepath = shaderPath.c_str();
                group.ahs.entryPoint = L"AHS_PRIMARY";
                group.ahs.exportName = L"GBufferAHS";
                group.ahs.targetProfile = L"lib_6_4";
                group.ahs.arguments = { L"-spirv", L"-T lib_6_4", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };
                CHECK(Shaders::Compile(vk.shaderCompiler, group.ahs, true), "compile GBuffer any hit shader!\n", log);

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
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.descriptorSet), "GBuffer Descriptor Set", VK_OBJECT_TYPE_DESCRIPTOR_SET);
            #endif

                return true;
            }

            bool CreatePipelines(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                // Release existing shader modules and pipeline
                resources.modules.Release(vk.device);
                vkDestroyPipeline(vk.device, resources.pipeline, nullptr);

                // Create the pipeline shader modules
                CHECK(CreateRayTracingShaderModules(vk.device, resources.shaders, resources.modules), "create GBuffer shader modules!\n", log);

                // Create the pipeline
                CHECK(CreateRayTracingPipeline(
                    vk.device,
                    vkResources.pipelineLayout,
                    resources.shaders, resources.modules,
                    &resources.pipeline), "create GBuffer pipeline!\n", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.pipeline), "GBuffer Pipeline", VK_OBJECT_TYPE_PIPELINE);
            #endif
                return true;
            }

            bool CreateShaderTable(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                // The Shader Table layout is as follows:
                //    Entry 0:  GBuffer Ray Generation Shader
                //    Entry 1:  GBuffer Trace Miss Shader
                //    Entry 2+: GBuffer Trace HitGroups
                // All shader records in the Shader Table must have the same size, so shader record size will be based on the largest required entry.
                // The entries must be aligned up to 64 bytes (when there is local data stored in the shader record).
                //   32 bytes for the shader identifier
                // = 32 bytes ->> aligns to 64 bytes

                // Release existing shader table and memory
                resources.shaderTableSize = 0;
                resources.shaderTableRecordSize = 0;
                vkDestroyBuffer(vk.device, resources.shaderTableUpload, nullptr);
                vkFreeMemory(vk.device, resources.shaderTableUploadMemory, nullptr);
                vkDestroyBuffer(vk.device, resources.shaderTable, nullptr);
                vkFreeMemory(vk.device, resources.shaderTableMemory, nullptr);

                uint32_t shaderGroupIdSize = vk.deviceRTPipelineProps.shaderGroupHandleSize;

                // Configure the shader record size (no shader record data)
                resources.shaderTableRecordSize += shaderGroupIdSize;
                resources.shaderTableRecordSize = ALIGN(vk.deviceRTPipelineProps.shaderGroupBaseAlignment, resources.shaderTableRecordSize);

                // 2 + numHitGroups shader records in the table
                resources.shaderTableSize = resources.modules.numGroups * resources.shaderTableRecordSize;
                resources.shaderTableSize = ALIGN(vk.deviceRTPipelineProps.shaderGroupBaseAlignment, resources.shaderTableSize);

                // Create the shader table upload buffer resource and memory
                BufferDesc desc = { resources.shaderTableSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
                CHECK(CreateBuffer(vk, desc, &resources.shaderTableUpload, &resources.shaderTableUploadMemory), "create GBuffer shader table upload resources!", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTableUpload), "GBuffer ShaderTableUpload", VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTableUploadMemory), "GBuffer ShaderTableUploadMemory", VK_OBJECT_TYPE_DEVICE_MEMORY);
            #endif

                // Create the shader table device buffer resource and memory
                desc = { resources.shaderTableSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
                CHECK(CreateBuffer(vk, desc, &resources.shaderTable, &resources.shaderTableMemory), "create GBuffer shader table resources!", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTable), "GBuffer ShaderTable", VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTableMemory), "GBuffer ShaderTableMemory", VK_OBJECT_TYPE_DEVICE_MEMORY);
            #endif

                return true;
            }

            bool UpdateShaderTable(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                uint32_t shaderGroupIdSize = vk.deviceRTPipelineProps.shaderGroupHandleSize;

                // Get the shader group IDs from the pipeline
                std::vector<uint8_t> shaderGroupIdBuffer(shaderGroupIdSize * resources.modules.numGroups);
                VKCHECK(vkGetRayTracingShaderGroupHandlesKHR(vk.device, resources.pipeline, 0, resources.modules.numGroups, (shaderGroupIdSize * resources.modules.numGroups), shaderGroupIdBuffer.data()));

                // Separate the shader group IDs into an array
                std::vector<uint8_t*> shaderGroupIds(resources.modules.numGroups);
                for (uint32_t i = 0; i < resources.modules.numGroups; ++i)
                {
                    shaderGroupIds[i] = shaderGroupIdBuffer.data() + i * shaderGroupIdSize;
                }

                // Write the shader table records to the upload buffer
                uint8_t* pData = nullptr;
                VKCHECK(vkMapMemory(vk.device, resources.shaderTableUploadMemory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(&pData)));

                uint32_t groupIndex = 0;

                // Entry 0: Ray Generation Shader
                memcpy(pData, shaderGroupIds[groupIndex++], shaderGroupIdSize);
                resources.shaderTableRGSStartAddress = GetBufferDeviceAddress(vk.device, resources.shaderTable);

                // Entry 1: Miss Shader
                pData += resources.shaderTableRecordSize;
                memcpy(pData, shaderGroupIds[groupIndex++], shaderGroupIdSize);
                resources.shaderTableMissTableStartAddress = resources.shaderTableRGSStartAddress + resources.shaderTableRecordSize;
                resources.shaderTableMissTableSize = resources.shaderTableRecordSize;

                // Entries 2+: Hit Groups
                for (uint32_t hitGroupIndex = 0; hitGroupIndex < static_cast<uint32_t>(resources.shaders.hitGroups.size()); hitGroupIndex++)
                {
                    pData += resources.shaderTableRecordSize;
                    memcpy(pData, shaderGroupIds[groupIndex++], shaderGroupIdSize);
                }
                resources.shaderTableHitGroupTableStartAddress = resources.shaderTableMissTableStartAddress + resources.shaderTableMissTableSize;
                resources.shaderTableHitGroupTableSize = static_cast<uint32_t>(resources.shaders.hitGroups.size()) * resources.shaderTableRecordSize;

                // Unmap
                vkUnmapMemory(vk.device, resources.shaderTableUploadMemory);

                // Schedule a copy of the shader table from the upload buffer to the device buffer
                VkBufferCopy bufferCopy = {};
                bufferCopy.size = resources.shaderTableSize;
                vkCmdCopyBuffer(vk.cmdBuffer[vk.frameIndex], resources.shaderTableUpload, resources.shaderTable, 1, &bufferCopy);

                return true;
            }

            bool UpdateDescriptorSets(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                // Store the data to be written to the descriptor set
                std::vector<VkWriteDescriptorSet> writeDescriptorSets;

                // Samplers
                VkDescriptorImageInfo samplersInfo[] =
                {
                    { vkResources.samplers[0], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED },  // bilinear wrap sampler
                    { vkResources.samplers[1], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED },  // point clamp sampler
                    { vkResources.samplers[2], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED }   // aniso wrap sampler
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
                cameraCBSet.dstBinding = DescriptorLayoutBindings::CB_CAMERA;
                cameraCBSet.dstArrayElement = 0;
                cameraCBSet.descriptorCount = 1;
                cameraCBSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                cameraCBSet.pBufferInfo = &cameraCBInfo;

                writeDescriptorSets.push_back(cameraCBSet);

                // Lights structured buffer
                VkDescriptorBufferInfo lightsSTBInfo = { vkResources.lightsSTB, 0, VK_WHOLE_SIZE };

                VkWriteDescriptorSet lightsSTBSet = {};
                lightsSTBSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                lightsSTBSet.dstSet = resources.descriptorSet;
                lightsSTBSet.dstBinding = DescriptorLayoutBindings::STB_LIGHTS;
                lightsSTBSet.dstArrayElement = 0;
                lightsSTBSet.descriptorCount = 1;
                lightsSTBSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                lightsSTBSet.pBufferInfo = &lightsSTBInfo;

                writeDescriptorSets.push_back(lightsSTBSet);

                // Materials structured buffer
                VkDescriptorBufferInfo materialsSTBInfo = { vkResources.materialsSTB, 0, VK_WHOLE_SIZE };

                VkWriteDescriptorSet materialsSTBSet = {};
                materialsSTBSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                materialsSTBSet.dstSet = resources.descriptorSet;
                materialsSTBSet.dstBinding = DescriptorLayoutBindings::STB_MATERIALS;
                materialsSTBSet.dstArrayElement = 0;
                materialsSTBSet.descriptorCount = 1;
                materialsSTBSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                materialsSTBSet.pBufferInfo = &materialsSTBInfo;

                writeDescriptorSets.push_back(materialsSTBSet);

                // Instances structured buffer
                VkDescriptorBufferInfo instancesSTBInfo = { vkResources.tlas.instances, 0, VK_WHOLE_SIZE };

                VkWriteDescriptorSet instancesSTBSet = {};
                instancesSTBSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                instancesSTBSet.dstSet = resources.descriptorSet;
                instancesSTBSet.dstBinding = DescriptorLayoutBindings::STB_INSTANCES;
                instancesSTBSet.dstArrayElement = 0;
                instancesSTBSet.descriptorCount = 1;
                instancesSTBSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                instancesSTBSet.pBufferInfo = &instancesSTBInfo;

                writeDescriptorSets.push_back(instancesSTBSet);

                // RWTex2D UAVs
                // GBufferA, GBufferB, GBufferC, GBufferD storage images
                VkDescriptorImageInfo ptImageInfo[] =
                {
                    { VK_NULL_HANDLE, vkResources.rt.GBufferAView, VK_IMAGE_LAYOUT_GENERAL },
                    { VK_NULL_HANDLE, vkResources.rt.GBufferBView, VK_IMAGE_LAYOUT_GENERAL },
                    { VK_NULL_HANDLE, vkResources.rt.GBufferCView, VK_IMAGE_LAYOUT_GENERAL },
                    { VK_NULL_HANDLE, vkResources.rt.GBufferDView, VK_IMAGE_LAYOUT_GENERAL }
                };

                VkWriteDescriptorSet rwTex2DSet = {};
                rwTex2DSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                rwTex2DSet.dstSet = resources.descriptorSet;
                rwTex2DSet.dstBinding = DescriptorLayoutBindings::UAV_START;
                rwTex2DSet.dstArrayElement = RWTex2DIndices::GBUFFERA;
                rwTex2DSet.descriptorCount = _countof(ptImageInfo);
                rwTex2DSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                rwTex2DSet.pImageInfo = ptImageInfo;

                writeDescriptorSets.push_back(rwTex2DSet);

                // Ray Tracing TLAS
                VkWriteDescriptorSetAccelerationStructureKHR tlasInfo = {};
                tlasInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                tlasInfo.accelerationStructureCount = 1;
                tlasInfo.pAccelerationStructures = &vkResources.tlas.asKHR;

                VkWriteDescriptorSet tlasSet = {};
                tlasSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                tlasSet.pNext = &tlasInfo;
                tlasSet.dstSet = resources.descriptorSet;
                tlasSet.dstBinding = DescriptorLayoutBindings::BVH_START;
                tlasSet.dstArrayElement = 0;
                tlasSet.descriptorCount = 1;
                tlasSet.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

                writeDescriptorSets.push_back(tlasSet);

                // Tex2D SRVs (default textures)
                VkDescriptorImageInfo tex2DInfo[] =
                {
                    { VK_NULL_HANDLE, vkResources.textureViews[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, // blue noise texture
                };

                VkWriteDescriptorSet tex2DSet = {};
                tex2DSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                tex2DSet.dstSet = resources.descriptorSet;
                tex2DSet.dstBinding = DescriptorLayoutBindings::SRV_START;
                tex2DSet.dstArrayElement = 0;
                tex2DSet.descriptorCount = _countof(tex2DInfo);
                tex2DSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                tex2DSet.pImageInfo = tex2DInfo;

                writeDescriptorSets.push_back(tex2DSet);

                // Tex2D SRVs (scene textures)
                std::vector<VkDescriptorImageInfo> sceneTexturesInfo;
                uint32_t numSceneTextures = static_cast<uint32_t>(vkResources.sceneTextureViews.size());
                if (numSceneTextures > 0)
                {
                    // Gather the scene textures
                    for (uint32_t textureIndex = 0; textureIndex < numSceneTextures; textureIndex++)
                    {
                        sceneTexturesInfo.push_back({ VK_NULL_HANDLE, vkResources.sceneTextureViews[textureIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
                    }

                    // Describe the scene textures
                    VkWriteDescriptorSet texturesSet = {};
                    texturesSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    texturesSet.dstSet = resources.descriptorSet;
                    texturesSet.dstBinding = DescriptorLayoutBindings::SRV_START;
                    texturesSet.dstArrayElement = Tex2DIndices::SCENE_TEXTURES;
                    texturesSet.descriptorCount = numSceneTextures;
                    texturesSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                    texturesSet.pImageInfo = sceneTexturesInfo.data();

                    writeDescriptorSets.push_back(texturesSet);
                }

                // ByteAddress SRVs (material indices, index / vertex buffers)
                std::vector<VkDescriptorBufferInfo> rawBuffersInfo;
                rawBuffersInfo.push_back({ vkResources.materialIndicesRB, 0, VK_WHOLE_SIZE });
                for (uint32_t bufferIndex = 0; bufferIndex < static_cast<uint32_t>(vkResources.sceneIBs.size()); bufferIndex++)
                {
                    rawBuffersInfo.push_back({ vkResources.sceneIBs[bufferIndex], 0, VK_WHOLE_SIZE });
                    rawBuffersInfo.push_back({ vkResources.sceneVBs[bufferIndex], 0, VK_WHOLE_SIZE });
                }

                VkWriteDescriptorSet rawBuffersSet = {};
                rawBuffersSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                rawBuffersSet.dstSet = resources.descriptorSet;
                rawBuffersSet.dstBinding = DescriptorLayoutBindings::RAW_SRV_START;
                rawBuffersSet.dstArrayElement = ByteAddressIndices::MATERIAL_INDICES;
                rawBuffersSet.descriptorCount = static_cast<uint32_t>(rawBuffersInfo.size());
                rawBuffersSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                rawBuffersSet.pBufferInfo = rawBuffersInfo.data();

                writeDescriptorSets.push_back(rawBuffersSet);

                // Update the descriptor set
                vkUpdateDescriptorSets(vk.device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

                return true;
            }

            //----------------------------------------------------------------------------------------------------------
            // Public Functions
            //----------------------------------------------------------------------------------------------------------

            /**
             * Create resources, shaders, and PSOs.
             */
            bool Initialize(Globals& vk, GlobalResources& vkResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
            {
                // Reset the command list before initialization
                CHECK(ResetCmdList(vk), "reset command list!", log);

                if (!LoadAndCompileShaders(vk, resources, log)) return false;
                if (!CreateDescriptorSets(vk, vkResources, resources, log)) return false;
                if (!CreatePipelines(vk, vkResources, resources, log)) return false;
                if (!CreateShaderTable(vk, vkResources, resources, log)) return false;

                if (!UpdateShaderTable(vk, vkResources, resources, log)) return false;
                if (!UpdateDescriptorSets(vk, vkResources, resources, log)) return false;

                // Execute GPU work to finish initialization
                VKCHECK(vkEndCommandBuffer(vk.cmdBuffer[vk.frameIndex]));

                VkSubmitInfo submitInfo = {};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &vk.cmdBuffer[vk.frameIndex];

                VKCHECK(vkQueueSubmit(vk.queue, 1, &submitInfo, VK_NULL_HANDLE));
                VKCHECK(vkQueueWaitIdle(vk.queue));

                WaitForGPU(vk);

                perf.AddStat("GBuffer", resources.cpuStat, resources.gpuStat);

                return true;
            }

            /**
             * Reload and compile shaders, recreate the pipeline, and recreate the shader table.
             */
            bool Reload(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                log << "Reloading GBuffer shaders...";
                if (!LoadAndCompileShaders(vk, resources, log)) return false;
                if (!CreatePipelines(vk, vkResources, resources, log)) return false;
                if (!UpdateShaderTable(vk, vkResources, resources, log)) return false;
                log << "done.\n";
                log << std::flush;

                return true;
            }

            /**
             * Resize, update descriptor sets. GBuffer textures are resized in Vulkan.cpp
             */
            bool Resize(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                log << "Updating GBuffer descriptor sets...";
                if (!UpdateDescriptorSets(vk, vkResources, resources, log)) return false;
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

                // Path Trace constants
                vkResources.constants.pt.rayNormalBias = config.pathTrace.rayNormalBias;
                vkResources.constants.pt.rayViewBias = config.pathTrace.rayViewBias;

                CPU_TIMESTAMP_END(resources.cpuStat);
            }

            /**
             * Record the workload to the global command list.
             */
            void Execute(Globals& vk, GlobalResources& vkResources, Resources& resources)
            {
            #ifdef GFX_PERF_MARKERS
                AddPerfMarker(vk, GFX_PERF_MARKER_ORANGE, "GBuffer");
            #endif
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);

                // Bind the pipeline
                vkCmdBindPipeline(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, resources.pipeline);

                // Bind the descriptor set
                vkCmdBindDescriptorSets(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vkResources.pipelineLayout, 0, 1, &resources.descriptorSet, 0, nullptr);

                // Set the push constants
                uint32_t offset = 0;
                GlobalConstants consts = vkResources.constants;
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, consts.app.GetSizeInBytes(), consts.app.GetData());
                offset += consts.app.GetAlignedSizeInBytes();
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, consts.pt.GetSizeInBytes(), consts.pt.GetData());
                offset += consts.pt.GetAlignedSizeInBytes();
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, consts.lights.GetSizeInBytes(), consts.lights.GetData());

                // Describe the shader table
                VkStridedDeviceAddressRegionKHR raygenRegion = {};
                raygenRegion.deviceAddress = resources.shaderTableRGSStartAddress;
                raygenRegion.size = resources.shaderTableRecordSize;
                raygenRegion.stride = resources.shaderTableRecordSize;

                VkStridedDeviceAddressRegionKHR missRegion = {};
                missRegion.deviceAddress = resources.shaderTableMissTableStartAddress;
                missRegion.size = resources.shaderTableRecordSize;
                missRegion.stride = resources.shaderTableRecordSize;

                VkStridedDeviceAddressRegionKHR hitRegion = {};
                hitRegion.deviceAddress = resources.shaderTableHitGroupTableStartAddress;
                hitRegion.size = resources.shaderTableRecordSize;
                hitRegion.stride = resources.shaderTableHitGroupTableSize;

                VkStridedDeviceAddressRegionKHR callableRegion = {};

                // Dispatch rays
                GPU_TIMESTAMP_BEGIN(resources.gpuStat->GetQueryBeginIndex());
                vkCmdTraceRaysKHR(vk.cmdBuffer[vk.frameIndex], &raygenRegion, &missRegion, &hitRegion, &callableRegion, vk.width, vk.height, 1);
                GPU_TIMESTAMP_END(resources.gpuStat->GetQueryEndIndex());

                // Wait for the ray trace to complete
                VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                ImageBarrierDesc barrier = { VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
                SetImageMemoryBarrier(vk.cmdBuffer[vk.frameIndex], vkResources.rt.GBufferA, barrier);
                SetImageMemoryBarrier(vk.cmdBuffer[vk.frameIndex], vkResources.rt.GBufferB, barrier);
                SetImageMemoryBarrier(vk.cmdBuffer[vk.frameIndex], vkResources.rt.GBufferC, barrier);
                SetImageMemoryBarrier(vk.cmdBuffer[vk.frameIndex], vkResources.rt.GBufferD, barrier);

            #ifdef GFX_PERF_MARKERS
                vkCmdEndDebugUtilsLabelEXT(vk.cmdBuffer[vk.frameIndex]);
            #endif

                CPU_TIMESTAMP_ENDANDRESOLVE(resources.cpuStat);
            }

            /**
             * Release resources.
             */
            void Cleanup(VkDevice& device, Resources& resources)
            {
                // Shader Table
                vkDestroyBuffer(device, resources.shaderTableUpload, nullptr);
                vkFreeMemory(device, resources.shaderTableUploadMemory, nullptr);
                vkDestroyBuffer(device, resources.shaderTable, nullptr);
                vkFreeMemory(device, resources.shaderTableMemory, nullptr);

                // Pipeline
                vkDestroyPipeline(device, resources.pipeline, nullptr);

                // Shaders
                resources.modules.Release(device);
                resources.shaders.Release();

                resources.shaderTableRecordSize = 0;
                resources.shaderTableMissTableSize = 0;
                resources.shaderTableHitGroupTableSize = 0;
            }

            /**
             * Write the GBuffer texture resources to disk.
             */
            bool WriteGBufferToDisk(Globals& vk, GlobalResources& vkResources, std::string directory)
            {
            #if (defined(_WIN32) || defined(WIN32))
                CoInitialize(NULL);
            #endif
                // Formats should match those from Graphics::Vulkan::CreateRenderTargets() in Vulkan.cpp
                bool success = WriteResourceToDisk(vk, directory + "/GBufferA.png", vkResources.rt.GBufferA, vk.width, vk.height, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_GENERAL);
                success &= WriteResourceToDisk(vk, directory + "/GBufferB.png", vkResources.rt.GBufferB, vk.width, vk.height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL);
                success &= WriteResourceToDisk(vk, directory + "/GBufferC.png", vkResources.rt.GBufferC, vk.width, vk.height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL);
                success &= WriteResourceToDisk(vk, directory + "/GBufferD.png", vkResources.rt.GBufferD, vk.width, vk.height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL);
                return success;
            }

        } // namespace Graphics::Vulkan::GBuffer

    } // namespace Graphics::Vulkan

    namespace GBuffer
    {

        bool Initialize(Globals& vk, GlobalResources& vkResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
        {
            return Graphics::Vulkan::GBuffer::Initialize(vk, vkResources, resources, perf, log);
        }

        bool Reload(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::Vulkan::GBuffer::Reload(vk, vkResources, resources, log);
        }

        bool Resize(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::Vulkan::GBuffer::Resize(vk, vkResources, resources, log);
        }

        void Update(Globals& vk, GlobalResources& vkResources, Resources& resources, const Configs::Config& config)
        {
            return Graphics::Vulkan::GBuffer::Update(vk, vkResources, resources, config);
        }

        void Execute(Globals& vk, GlobalResources& vkResources, Resources& resources)
        {
            return Graphics::Vulkan::GBuffer::Execute(vk, vkResources, resources);
        }

        void Cleanup(Globals& vk, Resources& resources)
        {
            Graphics::Vulkan::GBuffer::Cleanup(vk.device, resources);
        }

        bool WriteGBufferToDisk(Globals& vk, GlobalResources& vkResources, std::string directory)
        {
            return Graphics::Vulkan::GBuffer::WriteGBufferToDisk(vk, vkResources, directory);
        }

    } // namespace Graphics::GBuffer
}
