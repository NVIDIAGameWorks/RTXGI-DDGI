/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "graphics/DDGI.h"

using namespace rtxgi;
using namespace rtxgi::vulkan;

#if (RTXGI_DDGI_BINDLESS_RESOURCES && RTXGI_DDGI_RESOURCE_MANAGEMENT)
#error RTXGI SDK DDGI Managed Mode is not compatible with bindless resources!
#endif

namespace Graphics
{
    namespace Vulkan
    {
        namespace DDGI
        {

            const char* GetResourceName(std::string input, std::string& output, VkObjectType objType)
            {
                output = input;
                if(objType = VK_OBJECT_TYPE_DEVICE_MEMORY) output.append(" Memory");
                else if(objType = VK_OBJECT_TYPE_IMAGE_VIEW) output.append(" View");
                return output.c_str();
            }

            //----------------------------------------------------------------------------------------------------------
            // DDGIVolume Resource Creation Functions (Unmanaged Mode)
            //----------------------------------------------------------------------------------------------------------

        #if !RTXGI_DDGI_RESOURCE_MANAGEMENT
        #if !RTXGI_DDGI_BINDLESS_RESOURCES
            /**
             * Create the volume pipeline and descriptor set layouts (when *not* using bindless resources).
             */
            bool CreateDDGIVolumeLayouts(Globals& vk, Resources& resources)
            {
                // Get the descriptor set layout descriptors
                std::vector<VkDescriptorSetLayoutBinding> bindings;
                VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
                VkPushConstantRange pushConstantRange = {};
                VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};

                GetDDGIVolumeLayoutDescs(bindings, descriptorSetLayoutCreateInfo, pushConstantRange, pipelineLayoutCreateInfo);

                // Create the descriptor set layout
                VKCHECK(vkCreateDescriptorSetLayout(vk.device, &descriptorSetLayoutCreateInfo, nullptr, &resources.volumeDescriptorSetLayout));
            #ifdef RTXGI_GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.volumeDescriptorSetLayout), "DDGIVolume Descriptor Set Layout", VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);
            #endif

                // Set the descriptor set layout for the pipeline layout
                pipelineLayoutCreateInfo.pSetLayouts = &resources.volumeDescriptorSetLayout;

                // Create the pipeline layout
                VKCHECK(vkCreatePipelineLayout(vk.device, &pipelineLayoutCreateInfo, nullptr, &resources.volumePipelineLayout));
            #ifdef RTXGI_GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.volumePipelineLayout), "DDGIVolume Pipeline Layout", VK_OBJECT_TYPE_PIPELINE_LAYOUT);
            #endif

                return true;
            }

            /**
             * Create a descriptor set for each volume (when *not* using bindless resources).
             */
            bool CreateDDGIVolumeDescriptorSets(Globals& vk, GlobalResources& vkResources, Resources& resources, uint32_t numVolumes)
            {
                // Describe the descriptor set allocation
                VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
                descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                descriptorSetAllocateInfo.descriptorPool = vkResources.descriptorPool;
                descriptorSetAllocateInfo.descriptorSetCount = 1;
                descriptorSetAllocateInfo.pSetLayouts = &resources.volumeDescriptorSetLayout;

                // Allocate the descriptor sets
                for(uint32_t volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
                {
                    resources.volumeDescriptorSets.emplace_back();
                    VKCHECK(vkAllocateDescriptorSets(vk.device, &descriptorSetAllocateInfo, &resources.volumeDescriptorSets[volumeIndex]));
                #ifdef GFX_NAME_OBJECTS
                    std::string msg = "DDGIVolume[" + std::to_string(volumeIndex) + "]Descriptor Set";
                    SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.volumeDescriptorSets[volumeIndex]), msg.c_str(), VK_OBJECT_TYPE_DESCRIPTOR_SET);
                #endif
                }

                return true;
            }

            /**
             * Update the descriptor set for all selected volumes.
             * Call this before Updating volumes when in Managed Resource Mode or when not using bindless.
             */
            void UpdateDDGIVolumeDescriptorSets(Globals& vk, Resources& resources)
            {
                for(uint32_t volumeIndex = 0; volumeIndex < static_cast<uint32_t>(resources.volumes.size()); volumeIndex++)
                {
                    // Get the volume
                    DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[volumeIndex]);

                    // Store the data to be written to the descriptor set
                    std::vector<VkWriteDescriptorSet> writeDescriptorSets;

                    // DDGIVolume constants structured buffer
                    VkDescriptorBufferInfo constantsSTBInfo = { resources.constantsSTB, 0, VK_WHOLE_SIZE };
                    VkWriteDescriptorSet constantsSTBSet = {};
                    constantsSTBSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    constantsSTBSet.dstSet = *volume->GetDescriptorSetPtr();
                    constantsSTBSet.dstBinding = 0;
                    constantsSTBSet.dstArrayElement = 0;
                    constantsSTBSet.descriptorCount = 1;
                    constantsSTBSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    constantsSTBSet.pBufferInfo = &constantsSTBInfo;

                    writeDescriptorSets.push_back(constantsSTBSet);

                    // DDGIVolume textures
                    std::vector<VkDescriptorImageInfo> rwTex2DInfo;
                    rwTex2DInfo.push_back({ VK_NULL_HANDLE, volume->GetProbeRayDataView(), VK_IMAGE_LAYOUT_GENERAL });
                    rwTex2DInfo.push_back({ VK_NULL_HANDLE, volume->GetProbeIrradianceView(), VK_IMAGE_LAYOUT_GENERAL });
                    rwTex2DInfo.push_back({ VK_NULL_HANDLE, volume->GetProbeDistanceView(), VK_IMAGE_LAYOUT_GENERAL });
                    rwTex2DInfo.push_back({ VK_NULL_HANDLE, volume->GetProbeDataView(), VK_IMAGE_LAYOUT_GENERAL });

                    VkWriteDescriptorSet rwTex2DSet = {};
                    rwTex2DSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    rwTex2DSet.dstSet = *volume->GetDescriptorSetPtr();
                    rwTex2DSet.dstBinding = 1;
                    rwTex2DSet.dstArrayElement = 0;
                    rwTex2DSet.descriptorCount = static_cast<uint32_t>(rwTex2DInfo.size());
                    rwTex2DSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    rwTex2DSet.pImageInfo = rwTex2DInfo.data();

                    writeDescriptorSets.push_back(rwTex2DSet);

                    // Update the descriptor set
                    vkUpdateDescriptorSets(vk.device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
                }

            }
        #endif // !RTXGI_DDGI_BINDLESS_RESOURCES

