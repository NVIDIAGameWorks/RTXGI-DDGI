/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "graphics/RTAO.h"

namespace Graphics
{
    namespace Vulkan
    {
        namespace RTAO
        {
            const int RTAO_FILTER_BLOCK_SIZE = 8; // Block is NxN, 32 maximum

            //----------------------------------------------------------------------------------------------------------
            // Private Functions
            //----------------------------------------------------------------------------------------------------------

            bool CreateTextures(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                // Create the output (R8G8B8A8_UNORM) texture resource
                TextureDesc desc = { static_cast<uint32_t>(vk.width), static_cast<uint32_t>(vk.height), 1, 1, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT };
                CHECK(CreateTexture(vk, desc, &resources.RTAOOutput, &resources.RTAOOutputMemory, &resources.RTAOOutputView), "create RTAO output texture resource!\n", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.RTAOOutput), "RTAO Output", VK_OBJECT_TYPE_IMAGE);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.RTAOOutputMemory), "RTAO Output Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.RTAOOutputView), "RTAO Output View", VK_OBJECT_TYPE_IMAGE_VIEW);
            #endif

                // Create the raw texture resource
                CHECK(CreateTexture(vk, desc, &resources.RTAORaw, &resources.RTAORawMemory, &resources.RTAORawView), "create RTAO raw texture resource!\n", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.RTAORaw), "RTAO Raw", VK_OBJECT_TYPE_IMAGE);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.RTAORawMemory), "RTAO Raw Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.RTAORawView), "RTAO Raw View", VK_OBJECT_TYPE_IMAGE_VIEW);
            #endif

                // Store an alias of the RTAOOutput resource in the global render targets struct
                vkResources.rt.RTAOOutputView = resources.RTAOOutputView;

                // Transition the textures for general use
                ImageBarrierDesc barrier =
                {
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_GENERAL,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
                };
                SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], resources.RTAOOutput, barrier);
                SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], resources.RTAORaw, barrier);

                return true;
            }

            bool LoadAndCompileShaders(Globals& vk, Resources& resources, std::ofstream& log)
            {
                // Release existing shaders
                resources.rtShaders.Release();
                resources.filterCS.Release();

                std::wstring root = std::wstring(vk.shaderCompiler.root.begin(), vk.shaderCompiler.root.end());

                // Load and compile the ray generation shader
                resources.rtShaders.rgs.filepath = root + L"shaders/RTAOTraceRGS.hlsl";
                resources.rtShaders.rgs.entryPoint = L"RayGen";
                resources.rtShaders.rgs.exportName = L"RTAOTraceRGS";
                resources.rtShaders.rgs.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                CHECK(Shaders::Compile(vk.shaderCompiler, resources.rtShaders.rgs, true), "compile RTAO ray generation shader!\n", log);

                // Load and compile the miss shader
                resources.rtShaders.miss.filepath = root + L"shaders/Miss.hlsl";
                resources.rtShaders.miss.entryPoint = L"Miss";
                resources.rtShaders.miss.exportName = L"RTAOMiss";
                resources.rtShaders.miss.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                Shaders::AddDefine(resources.rtShaders.miss, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                CHECK(Shaders::Compile(vk.shaderCompiler, resources.rtShaders.miss, true), "compile RTAO miss shader!\n", log);

                // Add the hit group
                resources.rtShaders.hitGroups.emplace_back();

                Shaders::ShaderRTHitGroup& group = resources.rtShaders.hitGroups[0];
                group.exportName = L"RTAOHitGroup";

                // Load and compile the CHS
                group.chs.filepath = root + L"shaders/CHS.hlsl";
                group.chs.entryPoint = L"CHS_VISIBILITY";
                group.chs.exportName = L"RTAOCHS";
                group.chs.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                Shaders::AddDefine(group.chs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                CHECK(Shaders::Compile(vk.shaderCompiler, group.chs, true), "compile RTAO closest hit shader!\n", log);

                // Load and compile the AHS
                group.ahs.filepath = root + L"shaders/AHS.hlsl";
                group.ahs.entryPoint = L"AHS_GI";
                group.ahs.exportName = L"RTAOAHS";
                group.ahs.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                Shaders::AddDefine(group.ahs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                CHECK(Shaders::Compile(vk.shaderCompiler, group.ahs, true), "compile RTAO any hit shader!\n", log);

                // Load and compile the filter compute shader
                std::wstring blockSize = std::to_wstring(static_cast<int>(RTAO_FILTER_BLOCK_SIZE));

                resources.filterCS.filepath = root + L"shaders/RTAOFilterCS.hlsl";
                resources.filterCS.entryPoint = L"CS";
                resources.filterCS.targetProfile = L"cs_6_6";
                resources.filterCS.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                Shaders::AddDefine(resources.filterCS, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                Shaders::AddDefine(resources.filterCS, L"BLOCK_SIZE", blockSize);
                CHECK(Shaders::Compile(vk.shaderCompiler, resources.filterCS, true), "compile RTAO filter compute shader!\n", log);

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
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.descriptorSet), "RTAO Descriptor Set", VK_OBJECT_TYPE_DESCRIPTOR_SET);
            #endif

                return true;
            }

            bool CreatePipelines(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                // Release existing shader modules and pipelines
                resources.rtShaderModules.Release(vk.device);
                vkDestroyShaderModule(vk.device, resources.filterCSModule, nullptr);
                vkDestroyPipeline(vk.device, resources.rtPipeline, nullptr);
                vkDestroyPipeline(vk.device, resources.filterPipeline, nullptr);

                // Create the ray tracing pipeline shader modules
                CHECK(CreateRayTracingShaderModules(vk.device, resources.rtShaders, resources.rtShaderModules), "create RTAO RT shader modules!\n", log);

                // Create the filter compute shader module
                CHECK(CreateShaderModule(vk.device, resources.filterCS, &resources.filterCSModule), "create RTAO Filter shader module!\n", log);

                // Create the ray tracing pipeline
                CHECK(CreateRayTracingPipeline(vk.device, vkResources.pipelineLayout, resources.rtShaders, resources.rtShaderModules, &resources.rtPipeline), "create RTAO RT pipeline!\n", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rtPipeline), "RTAO RT Pipeline", VK_OBJECT_TYPE_PIPELINE);
            #endif

                // Create the filter compute pipeline
                CHECK(CreateComputePipeline(vk.device, vkResources.pipelineLayout, resources.filterCS, resources.filterCSModule, &resources.filterPipeline), "create RTAO Filter pipeline!\n", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.filterPipeline), "RTAO Filter Pipeline", VK_OBJECT_TYPE_PIPELINE);
            #endif

                return true;
            }

            bool CreateShaderTable(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                // The Shader Table layout is as follows:
                //    Entry 0:  RTAO Ray Generation Shader
                //    Entry 1:  RTAO Miss Shader
                //    Entry 2+: RTAO HitGroups
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

                // Find the shader table size
                resources.shaderTableSize = (2 + static_cast<uint32_t>(resources.rtShaders.hitGroups.size())) * resources.shaderTableRecordSize;
                resources.shaderTableSize = ALIGN(vk.deviceRTPipelineProps.shaderGroupBaseAlignment, resources.shaderTableSize);

                // Create the shader table upload buffer resource and memory
                BufferDesc desc = { resources.shaderTableSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
                CHECK(CreateBuffer(vk, desc, &resources.shaderTableUpload, &resources.shaderTableUploadMemory), "create RTAO shader table upload resources!", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTableUpload), "RTAO Shader Table Upload", VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTableUploadMemory), "RTAO Shader Table Upload Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
            #endif

                // Create the shader table device buffer resource and memory
                desc = { resources.shaderTableSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
                CHECK(CreateBuffer(vk, desc, &resources.shaderTable, &resources.shaderTableMemory), "create RTAO shader table resources!", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTable), "RTAO Shader Table", VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTableMemory), "RTAO Shader Table Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
            #endif

                return true;
            }

            bool UpdateShaderTable(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                uint32_t shaderGroupIdSize = vk.deviceRTPipelineProps.shaderGroupHandleSize;

                // Get the shader group IDs from the pipeline
                std::vector<uint8_t> shaderGroupIdBuffer(shaderGroupIdSize * resources.rtShaderModules.numGroups);
                VKCHECK(vkGetRayTracingShaderGroupHandlesKHR(vk.device, resources.rtPipeline, 0, resources.rtShaderModules.numGroups, (shaderGroupIdSize * resources.rtShaderModules.numGroups), shaderGroupIdBuffer.data()));

                // Separate the shader group IDs into an array
                std::vector<uint8_t*> shaderGroupIds(resources.rtShaderModules.numGroups);
                for (uint32_t i = 0; i < resources.rtShaderModules.numGroups; ++i)
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
                for (uint32_t hitGroupIndex = 0; hitGroupIndex < static_cast<uint32_t>(resources.rtShaders.hitGroups.size()); hitGroupIndex++)
                {
                    pData += resources.shaderTableRecordSize;
                    memcpy(pData, shaderGroupIds[groupIndex++], shaderGroupIdSize);
                }
                resources.shaderTableHitGroupTableStartAddress = resources.shaderTableMissTableStartAddress + resources.shaderTableMissTableSize;
                resources.shaderTableHitGroupTableSize = static_cast<uint32_t>(resources.rtShaders.hitGroups.size()) * resources.shaderTableRecordSize;

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
                VkDescriptorImageInfo samplers[] = { vkResources.samplers[SamplerIndices::BILINEAR_WRAP], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED };

                descriptor = &descriptors.emplace_back();
                descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor->dstSet = resources.descriptorSet;
                descriptor->dstBinding = DescriptorLayoutBindings::SAMPLERS;
                descriptor->dstArrayElement = SamplerIndices::BILINEAR_WRAP;
                descriptor->descriptorCount = _countof(samplers);
                descriptor->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                descriptor->pImageInfo = samplers;

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
                    { VK_NULL_HANDLE, vkResources.rt.GBufferDView, VK_IMAGE_LAYOUT_GENERAL },
                    { VK_NULL_HANDLE, resources.RTAOOutputView, VK_IMAGE_LAYOUT_GENERAL },
                    { VK_NULL_HANDLE, resources.RTAORawView, VK_IMAGE_LAYOUT_GENERAL }
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
             * Create resources used by the ray traced ambient occlusion pass.
             */
            bool Initialize(Globals& vk, GlobalResources& vkResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
            {
                // Reset the command list before initialization
                CHECK(ResetCmdList(vk), "reset command list!", log);

                if (!CreateTextures(vk, vkResources, resources, log)) return false;
                if (!LoadAndCompileShaders(vk, resources, log)) return false;
                if (!CreateDescriptorSets(vk, vkResources, resources, log)) return false;
                if (!CreatePipelines(vk, vkResources, resources, log)) return false;
                if (!CreateShaderTable(vk, vkResources, resources, log)) return false;

                if (!UpdateDescriptorSets(vk, vkResources, resources, log)) return false;
                if (!UpdateShaderTable(vk, vkResources, resources, log)) return false;

                perf.AddStat("RTAO", resources.cpuStat, resources.gpuStat);

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
            bool Reload(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                log << "Reloading RTAO shaders...";
                if (!LoadAndCompileShaders(vk, resources, log)) return false;
                if (!CreatePipelines(vk, vkResources, resources, log)) return false;
                if (!UpdateShaderTable(vk, vkResources, resources, log)) return false;
                log << "done.\n";
                log << std::flush;

                return true;
            }

            /**
             * Resize screen-space buffers.
             */
            bool Resize(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                // Release textures
                vkDestroyImageView(vk.device, resources.RTAOOutputView, nullptr);
                vkFreeMemory(vk.device, resources.RTAOOutputMemory, nullptr);
                vkDestroyImage(vk.device, resources.RTAOOutput, nullptr);

                vkDestroyImageView(vk.device, resources.RTAORawView, nullptr);
                vkFreeMemory(vk.device, resources.RTAORawMemory, nullptr);
                vkDestroyImage(vk.device, resources.RTAORaw, nullptr);

                if (!CreateTextures(vk, vkResources, resources, log)) return false;
                if (!UpdateDescriptorSets(vk, vkResources, resources, log)) return false;

                log << "RTAO resize, " << vk.width << "x" << vk.height << "\n";
                std::flush(log);
                return true;
            }

            /**
             * Update data before execute.
             */
            void Update(Globals& vk, GlobalResources& vkResources, Resources& resources, const Configs::Config& config)
            {
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);

                // RTAO constants
                resources.enabled = config.rtao.enabled;
                if (resources.enabled)
                {
                    vkResources.constants.rtao.rayLength = config.rtao.rayLength;
                    vkResources.constants.rtao.rayNormalBias = config.rtao.rayNormalBias;
                    vkResources.constants.rtao.rayViewBias = config.rtao.rayViewBias;
                    vkResources.constants.rtao.power = pow(2.f, config.rtao.powerLog);
                    vkResources.constants.rtao.filterDistanceSigma = config.rtao.filterDistanceSigma;
                    vkResources.constants.rtao.filterDepthSigma = config.rtao.filterDepthSigma;
                    vkResources.constants.rtao.filterBufferWidth = static_cast<uint32_t>(vk.width);
                    vkResources.constants.rtao.filterBufferHeight = static_cast<uint32_t>(vk.height);

                    float distanceKernel[6];
                    for (int i = 0; i < 6; ++i)
                    {
                        distanceKernel[i] = (float)exp(-float(i * i) / (2.f * config.rtao.filterDistanceSigma * config.rtao.filterDistanceSigma));
                    }

                    vkResources.constants.rtao.filterDistKernel0 = distanceKernel[0];
                    vkResources.constants.rtao.filterDistKernel1 = distanceKernel[1];
                    vkResources.constants.rtao.filterDistKernel2 = distanceKernel[2];
                    vkResources.constants.rtao.filterDistKernel3 = distanceKernel[3];
                    vkResources.constants.rtao.filterDistKernel4 = distanceKernel[4];
                    vkResources.constants.rtao.filterDistKernel5 = distanceKernel[5];
                }

                CPU_TIMESTAMP_END(resources.cpuStat);
            }

            /**
             * Record the graphics workload to the global command list.
             */
            void Execute(Globals& vk, GlobalResources& vkResources, Resources& resources)
            {
            #ifdef GFX_PERF_MARKERS
                AddPerfMarker(vk, GFX_PERF_MARKER_RED, "RTAO");
            #endif
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);
                if (resources.enabled)
                {
                    // Set the global constants
                    uint32_t offset = 0;
                    GlobalConstants consts = vkResources.constants;
                    offset += AppConsts::GetAlignedSizeInBytes();
                    offset += PathTraceConsts::GetAlignedSizeInBytes();
                    offset += LightingConsts::GetAlignedSizeInBytes();
                    vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, consts.rtao.GetSizeInBytes(), consts.rtao.GetData());

                    // Bind the pipeline
                    vkCmdBindPipeline(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, resources.rtPipeline);

                    // Bind the descriptor set
                    vkCmdBindDescriptorSets(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vkResources.pipelineLayout, 0, 1, &resources.descriptorSet, 0, nullptr);

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
                    GPU_TIMESTAMP_BEGIN(resources.gpuStat->GetGPUQueryBeginIndex());
                    vkCmdTraceRaysKHR(vk.cmdBuffer[vk.frameIndex], &raygenRegion, &missRegion, &hitRegion, &callableRegion, vk.width, vk.height, 1);
                    GPU_TIMESTAMP_END(resources.gpuStat->GetGPUQueryEndIndex());

                    // Wait for the ray trace to finish
                    VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                    ImageBarrierDesc barrier = { VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
                    SetImageMemoryBarrier(vk.cmdBuffer[vk.frameIndex], resources.RTAORaw, barrier);

                    // --- Run the filter compute shader ---------------------------------

                    // Bind the compute pipeline and dispatch threads
                    vkCmdBindPipeline(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_COMPUTE, resources.filterPipeline);

                    // Bind the descriptor set
                    vkCmdBindDescriptorSets(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_COMPUTE, vkResources.pipelineLayout, 0, 1, &resources.descriptorSet, 0, nullptr);

                    uint32_t groupsX = DivRoundUp(vk.width, RTAO_FILTER_BLOCK_SIZE);
                    uint32_t groupsY = DivRoundUp(vk.height, RTAO_FILTER_BLOCK_SIZE);
                    vkCmdDispatch(vk.cmdBuffer[vk.frameIndex], groupsX, groupsY, 1);

                    // Wait for the compute pass to finish
                    SetImageMemoryBarrier(vk.cmdBuffer[vk.frameIndex], resources.RTAOOutput, barrier);

                #ifdef GFX_PERF_MARKERS
                    vkCmdEndDebugUtilsLabelEXT(vk.cmdBuffer[vk.frameIndex]);
                #endif
                }
                CPU_TIMESTAMP_ENDANDRESOLVE(resources.cpuStat);
            }

            /**
             * Release resources.
             */
            void Cleanup(VkDevice& device, Resources& resources)
            {
                // Textures
                vkDestroyImageView(device, resources.RTAOOutputView, nullptr);
                vkFreeMemory(device, resources.RTAOOutputMemory, nullptr);
                vkDestroyImage(device, resources.RTAOOutput, nullptr);

                vkDestroyImageView(device, resources.RTAORawView, nullptr);
                vkFreeMemory(device, resources.RTAORawMemory, nullptr);
                vkDestroyImage(device, resources.RTAORaw, nullptr);

                // Shader Table
                vkDestroyBuffer(device, resources.shaderTableUpload, nullptr);
                vkFreeMemory(device, resources.shaderTableUploadMemory, nullptr);
                vkDestroyBuffer(device, resources.shaderTable, nullptr);
                vkFreeMemory(device, resources.shaderTableMemory, nullptr);

                // Pipelines
                vkDestroyPipeline(device, resources.rtPipeline, nullptr);
                vkDestroyPipeline(device, resources.filterPipeline, nullptr);

                // Shaders
                resources.rtShaderModules.Release(device);
                resources.rtShaders.Release();
                vkDestroyShaderModule(device, resources.filterCSModule, nullptr);
                resources.filterCS.Release();

                resources.shaderTableSize = 0;
                resources.shaderTableRecordSize = 0;
                resources.shaderTableMissTableSize = 0;
                resources.shaderTableHitGroupTableSize = 0;

                resources.shaderTableRGSStartAddress = 0;
                resources.shaderTableMissTableStartAddress = 0;
                resources.shaderTableHitGroupTableStartAddress = 0;
            }

            /**
             * Write the RTAO texture resources to disk.
             */
            bool WriteRTAOBuffersToDisk(Globals& vk, GlobalResources& vkResources, Resources& resources, std::string directory)
            {
            #if (defined(_WIN32) || defined(WIN32))
                CoInitialize(NULL);
            #endif
                // Formats should match those from CreateTextures() function above
                bool success = WriteResourceToDisk(vk, directory + "/R-RTAO-Raw", resources.RTAORaw, vk.width, vk.height, 1, VK_FORMAT_R8_UNORM, VK_IMAGE_LAYOUT_GENERAL);
                success &= WriteResourceToDisk(vk, directory + "/R-RTAO-Filtered", resources.RTAOOutput, vk.width, vk.height, 1, VK_FORMAT_R8_UNORM, VK_IMAGE_LAYOUT_GENERAL);
                return success;
            }

        } // namespace Graphics::Vulkan::RTAO

    } // namespace Graphics::Vulkan

    namespace RTAO
    {

        bool Initialize(Globals& vk, GlobalResources& vkResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
        {
            return Graphics::Vulkan::RTAO::Initialize(vk, vkResources, resources, perf, log);
        }

        bool Reload(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::Vulkan::RTAO::Reload(vk, vkResources, resources, log);
        }

        bool Resize(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::Vulkan::RTAO::Resize(vk, vkResources, resources, log);
        }

        void Update(Globals& vk, GlobalResources& vkResources, Resources& resources, const Configs::Config& config)
        {
            return Graphics::Vulkan::RTAO::Update(vk, vkResources, resources, config);
        }

        void Execute(Globals& vk, GlobalResources& vkResources, Resources& resources)
        {
            return Graphics::Vulkan::RTAO::Execute(vk, vkResources, resources);
        }

        void Cleanup(Globals& vk, Resources& resources)
        {
            Graphics::Vulkan::RTAO::Cleanup(vk.device, resources);
        }

        bool WriteRTAOBuffersToDisk(Globals& vk, GlobalResources& vkResources, Resources& resources, std::string directory)
        {
            return Graphics::Vulkan::RTAO::WriteRTAOBuffersToDisk(vk, vkResources, resources, directory);
        }

    } // namespace Graphics::RTAO
}
