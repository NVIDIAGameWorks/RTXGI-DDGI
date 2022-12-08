/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
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

#include <rtxgi/VulkanExtensions.h>

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
                if(objType == VK_OBJECT_TYPE_DEVICE_MEMORY) output.append(" Memory");
                else if(objType == VK_OBJECT_TYPE_IMAGE_VIEW) output.append(" View");
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
                VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
                VkPushConstantRange pushConstantRange = {};
                VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
                std::vector<VkDescriptorSetLayoutBinding> bindings;
                bindings.resize(GetDDGIVolumeLayoutBindingCount());

                // Fill out the layout descriptors
                GetDDGIVolumeLayoutDescs(descriptorSetLayoutCreateInfo, pushConstantRange, pipelineLayoutCreateInfo, bindings.data());

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
                    const DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[volumeIndex]);

                    // Store the data to be written to the descriptor set
                    VkWriteDescriptorSet* descriptor = nullptr;
                    std::vector<VkWriteDescriptorSet> descriptors;

                    // 0: Volume Constants StructuredBuffer
                    VkDescriptorBufferInfo volumeConstants = { resources.volumeConstantsSTB, 0, VK_WHOLE_SIZE };

                    descriptor = &descriptors.emplace_back();
                    descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptor->dstSet = *volume->GetDescriptorSetConstPtr();
                    descriptor->dstBinding = static_cast<uint32_t>(rtxgi::vulkan::EDDGIVolumeBindings::Constants);
                    descriptor->dstArrayElement = 0;
                    descriptor->descriptorCount = 1;
                    descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    descriptor->pBufferInfo = &volumeConstants;

                    // 1-4: Volume Texture Array UAVs
                    VkDescriptorImageInfo rwTex2D[] =
                    {
                        { VK_NULL_HANDLE, volume->GetProbeRayDataView(), VK_IMAGE_LAYOUT_GENERAL },
                        { VK_NULL_HANDLE, volume->GetProbeIrradianceView(), VK_IMAGE_LAYOUT_GENERAL },
                        { VK_NULL_HANDLE, volume->GetProbeDistanceView(), VK_IMAGE_LAYOUT_GENERAL },
                        { VK_NULL_HANDLE, volume->GetProbeDataView(), VK_IMAGE_LAYOUT_GENERAL }
                        { VK_NULL_HANDLE, volume->GetProbeVariabilityView(), VK_IMAGE_LAYOUT_GENERAL }
                        { VK_NULL_HANDLE, volume->GetProbeVariabilityAverageView(), VK_IMAGE_LAYOUT_GENERAL }
                    };

                    descriptor = &descriptors.emplace_back();
                    descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptor->dstSet = *volume->GetDescriptorSetConstPtr();
                    descriptor->dstBinding = static_cast<uint32_t>(EDDGIVolumeBindings::RayData);
                    descriptor->dstArrayElement = 0;
                    descriptor->descriptorCount = _countof(rwTex2D);
                    descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    descriptor->pImageInfo = rwTex2D;

                    // Update the descriptor set
                    vkUpdateDescriptorSets(vk.device, static_cast<uint32_t>(descriptors.size()), descriptors.data(), 0, nullptr);
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

                uint32_t arraySize = 0;
                // need to save averaging texture array size separately because it will be smaller for this texture, and arraySize is used below for barriers
                uint32_t variabilityAverageArraySize = 0;

                // Create the texture arrays
                {
                    uint32_t width = 0;
                    uint32_t height = 0;
                    VkFormat format = VK_FORMAT_UNDEFINED;

                    // Probe ray data texture
                    {
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::RayData, width, height, arraySize);
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::RayData, volumeDesc.probeRayDataFormat);

                        TextureDesc desc = { width, height, arraySize, 1, format, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT };
                        CHECK(CreateTexture(vk, desc, &volumeResources.unmanaged.probeRayData, &volumeResources.unmanaged.probeRayDataMemory, &volumeResources.unmanaged.probeRayDataView), "create DDGIVolume ray data texture array!", log);
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
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::Irradiance, width, height, arraySize);
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Irradiance, volumeDesc.probeIrradianceFormat);

                        TextureDesc desc = { width, height, arraySize, 1, format, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT };
                        CHECK(CreateTexture(vk, desc, &volumeResources.unmanaged.probeIrradiance, &volumeResources.unmanaged.probeIrradianceMemory, &volumeResources.unmanaged.probeIrradianceView), "create DDGIVolume irradiance texture array!", log);
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
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::Distance, width, height, arraySize);
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Distance, volumeDesc.probeDistanceFormat);

                        TextureDesc desc = { width, height, arraySize, 1, format, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT };
                        CHECK(CreateTexture(vk, desc, &volumeResources.unmanaged.probeDistance, &volumeResources.unmanaged.probeDistanceMemory, &volumeResources.unmanaged.probeDistanceView), "create DDGIVolume distance texture array!", log);
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
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::Data, width, height, arraySize);
                        if (width <= 0 || height <= 0) return false;
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Data, volumeDesc.probeDataFormat);

                        TextureDesc desc = { width, height, arraySize, 1, format, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT };
                        CHECK(CreateTexture(vk, desc, &volumeResources.unmanaged.probeData, &volumeResources.unmanaged.probeDataMemory, &volumeResources.unmanaged.probeDataView), "create DDGIVolume probe data texture!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string n = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Data";
                        std::string o = "";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeData), n.c_str(), VK_OBJECT_TYPE_IMAGE);
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeDataMemory), GetResourceName(n, o, VK_OBJECT_TYPE_DEVICE_MEMORY), VK_OBJECT_TYPE_DEVICE_MEMORY);
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeDataView), GetResourceName(n, o, VK_OBJECT_TYPE_IMAGE_VIEW), VK_OBJECT_TYPE_IMAGE_VIEW);
                    #endif
                    }

                    // Probe variability texture
                    {
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::Variability, width, height, arraySize);
                        if (width <= 0 || height <= 0) return false;
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Variability, volumeDesc.probeVariabilityFormat);

                        TextureDesc desc = { width, height, arraySize, 1, format, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT };
                        CHECK(CreateTexture(vk, desc, &volumeResources.unmanaged.probeVariability, &volumeResources.unmanaged.probeVariabilityMemory, &volumeResources.unmanaged.probeVariabilityView), "create DDGIVolume Probe variability texture!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string n = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Variability";
                        std::string o = "";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeVariability), n.c_str(), VK_OBJECT_TYPE_IMAGE);
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeVariabilityMemory), GetResourceName(n, o, VK_OBJECT_TYPE_DEVICE_MEMORY), VK_OBJECT_TYPE_DEVICE_MEMORY);
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeVariabilityView), GetResourceName(n, o, VK_OBJECT_TYPE_IMAGE_VIEW), VK_OBJECT_TYPE_IMAGE_VIEW);
                    #endif
                    }

                    // Probe variability average
                    {
                        GetDDGIVolumeTextureDimensions(volumeDesc, EDDGIVolumeTextureType::VariabilityAverage, width, height, variabilityAverageArraySize);
                        if (width <= 0 || height <= 0) return false;
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::VariabilityAverage, volumeDesc.probeVariabilityFormat);

                        TextureDesc desc = { width, height, variabilityAverageArraySize, 1, format, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT };
                        CHECK(CreateTexture(vk, desc, &volumeResources.unmanaged.probeVariabilityAverage, &volumeResources.unmanaged.probeVariabilityAverageMemory, &volumeResources.unmanaged.probeVariabilityAverageView), "create DDGIVolume Probe variability average texture!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string n = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Variability Average";
                        std::string o = "";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeVariabilityAverage), n.c_str(), VK_OBJECT_TYPE_IMAGE);
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeVariabilityAverageMemory), GetResourceName(n, o, VK_OBJECT_TYPE_DEVICE_MEMORY), VK_OBJECT_TYPE_DEVICE_MEMORY);
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeVariabilityAverageView), GetResourceName(n, o, VK_OBJECT_TYPE_IMAGE_VIEW), VK_OBJECT_TYPE_IMAGE_VIEW);
                    #endif
                        BufferDesc readbackDesc = { sizeof(float)*2, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT };
                        CHECK(CreateBuffer(vk, readbackDesc, &volumeResources.unmanaged.probeVariabilityReadback, &volumeResources.unmanaged.probeVariabilityReadbackMemory), "create DDGIVolume Probe variability readback buffer!", log);
                    #ifdef GFX_NAME_OBJECTS
                        n = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Variability Readback";
                        o = "";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeVariabilityReadback), n.c_str(), VK_OBJECT_TYPE_BUFFER);
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeVariabilityReadbackMemory), GetResourceName(n, o, VK_OBJECT_TYPE_DEVICE_MEMORY), VK_OBJECT_TYPE_DEVICE_MEMORY);
                    #endif
                    }
                }

                // Transition the resources for general use
                {
                    ImageBarrierDesc barrier =
                    {
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_GENERAL,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, arraySize }
                    };

                    SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], volumeResources.unmanaged.probeRayData, barrier);
                    SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], volumeResources.unmanaged.probeIrradiance, barrier);
                    SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], volumeResources.unmanaged.probeDistance, barrier);
                    SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], volumeResources.unmanaged.probeData, barrier);
                    SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], volumeResources.unmanaged.probeVariability, barrier);
                    barrier.subresourceRange.layerCount = variabilityAverageArraySize;
                    SetImageLayoutBarrier(vk.cmdBuffer[vk.frameIndex], volumeResources.unmanaged.probeVariabilityAverage, barrier);
                }

                // Set the pipeline layout and descriptor set
                {
                #if RTXGI_DDGI_BINDLESS_RESOURCES
                    // Pass handles to the global pipeline layout and descriptor set (bindless)
                    volumeResources.unmanaged.pipelineLayout = vkResources.pipelineLayout;
                    volumeResources.unmanaged.descriptorSet = resources.descriptorSet;
                #else
                    // Pass handles to the volume's pipeline layout and descriptor set (not bindless)
                    volumeResources.unmanaged.pipelineLayout = resources.volumePipelineLayout;
                    volumeResources.unmanaged.descriptorSet = resources.volumeDescriptorSets[volumeDesc.index];
                #endif
                }

                // Create the shader modules and pipelines
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

                        shaderIndex++;
                    }

                    // Probe Variability Reduction Pipeline
                    {
                        // Create the shader module
                        CHECK(CreateShaderModule(vk.device, volumeShaders[shaderIndex], &volumeResources.unmanaged.probeVariabilityPipelines.reductionModule), "create probe variability reduction module!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Variability Reduction Shader Module";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeVariabilityPipelines.reductionModule), name.c_str(), VK_OBJECT_TYPE_SHADER_MODULE);
                    #endif

                        // Create the pipeline
                        CHECK(CreateComputePipeline(
                            vk.device,
                            volumeResources.unmanaged.pipelineLayout,
                            volumeShaders[shaderIndex],
                            volumeResources.unmanaged.probeVariabilityPipelines.reductionModule,
                            &volumeResources.unmanaged.probeVariabilityPipelines.reductionPipeline), "", log);
                    #ifdef GFX_NAME_OBJECTS
                        name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Variability Reduction Pipeline";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeVariabilityPipelines.reductionPipeline), name.c_str(), VK_OBJECT_TYPE_PIPELINE);
                    #endif

                        shaderIndex++;
                    }

                    // Probe Variability Extra Reduction Pipeline
                    {
                        // Create the shader module
                        CHECK(CreateShaderModule(vk.device, volumeShaders[shaderIndex], &volumeResources.unmanaged.probeVariabilityPipelines.extraReductionModule), "create probe variability extra reduction module!", log);
                    #ifdef GFX_NAME_OBJECTS
                        std::string name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Variability Extra Reduction Shader Module";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeVariabilityPipelines.reductionModule), name.c_str(), VK_OBJECT_TYPE_SHADER_MODULE);
                    #endif

                        // Create the pipeline
                        CHECK(CreateComputePipeline(
                            vk.device,
                            volumeResources.unmanaged.pipelineLayout,
                            volumeShaders[shaderIndex],
                            volumeResources.unmanaged.probeVariabilityPipelines.extraReductionModule,
                            &volumeResources.unmanaged.probeVariabilityPipelines.extraReductionPipeline), "", log);
                    #ifdef GFX_NAME_OBJECTS
                        name = "DDGIVolume[" + std::to_string(volumeDesc.index) + "], Probe Variability Extra Reduction Pipeline";
                        SetObjectName(vk.device, reinterpret_cast<uint64_t>(volumeResources.unmanaged.probeVariabilityPipelines.extraReductionPipeline), name.c_str(), VK_OBJECT_TYPE_PIPELINE);
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

                // Texture Arrays
                vkDestroyImage(device, volume->GetProbeRayData(), nullptr);
                vkDestroyImage(device, volume->GetProbeIrradiance(), nullptr);
                vkDestroyImage(device, volume->GetProbeDistance(), nullptr);
                vkDestroyImage(device, volume->GetProbeData(), nullptr);
                vkDestroyImage(device, volume->GetProbeVariability(), nullptr);
                vkDestroyImage(device, volume->GetProbeVariabilityAverage(), nullptr);
                vkDestroyBuffer(device, volume->GetProbeVariabilityReadback(), nullptr);

                // Texture Array Memory
                vkFreeMemory(device, volume->GetProbeRayDataMemory(), nullptr);
                vkFreeMemory(device, volume->GetProbeIrradianceMemory(), nullptr);
                vkFreeMemory(device, volume->GetProbeDistanceMemory(), nullptr);
                vkFreeMemory(device, volume->GetProbeDataMemory(), nullptr);
                vkFreeMemory(device, volume->GetProbeVariabilityMemory(), nullptr);
                vkFreeMemory(device, volume->GetProbeVariabilityAverageMemory(), nullptr);
                vkFreeMemory(device, volume->GetProbeVariabilityReadbackMemory(), nullptr);

                // Texture Array Views
                vkDestroyImageView(device, volume->GetProbeRayDataView(), nullptr);
                vkDestroyImageView(device, volume->GetProbeIrradianceView(), nullptr);
                vkDestroyImageView(device, volume->GetProbeDistanceView(), nullptr);
                vkDestroyImageView(device, volume->GetProbeDataView(), nullptr);
                vkDestroyImageView(device, volume->GetProbeVariabilityView(), nullptr);
                vkDestroyImageView(device, volume->GetProbeVariabilityAverageView(), nullptr);

                // Shader Modules
                vkDestroyShaderModule(device, volume->GetProbeBlendingIrradianceModule(), nullptr);
                vkDestroyShaderModule(device, volume->GetProbeBlendingDistanceModule(), nullptr);
                vkDestroyShaderModule(device, volume->GetProbeRelocationModule(), nullptr);
                vkDestroyShaderModule(device, volume->GetProbeRelocationResetModule(), nullptr);
                vkDestroyShaderModule(device, volume->GetProbeClassificationModule(), nullptr);
                vkDestroyShaderModule(device, volume->GetProbeClassificationResetModule(), nullptr);

                vkDestroyShaderModule(device, volume->GetProbeVariabilityReductionModule(), nullptr);
                vkDestroyShaderModule(device, volume->GetProbeVariabilityExtraReductionModule(), nullptr);

                // Pipelines
                vkDestroyPipeline(device, volume->GetProbeBlendingIrradiancePipeline(), nullptr);
                vkDestroyPipeline(device, volume->GetProbeBlendingDistancePipeline(), nullptr);
                vkDestroyPipeline(device, volume->GetProbeRelocationPipeline(), nullptr);
                vkDestroyPipeline(device, volume->GetProbeRelocationResetPipeline(), nullptr);
                vkDestroyPipeline(device, volume->GetProbeClassificationPipeline(), nullptr);
                vkDestroyPipeline(device, volume->GetProbeClassificationResetPipeline(), nullptr);

                vkDestroyPipeline(device, volume->GetProbeVariabilityReductionPipeline(), nullptr);
                vkDestroyPipeline(device, volume->GetProbeVariabilityExtraReductionPipeline(), nullptr);

                // Clear pointers
                volume->Destroy();
            }
        #endif // !RTXGI_DDGI_RESOURCE_MANAGEMENT

            //----------------------------------------------------------------------------------------------------------
            // DDGIVolume Creation Helper Functions
            //----------------------------------------------------------------------------------------------------------

            /**
             * Populates a DDGIVolumeDesc structure from configuration data.
             */
            void GetDDGIVolumeDesc(const Configs::DDGIVolume& config, DDGIVolumeDesc& volumeDesc)
            {
                size_t size = config.name.size();
                volumeDesc.name = new char[size + 1];
                memset(volumeDesc.name, 0, size + 1);
                memcpy(volumeDesc.name, config.name.c_str(), size);

                volumeDesc.index = config.index;
                volumeDesc.rngSeed = config.rngSeed;
                volumeDesc.origin = { config.origin.x, config.origin.y, config.origin.z };
                volumeDesc.eulerAngles = { config.eulerAngles.x, config.eulerAngles.y, config.eulerAngles.z, };
                volumeDesc.probeSpacing = { config.probeSpacing.x, config.probeSpacing.y, config.probeSpacing.z };
                volumeDesc.probeCounts = { config.probeCounts.x, config.probeCounts.y, config.probeCounts.z, };
                volumeDesc.probeNumRays = config.probeNumRays;
                volumeDesc.probeNumIrradianceTexels = config.probeNumIrradianceTexels;
                volumeDesc.probeNumIrradianceInteriorTexels = (config.probeNumIrradianceTexels - 2);
                volumeDesc.probeNumDistanceTexels = config.probeNumDistanceTexels;
                volumeDesc.probeNumDistanceInteriorTexels = (config.probeNumDistanceTexels - 2);
                volumeDesc.probeHysteresis = config.probeHysteresis;
                volumeDesc.probeNormalBias = config.probeNormalBias;
                volumeDesc.probeViewBias = config.probeViewBias;
                volumeDesc.probeMaxRayDistance = config.probeMaxRayDistance;
                volumeDesc.probeIrradianceThreshold = config.probeIrradianceThreshold;
                volumeDesc.probeBrightnessThreshold = config.probeBrightnessThreshold;

                volumeDesc.showProbes = config.showProbes;
                volumeDesc.probeVisType = config.probeVisType;

                volumeDesc.probeRayDataFormat = config.textureFormats.rayDataFormat;
                volumeDesc.probeIrradianceFormat = config.textureFormats.irradianceFormat;
                volumeDesc.probeDistanceFormat = config.textureFormats.distanceFormat;
                volumeDesc.probeDataFormat = config.textureFormats.dataFormat;
                volumeDesc.probeVariabilityFormat = config.textureFormats.variabilityFormat;

                volumeDesc.probeRelocationEnabled = config.probeRelocationEnabled;
                volumeDesc.probeMinFrontfaceDistance = config.probeMinFrontfaceDistance;
                volumeDesc.probeClassificationEnabled = config.probeClassificationEnabled;
                volumeDesc.probeVariabilityEnabled = config.probeVariabilityEnabled;

                if (config.infiniteScrollingEnabled) volumeDesc.movementType = EDDGIVolumeMovementType::Scrolling;
                else volumeDesc.movementType = EDDGIVolumeMovementType::Default;
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

                // When using the application's pipeline layout for bindless, pass an offset
                // to where the DDGIConstants are in the application's push constants block
            #if RTXGI_DDGI_BINDLESS_RESOURCES
                volumeResources.bindless.pushConstantsOffset = GlobalConstants::GetAlignedSizeInBytes();
            #endif

                // Pass valid constants structured buffer pointers
                volumeResources.constantsBuffer = resources.volumeConstantsSTB;
                volumeResources.constantsBufferUpload = resources.volumeConstantsSTBUpload;
                volumeResources.constantsBufferUploadMemory = resources.volumeConstantsSTBUploadMemory;
                volumeResources.constantsBufferSizeInBytes = resources.volumeConstantsSTBSizeInBytes;

                // Regardless of what the host application chooses for resource binding, all SDK shaders can operate in either bound or bindless modes
                volumeResources.bindless.enabled = (bool)RTXGI_DDGI_BINDLESS_RESOURCES;

                // Set the resource indices structured buffer pointers and size
                volumeResources.bindless.resourceIndicesBuffer = resources.volumeResourceIndicesSTB;
                volumeResources.bindless.resourceIndicesBufferUpload = resources.volumeResourceIndicesSTBUpload;
                volumeResources.bindless.resourceIndicesBufferUploadMemory = resources.volumeResourceIndicesSTBUploadMemory;
                volumeResources.bindless.resourceIndicesBufferSizeInBytes = resources.volumeResourceIndicesSTBSizeInBytes;

                // Set the resource array indices of volume resources
                DDGIVolumeResourceIndices& resourceIndices = volumeResources.bindless.resourceIndices;
                resourceIndices.rayDataUAVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors());
                resourceIndices.rayDataSRVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors());
                resourceIndices.probeIrradianceUAVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 1;
                resourceIndices.probeIrradianceSRVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 1;
                resourceIndices.probeDistanceUAVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 2;
                resourceIndices.probeDistanceSRVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 2;
                resourceIndices.probeDataUAVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 3;
                resourceIndices.probeDataSRVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 3;
                resourceIndices.probeVariabilityUAVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 4;
                resourceIndices.probeVariabilitySRVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 4;
                resourceIndices.probeVariabilityAverageUAVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 5;
                resourceIndices.probeVariabilityAverageSRVIndex = (volumeDesc.index * rtxgi::GetDDGIVolumeNumTex2DArrayDescriptors()) + 5;

            #if RTXGI_DDGI_RESOURCE_MANAGEMENT
                // Enable "Managed Mode", the RTXGI SDK creates graphics objects
                volumeResources.managed.enabled = true;

                // Pass the Vulkan device and physical device to use for resource creation and memory allocation
                // Pass a valid descriptor pool to use for pipeline/descriptor layout creation
                volumeResources.managed.device = vk.device;
                volumeResources.managed.physicalDevice = vk.physicalDevice;
                volumeResources.managed.descriptorPool = vkResources.descriptorPool;

                // Pass compiled shader bytecode
                assert(volumeShaders.size() >= 2);
                volumeResources.managed.probeBlendingIrradianceCS = { volumeShaders[0].bytecode->GetBufferPointer(), volumeShaders[0].bytecode->GetBufferSize() };
                volumeResources.managed.probeBlendingDistanceCS = { volumeShaders[1].bytecode->GetBufferPointer(), volumeShaders[1].bytecode->GetBufferSize() };

                assert(volumeShaders.size() >= 4);
                volumeResources.managed.probeRelocation.updateCS = { volumeShaders[2].bytecode->GetBufferPointer(), volumeShaders[2].bytecode->GetBufferSize() };
                volumeResources.managed.probeRelocation.resetCS = { volumeShaders[3].bytecode->GetBufferPointer(), volumeShaders[3].bytecode->GetBufferSize() };

                assert(volumeShaders.size() >= 6);
                volumeResources.managed.probeClassification.updateCS = { volumeShaders[4].bytecode->GetBufferPointer(), volumeShaders[4].bytecode->GetBufferSize() };
                volumeResources.managed.probeClassification.resetCS = { volumeShaders[5].bytecode->GetBufferPointer(), volumeShaders[5].bytecode->GetBufferSize() };

                assert(volumeShaders.size() == 8);
                volumeResources.managed.probeVariability.reductionCS = { volumeShaders[6].bytecode->GetBufferPointer(), volumeShaders[6].bytecode->GetBufferSize() };
                volumeResources.managed.probeVariability.extraReductionCS = { volumeShaders[7].bytecode->GetBufferPointer(), volumeShaders[7].bytecode->GetBufferSize() };
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
                        SAFE_DELETE(resources.volumeDescs[volumeConfig.index].name);
                        SAFE_DELETE(resources.volumes[volumeConfig.index]);
                        resources.numVolumeVariabilitySamples[volumeConfig.index] = 0;
                    }
                }
                else
                {
                    resources.volumeDescs.emplace_back();
                    resources.volumes.emplace_back();
                    resources.numVolumeVariabilitySamples.emplace_back();
                }

                // Describe the DDGIVolume's properties
                DDGIVolumeDesc& volumeDesc = resources.volumeDescs[volumeConfig.index];
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
             * Creates the DDGIVolume resource indices structured buffer.
             */
            bool CreateDDGIVolumeResourceIndicesBuffer(Globals& vk, GlobalResources& vkResources, Resources& resources, uint32_t volumeCount, std::ofstream& log)
            {
                resources.volumeResourceIndicesSTBSizeInBytes = sizeof(DDGIVolumeResourceIndices) * volumeCount;
                if (resources.volumeResourceIndicesSTBSizeInBytes == 0) return true; // scenes with no DDGIVolumes are valid

                // Create the DDGIVolume resource indices upload buffer resources (double buffered)
                BufferDesc desc = { 2 * resources.volumeResourceIndicesSTBSizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
                CHECK(CreateBuffer(vk, desc, &resources.volumeResourceIndicesSTBUpload, &resources.volumeResourceIndicesSTBUploadMemory), "create DDGIVolume Resource Indices Upload Structured Buffer!\n", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.volumeResourceIndicesSTBUpload), "DDGIVolume Resource Indices Upload Structured Buffer", VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.volumeResourceIndicesSTBUploadMemory), "DDGIVolume Resource Indices Upload Structured Buffer Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
            #endif

                // Create the DDGIVolume resource indices device buffer resources
                desc.size = resources.volumeResourceIndicesSTBSizeInBytes;
                desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                desc.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                CHECK(CreateBuffer(vk, desc, &resources.volumeResourceIndicesSTB, &resources.volumeResourceIndicesSTBMemory), "create DDGIVolume Resource Indices Structured Buffer!\n", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.volumeResourceIndicesSTB), "DDGIVolume Resource Indices Structured Buffer", VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.volumeResourceIndicesSTBMemory), "DDGIVolume Resource Indices Structured Buffer Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
            #endif

                return true;
            }

            /**
             * Creates the DDGIVolume constants structured buffer.
             */
            bool CreateDDGIVolumeConstantsBuffer(Globals& vk, GlobalResources& vkResources, Resources& resources, uint32_t volumeCount, std::ofstream& log)
            {
                resources.volumeConstantsSTBSizeInBytes = sizeof(DDGIVolumeDescGPUPacked) * volumeCount;
                if (resources.volumeConstantsSTBSizeInBytes == 0) return true; // scenes with no DDGIVolumes are valid

                // Create the DDGIVolume constants upload buffer resources (double buffered)
                BufferDesc desc = { 2 * resources.volumeConstantsSTBSizeInBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
                CHECK(CreateBuffer(vk, desc, &resources.volumeConstantsSTBUpload, &resources.volumeConstantsSTBUploadMemory), "create DDGIVolume Constants Upload Structured Buffer!\n", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.volumeConstantsSTBUpload), "DDGIVolume Constants Upload Structured Buffer", VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.volumeConstantsSTBUploadMemory), "DDGIVolume Constants Upload Structured Buffer Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
            #endif

                // Create the DDGIVolume constants device buffer resources
                desc.size = resources.volumeConstantsSTBSizeInBytes;
                desc.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
                desc.memoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
                CHECK(CreateBuffer(vk, desc, &resources.volumeConstantsSTB, &resources.volumeConstantsSTBMemory), "create DDGIVolume Constants Structured Buffer!\n", log);
            #ifdef GFX_NAME_OBJECTS
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.volumeConstantsSTB), "DDGIVolume Constants Structured Buffer", VK_OBJECT_TYPE_BUFFER);
                SetObjectName(vk.device, reinterpret_cast<uint64_t>(resources.volumeConstantsSTBMemory), "DDGIVolume Constants Structured Buffer Memory", VK_OBJECT_TYPE_DEVICE_MEMORY);
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
                TextureDesc desc = { static_cast<uint32_t>(vk.width), static_cast<uint32_t>(vk.height), 1, 1, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT };
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
                    resources.rtShaders.rgs.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                    Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_PUSH_CONSTS_TYPE", L"2");                                                 // use the application's push constants layout
                    Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_PUSH_CONSTS_STRUCT_NAME", L"GlobalConstants");                            // specify the struct name of the application's push constants
                    Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_PUSH_CONSTS_VARIABLE_NAME", L"GlobalConst");                              // specify the variable name of the application's push constants
                    Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_VOLUME_INDEX_NAME", L"ddgi_volumeIndex");          // specify the name of the DDGIVolume index field in the application's push constants struct
                    Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_X_NAME", L"ddgi_reductionInputSizeX");  // specify the name of the DDGIVolume reduction pass input size fields the application's push constants struct
                    Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_Y_NAME", L"ddgi_reductionInputSizeY");
                    Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_Z_NAME", L"ddgi_reductionInputSizeZ");
                    Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                    Shaders::AddDefine(resources.rtShaders.rgs, L"RTXGI_COORDINATE_SYSTEM", std::to_wstring(RTXGI_COORDINATE_SYSTEM));
                    CHECK(Shaders::Compile(vk.shaderCompiler, resources.rtShaders.rgs, true), "compile DDGI probe tracing ray generation shader!\n", log);
                }

                // Load and compile the miss shader
                {
                    resources.rtShaders.miss.filepath = root + L"shaders/Miss.hlsl";
                    resources.rtShaders.miss.entryPoint = L"Miss";
                    resources.rtShaders.miss.exportName = L"DDGIProbeTraceMiss";
                    resources.rtShaders.miss.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                    Shaders::AddDefine(resources.rtShaders.miss, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
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
                    group.chs.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                    Shaders::AddDefine(group.chs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                    CHECK(Shaders::Compile(vk.shaderCompiler, group.chs, true), "compile DDGI probe tracing closest hit shader!\n", log);

                    // Load and compile the AHS
                    group.ahs.filepath = root + L"shaders/AHS.hlsl";
                    group.ahs.entryPoint = L"AHS_GI";
                    group.ahs.exportName = L"DDGIProbeTraceAHS";
                    group.ahs.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };
                    Shaders::AddDefine(group.ahs, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
                    CHECK(Shaders::Compile(vk.shaderCompiler, group.ahs, true), "compile DDGI probe tracing any hit shader!\n", log);

                    // Set the payload size
                    resources.rtShaders.payloadSizeInBytes = sizeof(PackedPayload);
                }

                // Load and compile the indirect lighting compute shader
                {
                    std::wstring shaderPath = root + L"shaders/IndirectCS.hlsl";
                    resources.indirectCS.filepath = shaderPath.c_str();
                    resources.indirectCS.entryPoint = L"CS";
                    resources.indirectCS.targetProfile = L"cs_6_6";
                    resources.indirectCS.arguments = { L"-spirv", L"-D __spirv__", L"-fspv-target-env=vulkan1.2" };

                    Shaders::AddDefine(resources.indirectCS, L"RTXGI_PUSH_CONSTS_TYPE", L"2");                                                 // use the application's push constants layout
                    Shaders::AddDefine(resources.indirectCS, L"RTXGI_PUSH_CONSTS_STRUCT_NAME", L"GlobalConstants");                            // specify the struct name of the application's push constants
                    Shaders::AddDefine(resources.indirectCS, L"RTXGI_PUSH_CONSTS_VARIABLE_NAME", L"GlobalConst");                              // specify the variable name of the application's push constants
                    Shaders::AddDefine(resources.indirectCS, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_VOLUME_INDEX_NAME", L"ddgi_volumeIndex");          // specify the name of the DDGIVolume index field in the application's push constants struct
                    Shaders::AddDefine(resources.indirectCS, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_X_NAME", L"ddgi_reductionInputSizeX");  // specify the name of the DDGIVolume reduction pass input size fields the application's push constants struct
                    Shaders::AddDefine(resources.indirectCS, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_Y_NAME", L"ddgi_reductionInputSizeY");
                    Shaders::AddDefine(resources.indirectCS, L"RTXGI_PUSH_CONSTS_FIELD_DDGI_REDUCTION_INPUT_SIZE_Z_NAME", L"ddgi_reductionInputSizeZ");
                    Shaders::AddDefine(resources.indirectCS, L"RTXGI_BINDLESS_TYPE", std::to_wstring(RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS));
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
                // Store the descriptors to be written to the descriptor set
                VkWriteDescriptorSet* descriptor = nullptr;
                std::vector<VkWriteDescriptorSet> descriptors;

                uint32_t numVolumes = static_cast<uint32_t>(resources.volumes.size());

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

                // 8: Texture2D UAVs
                VkDescriptorImageInfo rwTex2D[] =
                {
                    { VK_NULL_HANDLE, vkResources.rt.GBufferAView, VK_IMAGE_LAYOUT_GENERAL },
                    { VK_NULL_HANDLE, vkResources.rt.GBufferBView, VK_IMAGE_LAYOUT_GENERAL },
                    { VK_NULL_HANDLE, vkResources.rt.GBufferCView, VK_IMAGE_LAYOUT_GENERAL },
                    { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL }, // GBufferD
                    { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL }, // RTAOOutput
                    { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL }, // RTAORaw
                    { VK_NULL_HANDLE, resources.outputView, VK_IMAGE_LAYOUT_GENERAL },
                };

                descriptor = &descriptors.emplace_back();
                descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptor->dstSet = resources.descriptorSet;
                descriptor->dstBinding = DescriptorLayoutBindings::UAV_TEX2D;
                descriptor->dstArrayElement = RWTex2DIndices::GBUFFERA;
                descriptor->descriptorCount = _countof(rwTex2D);
                descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                descriptor->pImageInfo = rwTex2D;

                // 9: Texture2DArray UAVs
                std::vector<VkDescriptorImageInfo> rwTex2DArray;
                if (numVolumes > 0)
                {
                    for (uint32_t volumeIndex = 0; volumeIndex < static_cast<uint32_t>(resources.volumes.size()); volumeIndex++)
                    {
                        // Add the DDGIVolume texture arrays
                        const DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[volumeIndex]);
                        rwTex2DArray.push_back({ VK_NULL_HANDLE, volume->GetProbeRayDataView(), VK_IMAGE_LAYOUT_GENERAL });
                        rwTex2DArray.push_back({ VK_NULL_HANDLE, volume->GetProbeIrradianceView(), VK_IMAGE_LAYOUT_GENERAL });
                        rwTex2DArray.push_back({ VK_NULL_HANDLE, volume->GetProbeDistanceView(), VK_IMAGE_LAYOUT_GENERAL });
                        rwTex2DArray.push_back({ VK_NULL_HANDLE, volume->GetProbeDataView(), VK_IMAGE_LAYOUT_GENERAL });
                        rwTex2DArray.push_back({ VK_NULL_HANDLE, volume->GetProbeVariabilityView(), VK_IMAGE_LAYOUT_GENERAL });
                        rwTex2DArray.push_back({ VK_NULL_HANDLE, volume->GetProbeVariabilityAverageView(), VK_IMAGE_LAYOUT_GENERAL });
                    }

                    descriptor = &descriptors.emplace_back();
                    descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptor->dstSet = resources.descriptorSet;
                    descriptor->dstBinding = DescriptorLayoutBindings::UAV_TEX2DARRAY;
                    descriptor->dstArrayElement = 0;
                    descriptor->descriptorCount = static_cast<uint32_t>(rwTex2DArray.size());
                    descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    descriptor->pImageInfo = rwTex2DArray.data();
                }

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
                uint32_t numSceneTextures = static_cast<uint32_t>(vkResources.sceneTextureViews.size());
                if (numSceneTextures > 0)
                {
                    for (uint32_t textureIndex = 0; textureIndex < numSceneTextures; textureIndex++)
                    {
                        tex2D.push_back({ VK_NULL_HANDLE, vkResources.sceneTextureViews[textureIndex], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
                    }

                    descriptor = &descriptors.emplace_back();
                    descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptor->dstSet = resources.descriptorSet;
                    descriptor->dstBinding = DescriptorLayoutBindings::SRV_TEX2D;
                    descriptor->dstArrayElement = Tex2DIndices::SCENE_TEXTURES;
                    descriptor->descriptorCount = static_cast<uint32_t>(tex2D.size());
                    descriptor->descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                    descriptor->pImageInfo = tex2D.data();
                }

                // 12: Texture2DArray SRVs
                std::vector<VkDescriptorImageInfo> tex2DArray;
                if (numVolumes > 0)
                {
                    for (uint32_t volumeIndex = 0; volumeIndex < static_cast<uint32_t>(resources.volumes.size()); volumeIndex++)
                    {
                        // Add the DDGIVolume texture arrays
                        const DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[volumeIndex]);
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

            void RayTraceVolumes(Globals& vk, GlobalResources& vkResources, Resources& resources)
            {
            #ifdef GFX_PERF_MARKERS
                AddPerfMarker(vk, GFX_PERF_MARKER_GREEN, "Ray Trace DDGIVolumes");
            #endif

                // Update the push constants
                uint32_t offset = 0;
                GlobalConstants consts = vkResources.constants;
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, AppConsts::GetAlignedSizeInBytes(), consts.app.GetData());
                offset += AppConsts::GetAlignedSizeInBytes();
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, PathTraceConsts::GetAlignedSizeInBytes(), consts.pt.GetData());
                offset += PathTraceConsts::GetAlignedSizeInBytes();
                vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, LightingConsts::GetAlignedSizeInBytes(), consts.lights.GetData());

                // Bind the descriptor set
                vkCmdBindDescriptorSets(vk.cmdBuffer[vk.frameIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, vkResources.pipelineLayout, 0, 1, &resources.descriptorSet, 0, nullptr);

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

                // Barriers
                std::vector<VkImageMemoryBarrier> barriers;
                VkImageMemoryBarrier barrier = {};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.oldLayout = barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

                // DDGI push constants offset
                offset = GlobalConstants::GetAlignedSizeInBytes();

                // Trace probe rays for each volume
                for (uint32_t volumeIndex = 0; volumeIndex < static_cast<uint32_t>(resources.selectedVolumes.size()); volumeIndex++)
                {
                    // Get the volume
                    const DDGIVolume* volume = resources.selectedVolumes[volumeIndex];

                    // Update the push constants
                    vkCmdPushConstants(vk.cmdBuffer[vk.frameIndex], vkResources.pipelineLayout, VK_SHADER_STAGE_ALL, offset, DDGIRootConstants::GetSizeInBytes(), volume->GetPushConstants().GetData());

                    uint32_t width, height, depth;
                    volume->GetRayDispatchDimensions(width, height, depth);

                    // Trace probe rays
                    vkCmdTraceRaysKHR(
                        vk.cmdBuffer[vk.frameIndex],
                        &raygenRegion,
                        &missRegion,
                        &hitRegion,
                        &callableRegion,
                        width,
                        height,
                        depth);

                    // Barrier(s)
                    barrier.image = volume->GetProbeRayData();
                    barriers.push_back(barrier);
                }

                // Wait for the ray traces to complete
                if (!barriers.empty())
                {
                    vkCmdPipelineBarrier(
                        vk.cmdBuffer[vk.frameIndex],
                        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                        0,
                        0, nullptr,
                        0, nullptr,
                        static_cast<uint32_t>(barriers.size()), barriers.data());
                }

            #ifdef GFX_PERF_MARKERS
                vkCmdEndDebugUtilsLabelEXT(vk.cmdBuffer[vk.frameIndex]);
            #endif
            }

            void GatherIndirectLighting(Globals& vk, GlobalResources& vkResources, Resources& resources)
            {
            #ifdef GFX_PERF_MARKERS
                AddPerfMarker(vk, GFX_PERF_MARKER_GREEN, "Indirect Lighting");
            #endif

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
                // Validate the SDK version
                assert(RTXGI_VERSION::major == 1);
                assert(RTXGI_VERSION::minor == 3);
                assert(RTXGI_VERSION::revision == 5);
                assert(std::strcmp(RTXGI_VERSION::getVersionString(), "1.3.5") == 0);

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

                // Create the DDGIVolume resource indices structured buffer
                if (!CreateDDGIVolumeResourceIndicesBuffer(vk, vkResources, resources, numVolumes, log)) return false;

                // Create the DDGIVolume constants structured buffer
                if (!CreateDDGIVolumeConstantsBuffer(vk, vkResources, resources, numVolumes, log)) return false;

            #ifdef RTXGI_EXPORT_DLL
                // Initialize the RTXGI SDK's Vulkan extensions when using the dynamic library
                rtxgi::vulkan::LoadExtensions(vk.device);
            #endif

                // Initialize the DDGIVolumes
                for (uint32_t volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
                {
                    Configs::DDGIVolume volumeConfig = config.ddgi.volumes[volumeIndex];
                    if (!CreateDDGIVolume(vk, vkResources, resources, volumeConfig, log)) return false;

                    // Clear the volume's probes at initialization
                    DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[volumeIndex]);
                    volume->ClearProbes(vk.cmdBuffer[vk.frameIndex]);
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
                resources.rtStat = perf.AddGPUStat("  Probe Trace");
                resources.blendStat = perf.AddGPUStat("  Blend");
                resources.relocateStat = perf.AddGPUStat("  Relocate");
                resources.classifyStat = perf.AddGPUStat("  Classify");
                resources.lightingStat = perf.AddGPUStat("  Lighting");
                resources.variabilityStat = perf.AddGPUStat("  Variability");

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
             * Reload and compile shaders, recreate shader modules and pipelines, and update the shader table.
             */
            bool Reload(Globals& vk, GlobalResources& vkResources, Resources& resources, const Configs::Config& config, std::ofstream& log)
            {
                log << "Reloading DDGI shaders...";

                uint32_t numVolumes = static_cast<uint32_t>(config.ddgi.volumes.size());

                if (!LoadAndCompileShaders(vk, resources, numVolumes, log)) return false;
                if (!CreatePipelines(vk, vkResources, resources, log)) return false;

                // Reinitialize the DDGIVolumes
                for (uint32_t volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
                {
                    Configs::DDGIVolume volumeConfig = config.ddgi.volumes[volumeIndex];
                    if (!CreateDDGIVolume(vk, vkResources, resources, volumeConfig, log)) return false;
                }

                if (!UpdateShaderTable(vk, vkResources, resources, log)) return false;
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
                        resources.numVolumeVariabilitySamples[config.ddgi.selectedVolume] = 0;
                    }

                    // Select the active volumes
                    resources.selectedVolumes.clear();
                    for (UINT volumeIndex = 0; volumeIndex < static_cast<UINT>(resources.volumes.size()); volumeIndex++)
                    {
                        // TODO: processing to determine which volumes are in-frustum, active, and prioritized for update / render

                        // Get the volume
                        DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[volumeIndex]);

                        // If the scene's lights, skylight, or geometry have changed *or* the volume moves *or the probes are reset, reset variability
                        if (config.ddgi.volumes[volumeIndex].clearProbeVariability) resources.numVolumeVariabilitySamples[volumeIndex] = 0;

                        // Skip volumes whose variability measurement is low enough to be considered converged
                        // Enforce a minimum of 16 samples to filter out early outliers
                        const uint32_t MinimumVariabilitySamples = 16;
                        float volumeAverageVariability = volume->GetVolumeAverageVariability();
                        bool isConverged = volume->GetProbeVariabilityEnabled()
                                                && (resources.numVolumeVariabilitySamples[volumeIndex]++ > MinimumVariabilitySamples)
                                                && (volumeAverageVariability < config.ddgi.volumes[config.ddgi.selectedVolume].probeVariabilityThreshold);
                        
                        // Add the volume to the list of volumes to update (it hasn't converged)
                        if (!isConverged) resources.selectedVolumes.push_back(volume);
                    }

                    // Update the DDGIVolume constants
                    for (uint32_t volumeIndex = 0; volumeIndex < static_cast<uint32_t>(resources.selectedVolumes.size()); volumeIndex++)
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
                GPU_TIMESTAMP_BEGIN(resources.gpuStat->GetGPUQueryBeginIndex());
                if (resources.enabled)
                {
                    UINT numVolumes = static_cast<UINT>(resources.selectedVolumes.size());

                    // Upload volume resource indices and constants
                    rtxgi::vulkan::UploadDDGIVolumeResourceIndices(vk.device, vk.cmdBuffer[vk.frameIndex], vk.frameIndex, numVolumes, resources.selectedVolumes.data());
                    rtxgi::vulkan::UploadDDGIVolumeConstants(vk.device, vk.cmdBuffer[vk.frameIndex], vk.frameIndex, numVolumes, resources.selectedVolumes.data());

                    // Trace rays from DDGI probes to sample the environment
                    GPU_TIMESTAMP_BEGIN(resources.rtStat->GetGPUQueryBeginIndex());
                    RayTraceVolumes(vk, vkResources, resources);
                    GPU_TIMESTAMP_END(resources.rtStat->GetGPUQueryEndIndex());

                    // Update volume probes
                    GPU_TIMESTAMP_BEGIN(resources.blendStat->GetGPUQueryBeginIndex());
                    rtxgi::vulkan::UpdateDDGIVolumeProbes(vk.cmdBuffer[vk.frameIndex], numVolumes, resources.selectedVolumes.data());
                    GPU_TIMESTAMP_END(resources.blendStat->GetGPUQueryEndIndex());

                    // Relocate probes if the feature is enabled
                    GPU_TIMESTAMP_BEGIN(resources.relocateStat->GetGPUQueryBeginIndex());
                    rtxgi::vulkan::RelocateDDGIVolumeProbes(vk.cmdBuffer[vk.frameIndex], numVolumes, resources.selectedVolumes.data());
                    GPU_TIMESTAMP_END(resources.relocateStat->GetGPUQueryEndIndex());

                    // Classify probes if the feature is enabled
                    GPU_TIMESTAMP_BEGIN(resources.classifyStat->GetGPUQueryBeginIndex());
                    rtxgi::vulkan::ClassifyDDGIVolumeProbes(vk.cmdBuffer[vk.frameIndex], numVolumes, resources.selectedVolumes.data());
                    GPU_TIMESTAMP_END(resources.classifyStat->GetGPUQueryEndIndex());

                    // Calculate variability
                    GPU_TIMESTAMP_BEGIN(resources.variabilityStat->GetGPUQueryBeginIndex());
                    rtxgi::vulkan::CalculateDDGIVolumeVariability(vk.cmdBuffer[vk.frameIndex], numVolumes, resources.selectedVolumes.data());
                    // The readback happens immediately, not recorded on the command list, so will return a value from a previous update
                    rtxgi::vulkan::ReadbackDDGIVolumeVariability(vk.device, numVolumes, resources.selectedVolumes.data());
                    GPU_TIMESTAMP_END(resources.variabilityStat->GetGPUQueryEndIndex());

                    // Render the indirect lighting to screen-space
                    GPU_TIMESTAMP_BEGIN(resources.lightingStat->GetGPUQueryBeginIndex());
                    GatherIndirectLighting(vk, vkResources, resources);
                    GPU_TIMESTAMP_END(resources.lightingStat->GetGPUQueryEndIndex());
                }
                GPU_TIMESTAMP_END(resources.gpuStat->GetGPUQueryEndIndex());
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

                // Resource Indices
                vkDestroyBuffer(device, resources.volumeResourceIndicesSTBUpload, nullptr);
                vkFreeMemory(device, resources.volumeResourceIndicesSTBUploadMemory, nullptr);
                vkDestroyBuffer(device, resources.volumeResourceIndicesSTB, nullptr);
                vkFreeMemory(device, resources.volumeResourceIndicesSTBMemory, nullptr);

                // Constants
                vkDestroyBuffer(device, resources.volumeConstantsSTBUpload, nullptr);
                vkFreeMemory(device, resources.volumeConstantsSTBUploadMemory, nullptr);
                vkDestroyBuffer(device, resources.volumeConstantsSTB, nullptr);
                vkFreeMemory(device, resources.volumeConstantsSTBMemory, nullptr);

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
                    SAFE_DELETE(resources.volumeDescs[volumeIndex].name);
                    resources.volumes[volumeIndex]->Destroy();
                    SAFE_DELETE(resources.volumes[volumeIndex]);
                }
            }

            /**
             * Write the DDGI Volume texture resources to disk.
             * Note: not storing ray data or probe distance (for now) since WIC doesn't auto-convert 2 channel texture formats
             */
            bool WriteVolumesToDisk(Globals& vk, GlobalResources& vkResources, Resources& resources, std::string directory)
            {
            #if (defined(_WIN32) || defined(WIN32))
                CoInitialize(NULL);
            #endif
                bool success = true;
                for (UINT volumeIndex = 0; volumeIndex < static_cast<UINT>(resources.volumes.size()); volumeIndex++)
                {
                    // Get the volume
                    const DDGIVolume* volume = static_cast<DDGIVolume*>(resources.volumes[volumeIndex]);

                    // Start constructing the filename
                    std::string baseName = directory + "/DDGIVolume[" + volume->GetName() + "]";

                    // Write probe irradiance
                    std::string filename = baseName + "-Irradiance";

                    uint32_t width = 0, height = 0, arraySize = 0;
                    rtxgi::DDGIVolumeDesc desc = volume->GetDesc();
                    GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::Irradiance, width, height, arraySize);
                    VkFormat format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Irradiance, desc.probeIrradianceFormat);
                    success &= WriteResourceToDisk(vk, filename, volume->GetProbeIrradiance(), width, height, arraySize, format, VK_IMAGE_LAYOUT_GENERAL);

                    // Write probe data
                    if(volume->GetProbeRelocationEnabled() || volume->GetProbeClassificationEnabled())
                    {
                        filename = baseName + "-ProbeData";
                        GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::Data, width, height, arraySize);
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Data, desc.probeDataFormat);
                        success &= WriteResourceToDisk(vk, filename, volume->GetProbeData(), width, height, arraySize, format, VK_IMAGE_LAYOUT_GENERAL);
                    }

                    // Write probe variability
                    if (volume->GetProbeVariabilityEnabled())
                    {
                        filename = baseName + "-Probe-Variability";
                        GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::Variability, width, height, arraySize);
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Variability, desc.probeVariabilityFormat);
                        success &= WriteResourceToDisk(vk, filename, volume->GetProbeVariability(), width, height, arraySize, format, VK_IMAGE_LAYOUT_GENERAL);
                        filename = baseName + "-Probe-Variability-Average";
                        GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::VariabilityAverage, width, height, arraySize);
                        format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::VariabilityAverage, desc.probeVariabilityFormat);
                        success &= WriteResourceToDisk(vk, filename, volume->GetProbeVariabilityAverage(), width, height, arraySize, format, VK_IMAGE_LAYOUT_GENERAL);
                    }
                }
                return success;
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

        bool WriteVolumesToDisk(Globals& vk, GlobalResources& vkResources, Resources& resources, std::string directory)
        {
            return Graphics::Vulkan::DDGI::WriteVolumesToDisk(vk, vkResources, resources, directory);
        }

    } // namespace Graphics::DDGI
}
