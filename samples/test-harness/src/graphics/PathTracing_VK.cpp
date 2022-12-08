/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "graphics/PathTracing.h"

namespace Graphics
{
    namespace Vulkan
    {
        namespace PathTracing
        {
            //----------------------------------------------------------------------------------------------------------
            // Private Functions
            //----------------------------------------------------------------------------------------------------------

            bool CreateTextures(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                // Create the output (R8G8B8A8_UNORM) texture resource
                // TODO: Why not VK_FORMAT_R8G8B8A8_UNORM?
                TextureDesc info = { static_cast<uint32_t>(vk.width), static_cast<uint32_t>(vk.height), 1, 1, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT };
                CHECK(CreateTexture(vk, info, &resources.PTOutput, &resources.PTOutputMemory, &resources.PTOutputView), "create path tracing output texture resources!\n", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.PTOutput), "PT Output", VK_OBJECT_TYPE_IMAGE);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.PTOutputMemory), "PT Output Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.PTOutputView), "PT Output View", VK_OBJECT_TYPE_IMAGE_VIEW);
            #endif

                // Create the accumulation (R32G32B32A32_FLOAT) texture resource
                info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
                CHECK(CreateTexture(vk, info, &resources.PTAccumulation, &resources.PTAccumulationMemory, &resources.PTAccumulationView), "create path tracing accumulation texture resources!\n", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.PTAccumulation), "PT Accumulation", VK_OBJECT_TYPE_IMAGE);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.PTAccumulationMemory), "PT Accumulation Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.PTAccumulationView), "PT Accumulation View", VK_OBJECT_TYPE_IMAGE_VIEW);
            #endif

                // Transition the textures to layout general
                ImageBarrierDesc barrier =
                {
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_GENERAL,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
                };
                SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], resources.PTOutput, barrier);
                SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], resources.PTAccumulation, barrier);

                return true;
            }

            bool LoadAndCompileShaders(Globals& vk, Resources& resources, std::ofstream& log)
            {
                // Release existing shaders
                resources.shaders.Release();

                std::wstring root = std::wstring(vk.shaderCompiler.root.begin(), vk.shaderCompiler.root.end());

                // Load and compile the ray generation shader
                resources.shaders.rgs.filepath = root + L"shaders/PathTraceRGS.hlsl";
                resources.shaders.rgs.entryPoint = L"RayGen";
                resources.shaders.rgs.exportName = L"PathTraceRGS";
                resources.shaders.rgs.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2"};\
                Shaders::AddDefine(resources.shaders.rgs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                Shaders::AddDefine(resources.shaders.rgs, L"GFX_NVAPI", std::to_wstring(0));
                CHECK(Shaders::Compile(vk.shaderCompiler, resources.shaders.rgs, true), "compile path tracing ray generation shader!\n", log);

                // Load and compile the miss shader
                resources.shaders.miss.filepath = root + L"shaders/Miss.hlsl";
                resources.shaders.miss.entryPoint = L"Miss";
                resources.shaders.miss.exportName = L"PathTraceMiss";
                resources.shaders.miss.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                Shaders::AddDefine(resources.shaders.miss, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                CHECK(Shaders::Compile(vk.shaderCompiler, resources.shaders.miss, true), "compile path tracing miss shader!\n", log);

                // Add the hit group
                resources.shaders.hitGroups.emplace_back();

                Shaders::ShaderRTHitGroup& group = resources.shaders.hitGroups[0];
                group.exportName = L"PathTraceHitGroup";

                // Load and compile the CHS
                group.chs.filepath = root + L"shaders/CHS.hlsl";
                group.chs.entryPoint = L"CHS_LOD0";
                group.chs.exportName = L"PathTraceCHS";
                group.chs.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                Shaders::AddDefine(group.chs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                CHECK(Shaders::Compile(vk.shaderCompiler, group.chs, true), "compile path tracing closest hit shader!\n", log);

                // Load and compile the AHS
                group.ahs.filepath = root + L"shaders/AHS.hlsl";
                group.ahs.entryPoint = L"AHS_LOD0";
                group.ahs.exportName = L"PathTraceAHS";
                group.ahs.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                Shaders::AddDefine(group.ahs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                CHECK(Shaders::Compile(vk.shaderCompiler, group.ahs, true), "compile path tracing any hit shader!\n", log);

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
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.descriptorSet), "PT Descriptor Set", VK_OBJECT_TYPE_DESCRIPTOR_SET);
            #endif

                return true;
            }

            bool CreatePipelines(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                // Release existing shader modules and pipeline
                resources.modules.Release(vk.device);
                vkDestroyPipeline(vk.device, resources.pipeline, nullptr);

                // Create the pipeline shader modules
                CHECK(CreateRayTracingShaderModules(vk.device, resources.shaders, resources.modules), "create path tracing shader modules!\n", log);

                // Create the pipeline
                CHECK(CreateRayTracingPipeline(vk.device, vkResources.pipelineLayout, resources.shaders, resources.modules, &resources.pipeline), "create path tracing pipeline!\n", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.pipeline), "PT Pipeline", VK_OBJECT_TYPE_PIPELINE);
            #endif
                return true;
            }

            bool CreateShaderTable(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                // The Shader Table layout is as follows:
                //    Entry 0:  Path Trace Ray Generation Shader
                //    Entry 1:  Path Trace Miss Shader
                //    Entry 2+: Path Trace HitGroups
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
                resources.shaderTableSize = (2 + static_cast<uint32_t>(resources.shaders.hitGroups.size())) * resources.shaderTableRecordSize;
                resources.shaderTableSize = ALIGN(vk.deviceRTPipelineProps.shaderGroupBaseAlignment, resources.shaderTableSize);

                // Create the shader table upload buffer resource and memory
                BufferDesc desc = { resources.shaderTableSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
                CHECK(CreateBuffer(vk, desc, &resources.shaderTableUpload, &resources.shaderTableUploadMemory), "create path tracing shader table upload resources!", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTableUpload), "PT Shader Table Upload", VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTableUploadMemory), "PT Shader Table Upload Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
            #endif

                // Create the shader table device buffer resource and memory
                desc = { resources.shaderTableSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
                CHECK(CreateBuffer(vk, desc, &resources.shaderTable, &resources.shaderTableMemory), "create path tracing shader table resources!", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTable), "PT Shader Table", VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTableMemory), "PT Shader Table Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
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
                // Store the descriptors to write to the descriptor set
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
                    { VK_NULL_HANDLE, resources.PTOutputView, VK_IMAGE_LAYOUT_GENERAL },
                    { VK_NULL_HANDLE, resources.PTAccumulationView, VK_IMAGE_LAYOUT_GENERAL }
                };

                descriptor = &descriptors.emplace_back();
                descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor->dstSet = resources.descriptorSet;
                descriptor->dstBinding = DescriptorLayoutBindings::UAV_TEX2D;
                descriptor->dstArrayElement = RWTex2DIndices::PT_OUTPUT;
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
             * Create resources used by the path tracing pass.
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

                // Execute GPU work to finish initialization
                VKCHECK(vkEndCommandBuffer(vk.cmdBuffer[vk.frameIndex]));

                VkSubmitInfo submitInfo = {};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &vk.cmdBuffer[vk.frameIndex];

                VKCHECK(vkQueueSubmit(vk.queue, 1, &submitInfo, VK_NULL_HANDLE));
                VKCHECK(vkQueueWaitIdle(vk.queue));

                WaitForGPU(vk);

                perf.AddStat("Path Tracing", resources.cpuStat, resources.gpuStat);

                return true;
            }

            /**
             * Reload and compile shaders, recreate the pipeline, and recreate the shader table.
             */
            bool Reload(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                log << "Reloading Path Tracing shaders...";
                if (!LoadAndCompileShaders(vk, resources, log)) return false;
                if (!CreatePipelines(vk, vkResources, resources, log)) return false;
                if (!UpdateShaderTable(vk, vkResources, resources, log)) return false;
                log << "done.\n";
                log << std::flush;

                return true;
            }

            /**
             * Resize screen-space intermediate buffers.
             */
            bool Resize(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                // Release textures
                vkDestroyImageView(vk.device, resources.PTOutputView, nullptr);
                vkFreeMemory(vk.device, resources.PTOutputMemory, nullptr);
                vkDestroyImage(vk.device, resources.PTOutput, nullptr);

                vkDestroyImageView(vk.device, resources.PTAccumulationView, nullptr);
                vkFreeMemory(vk.device, resources.PTAccumulationMemory, nullptr);
                vkDestroyImage(vk.device, resources.PTAccumulation, nullptr);

                // Recreate textures and descriptor set
                if (!CreateTextures(vk, vkResources, resources, log)) return false;
                if (!UpdateDescriptorSets(vk, vkResources, resources, log)) return false;

                log << "Path Tracing resize, " << vk.width << "x" << vk.height << "\n";
                std::flush(log);
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
                vkResources.constants.pt.numBounces = config.pathTrace.numBounces;
                vkResources.constants.pt.samplesPerPixel = config.pathTrace.samplesPerPixel;
                vkResources.constants.pt.SetAntialiasing(config.pathTrace.antialiasing);
                vkResources.constants.pt.SetShaderExecutionReordering(false);

                // Post Process constants
               vkResources.constants.post.useFlags = POSTPROCESS_FLAG_USE_NONE;
               if (config.postProcess.enabled)
               {
                   if (config.postProcess.exposure.enabled) vkResources.constants.post.useFlags |= POSTPROCESS_FLAG_USE_EXPOSURE;
                   if (config.postProcess.tonemap.enabled) vkResources.constants.post.useFlags |= POSTPROCESS_FLAG_USE_TONEMAPPING;
                   if (config.postProcess.dither.enabled) vkResources.constants.post.useFlags |= POSTPROCESS_FLAG_USE_DITHER;
                   if (config.postProcess.gamma.enabled) vkResources.constants.post.useFlags |= POSTPROCESS_FLAG_USE_GAMMA;
                   vkResources.constants.post.exposure = pow(2.f, config.postProcess.exposure.fstops);
               }

               CPU_TIMESTAMP_END(resources.cpuStat);
            }

            /**
             * Record the graphics workload to the global command list.
             */
            void Execute(Globals& vk, GlobalResources& vkResources, Resources& resources)
            {
            #ifdef GFX_PERF_MARKERS
                AddPerfMarker(vk, GFX_PERF_MARKER_YELLOW, "Path Tracing");
            #endif
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);

                // Set the push constants
                uint32_t offset = 0;
                GlobalConstants consts = vkResources.constants;
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, AppConsts::GetSizeInBytes(), consts.app.GetData());
                offset += AppConsts::GetAlignedSizeInBytes();
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, PathTraceConsts::GetSizeInBytes(), consts.pt.GetData());
                offset += PathTraceConsts::GetAlignedSizeInBytes();
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, LightingConsts::GetSizeInBytes(), consts.lights.GetData());
                offset += LightingConsts::GetAlignedSizeInBytes();
                offset += RTAOConsts::GetAlignedSizeInBytes();
                offset += CompositeConsts::GetAlignedSizeInBytes();
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, PostProcessConsts::GetSizeInBytes(), consts.post.GetData());

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

                // Transition the output buffer layout to transfer source
                ImageBarrierDesc barrier =
                {
                    VK_IMAGE_LAYOUT_GENERAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
                };
                SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], resources.PTOutput, barrier);

                // Transition the back buffer layout to transfer destination
                barrier =
                {
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
                };
                SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], vk.swapChainImage[vk.frameIndex], barrier);

                // Copy the output buffer to the back buffer
                VkImageCopy copyRegion = {};
                copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
                copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
                copyRegion.extent = { static_cast<uint32_t>(vk.width), static_cast<uint32_t>(vk.height), 1 };
                vkCmdCopyImage(vk.cmdBuffer[vk.frameIndex], resources.PTOutput, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vk.swapChainImage[vk.frameIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

                // Transition the back buffer layout to present
                barrier =
                {
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
                };
                SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], vk.swapChainImage[vk.frameIndex], barrier);

                // Transition output buffer layout to general
                barrier =
                {
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_IMAGE_LAYOUT_GENERAL,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
                };
                SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], resources.PTOutput, barrier);

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
                // Textures
                vkDestroyImageView(device, resources.PTOutputView, nullptr);
                vkFreeMemory(device, resources.PTOutputMemory, nullptr);
                vkDestroyImage(device, resources.PTOutput, nullptr);

                vkDestroyImageView(device, resources.PTAccumulationView, nullptr);
                vkFreeMemory(device, resources.PTAccumulationMemory, nullptr);
                vkDestroyImage(device, resources.PTAccumulation, nullptr);

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

        } // namespace Graphics::Vulkan::PathTracing

    } // namespace Graphics::Vulkan

    namespace PathTracing
    {

        bool Initialize(Globals& vk, GlobalResources& vkResources, Resources& resources, Instrumentation::Performance& perf, std::ofstream& log)
        {
            return Graphics::Vulkan::PathTracing::Initialize(vk, vkResources, resources, perf, log);
        }

        bool Reload(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::Vulkan::PathTracing::Reload(vk, vkResources, resources, log);
        }

        bool Resize(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::Vulkan::PathTracing::Resize(vk, vkResources, resources, log);
        }

        void Update(Globals& vk, GlobalResources& vkResources, Resources& resources, const Configs::Config& config)
        {
            return Graphics::Vulkan::PathTracing::Update(vk, vkResources, resources, config);
        }

        void Execute(Globals& vk, GlobalResources& vkResources, Resources& resources)
        {
            return Graphics::Vulkan::PathTracing::Execute(vk, vkResources, resources);
        }

        void Cleanup(Globals& vk, Resources& resources)
        {
            Graphics::Vulkan::PathTracing::Cleanup(vk.device, resources);
        }

    } // namespace Graphics::PathTracing
}