            /**
             * Create resources used by a DDGIVolume.
             */
            bool CreateDDGIVolumeResources(
                Globals& vk,
                GlobalResources& vkResources,
                Resources& resources,
                const DDGIVolumeDesc& volumeDesc,
                DDGIVolumeResources& volumeResources,
                std::vector<Shaders::ShaderProgram>& volumeShaders,
                std::ofstream& log)
            {
                log << "\tCreating resources for DDGIVolume: \"" << volumeDesc.name << "\"...";
                std::flush(log);

                // Create the volume's textures
                {
                    uint32_t width = 0;
                    uint32_t height = 0;
                    VkFormat format = VK_FORMAT_UNDEFINED;

                    // Probe ray data texture
                    {
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::RayData, width, height);
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::RayData, volumeDesc.probeRayDataFormat);

                        TextureDesc desc = { width, height, 1, format, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT };
                        CHECK(CreateTexture(vk, desc, &volumeResources.unmanaged.probeRayData, &volumeResources.unmanaged.probeRayDataMemory, &volumeResources.unmanaged.probeRayDataView), "create DDGIVolume ray data texture!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string n = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Ray Data";
                        std::string o = "";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeRayData), n.c_str(), VK_OBJECT_TYPE_IMAGE);
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeRayDataMemory), GetResourceName(n, o, VK_OBJECT_TYPE_DEVICE_MEMORY), VK_OBJECT_TYPE_DEVICE_MEMORY);
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeRayDataView), GetResourceName(n, o, VK_OBJECT_TYPE_IMAGE_VIEW), VK_OBJECT_TYPE_IMAGE_VIEW);
                    #endif
                    }

                    // Probe irradiance texture
                    {
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::Irradiance, width, height);
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Irradiance, volumeDesc.probeIrradianceFormat);

                        TextureDesc desc = { width, height, 1, format, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT };
                        CHECK(CreateTexture(vk, desc, &volumeResources.unmanaged.probeIrradiance, &volumeResources.unmanaged.probeIrradianceMemory, &volumeResources.unmanaged.probeIrradianceView), "create DDGIVolume irradiance texture!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string n = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Irradiance";
                        std::string o;
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeIrradiance), n.c_str(), VK_OBJECT_TYPE_IMAGE);
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeIrradianceMemory), GetResourceName(n, o, VK_OBJECT_TYPE_DEVICE_MEMORY), VK_OBJECT_TYPE_DEVICE_MEMORY);
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeIrradianceView), GetResourceName(n, o, VK_OBJECT_TYPE_IMAGE_VIEW), VK_OBJECT_TYPE_IMAGE_VIEW);
                    #endif
                    }

                    // Probe distance texture
                    {
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::Distance, width, height);
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Distance, volumeDesc.probeDistanceFormat);

                        TextureDesc desc = { width, height, 1, format, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT };
                        CHECK(CreateTexture(vk, desc, &volumeResources.unmanaged.probeDistance, &volumeResources.unmanaged.probeDistanceMemory, &volumeResources.unmanaged.probeDistanceView), "create DDGIVolume distance texture!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string n = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Distance";
                        std::string o = "";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeDistance), n.c_str(), VK_OBJECT_TYPE_IMAGE);
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeDistanceMemory), GetResourceName(n, o, VK_OBJECT_TYPE_DEVICE_MEMORY), VK_OBJECT_TYPE_DEVICE_MEMORY);
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeDistanceView), GetResourceName(n, o, VK_OBJECT_TYPE_IMAGE_VIEW), VK_OBJECT_TYPE_IMAGE_VIEW);
                    #endif
                    }

                    // Probe data texture
                    {
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::Data, width, height);
                        if (width <= 0) return false;
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Data, volumeDesc.probeDataFormat);

                        TextureDesc desc = { width, height, 1, format, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT };
                        CHECK(CreateTexture(vk, desc, &volumeResources.unmanaged.probeData, &volumeResources.unmanaged.probeDataMemory, &volumeResources.unmanaged.probeDataView), "", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string n = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Data";
                        std::string o = "";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeData), n.c_str(), VK_OBJECT_TYPE_IMAGE);
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeDataMemory), GetResourceName(n, o, VK_OBJECT_TYPE_DEVICE_MEMORY), VK_OBJECT_TYPE_DEVICE_MEMORY);
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeDataView), GetResourceName(n, o, VK_OBJECT_TYPE_IMAGE_VIEW), VK_OBJECT_TYPE_IMAGE_VIEW);
                    #endif
                    }
                }

                // Transition the volume's resources for general use
                {
                    ImageBarrierDesc barrier =
                    {
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_GENERAL,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
                    };

                    SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], volumeResources.unmanaged.probeRayData, barrier);
                    SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], volumeResources.unmanaged.probeIrradiance, barrier);
                    SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], volumeResources.unmanaged.probeDistance, barrier);
                    SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], volumeResources.unmanaged.probeData, barrier);
                }

                // Pipeline Layout and Descriptor Set
                {
                #if !RTXGI_DDGI_BINDLESS_RESOURCES
                    // Pass handles to the volume's pipeline layout and descriptor set (not using bindless)
                    volumeResources.unmanaged.pipelineLayout = resources.volumePipelineLayout;
                    volumeResources.unmanaged.descriptorSet = resources.volumeDescriptorSets[volumeDesc.index];
                #else
                    // Pass handles to the global pipeline layout and descriptor set (bindless)
                    volumeResources.unmanaged.pipelineLayout = vkResources.pipelineLayout;
                    volumeResources.unmanaged.descriptorSet = resources.descriptorSet;
                #endif
                }

                // Create the volume's shader modules and pipelines
                {
                    uint32_t shaderIndex = 0;

                    // Probe Irradiance Blending Pipeline
                    {
                        // Create the shader module
                        CHECK(CreateShaderModule(vk.device, volumeShaders[shaderIndex], &volumeResources.unmanaged.probeBlendingIrradianceModule), "create probe blending (irradiance) shader module!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Irradiance Blending Shader Module";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeBlendingIrradianceModule), name.c_str(), VK_OBJECT_TYPE_SHADER_MODULE);
                    #endif

                        // Create the pipeline
                        CHECK(CreateComputePipeline(
                            vk.device,
                            volumeResources.unmanaged.pipelineLayout,
                            volumeShaders[shaderIndex],
                            volumeResources.unmanaged.probeBlendingIrradianceModule,
                            &volumeResources.unmanaged.probeBlendingIrradiancePipeline), "create probe blending (irradiance) pipeline!", log);
                    #ifdef GFX_NAME_OBJECTS
                        name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Irradiance Blending Pipeline";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeBlendingIrradiancePipeline), name.c_str(), VK_OBJECT_TYPE_PIPELINE);
                    #endif
                        shaderIndex++;
                    }

                    // Probe Distance Blending Pipeline
                    {
                        // Create the shader module
                        CHECK(CreateShaderModule(vk.device, volumeShaders[shaderIndex], &volumeResources.unmanaged.probeBlendingDistanceModule), "create probe blending (distance) shader module!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Distance Blending Shader Module";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeBlendingDistanceModule), name.c_str(), VK_OBJECT_TYPE_SHADER_MODULE);
                    #endif

                        // Create the pipeline
                        CHECK(CreateComputePipeline(
                            vk.device,
                            volumeResources.unmanaged.pipelineLayout,
                            volumeShaders[shaderIndex],
                            volumeResources.unmanaged.probeBlendingDistanceModule,
                            &volumeResources.unmanaged.probeBlendingDistancePipeline), "create probe blending (distance) pipeline!", log);
                    #ifdef GFX_NAME_OBJECTS
                        name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Distance Blending Pipeline";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeBlendingDistancePipeline), name.c_str(), VK_OBJECT_TYPE_PIPELINE);
                    #endif
                        shaderIndex++;
                    }

                    // Border Row Update (Irradiance) Pipeline
                    {
                        // Create the shader module
                        CHECK(CreateShaderModule(vk.device, volumeShaders[shaderIndex], &volumeResources.unmanaged.probeBorderRowUpdateIrradianceModule), "create probe border row update (irradiance) shader module!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Border Row Update (Irradiance) Shader Module";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeBorderRowUpdateIrradianceModule), name.c_str(), VK_OBJECT_TYPE_SHADER_MODULE);
                    #endif

                        // Create the pipeline
                        CHECK(CreateComputePipeline(
                            vk.device,
                            volumeResources.unmanaged.pipelineLayout,
                            volumeShaders[shaderIndex],
                            volumeResources.unmanaged.probeBorderRowUpdateIrradianceModule,
                            &volumeResources.unmanaged.probeBorderRowUpdateIrradiancePipeline), "", log);
                    #ifdef GFX_NAME_OBJECTS
                        name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Border Row Update (Irradiance) Pipeline";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeBorderRowUpdateIrradiancePipeline), name.c_str(), VK_OBJECT_TYPE_PIPELINE);
                    #endif
                        shaderIndex++;
                    }

                    // Border Column Update (Irradiance) Pipeline
                    {
                        // Create the shader module
                        CHECK(CreateShaderModule(vk.device, volumeShaders[shaderIndex], &volumeResources.unmanaged.probeBorderColumnUpdateIrradianceModule), "create probe border column update (irradiance) shader module!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Border Column Update (Irradiance) Shader Module";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeBorderColumnUpdateIrradianceModule), name.c_str(), VK_OBJECT_TYPE_SHADER_MODULE);
                    #endif

                        // Create the pipeline
                        CHECK(CreateComputePipeline(
                            vk.device,
                            volumeResources.unmanaged.pipelineLayout,
                            volumeShaders[shaderIndex],
                            volumeResources.unmanaged.probeBorderColumnUpdateIrradianceModule,
                            &volumeResources.unmanaged.probeBorderColumnUpdateIrradiancePipeline), "", log);
                    #ifdef GFX_NAME_OBJECTS
                        name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Border Column Update (Irradiance) Pipeline";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeBorderColumnUpdateIrradiancePipeline), name.c_str(), VK_OBJECT_TYPE_PIPELINE);
                    #endif

                        shaderIndex++;
                    }

                    // Border Row Update (Distance) Pipeline
                    {
                        // Create the shader module
                        CHECK(CreateShaderModule(vk.device, volumeShaders[shaderIndex], &volumeResources.unmanaged.probeBorderRowUpdateDistanceModule), "create probe border row update (distance) shader module!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Border Row Update (Distance) Shader Module";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeBorderRowUpdateDistanceModule), name.c_str(), VK_OBJECT_TYPE_SHADER_MODULE);
                    #endif

                        // Create the pipeline
                        CHECK(CreateComputePipeline(
                            vk.device,
                            volumeResources.unmanaged.pipelineLayout,
                            volumeShaders[shaderIndex],
                            volumeResources.unmanaged.probeBorderRowUpdateDistanceModule,
                            &volumeResources.unmanaged.probeBorderRowUpdateDistancePipeline), "", log);
                    #ifdef GFX_NAME_OBJECTS
                        name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Border Row Update (Distance) Pipeline";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeBorderRowUpdateDistancePipeline), name.c_str(), VK_OBJECT_TYPE_PIPELINE);
                    #endif

                        shaderIndex++;
                    }

                    // Border Column Update (Distance) Pipeline
                    {
                        // Create the shader module
                        CHECK(CreateShaderModule(vk.device, volumeShaders[shaderIndex], &volumeResources.unmanaged.probeBorderColumnUpdateDistanceModule), "create probe border column update (distance) shader module!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Border Column Update (Distance) Shader Module";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeBorderColumnUpdateDistanceModule), name.c_str(), VK_OBJECT_TYPE_SHADER_MODULE);
                    #endif

                        // Create the pipeline
                        CHECK(CreateComputePipeline(
                            vk.device,
                            volumeResources.unmanaged.pipelineLayout,
                            volumeShaders[shaderIndex],
                            volumeResources.unmanaged.probeBorderColumnUpdateDistanceModule,
                            &volumeResources.unmanaged.probeBorderColumnUpdateDistancePipeline), "", log);
                    #ifdef GFX_NAME_OBJECTS
                        name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Border Column Update (Distance) Pipeline";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeBorderColumnUpdateDistancePipeline), name.c_str(), VK_OBJECT_TYPE_PIPELINE);
                    #endif

                        shaderIndex++;
                    }

                    // Probe Relocation Pipeline
                    {
                        // Create the shader module
                        CHECK(CreateShaderModule(vk.device, volumeShaders[shaderIndex], &volumeResources.unmanaged.probeRelocation.updateModule), "create probe relocation shader module!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Relocation Shader Module";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeRelocation.updateModule), name.c_str(), VK_OBJECT_TYPE_SHADER_MODULE);
                    #endif

                        // Create the pipeline
                        CHECK(CreateComputePipeline(
                            vk.device,
                            volumeResources.unmanaged.pipelineLayout,
                            volumeShaders[shaderIndex],
                            volumeResources.unmanaged.probeRelocation.updateModule,
                            &volumeResources.unmanaged.probeRelocation.updatePipeline), "", log);
                    #ifdef GFX_NAME_OBJECTS
                        name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Relocation Pipeline";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeRelocation.updatePipeline), name.c_str(), VK_OBJECT_TYPE_PIPELINE);
                    #endif

                        shaderIndex++;
                    }

                    // Probe Relocation Reset Pipeline
                    {
                        // Create the shader module
                        CHECK(CreateShaderModule(vk.device, volumeShaders[shaderIndex], &volumeResources.unmanaged.probeRelocation.resetModule), "create probe relocation reset shader module!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Relocation Reset Shader Module";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeRelocation.resetModule), name.c_str(), VK_OBJECT_TYPE_SHADER_MODULE);
                    #endif

                        // Create the pipeline
                        CHECK(CreateComputePipeline(
                            vk.device,
                            volumeResources.unmanaged.pipelineLayout,
                            volumeShaders[shaderIndex],
                            volumeResources.unmanaged.probeRelocation.resetModule,
                            &volumeResources.unmanaged.probeRelocation.resetPipeline), "", log);
                    #ifdef GFX_NAME_OBJECTS
                        name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Relocation Reset Pipeline";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeRelocation.resetPipeline), name.c_str(), VK_OBJECT_TYPE_PIPELINE);
                    #endif

                        shaderIndex++;
                    }

                    // Probe Classification Pipeline
                    {
                        // Create the shader module
                        CHECK(CreateShaderModule(vk.device, volumeShaders[shaderIndex], &volumeResources.unmanaged.probeClassification.updateModule), "create probe classification shader module!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Classification Shader Module";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeClassification.updateModule), name.c_str(), VK_OBJECT_TYPE_SHADER_MODULE);
                    #endif

                        // Create the pipeline
                        CHECK(CreateComputePipeline(
                            vk.device,
                            volumeResources.unmanaged.pipelineLayout,
                            volumeShaders[shaderIndex],
                            volumeResources.unmanaged.probeClassification.updateModule,
                            &volumeResources.unmanaged.probeClassification.updatePipeline), "", log);
                    #ifdef GFX_NAME_OBJECTS
                        name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Classification Pipeline";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeClassification.updatePipeline), name.c_str(), VK_OBJECT_TYPE_PIPELINE);
                    #endif

                        shaderIndex++;
                    }

                    // Probe Classification Reset Pipeline
                    {
                        // Create the shader module
                        CHECK(CreateShaderModule(vk.device, volumeShaders[shaderIndex], &volumeResources.unmanaged.probeClassification.resetModule), "create probe classification reset shader module!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Classification Reset Shader Module";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeClassification.resetModule), name.c_str(), VK_OBJECT_TYPE_SHADER_MODULE);
                    #endif

                        // Create the pipeline
                        CHECK(CreateComputePipeline(
                            vk.device,
                            volumeResources.unmanaged.pipelineLayout,
                            volumeShaders[shaderIndex],
                            volumeResources.unmanaged.probeClassification.resetModule,
                            &volumeResources.unmanaged.probeClassification.resetPipeline), "", log);
                    #ifdef GFX_NAME_OBJECTS
                        name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Classification Reset Pipeline";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeClassification.resetPipeline), name.c_str(), VK_OBJECT_TYPE_PIPELINE);
                    #endif
                    }
                }

                log << "done.\n";
                std::flush(log);
                return true;
            }

            /**
             * Release resources used by a DDGIVolume.
             */
            void DestroyDDGIVolumeResources(VkDevice device, Resources& resources, size_t volumeIndex)
            {
                // Get the volume
                DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[volumeIndex]);

                // Textures
                vkDestroyImage(device, volume->GetProbeRayData(), nullptr);
                vkDestroyImage(device, volume->GetProbeIrradiance(), nullptr);
                vkDestroyImage(device, volume->GetProbeDistance(), nullptr);
                vkDestroyImage(device, volume->GetProbeData(), nullptr);

                // Texture memory
                vkFreeMemory(device, volume->GetProbeRayDataMemory(), nullptr);
                vkFreeMemory(device, volume->GetProbeIrradianceMemory(), nullptr);
                vkFreeMemory(device, volume->GetProbeDistanceMemory(), nullptr);
                vkFreeMemory(device, volume->GetProbeDataMemory(), nullptr);

                // Texture Views
                vkDestroyImageView(device, volume->GetProbeRayDataView(), nullptr);
                vkDestroyImageView(device, volume->GetProbeIrradianceView(), nullptr);
                vkDestroyImageView(device, volume->GetProbeDistanceView(), nullptr);
                vkDestroyImageView(device, volume->GetProbeDataView(), nullptr);

                // Shader modules
                vkDestroyShaderModule(device, volume->GetProbeBlendingIrradianceModule(), nullptr);
                vkDestroyShaderModule(device, volume->GetProbeBlendingDistanceModule(), nullptr);
                vkDestroyShaderModule(device, volume->GetProbeBorderRowUpdateIrradianceModule(), nullptr);
                vkDestroyShaderModule(device, volume->GetProbeBorderColumnUpdateIrradianceModule(), nullptr);
                vkDestroyShaderModule(device, volume->GetProbeBorderRowUpdateDistanceModule(), nullptr);
                vkDestroyShaderModule(device, volume->GetProbeBorderColumnUpdateDistanceModule(), nullptr);

                vkDestroyShaderModule(device, volume->GetProbeRelocationModule(), nullptr);
                vkDestroyShaderModule(device, volume->GetProbeRelocationResetModule(), nullptr);

                vkDestroyShaderModule(device, volume->GetProbeClassificationModule(), nullptr);
                vkDestroyShaderModule(device, volume->GetProbeClassificationResetModule(), nullptr);

                // Pipelines
                vkDestroyPipeline(device, volume->GetProbeBlendingIrradiancePipeline(), nullptr);
                vkDestroyPipeline(device, volume->GetProbeBlendingDistancePipeline(), nullptr);
                vkDestroyPipeline(device, volume->GetProbeBorderRowUpdateIrradiancePipeline(), nullptr);
                vkDestroyPipeline(device, volume->GetProbeBorderColumnUpdateIrradiancePipeline(), nullptr);
                vkDestroyPipeline(device, volume->GetProbeBorderRowUpdateDistancePipeline(), nullptr);
                vkDestroyPipeline(device, volume->GetProbeBorderColumnUpdateDistancePipeline(), nullptr);

                vkDestroyPipeline(device, volume->GetProbeRelocationPipeline(), nullptr);
                vkDestroyPipeline(device, volume->GetProbeRelocationResetPipeline(), nullptr);

                vkDestroyPipeline(device, volume->GetProbeClassificationPipeline(), nullptr);
                vkDestroyPipeline(device, volume->GetProbeClassificationResetPipeline(), nullptr);

                // Clear pointers
                volume->Destroy();
            }
        #endif

            //----------------------------------------------------------------------------------------------------------
            // DDGIVolume Creation Helper Functions
            //----------------------------------------------------------------------------------------------------------

            /**
             * Populates a DDGIVolumeDesc structure from configuration data.
             */
            void GetDDGIVolumeDesc(const Configs::DDGIVolume& config, DDGIVolumeDesc& volumeDesc)
            {
                volumeDesc.name = config.name;
                volumeDesc.index = config.index;
                volumeDesc.origin = { config.origin.x, config.origin.y, config.origin.z };
                volumeDesc.eulerAngles = { config.eulerAngles.x, config.eulerAngles.y, config.eulerAngles.z, };
                volumeDesc.probeSpacing = { config.probeSpacing.x, config.probeSpacing.y, config.probeSpacing.z };
                volumeDesc.probeCounts = { config.probeCounts.x, config.probeCounts.y, config.probeCounts.z, };
                volumeDesc.probeNumRays = config.probeNumRays;
                volumeDesc.probeNumIrradianceTexels = config.probeNumIrradianceTexels;
                volumeDesc.probeNumDistanceTexels = config.probeNumDistanceTexels;
                volumeDesc.probeHysteresis = config.probeHysteresis;
                volumeDesc.probeNormalBias = config.probeNormalBias;
                volumeDesc.probeViewBias = config.probeViewBias;
                volumeDesc.probeMaxRayDistance = config.probeMaxRayDistance;
                volumeDesc.probeIrradianceThreshold = config.probeIrradianceThreshold;
                volumeDesc.probeBrightnessThreshold = config.probeBrightnessThreshold;

                volumeDesc.showProbes = config.showProbes;

                volumeDesc.probeRayDataFormat = config.textureFormats.rayDataFormat;
                volumeDesc.probeIrradianceFormat = config.textureFormats.irradianceFormat;
                volumeDesc.probeDistanceFormat = config.textureFormats.distanceFormat;
                volumeDesc.probeDataFormat = config.textureFormats.dataFormat;

                volumeDesc.probeRelocationEnabled = config.probeRelocationEnabled;
                volumeDesc.probeMinFrontfaceDistance = config.probeMinFrontfaceDistance;

                volumeDesc.probeClassificationEnabled = config.probeClassificationEnabled;

                if (config.infiniteScrollingEnabled)
                {
                    volumeDesc.movementType = EDDGIVolumeMovementType::Scrolling;
                }
                else
                {
                    volumeDesc.movementType = EDDGIVolumeMovementType::Default;
                }
            }

            /**
             * Populates a DDGIVolumeResource structure.
             * In unmanaged resource mode, the application creates DDGIVolume graphics resources in CreateDDGIVolumeResources().
             * In managed resource mode, the RTXGI SDK creates DDGIVolume graphics resources.
             */
            bool GetDDGIVolumeResources(
                Globals& vk,
                GlobalResources& vkResources,
                Resources& resources,
                const DDGIVolumeDesc& volumeDesc,
                DDGIVolumeResources& volumeResources,
                std::vector<Shaders::ShaderProgram>& volumeShaders,
                std::ofstream& log)
            {
                std::string msg;

                // Load and compile the volume's shaders
                msg = "failed to compile shaders for DDGIVolume[" + std::to_string(volumeDesc.index) + "] (\"" + volumeDesc.name + "\")!\n";
                CHECK(Graphics::DDGI::CompileDDGIVolumeShaders(vk, volumeDesc, volumeShaders, true, log), msg.c_str(), log);

                // Pass an offset to where the DDGIConstants are located in the push constants block when in bindless mode
            #if RTXGI_DDGI_BINDLESS_RESOURCES
                volumeResources.descriptorBindlessDesc.pushConstantsOffset = GlobalConstants::GetAlignedSizeInBytes();
            #endif

                // Pass the indices of the volume's UAV and SRV locations in bindless resource arrays.
                // This value is set here since the Test Harness *always* access resources bindlessly when ray tracing. See RayTraceVolume().
                // When the SDK shaders are not in bindless mode, they ignore these values.
                volumeResources.descriptorBindlessDesc.uavOffset = RWTex2DIndices::DDGI_VOLUME;
                volumeResources.descriptorBindlessDesc.srvOffset = Tex2DIndices::DDGI_VOLUME;

                // Pass valid constants structured buffer pointers
                volumeResources.constantsBuffer = resources.constantsSTB;
                volumeResources.constantsBufferUpload = resources.constantsSTBUpload;
                volumeResources.constantsBufferUploadMemory = resources.constantsSTBUploadMemory;
                volumeResources.constantsBufferSizeInBytes = resources.constantsSTBSizeInBytes;

            #if RTXGI_DDGI_RESOURCE_MANAGEMENT
                // Enable "Managed Mode", the RTXGI SDK creates graphics objects
                volumeResources.managed.enabled = true;

                // Pass the Vulkan device and physical device to use for resource creation and memory allocation
                // Pass a valid descriptor pool to use for pipeline/descriptor layout creation
                volumeResources.managed.device = vk.device;
                volumeResources.managed.physicalDevice = vk.physicalDevice;
                volumeResources.managed.descriptorPool = vkResources.descriptorPool;

                // Pass compiled shader bytecode
                assert(volumeShaders.size() >= 6);
                volumeResources.managed.probeBlendingIrradianceCS = { volumeShaders[0].bytecode->GetBufferPointer(), volumeShaders[0].bytecode->GetBufferSize() };
                volumeResources.managed.probeBlendingDistanceCS = { volumeShaders[1].bytecode->GetBufferPointer(), volumeShaders[1].bytecode->GetBufferSize() };
                volumeResources.managed.probeBorderRowUpdateIrradianceCS = { volumeShaders[2].bytecode->GetBufferPointer(), volumeShaders[2].bytecode->GetBufferSize() };
                volumeResources.managed.probeBorderColumnUpdateIrradianceCS = { volumeShaders[3].bytecode->GetBufferPointer(), volumeShaders[3].bytecode->GetBufferSize() };
                volumeResources.managed.probeBorderRowUpdateDistanceCS = { volumeShaders[4].bytecode->GetBufferPointer(), volumeShaders[4].bytecode->GetBufferSize() };
                volumeResources.managed.probeBorderColumnUpdateDistanceCS = { volumeShaders[5].bytecode->GetBufferPointer(), volumeShaders[5].bytecode->GetBufferSize() };

                assert(volumeShaders.size() >= 8);
                volumeResources.managed.probeRelocation.updateCS = { volumeShaders[6].bytecode->GetBufferPointer(), volumeShaders[6].bytecode->GetBufferSize() };
                volumeResources.managed.probeRelocation.resetCS = { volumeShaders[7].bytecode->GetBufferPointer(), volumeShaders[7].bytecode->GetBufferSize() };

                assert(volumeShaders.size() == 10);
                volumeResources.managed.probeClassification.updateCS = { volumeShaders[8].bytecode->GetBufferPointer(), volumeShaders[8].bytecode->GetBufferSize() };
                volumeResources.managed.probeClassification.resetCS = { volumeShaders[9].bytecode->GetBufferPointer(), volumeShaders[9].bytecode->GetBufferSize() };
            #else
                // Enable "Unmanaged Mode", the application creates graphics objects
                volumeResources.unmanaged.enabled = true;

                // Create the volume's resources
                msg = "failed to create resources for DDGIVolume[" + std::to_string(volumeDesc.index) + "] (\"" + volumeDesc.name + "\")!\n";
                CHECK(CreateDDGIVolumeResources(vk, vkResources, resources, volumeDesc, volumeResources, volumeShaders, log), msg.c_str(), log);
            #endif

                return true;
            }

            /**
             * Create a DDGIVolume.
             */
            bool CreateDDGIVolume(
                Globals& vk,
                GlobalResources& vkResources,
                Resources& resources,
                const Configs::DDGIVolume& volumeConfig,
                std::ofstream& log)
            {
                // Destroy the volume if one already exists at the given index
                if (volumeConfig.index < static_cast<uint32_t>(resources.volumes.size()))
                {
                    if (resources.volumes[volumeConfig.index])
                    {
                    #if RTXGI_DDGI_RESOURCE_MANAGEMENT
                        resources.volumes[volumeConfig.index]->Destroy();
                    #else
                        DestroyDDGIVolumeResources(vk.device, resources, volumeConfig.index);
                    #endif
                        delete resources.volumes[volumeConfig.index];
                        resources.volumes[volumeConfig.index] = nullptr;
                    }
                }
                else
                {
                    resources.volumes.emplace_back();
                }

                // Describe the DDGIVolume's properties
                DDGIVolumeDesc volumeDesc;
                GetDDGIVolumeDesc(volumeConfig, volumeDesc);

                // Describe the DDGIVolume's resources and shaders
                DDGIVolumeResources volumeResources;
                std::vector<Shaders::ShaderProgram> volumeShaders;
                GetDDGIVolumeResources(vk, vkResources, resources, volumeDesc, volumeResources, volumeShaders, log);

                // Create a new DDGIVolume
                DDGIVolume* volume = new DDGIVolume();

            #if RTXGI_DDGI_RESOURCE_MANAGEMENT
                ERTXGIStatus status = volume->Create(vk.cmdBuffer[vk.frameIndex], volumeDesc, volumeResources);
            #else
                ERTXGIStatus status = volume->Create(volumeDesc, volumeResources);
            #endif
                if (status != ERTXGIStatus::OK)
                {
                    log << "\nError: failed to create the DDGIVolume!\n";
                    return false;
                }

                // Store the volume's pointer
                resources.volumes[volumeConfig.index] = volume;

                // Release the volume's shader bytecode
                for (size_t shaderIndex = 0; shaderIndex < volumeShaders.size(); shaderIndex++)
                {
                    volumeShaders[shaderIndex].Release();
                }
                volumeShaders.clear();

                return true;
            }

            /**
             * Creates the DDGIVolume constants structured buffer.
             */
            bool CreateDDGIVolumeConstantsBuffer(Globals& vk, GlobalResources& vkResources, Resources& resources, uint32_t volumeCount, std::ofstream& log)
            {
                resources.constantsSTBSizeInBytes = sizeof(DDGIVolumeDescGPUPacked) * volumeCount;
                if (resources.constantsSTBSizeInBytes == 0) return true; // scenes with no DDGIVolumes are valid

                // Create the DDGIVolume constants upload buffer resource (double buffered)
                BufferDesc desc = { 2 * resources.constantsSTBSizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
                CHECK(CreateBuffer(vk, desc, &resources.constantsSTBUpload, &resources.constantsSTBUploadMemory), "create DDGIVolume constants upload structured buffer!\n", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.constantsSTBUpload), "DDGIVolume Constants Upload Structured Buffer", VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.constantsSTBUploadMemory), "DDGIVolume Constants Upload Structured Buffer Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
            #endif

                // Create the DDGIVolume constants device buffer resource
                desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                desc.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                CHECK(CreateBuffer(vk, desc, &resources.constantsSTB, &resources.constantsSTBMemory), "create DDGIVolume constants structured buffer!\n", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.constantsSTB), "DDGIVolume Constants Structured Buffer", VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.constantsSTBMemory), "DDGIVolume Constants Structured Buffer Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
            #endif

                return true;
            }

            //----------------------------------------------------------------------------------------------------------
            // Private Functions
            //----------------------------------------------------------------------------------------------------------

            bool CreateTextures(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                // Release existing output texture
                vkDestroyImage(vk.device, resources.output, nullptr);
                vkDestroyImageView(vk.device, resources.outputView, nullptr);
                vkFreeMemory(vk.device, resources.outputMemory, nullptr);

                // Create the output (R16G16B16A16_FLOAT) texture resource
                TextureDesc desc = { static_cast<uint32_t>(vk.width), static_cast<uint32_t>(vk.height), 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT };
                CHECK(CreateTexture(vk, desc, &resources.output, &resources.outputMemory, &resources.outputView), "create DDGI output texture resource!\n", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.output), "DDGI Output", VK_OBJECT_TYPE_IMAGE);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.outputMemory), "DDGI Output Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.outputView), "DDGI Output View", VK_OBJECT_TYPE_IMAGE_VIEW);
            #endif

                // Store an alias of the DDGI Output resource in the global render targets struct
                vkResources.rt.DDGIOutputView = resources.outputView;

                // Transition the texture for general use
                ImageBarrierDesc barrier =
                {
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_GENERAL,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
                };
                SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], resources.output, barrier);

                return true;
            }

            bool LoadAndCompileShaders(Globals& vk, Resources& resources, UINT numVolumes, std::ofstream& log)
            {
                // Release existing shaders
                resources.rtShaders.Release();
                resources.indirectCS.Release();

                std::wstring root = std::wstring(vk.shaderCompiler.root.begin(), vk.shaderCompiler.root.end());

                // Load and compile the ray generation shader
                {
                    resources.rtShaders.rgs.filepath = root + L"shaders/ddgi/ProbeTraceRGS.hlsl";
                    resources.rtShaders.rgs.entryPoint = L"RayGen";
                    resources.rtShaders.rgs.exportName = L"DDGIProbeTraceRGS";
                    resources.rtShaders.rgs.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };

                    Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_COORDINATE_SYSTEM", std::to_wstring(RTXGI_COORDINATE_SYSTEM));

                    CHECK(Shaders::Compile(vk.shaderCompiler, resources.rtShaders.rgs, true), "compile DDGI probe tracing ray generation shader!\n", log);
                }

                // Load and compile the miss shader
                {
                    resources.rtShaders.miss.filepath = root + L"shaders/Miss.hlsl";
                    resources.rtShaders.miss.entryPoint = L"Miss";
                    resources.rtShaders.miss.exportName = L"DDGIProbeTraceMiss";
                    resources.rtShaders.miss.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };
                    CHECK(Shaders::Compile(vk.shaderCompiler, resources.rtShaders.miss, true), "compile DDGI probe tracing miss shader!\n", log);
                }

                // Add the hit group
                {
                    resources.rtShaders.hitGroups.emplace_back();

                    Shaders::ShaderRTHitGroup& group = resources.rtShaders.hitGroups[0];
                    group.exportName = L"DDGIProbeTraceHitGroup";

                    // Load and compile the CHS
                    group.chs.filepath = root + L"shaders/CHS.hlsl";
                    group.chs.entryPoint = L"CHS_GI";
                    group.chs.exportName = L"DDGIProbeTraceCHS";
                    group.chs.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };
                    CHECK(Shaders::Compile(vk.shaderCompiler, group.chs, true), "compile DDGI probe tracing closest hit shader!\n", log);

                    // Load and compile the AHS
                    group.ahs.filepath = root + L"shaders/AHS.hlsl";
                    group.ahs.entryPoint = L"AHS_GI";
                    group.ahs.exportName = L"DDGIProbeTraceAHS";
                    group.ahs.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };
                    CHECK(Shaders::Compile(vk.shaderCompiler, group.ahs, true), "compile DDGI probe tracing any hit shader!\n", log);

                    // Set the payload size
                    resources.rtShaders.payloadSizeInBytes = sizeof(PackedPayload);
                }

                // Load and compile the indirect lighting compute shader
                {
                    std::wstring shaderPath = root + L"shaders/IndirectCS.hlsl";
                    resources.indirectCS.filepath = shaderPath.c_str();
                    resources.indirectCS.entryPoint = L"CS";
                    resources.indirectCS.targetProfile = L"cs_6_0";
                    resources.indirectCS.arguments = { L"-spirv", L"-D SPIRV=1", L"-fspv-target-env=vulkan1.2" };

                    Shaders::AddDefine(resources.indirectCS, L"RTXGI_COORDINATE_SYSTEM", std::to_wstring(RTXGI_COORDINATE_SYSTEM));
                    Shaders::AddDefine(resources.indirectCS, L"RTXGI_DDGI_NUM_VOLUMES", std::to_wstring(numVolumes));
                    Shaders::AddDefine(resources.indirectCS, L"THGP_DIM_X", L"8");
                    Shaders::AddDefine(resources.indirectCS, L"THGP_DIM_Y", L"4");

                    CHECK(Shaders::Compile(vk.shaderCompiler, resources.indirectCS, true), "compile indirect lighting compute shader!\n", log);
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
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.descriptorSet), "DDGI Descriptor Set", VK_OBJECT_TYPE_DESCRIPTOR_SET);
            #endif
                return true;
            }

            bool CreatePipelines(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                // Release existing shader modules and pipeline
                resources.rtShaderModules.Release(vk.device);
                vkDestroyShaderModule(vk.device, resources.indirectShaderModule, nullptr);
                vkDestroyPipeline(vk.device, resources.rtPipeline, nullptr);
                vkDestroyPipeline(vk.device, resources.indirectPipeline, nullptr);

                // Create the RT pipeline shader modules
                CHECK(CreateRayTracingShaderModules(vk.device, resources.rtShaders, resources.rtShaderModules), "create DDGI RT shader modules!\n", log);

                // Create the indirect lighting shader module
                CHECK(CreateShaderModule(vk.device, resources.indirectCS, &resources.indirectShaderModule), "create DDGI indirect lighting shader module!\n", log);

                // Create the RT pipeline
                CHECK(CreateRayTracingPipeline(
                    vk.device,
                    vkResources.pipelineLayout,
                    resources.rtShaders,
                    resources.rtShaderModules,
                    &resources.rtPipeline),
                    "create DDGI RT pipeline!\n", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.rtPipeline), "DDGI RT Pipeline", VK_OBJECT_TYPE_PIPELINE);
            #endif

                // Create the indirect lighting pipeline
                CHECK(CreateComputePipeline(
                    vk.device,
                    vkResources.pipelineLayout,
                    resources.indirectCS,
                    resources.indirectShaderModule,
                    &resources.indirectPipeline), "create indirect lighting PSO!\n", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.indirectPipeline), "DDGI Indirect Lighting Pipeline", VK_OBJECT_TYPE_PIPELINE);
            #endif

                return true;
            }

            bool CreateShaderTable(Globals& vk, Resources& resources, std::ofstream& log)
            {
                // The Shader Table layout is as follows:
                //    Entry 0:  DDGI Ray Generation Shader
                //    Entry 1:  DDGI Miss Shader
                //    Entry 2+: DDGI HitGroups
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

                // Create the shader table upload buffer resource and memory
                BufferDesc desc = { resources.shaderTableSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
                CHECK(CreateBuffer(vk, desc, &resources.shaderTableUpload, &resources.shaderTableUploadMemory), "create RTAO shader table upload resources!", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTableUpload), "DDGI Shader Table Upload", VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTableUploadMemory), "DDGI Shader Table Upload Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
            #endif

                // Create the shader table device buffer resource and memory
                desc = { resources.shaderTableSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
                CHECK(CreateBuffer(vk, desc, &resources.shaderTable, &resources.shaderTableMemory), "create DDGI shader table resources!", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTable), "DDGI Shader Table", VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.shaderTableMemory), "DDGI Shader Table Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
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

                // Write shader table records
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
                std::vector<VkWriteDescriptorSet> writeDescriptorSets;

                // Samplers
                VkDescriptorImageInfo samplersInfo[] =
                {
                    { vkResources.samplers[0], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED }  // bilinear sampler
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

                // RWTex2D UAVs
                // DDGIOutput and DDGIVolume storage images
                std::vector<VkDescriptorImageInfo> rwTex2DInfo;
                rwTex2DInfo.push_back({ VK_NULL_HANDLE, vkResources.rt.GBufferAView, VK_IMAGE_LAYOUT_GENERAL });
                rwTex2DInfo.push_back({ VK_NULL_HANDLE, vkResources.rt.GBufferBView, VK_IMAGE_LAYOUT_GENERAL });
                rwTex2DInfo.push_back({ VK_NULL_HANDLE, vkResources.rt.GBufferCView, VK_IMAGE_LAYOUT_GENERAL });
                rwTex2DInfo.push_back({ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL }); // GBufferD
                rwTex2DInfo.push_back({ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL }); // RTAOOutput
                rwTex2DInfo.push_back({ VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL }); // RTAORaw
                rwTex2DInfo.push_back({ VK_NULL_HANDLE, resources.outputView, VK_IMAGE_LAYOUT_GENERAL });
                for (uint32_t volumeIndex = 0; volumeIndex < static_cast<uint32_t>(resources.volumes.size()); volumeIndex++)
                {
                    DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[volumeIndex]);

                    rwTex2DInfo.push_back({ VK_NULL_HANDLE, volume->GetProbeRayDataView(), VK_IMAGE_LAYOUT_GENERAL });
                    rwTex2DInfo.push_back({ VK_NULL_HANDLE, volume->GetProbeIrradianceView(), VK_IMAGE_LAYOUT_GENERAL });
                    rwTex2DInfo.push_back({ VK_NULL_HANDLE, volume->GetProbeDistanceView(), VK_IMAGE_LAYOUT_GENERAL });
                    rwTex2DInfo.push_back({ VK_NULL_HANDLE, volume->GetProbeDataView(), VK_IMAGE_LAYOUT_GENERAL });
                }

                VkWriteDescriptorSet rwTex2DSet = {};
                rwTex2DSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                rwTex2DSet.dstSet = resources.descriptorSet;
                rwTex2DSet.dstBinding = DescriptorLayoutBindings::UAV_START;
                rwTex2DSet.dstArrayElement = RWTex2DIndices::GBUFFERA;
                rwTex2DSet.descriptorCount = static_cast<uint32_t>(rwTex2DInfo.size());
                rwTex2DSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                rwTex2DSet.pImageInfo = rwTex2DInfo.data();

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

                // Tex2D SRVs
                // DDGIOutput and DDGIVolume storage images
                std::vector<VkDescriptorImageInfo> tex2DInfo;
                for (uint32_t volumeIndex = 0; volumeIndex < static_cast<uint32_t>(resources.volumes.size()); volumeIndex++)
                {
                    DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[volumeIndex]);

                    tex2DInfo.push_back({ VK_NULL_HANDLE, volume->GetProbeRayDataView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
                    tex2DInfo.push_back({ VK_NULL_HANDLE, volume->GetProbeIrradianceView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
                    tex2DInfo.push_back({ VK_NULL_HANDLE, volume->GetProbeDistanceView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
                    tex2DInfo.push_back({ VK_NULL_HANDLE, volume->GetProbeDataView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
                }

                VkWriteDescriptorSet tex2DSet = {};
                tex2DSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                tex2DSet.dstSet = resources.descriptorSet;
                tex2DSet.dstBinding = DescriptorLayoutBindings::SRV_START;
                tex2DSet.dstArrayElement = Tex2DIndices::DDGI_VOLUME;
                tex2DSet.descriptorCount = static_cast<uint32_t>(tex2DInfo.size());
                tex2DSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                tex2DSet.pImageInfo = tex2DInfo.data();

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

            void RayTraceVolumes(Globals& vk, GlobalResources& vkResources, Resources& resources)
            {
            #ifdef GFX_PERF_MARKERS
                AddPerfMarker(vk, GFX_PERF_MARKER_GREEN, "Ray Trace Volumes");
            #endif

                // Bind the descriptor set
                vkCmdBindDescriptorSets(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vkResources.pipelineLayout, 0, 1, &resources.descriptorSet, 0, nullptr);

                // Update the global push constants
                uint32_t offset = 0;
                GlobalConstants consts = vkResources.constants;
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, AppConsts::GetAlignedSizeInBytes(), consts.app.GetData());
                offset += AppConsts::GetAlignedSizeInBytes();
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, PathTraceConsts::GetAlignedSizeInBytes(), consts.pt.GetData());
                offset += PathTraceConsts::GetAlignedSizeInBytes();
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, LightingConsts::GetAlignedSizeInBytes(), consts.lights.GetData());

                offset = GlobalConstants::GetAlignedSizeInBytes();

                // Bind the pipeline
                vkCmdBindPipeline(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, resources.rtPipeline);

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

                // Constants
                DDGIConstants ddgi = {};
                ddgi.uavOffset = RWTex2DIndices::DDGI_VOLUME;
                ddgi.srvOffset = Tex2DIndices::DDGI_VOLUME;

                // Barriers
                std::vector<VkImageMemoryBarrier> barriers;
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.oldLayout = barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

                uint32_t volumeIndex;
                for (volumeIndex = 0; volumeIndex < static_cast<uint32_t>(resources.selectedVolumes.size()); volumeIndex++)
                {
                    const DDGIVolume* volume = resources.selectedVolumes[volumeIndex];

                    // Update the DDGI push constants
                    ddgi.volumeIndex = volume->GetIndex();
                    vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, DDGIConstants::GetSizeInBytes(), ddgi.GetData());

                    // Trace probe rays
                    vkCmdTraceRaysKHR(
                        vk.cmdBuffer[vk.frameIndex],
                        &raygenRegion,
                        &missRegion,
                        &hitRegion,
                        &callableRegion,
                        volume->GetNumRaysPerProbe(),
                        volume->GetNumProbes(),
                        1);

                    // Barrier(s)
                    barrier.image = volume->GetProbeRayData();
                    barriers.push_back(barrier);
                }

                // Wait for the ray traces to complete
                vkCmdPipelineBarrier(
                    vk.cmdBuffer[vk.frameIndex],
                    VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                    VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                    0,
                    0, nullptr,
                    0, nullptr,
                    static_cast<uint32_t>(barriers.size()), barriers.data());

            #ifdef GFX_PERF_MARKERS
                vkCmdEndDebugUtilsLabelEXT(vk.cmdBuffer[vk.frameIndex]);
            #endif
            }

            //----------------------------------------------------------------------------------------------------------
            // Public Functions
            //----------------------------------------------------------------------------------------------------------

            /**
             * Create resources used by the DDGI passes.
             */
            bool Initialize(Globals& vk, GlobalResources& vkResources, Resources& resources, const Configs::Config& config, Instrumentation::Performance& perf, std::ofstream& log)
            {
                // Reset the command list before initialization
                CHECK(ResetCmdList(vk), "reset command list!", log);

                uint32_t numVolumes = static_cast<uint32_t>(config.ddgi.volumes.size());

                if (!CreateTextures(vk, vkResources, resources, log)) return false;
                if (!LoadAndCompileShaders(vk, resources, numVolumes, log)) return false;
                if (!CreateDescriptorSets(vk, vkResources, resources, log)) return false;
                if (!CreatePipelines(vk, vkResources, resources, log)) return false;
                if (!CreateShaderTable(vk, resources, log)) return false;

                // Create the DDGIVolume pipeline layout and descriptor sets
            #if !RTXGI_DDGI_RESOURCE_MANAGEMENT && !RTXGI_DDGI_BINDLESS_RESOURCES
                if (!CreateDDGIVolumeLayouts(vk, resources)) return false;
                if (!CreateDDGIVolumeDescriptorSets(vk, vkResources, resources, numVolumes)) return false;
            #endif

                // Create the DDGIVolume constants structured buffer
                if (!CreateDDGIVolumeConstantsBuffer(vk, vkResources, resources, numVolumes, log)) return false;

                // Initialize the DDGIVolumes
                for (uint32_t volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
                {
                    Configs::DDGIVolume volumeConfig = config.ddgi.volumes[volumeIndex];
                    if (!CreateDDGIVolume(vk, vkResources, resources, volumeConfig, log)) return false;
                }

                // Initialize the shader table and bindless descriptor set
                if (!UpdateShaderTable(vk, vkResources, resources, log)) return false;
                if (!UpdateDescriptorSets(vk, vkResources, resources, log)) return false;

                // Update the volume descriptor sets (when in unmanaged, bound resources mode)
            #if !RTXGI_DDGI_RESOURCE_MANAGEMENT && !RTXGI_DDGI_BINDLESS_RESOURCES
                UpdateDDGIVolumeDescriptorSets(vk, resources);
            #endif

                // Setup performance stats
                perf.AddStat("DDGI", resources.cpuStat, resources.gpuStat);
                resources.classifyStat = perf.AddGPUStat("  Classify");
                resources.rtStat = perf.AddGPUStat("  Probe Trace");
                resources.blendStat = perf.AddGPUStat("  Blend");
                resources.relocateStat = perf.AddGPUStat("  Relocate");
                resources.lightingStat = perf.AddGPUStat("  Lighting");

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
             * Reload and compile shaders, recreate shader modules and pipelines, and recreate the shader table.
             */
            bool Reload(Globals& vk, GlobalResources& vkResources, Resources& resources, const Configs::Config& config, std::ofstream& log)
            {
                log << "Reloading DDGI shaders...";
                if (!LoadAndCompileShaders(vk, resources, static_cast<uint32_t>(config.ddgi.volumes.size()), log)) return false;
                if (!CreatePipelines(vk, vkResources, resources, log)) return false;
                if (!UpdateShaderTable(vk, vkResources, resources, log)) return false;

                // Reinitialize the DDGIVolumes
                for (uint32_t volumeIndex = 0; volumeIndex < static_cast<uint32_t>(resources.volumes.size()); volumeIndex++)
                {
                    Configs::DDGIVolume volumeConfig = config.ddgi.volumes[volumeIndex];
                    if (!CreateDDGIVolume(vk, vkResources, resources, volumeConfig, log)) return false;
                }
                if (!UpdateDescriptorSets(vk, vkResources, resources, log)) return false;

                log << "done.\n";
                log << std::flush;

                return true;
            }

            /**
             * Resize screen-space buffers and update descriptor sets.
             */
            bool Resize(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
            {
                if (!CreateTextures(vk, vkResources, resources, log)) return false;
                if (!UpdateDescriptorSets(vk, vkResources, resources, log)) return false;
                log << "DDGI resize, " << vk.width << "x" << vk.height << "\n";
                std::flush(log);
                return true;
            }

            /**
             * Update data before execute.
             */
            void Update(Globals& vk, GlobalResources& vkResources, Resources& resources, Configs::Config& config)
            {
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);

                resources.enabled = config.ddgi.enabled;
                if (resources.enabled)
                {
                    // Path Trace constants
                    vkResources.constants.pt.rayNormalBias = config.pathTrace.rayNormalBias;
                    vkResources.constants.pt.rayViewBias = config.pathTrace.rayViewBias;

                    // Clear the selected volume, if necessary
                    if (config.ddgi.volumes[config.ddgi.selectedVolume].clearProbes)
                    {
                        DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[config.ddgi.selectedVolume]);
                        volume->ClearProbes(vk.cmdBuffer[vk.frameIndex]);

                        config.ddgi.volumes[config.ddgi.selectedVolume].clearProbes = 0;
                    }

                    // Select the active volumes
                    resources.selectedVolumes.clear();
                    for (UINT volumeIndex = 0; volumeIndex < static_cast<UINT>(resources.volumes.size()); volumeIndex++)
                    {
                        // TODO: processing to determine which volumes are in-frustum, active, and prioritized for update / render
                        // For now, just select all volumes
                        DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[volumeIndex]);
                        resources.selectedVolumes.push_back(volume);
                    }

                    // Update the DDGIVolume constants
                    for (uint32_t volumeIndex = 0; volumeIndex < static_cast<uint32_t>(resources.volumes.size()); volumeIndex++)
                    {
                        resources.selectedVolumes[volumeIndex]->Update();
                    }
                }
                CPU_TIMESTAMP_END(resources.cpuStat);
            }

            /**
             * Record the graphics workload to the global command list.
             */
            void Execute(Globals& vk, GlobalResources& vkResources, Resources& resources)
            {
            #ifdef GFX_PERF_MARKERS
                AddPerfMarker(vk, GFX_PERF_MARKER_GREEN, "RTXGI: DDGI");
            #endif
                CPU_TIMESTAMP_BEGIN(resources.cpuStat);
                GPU_TIMESTAMP_BEGIN(resources.gpuStat->GetQueryBeginIndex());
                if (resources.enabled)
                {
                    UINT numVolumes = static_cast<UINT>(resources.selectedVolumes.size());

                    // Upload volume constants
                    rtxgi::vulkan::UploadDDGIVolumeConstants(vk.device, vk.cmdBuffer[vk.frameIndex], vk.frameIndex, numVolumes, resources.selectedVolumes.data());

                    // Trace rays from probes to sample the environment
                    GPU_TIMESTAMP_BEGIN(resources.rtStat->GetQueryBeginIndex());
                    RayTraceVolumes(vk, vkResources, resources);
                    GPU_TIMESTAMP_END(resources.rtStat->GetQueryEndIndex());

                    // Update volume probes
                    GPU_TIMESTAMP_BEGIN(resources.blendStat->GetQueryBeginIndex());
                    rtxgi::vulkan::UpdateDDGIVolumeProbes(vk.cmdBuffer[vk.frameIndex], numVolumes, resources.selectedVolumes.data());
                    GPU_TIMESTAMP_END(resources.blendStat->GetQueryEndIndex());

                    // Relocate probes if the feature is enabled
                    GPU_TIMESTAMP_BEGIN(resources.relocateStat->GetQueryBeginIndex());
                    rtxgi::vulkan::RelocateDDGIVolumeProbes(vk.cmdBuffer[vk.frameIndex], numVolumes, resources.selectedVolumes.data());
                    GPU_TIMESTAMP_END(resources.relocateStat->GetQueryEndIndex());

                    // Classify probes if the feature is enabled
                    GPU_TIMESTAMP_BEGIN(resources.classifyStat->GetQueryBeginIndex());
                    rtxgi::vulkan::ClassifyDDGIVolumeProbes(vk.cmdBuffer[vk.frameIndex], numVolumes, resources.selectedVolumes.data());
                    GPU_TIMESTAMP_END(resources.classifyStat->GetQueryEndIndex());

                    // Render the indirect lighting to screen-space
                    {
                    #ifdef GFX_PERF_MARKERS
                        AddPerfMarker(vk, GFX_PERF_MARKER_GREEN, "Indirect Lighting");
                    #endif
                        GPU_TIMESTAMP_BEGIN(resources.lightingStat->GetQueryBeginIndex());

                        // Set the push constants
                        DDGIConstants ddgi = {};
                        ddgi.volumeIndex = 0;
                        ddgi.uavOffset = RWTex2DIndices::DDGI_VOLUME;
                        ddgi.srvOffset = Tex2DIndices::DDGI_VOLUME;
                        vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, GlobalConstants::GetAlignedSizeInBytes(), DDGIConstants::GetSizeInBytes(), ddgi.GetData());

                        // Bind the descriptor set
                        vkCmdBindDescriptorSets(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_COMPUTE, vkResources.pipelineLayout, 0, 1, &resources.descriptorSet, 0, nullptr);

                        // Bind the compute pipeline and dispatch threads
                        vkCmdBindPipeline(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_COMPUTE, resources.indirectPipeline);

                        // Dispatch threads
                        uint32_t groupsX = DivRoundUp(vk.width, 8);
                        uint32_t groupsY = DivRoundUp(vk.height, 4);
                        vkCmdDispatch(vk.cmdBuffer[vk.frameIndex], groupsX, groupsY, 1);

                        // Wait for the compute pass to finish
                        ImageBarrierDesc barrier = { VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };
                        SetImageMemoryBarrier(vk.cmdBuffer[vk.frameIndex], resources.output, barrier);

                        GPU_TIMESTAMP_END(resources.lightingStat->GetQueryEndIndex());
                    #ifdef GFX_PERF_MARKERS
                        vkCmdEndDebugUtilsLabelEXT(vk.cmdBuffer[vk.frameIndex]);
                    #endif
                    }
                }

                GPU_TIMESTAMP_END(resources.gpuStat->GetQueryEndIndex());
                CPU_TIMESTAMP_ENDANDRESOLVE(resources.cpuStat);
            #ifdef GFX_PERF_MARKERS
                vkCmdEndDebugUtilsLabelEXT(vk.cmdBuffer[vk.frameIndex]);
            #endif
            }

            /**
             * Release resources.
             */
            void Cleanup(VkDevice device, Resources& resources)
            {
                // Textures
                vkDestroyImage(device, resources.output, nullptr);
                vkDestroyImageView(device, resources.outputView, nullptr);
                vkFreeMemory(device, resources.outputMemory, nullptr);

                // Shader Table
                vkDestroyBuffer(device, resources.shaderTableUpload, nullptr);
                vkFreeMemory(device, resources.shaderTableUploadMemory, nullptr);
                vkDestroyBuffer(device, resources.shaderTable, nullptr);
                vkFreeMemory(device, resources.shaderTableMemory, nullptr);

                // Pipelines
                vkDestroyPipeline(device, resources.rtPipeline, nullptr);
                vkDestroyPipeline(device, resources.indirectPipeline, nullptr);

                // Shaders
                resources.rtShaderModules.Release(device);
                resources.rtShaders.Release();
                vkDestroyShaderModule(device, resources.indirectShaderModule, nullptr);
                resources.indirectCS.Release();

                resources.shaderTableSize = 0;
                resources.shaderTableRecordSize = 0;
                resources.shaderTableMissTableSize = 0;
                resources.shaderTableHitGroupTableSize = 0;

                resources.shaderTableRGSStartAddress = 0;
                resources.shaderTableMissTableStartAddress = 0;
                resources.shaderTableHitGroupTableStartAddress = 0;

                // Constants
                vkDestroyBuffer(device, resources.constantsSTBUpload, nullptr);
                vkFreeMemory(device, resources.constantsSTBUploadMemory, nullptr);
                vkDestroyBuffer(device, resources.constantsSTB, nullptr);
                vkFreeMemory(device, resources.constantsSTBMemory, nullptr);

                // DDGIVolumes layouts and descriptor set
            #if !RTXGI_DDGI_RESOURCE_MANAGEMENT && !RTXGI_DDGI_BINDLESS_RESOURCES
                vkDestroyPipelineLayout(device, resources.volumePipelineLayout, nullptr);
                vkDestroyDescriptorSetLayout(device, resources.volumeDescriptorSetLayout, nullptr);

                resources.volumePipelineLayout = nullptr;
                resources.volumeDescriptorSetLayout = nullptr;
            #endif

                // Release volumes
                for (size_t volumeIndex = 0; volumeIndex < resources.volumes.size(); volumeIndex++)
                {
                #if !RTXGI_DDGI_RESOURCE_MANAGEMENT
                    DestroyDDGIVolumeResources(device, resources, volumeIndex);
                #endif
                    resources.volumes[volumeIndex]->Destroy();
                    delete resources.volumes[volumeIndex];
                    resources.volumes[volumeIndex] = nullptr;
                }
            }

        } // namespace Graphics::Vulkan::DDGI

    } // namespace Graphics::Vulkan

    namespace DDGI
    {

        bool Initialize(Globals& vk, GlobalResources& vkResources, Resources& resources, const Configs::Config& config, Instrumentation::Performance& perf, std::ofstream& log)
        {
            return Graphics::Vulkan::DDGI::Initialize(vk, vkResources, resources, config, perf, log);
        }

        bool Reload(Globals& vk, GlobalResources& vkResources, Resources& resources, const Configs::Config& config, std::ofstream& log)
        {
            return Graphics::Vulkan::DDGI::Reload(vk, vkResources, resources, config, log);
        }

        bool Resize(Globals& vk, GlobalResources& vkResources, Resources& resources, std::ofstream& log)
        {
            return Graphics::Vulkan::DDGI::Resize(vk, vkResources, resources, log);
        }

        void Update(Globals& vk, GlobalResources& vkResources, Resources& resources, Configs::Config& config)
        {
            return Graphics::Vulkan::DDGI::Update(vk, vkResources, resources, config);
        }

        void Execute(Globals& vk, GlobalResources& vkResources, Resources& resources)
        {
            return Graphics::Vulkan::DDGI::Execute(vk, vkResources, resources);
        }

        void Cleanup(Globals& vk, Resources& resources)
        {
            Graphics::Vulkan::DDGI::Cleanup(vk.device, resources);
        }

    } // namespace Graphics::DDGI
}
