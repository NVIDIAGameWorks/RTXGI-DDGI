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
                resources.shaders.rgs.filepath = root + L"shaders/GBufferRGS.hlsl";
                resources.shaders.rgs.entryPoint = L"RayGen";
                resources.shaders.rgs.exportName = L"GBufferRGS";
                resources.shaders.rgs.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                Shaders::AddDefine(resources.shaders.rgs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                CHECK(Shaders::Compile(vk.shaderCompiler, resources.shaders.rgs, true), "compile GBuffer ray generation shader!\n", log);

                // Load and compile the miss shader
                resources.shaders.miss.filepath = root + L"shaders/Miss.hlsl";
                resources.shaders.miss.entryPoint = L"Miss";
                resources.shaders.miss.exportName = L"GBufferMiss";
                resources.shaders.miss.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                Shaders::AddDefine(resources.shaders.miss, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                CHECK(Shaders::Compile(vk.shaderCompiler, resources.shaders.miss, true), "compile GBuffer miss shader!\n", log);

                // Add the hit group
                resources.shaders.hitGroups.emplace_back();

                Shaders::ShaderRTHitGroup& group = resources.shaders.hitGroups[0];
                group.exportName = L"GBufferHitGroup";

                // Load and compile the CHS
                group.chs.filepath = root + L"shaders/CHS.hlsl";
                group.chs.entryPoint = L"CHS_PRIMARY";
                group.chs.exportName = L"GBufferCHS";
                group.chs.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                Shaders::AddDefine(group.chs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                CHECK(Shaders::Compile(vk.shaderCompiler, group.chs, true), "compile GBuffer closest hit shader!\n", log);

                // Load and compile the AHS
                group.ahs.filepath = root + L"shaders/AHS.hlsl";
                group.ahs.entryPoint = L"AHS_PRIMARY";
                group.ahs.exportName = L"GBufferAHS";
                group.ahs.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                Shaders::AddDefine(group.ahs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
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
                VkWriteDescriptorSet* descriptor = nullptr;
                std::vector<VkWriteDescriptorSet> descriptors;

                // 0: Samplers
                VkDescriptorImageInfo samplers[] = { vkResources.samplers[SamplerIndices::ANISO_WRAP], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED };

                descriptor = &descriptors.emplace_back();
                descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor->dstSet = resources.descriptorSet;
                descriptor->dstBinding = DescriptorLayoutBindings::SAMPLERS;
                descriptor->dstArrayElement = SamplerIndices::ANISO_WRAP;
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

                // 2: Lights StructuredBuffer
                VkDescriptorBufferInfo lights = { vkResources.lightsSTB, 0, VK_WHOLE_SIZE };

                descriptor = &descriptors.emplace_back();
                descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor->dstSet = resources.descriptorSet;
                descriptor->dstBinding = DescriptorLayoutBindings::STB_LIGHTS;
                descriptor->dstArrayElement = 0;
                descriptor->descriptorCount = 1;
                descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                descriptor->pBufferInfo = &lights;

                // 3: Materials StructuredBuffer
                VkDescriptorBufferInfo materials = { vkResources.materialsSTB, 0, VK_WHOLE_SIZE };

                descriptor = &descriptors.emplace_back();
                descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor->dstSet = resources.descriptorSet;
                descriptor->dstBinding = DescriptorLayoutBindings::STB_MATERIALS;
                descriptor->dstArrayElement = 0;
                descriptor->descriptorCount = 1;
                descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                descriptor->pBufferInfo = &materials;

                // 4: Scene TLAS Instances StructuredBuffer
                VkDescriptorBufferInfo instances = { vkResources.tlas.instances, 0, VK_WHOLE_SIZE };

                descriptor = &descriptors.emplace_back();
                descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor->dstSet = resources.descriptorSet;
                descriptor->dstBinding = DescriptorLayoutBindings::STB_TLAS_INSTANCES;
                descriptor->dstArrayElement = 0;
                descriptor->descriptorCount = 1;
                descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                descriptor->pBufferInfo = &instances;

                // 8: Texture2D UAVs
                VkDescriptorImageInfo rwTex2D[] =
                {
                    { VK_NULL_HANDLE, vkResources.rt.GBufferAView, VK_IMAGE_LAYOUT_GENERAL },
                    { VK_NULL_HANDLE, vkResources.rt.GBufferBView, VK_IMAGE_LAYOUT_GENERAL },
                    { VK_NULL_HANDLE, vkResources.rt.GBufferCView, VK_IMAGE_LAYOUT_GENERAL },
                    { VK_NULL_HANDLE, vkResources.rt.GBufferDView, VK_IMAGE_LAYOUT_GENERAL }
                };

                descriptor = &descriptors.emplace_back();
                descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor->dstSet = resources.descriptorSet;
                descriptor->dstBinding = DescriptorLayoutBindings::UAV_TEX2D;
                descriptor->dstArrayElement = RWTex2DIndices::GBUFFERA;
                descriptor->descriptorCount = _countof(rwTex2D);
                descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                descriptor->pImageInfo = rwTex2D;

                // 10: Scene TLAS
                VkWriteDescriptorSetAccelerationStructureKHR sceneTLAS = {};
                sceneTLAS.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
                sceneTLAS.accelerationStructureCount = 1;
                sceneTLAS.pAccelerationStructures = &vkResources.tlas.asKHR;

                descriptor = &descriptors.emplace_back();
                descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor->dstSet = resources.descriptorSet;
                descriptor->dstBinding = DescriptorLayoutBindings::SRV_TLAS;
                descriptor->dstArrayElement = TLASIndices::SCENE;
                descriptor->descriptorCount = 1;
                descriptor->descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
                descriptor->pNext = &sceneTLAS;

                // 11: Texture2D SRVs
                std::vector<VkDescriptorImageInfo> tex2D;
                tex2D.push_back({ VK_NULL_HANDLE, vkResources.textureViews[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }); // blue noise texture
                tex2D.push_back({ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }); // imgui font texture

                // Scene textures
                uint32_t numSceneTextures = static_cast<uint32_t>(vkResources.sceneTextureViews.size());
                if (numSceneTextures > 0)
                {
                    for (uint32_t textureIndex = 0; textureIndex < numSceneTextures; textureIndex++)
                    {
                        tex2D.push_back({ VK_NULL_HANDLE, vkResources.sceneTextureViews[textureIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
                    }
                }

                descriptor = &descriptors.emplace_back();
                descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor->dstSet = resources.descriptorSet;
                descriptor->dstBinding = DescriptorLayoutBindings::SRV_TEX2D;
                descriptor->dstArrayElement = Tex2DIndices::BLUE_NOISE;
                descriptor->descriptorCount = static_cast<uint32_t>(tex2D.size());
                descriptor->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                descriptor->pImageInfo = tex2D.data();

                // 13: ByteAddressBuffer SRVs (mesh offsets, geometry data, index & vertex buffers)
                std::vector<VkDescriptorBufferInfo> byteAddressBuffers;
                byteAddressBuffers.push_back({ vkResources.meshOffsetsRB, 0, VK_WHOLE_SIZE }); // mesh offsets
                byteAddressBuffers.push_back({ vkResources.geometryDataRB, 0, VK_WHOLE_SIZE }); // geometry data

                // Scene index and vertex buffers
                for (uint32_t bufferIndex = 0; bufferIndex < static_cast<uint32_t>(vkResources.sceneIBs.size()); bufferIndex++)
                {
                    byteAddressBuffers.push_back({ vkResources.sceneIBs[bufferIndex], 0, VK_WHOLE_SIZE });
                    byteAddressBuffers.push_back({ vkResources.sceneVBs[bufferIndex], 0, VK_WHOLE_SIZE });
                }

                descriptor = &descriptors.emplace_back();
                descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor->dstSet = resources.descriptorSet;
                descriptor->dstBinding = DescriptorLayoutBindings::SRV_BYTEADDRESS;
                descriptor->dstArrayElement = ByteAddressIndices::MATERIAL_INDICES;
                descriptor->descriptorCount = static_cast<uint32_t>(byteAddressBuffers.size());
                descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                descriptor->pBufferInfo = byteAddressBuffers.data();

                // Update the descriptor set
                vkUpdateDescriptorSets(vk.device, static_cast<uint32_t>(descriptors.size()), descriptors.data(), 0, nullptr);

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

                // Set the push constants
                uint32_t offset = 0;
                GlobalConstants consts = vkResources.constants;
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, consts.app.GetSizeInBytes(), consts.app.GetData());
                offset += AppConsts::GetAlignedSizeInBytes();
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, consts.pt.GetSizeInBytes(), consts.pt.GetData());
                offset += PathTraceConsts::GetAlignedSizeInBytes();
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, consts.lights.GetSizeInBytes(), consts.lights.GetData());

                // Bind the pipeline
                vkCmdBindPipeline(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, resources.pipeline);

                // Bind the descriptor set
                vkCmdBindDescriptorSets(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vkResources.pipelineLayout, 0, 1, &resources.descriptorSet, 0, nullptr);

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
                GPU_TIMESTAMP_BEGIN(resources.gpuStat->GetGPUQueryBeginIndex());
                vkCmdTraceRaysKHR(vk.cmdBuffer[vk.frameIndex], &raygenRegion, &missRegion, &hitRegion, &callableRegion, vk.width, vk.height, 1);
                GPU_TIMESTAMP_END(resources.gpuStat->GetGPUQueryEndIndex());

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
                bool success = WriteResourceToDisk(vk, directory + "/R-GBufferA", vkResources.rt.GBufferA, vk.width, vk.height, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_GENERAL);
                success &= WriteResourceToDisk(vk, directory + "/R-GBufferB", vkResources.rt.GBufferB, vk.width, vk.height, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL);
                success &= WriteResourceToDisk(vk, directory + "/R-GBufferC", vkResources.rt.GBufferC, vk.width, vk.height, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL);
                success &= WriteResourceToDisk(vk, directory + "/R-GBufferD", vkResources.rt.GBufferD, vk.width, vk.height, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_GENERAL);
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
