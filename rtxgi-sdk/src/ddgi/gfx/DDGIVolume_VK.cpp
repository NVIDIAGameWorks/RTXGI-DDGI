/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "rtxgi/ddgi/gfx/DDGIVolume_VK.h"

#include <cstring>

#define VKFAILED(x) (x != VK_SUCCESS)

#ifdef RTXGI_GFX_NAME_OBJECTS
/**
 * Sets a debug name for an object.
 */
void SetObjectName(VkDevice device, uint64_t handle, const char* name, VkObjectType type)
{
    VkDebugUtilsObjectNameInfoEXT objectNameInfo = {};
    objectNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    objectNameInfo.pNext = nullptr;
    objectNameInfo.objectType = type;
    objectNameInfo.objectHandle = handle;
    objectNameInfo.pObjectName = name;

    vkSetDebugUtilsObjectNameEXT(device, &objectNameInfo);
}
#endif

/**
 * Add a performance marker to the command buffer.
 */
void AddPerfMarker(VkCommandBuffer cmdBuffer, uint8_t r, uint8_t g, uint8_t b, std::string name)
{
    VkDebugUtilsLabelEXT label = {};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = name.c_str();
    label.color[0] = (float)r / 255.f;
    label.color[1] = (float)g / 255.f;
    label.color[2] = (float)b / 255.f;
    label.color[3] = 1.f;
    vkCmdBeginDebugUtilsLabelEXT(cmdBuffer, &label);
}

namespace rtxgi
{
    namespace vulkan
    {
        enum EDDGIVolumeLayoutBindings
        {
            Constants = 0,
            ProbeRayData,
            ProbeIrradiance,
            ProbeDistance,
            ProbeData,
        };

        //------------------------------------------------------------------------
        // Private RTXGI Namespace Helper Functions
        //------------------------------------------------------------------------

        ERTXGIStatus ValidateManagedResourcesDesc(const DDGIVolumeManagedResourcesDesc& desc)
        {
            // Vulkan devices and descriptor pool
            if (desc.device == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_DEVICE;
            if (desc.physicalDevice == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PHYSICAL_DEVICE;
            if (desc.descriptorPool == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_DESCRIPTOR_POOL;

            // Shader bytecode
            if (!ValidateShaderBytecode(desc.probeBlendingIrradianceCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_BLENDING_IRRADIANCE;
            if (!ValidateShaderBytecode(desc.probeBlendingDistanceCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_BLENDING_DISTANCE;
            if (!ValidateShaderBytecode(desc.probeBorderRowUpdateIrradianceCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_BORDER_ROW_UPDATE_IRRADIANCE;
            if (!ValidateShaderBytecode(desc.probeBorderRowUpdateDistanceCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_BORDER_ROW_UPDATE_DISTANCE;
            if (!ValidateShaderBytecode(desc.probeBorderColumnUpdateIrradianceCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_BORDER_COLUMN_UPDATE_IRRADIANCE;
            if (!ValidateShaderBytecode(desc.probeBorderColumnUpdateDistanceCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_BORDER_COLUMN_UPDATE_DISTANCE;

            if (!ValidateShaderBytecode(desc.probeRelocation.updateCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_RELOCATION;
            if (!ValidateShaderBytecode(desc.probeRelocation.resetCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_RELOCATION_RESET;

            if (!ValidateShaderBytecode(desc.probeClassification.updateCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_CLASSIFICATION;
            if (!ValidateShaderBytecode(desc.probeClassification.resetCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_CLASSIFICATION_RESET;

            return ERTXGIStatus::OK;
        }

        ERTXGIStatus ValidateUnmanagedResourcesDesc(const DDGIVolumeUnmanagedResourcesDesc& desc)
        {
            // Pipeline Layout and Descriptor Set
            if (desc.pipelineLayout == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_LAYOUT;
            if (desc.descriptorSet == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_DESCRIPTOR_SET;

            // Textures
            if (desc.probeRayData == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_RAY_DATA;
            if (desc.probeIrradiance == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_IRRADIANCE;
            if (desc.probeDistance == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_DISTANCE;
            if (desc.probeData == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_DATA;

            // Texture Memory
            if (desc.probeRayDataMemory == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_MEMORY_PROBE_RAY_DATA;
            if (desc.probeIrradianceMemory == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_MEMORY_PROBE_IRRADIANCE;
            if (desc.probeDistanceMemory == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_MEMORY_PROBE_DISTANCE;
            if (desc.probeDataMemory == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_MEMORY_PROBE_DATA;

            // Texture Views
            if (desc.probeRayDataView == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_VIEW_PROBE_RAY_DATA;
            if (desc.probeIrradianceView == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_VIEW_PROBE_IRRADIANCE;
            if (desc.probeDistanceView == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_VIEW_PROBE_DISTANCE;
            if (desc.probeDataView == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_VIEW_PROBE_DATA;

            // Shader Modules
            if (desc.probeBlendingIrradianceModule == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_SHADER_MODULE_PROBE_BLENDING_IRRADIANCE;
            if (desc.probeBlendingDistanceModule == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_SHADER_MODULE_PROBE_BLENDING_DISTANCE;
            if (desc.probeBorderRowUpdateIrradianceModule == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_SHADER_MODULE_BORDER_ROW_UPDATE_IRRADIANCE;
            if (desc.probeBorderRowUpdateDistanceModule == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_SHADER_MODULE_BORDER_ROW_UPDATE_DISTANCE;
            if (desc.probeBorderColumnUpdateIrradianceModule == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_SHADER_MODULE_BORDER_COLUMN_UPDATE_IRRADIANCE;
            if (desc.probeBorderColumnUpdateDistanceModule == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_SHADER_MODULE_BORDER_COLUMN_UPDATE_DISTANCE;

            if (desc.probeRelocation.updateModule == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_SHADER_MODULE_PROBE_RELOCATION;
            if (desc.probeRelocation.resetModule == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_SHADER_MODULE_PROBE_RELOCATION_RESET;

            if (desc.probeClassification.updateModule == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_SHADER_MODULE_PROBE_CLASSIFICATION;
            if (desc.probeClassification.resetModule == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_SHADER_MODULE_PROBE_CLASSIFICATION_RESET;

            // Pipelines
            if (desc.probeBlendingIrradiancePipeline == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_PROBE_BLENDING_IRRADIANCE;
            if (desc.probeBlendingDistancePipeline == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_PROBE_BLENDING_DISTANCE;
            if (desc.probeBorderRowUpdateIrradiancePipeline == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_BORDER_ROW_UPDATE_IRRADIANCE;
            if (desc.probeBorderRowUpdateDistancePipeline == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_BORDER_ROW_UPDATE_DISTANCE;
            if (desc.probeBorderColumnUpdateIrradiancePipeline == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_BORDER_COLUMN_UPDATE_IRRADIANCE;
            if (desc.probeBorderColumnUpdateDistancePipeline == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_BORDER_COLUMN_UPDATE_DISTANCE;

            if (desc.probeRelocation.updatePipeline == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_PROBE_RELOCATION;
            if (desc.probeRelocation.resetPipeline == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_PROBE_RELOCATION_RESET;

            if (desc.probeClassification.updatePipeline == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_PROBE_CLASSIFICATION;
            if (desc.probeClassification.resetPipeline == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_PROBE_CLASSIFICATION_RESET;

            return ERTXGIStatus::OK;
        }

        //------------------------------------------------------------------------
        // Public RTXGI Namespace DDGI Functions
        //------------------------------------------------------------------------

        VkFormat GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType type, uint32_t format)
        {
            if (type == EDDGIVolumeTextureType::RayData)
            {
                if (format == 0) return VK_FORMAT_R32G32_SFLOAT;
                else if (format == 1) return VK_FORMAT_R32G32B32A32_SFLOAT;
            }
            else if (type == EDDGIVolumeTextureType::Irradiance)
            {
                if (format == 0) return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
                else if (format == 1) return VK_FORMAT_R16G16B16A16_SFLOAT;
                else if (format == 2) return VK_FORMAT_R32G32B32A32_SFLOAT;
            }
            else if (type == EDDGIVolumeTextureType::Distance)
            {
                if (format == 0) return VK_FORMAT_R16G16_SFLOAT;  // Note: in large environments FP16 may not be sufficient
                else if (format == 1) return VK_FORMAT_R32G32_SFLOAT;
            }
            else if (type == EDDGIVolumeTextureType::Data)
            {
                if (format == 0) return VK_FORMAT_R16G16B16A16_SFLOAT;
                else if (format == 1) return VK_FORMAT_R32G32B32A32_SFLOAT;
            }
            return VK_FORMAT_UNDEFINED;
        }

        void GetDDGIVolumeLayoutDescs(
            std::vector<VkDescriptorSetLayoutBinding>& bindings,
            VkDescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo,
            VkPushConstantRange& pushConstantRange,
            VkPipelineLayoutCreateInfo& pipelineLayoutCreateInfo)
        {
            // Describe the descriptor set layout bindings
            // 1 DDGIVolume constants structured buffer     (0)
            // 1 Storage Image for ray data                 (1)
            // 1 Storage Image for probe irradiance         (2)
            // 1 Storage Image for probe distance           (3)
            // 1 Storage Image for probe data               (4)

            // 0: DDGIVolume constants structured buffer
            VkDescriptorSetLayoutBinding constantsBinding = {};
            constantsBinding.binding = EDDGIVolumeLayoutBindings::Constants;
            constantsBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            constantsBinding.descriptorCount = 1;
            constantsBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            bindings.push_back(constantsBinding);

            // 1: Probe ray data
            VkDescriptorSetLayoutBinding rayDataBinding = {};
            rayDataBinding.binding = EDDGIVolumeLayoutBindings::ProbeRayData;
            rayDataBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            rayDataBinding.descriptorCount = 1;
            rayDataBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            bindings.push_back(rayDataBinding);

            // 2: Irradiance
            VkDescriptorSetLayoutBinding irradianceBinding = {};
            irradianceBinding.binding = EDDGIVolumeLayoutBindings::ProbeIrradiance;
            irradianceBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            irradianceBinding.descriptorCount = 1;
            irradianceBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            bindings.push_back(irradianceBinding);

            // 3: Distance
            VkDescriptorSetLayoutBinding distanceBinding = {};
            distanceBinding.binding = EDDGIVolumeLayoutBindings::ProbeDistance;
            distanceBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            distanceBinding.descriptorCount = 1;
            distanceBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            bindings.push_back(distanceBinding);

            // 4: Probe Data
            VkDescriptorSetLayoutBinding dataBinding = {};
            dataBinding.binding = EDDGIVolumeLayoutBindings::ProbeData;
            dataBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            dataBinding.descriptorCount = 1;
            dataBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            bindings.push_back(dataBinding);

            // Describe the descriptor set layout
            descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
            descriptorSetLayoutCreateInfo.pBindings = bindings.data();

            // Describe the push constants
            pushConstantRange.stageFlags = VK_SHADER_STAGE_ALL;
            pushConstantRange.offset = 0;
            pushConstantRange.size = DDGIConstants::GetAlignedSizeInBytes();

            // Describe the pipeline layout
            pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutCreateInfo.setLayoutCount = 1;
            pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
            pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
        }

        ERTXGIStatus UploadDDGIVolumeConstants(VkDevice device, VkCommandBuffer cmdBuffer, uint32_t bufferingIndex, uint32_t numVolumes, DDGIVolume** volumes)
        {
            // Copy the constants for each volume
            for (uint32_t volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                // Get the volume
                const DDGIVolume* volume = volumes[volumeIndex];

                // Validate the upload and device buffers
                if (volume->GetConstantsBuffer() == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_CONSTANTS_BUFFER;
                if (volume->GetConstantsBufferUpload() == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_CONSTANTS_UPLOAD_BUFFER;
                if (volume->GetConstantsBufferUploadMemory() == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_CONSTANTS_UPLOAD_MEMORY;

                // Offset to the constants data to write to (e.g. double buffering)
                uint64_t bufferOffset = volume->GetConstantsBufferSizeInBytes() * bufferingIndex;

                // Offset to the volume in current constants buffer
                uint32_t volumeOffset = (volume->GetIndex() * (uint32_t)sizeof(DDGIVolumeDescGPUPacked));

                // Offset to the volume constants in the upload buffer
                uint64_t srcOffset = (bufferOffset + volumeOffset);

                // Get the packed DDGIVolume GPU descriptor
                DDGIVolumeDescGPUPacked gpuDesc = volume->GetDescGPUPacked();

                // Map the constant buffer and update it
                void* pData = nullptr;
                VkResult result = vkMapMemory(device, volume->GetConstantsBufferUploadMemory(), srcOffset, sizeof(DDGIVolumeDescGPUPacked), 0, &pData);
                if (VKFAILED(result)) return ERTXGIStatus::ERROR_DDGI_MAP_FAILURE_CONSTANTS_UPLOAD_BUFFER;

                memcpy(pData, &gpuDesc, sizeof(DDGIVolumeDescGPUPacked));

                vkUnmapMemory(device, volume->GetConstantsBufferUploadMemory());

                // Schedule a copy of the upload buffer to the device buffer
                VkBufferCopy bufferCopy = {};
                bufferCopy.size = sizeof(DDGIVolumeDescGPUPacked);
                bufferCopy.srcOffset = srcOffset;
                bufferCopy.dstOffset = volumeOffset;
                vkCmdCopyBuffer(cmdBuffer, volume->GetConstantsBufferUpload(), volume->GetConstantsBuffer(), 1, &bufferCopy);
            }

            return ERTXGIStatus::OK;
        }

        ERTXGIStatus UpdateDDGIVolumeProbes(VkCommandBuffer cmdBuffer, uint32_t numVolumes, DDGIVolume** volumes)
        {
            if (bInsertPerfMarkers)AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, "Update Probes");

            uint32_t volumeIndex;
            std::vector<VkImageMemoryBarrier> barriers;

            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            // Probe Blending
            if (bInsertPerfMarkers)AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, "Probe Blending");

            // Irradiance
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];

                // Set push constants
                DDGIConstants consts =
                {
                    volume->GetIndex(),
                    volume->GetDescriptorBindlessUAVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                    volume->GetDescriptorBindlessSRVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                };

                // Bind the descriptor set and push constants
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetPipelineLayout(), 0, 1, volume->GetDescriptorSetConstPtr(), 0, nullptr);
                vkCmdPushConstants(cmdBuffer, volume->GetPipelineLayout(), VK_SHADER_STAGE_ALL, volume->GetPushConstantsOffset(), DDGIConstants::GetSizeInBytes(), consts.GetData());

                // Get the number of probes on the X and Y dimensions of the texture
                uint32_t probeCountX, probeCountY;
                GetDDGIVolumeProbeCounts(volume->GetDesc(), probeCountX, probeCountY);

                // Probe irradiance blending
                {
                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers())
                    {
                        std::string msg = "Irradiance, DDGIVolume[" + std::to_string(volume->GetIndex()) + "] - \"" + volume->GetName() + "\"";
                        AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, msg.c_str());
                    }

                    // Bind the pipeline and dispatch threads
                    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetProbeBlendingIrradiancePipeline());
                    vkCmdDispatch(cmdBuffer, probeCountX, probeCountY, 1);

                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers()) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);
                }

                // Add a barrier
                barrier.image = volume->GetProbeIrradiance();
                barriers.push_back(barrier);
            }

            // Distance
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];

                // Set push constants
                DDGIConstants consts =
                {
                    volume->GetIndex(),
                    volume->GetDescriptorBindlessUAVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                    volume->GetDescriptorBindlessSRVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                };

                // Bind the descriptor set and push constants
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetPipelineLayout(), 0, 1, volume->GetDescriptorSetConstPtr(), 0, nullptr);
                vkCmdPushConstants(cmdBuffer, volume->GetPipelineLayout(), VK_SHADER_STAGE_ALL, volume->GetPushConstantsOffset(), DDGIConstants::GetSizeInBytes(), consts.GetData());

                // Get the number of probes on the X and Y dimensions of the texture
                uint32_t probeCountX, probeCountY;
                GetDDGIVolumeProbeCounts(volume->GetDesc(), probeCountX, probeCountY);

                // Probe distance blending
                {
                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers())
                    {
                        std::string msg = "Distance, DDGIVolume[" + std::to_string(volume->GetIndex()) + "] - \"" + volume->GetName() + "\"";
                        AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, msg.c_str());
                    }

                    // Bind the pipeline and dispatch threads
                    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetProbeBlendingDistancePipeline());
                    vkCmdDispatch(cmdBuffer, probeCountX, probeCountY, 1);

                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers()) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);
                }

                // Add a barrier
                barrier.image = volume->GetProbeDistance();
                barriers.push_back(barrier);
            }

            if (bInsertPerfMarkers) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);

            // Wait for the irradiance and distance blending passes
            // to complete before updating the borders
            if(!barriers.empty())
            {
                vkCmdPipelineBarrier(
                    cmdBuffer,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    static_cast<uint32_t>(barriers.size()), barriers.data());
            }

            // Probe Border Update
            if (bInsertPerfMarkers) AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, "Probe Border Update");

            float groupSize = 8.f;
            uint32_t numThreadsX, numThreadsY;
            uint32_t numGroupsX, numGroupsY;
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];

                DDGIConstants consts =
                {
                    volume->GetIndex(),
                    volume->GetDescriptorBindlessUAVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                    volume->GetDescriptorBindlessSRVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                };

                // Bind the descriptor set and push constants
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetPipelineLayout(), 0, 1, volume->GetDescriptorSetConstPtr(), 0, nullptr);
                vkCmdPushConstants(cmdBuffer, volume->GetPipelineLayout(), VK_SHADER_STAGE_ALL, volume->GetPushConstantsOffset(), DDGIConstants::GetSizeInBytes(), consts.GetData());

                // Get the number of probes on the X and Y dimensions of the texture
                uint32_t probeCountX, probeCountY;
                GetDDGIVolumeProbeCounts(volume->GetDesc(), probeCountX, probeCountY);

                // Probe irradiance border update
                {
                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers())
                    {
                        std::string msg = "Irradiance, DDGIVolume[" + std::to_string(volume->GetIndex()) + "] - \"" + volume->GetName() + "\"";
                        AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, msg.c_str());
                    }

                    // Rows
                    numThreadsX = (probeCountX * ((uint32_t)volume->GetDesc().probeNumIrradianceTexels + 2));
                    numThreadsY = probeCountY;
                    numGroupsX = (uint32_t)ceil((float)numThreadsX / groupSize);
                    numGroupsY = (uint32_t)ceil((float)numThreadsY / groupSize);

                    // Bind the pipeline and dispatch threads
                    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetProbeBorderRowUpdateIrradiancePipeline());
                    vkCmdDispatch(cmdBuffer, numGroupsX, numGroupsY, 1);

                    // Columns
                    numThreadsX = probeCountX;
                    numThreadsY = (probeCountY * ((uint32_t)volume->GetDesc().probeNumIrradianceTexels + 2));
                    numGroupsX = (uint32_t)ceil((float)numThreadsX / groupSize);
                    numGroupsY = (uint32_t)ceil((float)numThreadsY / groupSize);

                    // Bind the pipeline and dispatch threads
                    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetProbeBorderColumnUpdateIrradiancePipeline());
                    vkCmdDispatch(cmdBuffer, numGroupsX, numGroupsY, 1);

                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers()) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);
                }

                // Probe distance border update
                {
                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers())
                    {
                        std::string msg = "Distance, DDGIVolume[" + std::to_string(volume->GetIndex()) + "] - \"" + volume->GetName() + "\"";
                        AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, msg.c_str());
                    }

                    // Rows
                    numThreadsX = (probeCountX * ((uint32_t)volume->GetDesc().probeNumDistanceTexels + 2));
                    numThreadsY = probeCountY;
                    numGroupsX = (uint32_t)ceil((float)numThreadsX / groupSize);
                    numGroupsY = (uint32_t)ceil((float)numThreadsY / groupSize);

                    // Bind the pipeline and dispatch threads
                    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetProbeBorderRowUpdateDistancePipeline());
                    vkCmdDispatch(cmdBuffer, numGroupsX, numGroupsY, 1);

                    // Columns
                    numThreadsX = probeCountX;
                    numThreadsY = (probeCountY * ((uint32_t)volume->GetDesc().probeNumDistanceTexels + 2));
                    numGroupsX = (uint32_t)ceil((float)numThreadsX / groupSize);
                    numGroupsY = (uint32_t)ceil((float)numThreadsY / groupSize);

                    // Bind the pipeline and dispatch threads
                    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetProbeBorderColumnUpdateDistancePipeline());
                    vkCmdDispatch(cmdBuffer, numGroupsX, numGroupsY, 1);

                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers()) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);
                }
            }

            if (bInsertPerfMarkers) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);

            // Wait for the irradiance and distance blending passes
            // to complete before updating the borders
            if(!barriers.empty())
            {
                vkCmdPipelineBarrier(
                    cmdBuffer,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    static_cast<uint32_t>(barriers.size()), barriers.data());
            }

            return ERTXGIStatus::OK;
        }

        ERTXGIStatus RelocateDDGIVolumeProbes(VkCommandBuffer cmdBuffer, uint32_t numVolumes, DDGIVolume** volumes)
        {
            if (bInsertPerfMarkers) AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, "Relocate Probes");

            uint32_t volumeIndex;
            std::vector<VkImageMemoryBarrier> barriers;

            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            // Probe Relocation Reset
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];
                if (!volume->GetProbeRelocationEnabled()) continue;     // Skip if relocation is not enabled for this volume
                if (!volume->GetProbeRelocationNeedsReset()) continue;  // Skip if the volume doesn't need to be reset

                // Set push constants
                DDGIConstants consts =
                {
                    volume->GetIndex(),
                    volume->GetDescriptorBindlessUAVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                    volume->GetDescriptorBindlessSRVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                };

                // Bind descriptor set and push constants
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetPipelineLayout(), 0, 1, volume->GetDescriptorSetConstPtr(), 0, nullptr);
                vkCmdPushConstants(cmdBuffer, volume->GetPipelineLayout(), VK_SHADER_STAGE_ALL, volume->GetPushConstantsOffset(), DDGIConstants::GetSizeInBytes(), consts.GetData());

                const float groupSizeX = 32.f;

                // Reset all probe offsets to zero
                uint32_t numGroupsX = (uint32_t)ceil((float)volume->GetNumProbes() / groupSizeX);
                vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetProbeRelocationResetPipeline());
                vkCmdDispatch(cmdBuffer, numGroupsX, 1, 1);

                // Update the reset flag
                volumes[volumeIndex]->SetProbeRelocationNeedsReset(false);

                // Add a barrier
                barrier.image = volume->GetProbeData();
                barriers.push_back(barrier);
            }

            // Probe Relocation Reset Barrier(s)
            if(!barriers.empty())
            {
                // Wait for the compute pass to complete
                vkCmdPipelineBarrier(
                    cmdBuffer,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    static_cast<uint32_t>(barriers.size()), barriers.data());
            }

            barriers.clear();

            // Probe Relocation
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];
                if (!volume->GetProbeRelocationEnabled()) continue;  // Skip if relocation is not enabled for this volume

                // Set push constants
                DDGIConstants consts =
                {
                    volume->GetIndex(),
                    volume->GetDescriptorBindlessUAVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                    volume->GetDescriptorBindlessSRVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                };

                // Bind descriptor set and push constants
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetPipelineLayout(), 0, 1, volume->GetDescriptorSetConstPtr(), 0, nullptr);
                vkCmdPushConstants(cmdBuffer, volume->GetPipelineLayout(), VK_SHADER_STAGE_ALL, volume->GetPushConstantsOffset(), DDGIConstants::GetSizeInBytes(), consts.GetData());

                float groupSizeX = 32.f;

                // Probe relocation
                uint32_t numGroupsX = (uint32_t)ceil((float)volume->GetNumProbes() / groupSizeX);
                vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetProbeRelocationPipeline());
                vkCmdDispatch(cmdBuffer, numGroupsX, 1, 1);

                // Add a barrier
                barrier.image = volume->GetProbeData();
                barriers.push_back(barrier);
            }

            // Probe Relocation Barrier(s)
            if (!barriers.empty())
            {
                // Wait for the compute pass to complete
                vkCmdPipelineBarrier(
                    cmdBuffer,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    static_cast<uint32_t>(barriers.size()), barriers.data());
            }

            if (bInsertPerfMarkers) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);

            return ERTXGIStatus::OK;
        }

        ERTXGIStatus ClassifyDDGIVolumeProbes(VkCommandBuffer cmdBuffer, uint32_t numVolumes, DDGIVolume** volumes)
        {
            if (bInsertPerfMarkers) AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, "Classify Probes");

            uint32_t volumeIndex;
            std::vector<VkImageMemoryBarrier> barriers;

            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            // Probe Classification Reset
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];
                if (!volume->GetProbeClassificationEnabled()) continue;     // Skip if classification is not enabled for this volume
                if (!volume->GetProbeClassificationNeedsReset()) continue;  // Skip if the volume doesn't need to be reset

                // Set push constants
                DDGIConstants consts =
                {
                    volume->GetIndex(),
                    volume->GetDescriptorBindlessUAVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                    volume->GetDescriptorBindlessSRVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                };

                // Bind descriptor set and push constants
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetPipelineLayout(), 0, 1, volume->GetDescriptorSetConstPtr(), 0, nullptr);
                vkCmdPushConstants(cmdBuffer, volume->GetPipelineLayout(), VK_SHADER_STAGE_ALL, volume->GetPushConstantsOffset(), DDGIConstants::GetSizeInBytes(), consts.GetData());

                const float groupSizeX = 32.f;

                // Reset all probe states to the ACTIVE state
                uint32_t numGroupsX = (uint32_t)ceil((float)volume->GetNumProbes() / groupSizeX);
                vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetProbeClassificationResetPipeline());
                vkCmdDispatch(cmdBuffer, numGroupsX, 1, 1);

                // Update the reset flag
                volumes[volumeIndex]->SetProbeClassificationNeedsReset(false);

                // Add a barrier
                barrier.image = volume->GetProbeData();
                barriers.push_back(barrier);
            }

            // Probe Classification Reset Barrier(s)
            if (!barriers.empty())
            {
                // Wait for the compute pass to complete
                vkCmdPipelineBarrier(
                    cmdBuffer,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    static_cast<uint32_t>(barriers.size()), barriers.data());
            }

            barriers.clear();

            // Probe Classification
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];
                if (!volume->GetProbeClassificationEnabled()) continue;  // Skip if classification is not enabled for this volume

                // Set push constants
                DDGIConstants consts =
                {
                    volume->GetIndex(),
                    volume->GetDescriptorBindlessUAVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                    volume->GetDescriptorBindlessSRVOffset(), // ignored when shaders do not define RTXGI_DDGI_BINDLESS_RESOURCES
                };

                // Bind descriptor set and push constants
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetPipelineLayout(), 0, 1, volume->GetDescriptorSetConstPtr(), 0, nullptr);
                vkCmdPushConstants(cmdBuffer, volume->GetPipelineLayout(), VK_SHADER_STAGE_ALL, volume->GetPushConstantsOffset(), DDGIConstants::GetSizeInBytes(), consts.GetData());

                const float groupSizeX = 32.f;

                // Probe classification
                uint32_t numGroupsX = (uint32_t)ceil((float)volume->GetNumProbes() / groupSizeX);
                vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetProbeClassificationPipeline());
                vkCmdDispatch(cmdBuffer, numGroupsX, 1, 1);

                // Add a barrier
                barrier.image = volume->GetProbeData();
                barriers.push_back(barrier);
            }

            // Probe Classification Barrier(s)
            if (!barriers.empty())
            {
                // Wait for the compute pass to complete
                vkCmdPipelineBarrier(
                    cmdBuffer,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    static_cast<uint32_t>(barriers.size()), barriers.data());
            }

            if (bInsertPerfMarkers) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);

            return ERTXGIStatus::OK;
        }

        //------------------------------------------------------------------------
        // Private DDGIVolume Functions
        //------------------------------------------------------------------------

    #if RTXGI_DDGI_RESOURCE_MANAGEMENT
        void DDGIVolume::ReleaseManagedResources()
        {
            // Release the pipeline layout and descriptor set layout
            vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
            vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

            // Release the existing shader modules
            vkDestroyShaderModule(m_device, m_probeBlendingIrradianceModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeBlendingDistanceModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeBorderRowUpdateIrradianceModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeBorderRowUpdateDistanceModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeBorderColumnUpdateIrradianceModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeBorderColumnUpdateDistanceModule, nullptr);

            vkDestroyShaderModule(m_device, m_probeRelocationModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeRelocationResetModule, nullptr);

            vkDestroyShaderModule(m_device, m_probeClassificationModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeClassificationResetModule, nullptr);

            // Release the existing compute pipelines
            vkDestroyPipeline(m_device, m_probeBlendingIrradiancePipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeBlendingDistancePipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeBorderRowUpdateIrradiancePipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeBorderRowUpdateDistancePipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeBorderColumnUpdateIrradiancePipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeBorderColumnUpdateDistancePipeline, nullptr);

            vkDestroyPipeline(m_device, m_probeRelocationPipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeRelocationResetPipeline, nullptr);

            vkDestroyPipeline(m_device, m_probeClassificationPipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeClassificationResetPipeline, nullptr);
        }

        ERTXGIStatus DDGIVolume::CreateManagedResources(const DDGIVolumeDesc& desc, const DDGIVolumeManagedResourcesDesc& managed)
        {
            bool deviceChanged = IsDeviceChanged(managed);

            // Create the descriptor set layout, pipeline layout, and pipelines
            if (deviceChanged)
            {
                // The device may have changed, release resources on that device
                if (m_device != nullptr) ReleaseManagedResources();

                // Store the handle to the new device and descriptor pool
                m_device = managed.device;
                m_physicalDevice = managed.physicalDevice;
                m_descriptorPool = managed.descriptorPool;

                // Create the descriptor set layout and pipeline layout
                if(!CreateLayouts()) return ERTXGIStatus::ERROR_DDGI_VK_CREATE_FAILURE_LAYOUTS;

                // Create the compute pipelines
                if (!CreateComputePipeline(
                    managed.probeBlendingIrradianceCS,
                    "DDGIProbeBlendingCS",
                    &m_probeBlendingIrradianceModule,
                    &m_probeBlendingIrradiancePipeline,
                    "Probe Irradiance Blending")) return ERTXGIStatus::ERROR_DDGI_VK_CREATE_FAILURE_PIPELINE;

                if (!CreateComputePipeline(
                    managed.probeBlendingDistanceCS,
                    "DDGIProbeBlendingCS",
                    &m_probeBlendingDistanceModule,
                    &m_probeBlendingDistancePipeline,
                    "Probe Distance Blending")) return ERTXGIStatus::ERROR_DDGI_VK_CREATE_FAILURE_PIPELINE;

                if (!CreateComputePipeline(
                    managed.probeBorderRowUpdateIrradianceCS,
                    "DDGIProbeBorderRowUpdateCS",
                    &m_probeBorderRowUpdateIrradianceModule,
                    &m_probeBorderRowUpdateIrradiancePipeline,
                    "Probe Border Row Update (Irradiance)")) return ERTXGIStatus::ERROR_DDGI_VK_CREATE_FAILURE_PIPELINE;

                if (!CreateComputePipeline(
                    managed.probeBorderRowUpdateDistanceCS,
                    "DDGIProbeBorderRowUpdateCS",
                    &m_probeBorderRowUpdateDistanceModule,
                    &m_probeBorderRowUpdateDistancePipeline,
                    "Probe Border Row Update (Distance)")) return ERTXGIStatus::ERROR_DDGI_VK_CREATE_FAILURE_PIPELINE;

                if (!CreateComputePipeline(
                    managed.probeBorderColumnUpdateIrradianceCS,
                    "DDGIProbeBorderColumnUpdateCS",
                    &m_probeBorderColumnUpdateIrradianceModule,
                    &m_probeBorderColumnUpdateIrradiancePipeline,
                    "Probe Border Column Update (Irradiance)")) return ERTXGIStatus::ERROR_DDGI_VK_CREATE_FAILURE_PIPELINE;

                if (!CreateComputePipeline(
                    managed.probeBorderColumnUpdateDistanceCS,
                    "DDGIProbeBorderColumnUpdateCS",
                    &m_probeBorderColumnUpdateDistanceModule,
                    &m_probeBorderColumnUpdateDistancePipeline,
                    "Probe Border Column Update (Distance)")) return ERTXGIStatus::ERROR_DDGI_VK_CREATE_FAILURE_PIPELINE;

                if (!CreateComputePipeline(
                    managed.probeRelocation.updateCS,
                    "DDGIProbeRelocationCS",
                    &m_probeRelocationModule,
                    &m_probeRelocationPipeline,
                    "Probe Relocation")) return ERTXGIStatus::ERROR_DDGI_VK_CREATE_FAILURE_PIPELINE;

                if (!CreateComputePipeline(
                    managed.probeRelocation.resetCS,
                    "DDGIProbeRelocationResetCS",
                    &m_probeRelocationResetModule,
                    &m_probeRelocationResetPipeline,
                    "Probe Relocation Reset")) return ERTXGIStatus::ERROR_DDGI_VK_CREATE_FAILURE_PIPELINE;

                if (!CreateComputePipeline(
                    managed.probeClassification.updateCS,
                    "DDGIProbeClassificationCS",
                    &m_probeClassificationModule,
                    &m_probeClassificationPipeline,
                    "Probe Classification")) return ERTXGIStatus::ERROR_DDGI_VK_CREATE_FAILURE_PIPELINE;

                if (!CreateComputePipeline(
                    managed.probeClassification.resetCS,
                    "DDGIProbeClassificationResetCS",
                    &m_probeClassificationResetModule,
                    &m_probeClassificationResetPipeline,
                    "Probe Classification Reset")) return ERTXGIStatus::ERROR_DDGI_VK_CREATE_FAILURE_PIPELINE;
            }

            // Create the textures
            if (deviceChanged || m_desc.ShouldAllocateProbes(desc))
            {
                // Probe counts have changed. The textures are the wrong size or aren't allocated yet.
                // (Re)allocate the probe ray data, irradiance, distance, and data textures.
                if (!CreateProbeRayData(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_RAY_DATA;
                if (!CreateProbeIrradiance(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_IRRADIANCE;
                if (!CreateProbeDistance(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_DISTANCE;
                if (!CreateProbeData(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_DATA;
            }
            else
            {
                if (m_desc.ShouldAllocateRayData(desc))
                {
                    // The number of probe rays to trace per frame has changed. Reallocate the radiance texture.
                    if (!CreateProbeRayData(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_RAY_DATA;
                }

                if (m_desc.ShouldAllocateIrradiance(desc))
                {
                    // The number of irradiance texels per probe has changed. Reallocate the irradiance texture.
                    if (!CreateProbeIrradiance(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_IRRADIANCE;
                }

                if (m_desc.ShouldAllocateDistance(desc))
                {
                    // The number of distance texels per probe has changed. Reallocate the distance texture.
                    if (!CreateProbeDistance(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_DISTANCE;
                }
            }

            return ERTXGIStatus::OK;
        }
    #else
        void DDGIVolume::StoreUnmanagedResourcesDesc(const DDGIVolumeUnmanagedResourcesDesc& unmanaged)
        {
            // Pipeline layout and descriptor set
            m_pipelineLayout = unmanaged.pipelineLayout;
            m_descriptorSet = unmanaged.descriptorSet;

            // Textures
            m_probeRayData = unmanaged.probeRayData;
            m_probeIrradiance = unmanaged.probeIrradiance;
            m_probeDistance = unmanaged.probeDistance;
            m_probeData = unmanaged.probeData;

            // Texture Memory
            m_probeRayDataMemory = unmanaged.probeRayDataMemory;
            m_probeIrradianceMemory = unmanaged.probeIrradianceMemory;
            m_probeDistanceMemory = unmanaged.probeDistanceMemory;
            m_probeDataMemory = unmanaged.probeDataMemory;

            // Texture Views
            m_probeRayDataView = unmanaged.probeRayDataView;
            m_probeIrradianceView = unmanaged.probeIrradianceView;
            m_probeDistanceView = unmanaged.probeDistanceView;
            m_probeDataView = unmanaged.probeDataView;

            // Shader modules
            m_probeBlendingIrradianceModule = unmanaged.probeBlendingIrradianceModule;
            m_probeBlendingDistanceModule = unmanaged.probeBlendingDistanceModule;
            m_probeBorderRowUpdateIrradianceModule = unmanaged.probeBorderRowUpdateIrradianceModule;
            m_probeBorderRowUpdateDistanceModule = unmanaged.probeBorderRowUpdateDistanceModule;
            m_probeBorderColumnUpdateIrradianceModule = unmanaged.probeBorderColumnUpdateIrradianceModule;
            m_probeBorderColumnUpdateDistanceModule = unmanaged.probeBorderColumnUpdateDistanceModule;

            m_probeRelocationModule = unmanaged.probeRelocation.updateModule;
            m_probeRelocationResetModule = unmanaged.probeRelocation.resetModule;

            m_probeClassificationModule = unmanaged.probeClassification.updateModule;
            m_probeClassificationResetModule = unmanaged.probeClassification.resetModule;

            // Pipelines
            m_probeBlendingIrradiancePipeline = unmanaged.probeBlendingIrradiancePipeline;
            m_probeBlendingDistancePipeline = unmanaged.probeBlendingDistancePipeline;
            m_probeBorderRowUpdateIrradiancePipeline = unmanaged.probeBorderRowUpdateIrradiancePipeline;
            m_probeBorderRowUpdateDistancePipeline = unmanaged.probeBorderRowUpdateDistancePipeline;
            m_probeBorderColumnUpdateIrradiancePipeline = unmanaged.probeBorderColumnUpdateIrradiancePipeline;
            m_probeBorderColumnUpdateDistancePipeline = unmanaged.probeBorderColumnUpdateDistancePipeline;

            m_probeRelocationPipeline = unmanaged.probeRelocation.updatePipeline;
            m_probeRelocationResetPipeline = unmanaged.probeRelocation.resetPipeline;

            m_probeClassificationPipeline = unmanaged.probeClassification.updatePipeline;
            m_probeClassificationResetPipeline = unmanaged.probeClassification.resetPipeline;
        }
    #endif

        //------------------------------------------------------------------------
        // Public DDGIVolume Functions
        //------------------------------------------------------------------------

    #if RTXGI_DDGI_RESOURCE_MANAGEMENT
        ERTXGIStatus DDGIVolume::Create(VkCommandBuffer cmdBuffer, const DDGIVolumeDesc& desc, const DDGIVolumeResources& resources)
    #else
        ERTXGIStatus DDGIVolume::Create(const DDGIVolumeDesc& desc, const DDGIVolumeResources& resources)
    #endif
        {
            // Validate the probe counts
            if (desc.probeCounts.x <= 0 || desc.probeCounts.y <= 0 || desc.probeCounts.z <= 0)
            {
                return ERTXGIStatus::ERROR_DDGI_INVALID_PROBE_COUNTS;
            }

            // Validate the constants buffer
        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            if (resources.constantsBuffer == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_CONSTANTS_BUFFER;
        #endif

            // Validate the resource structures
            if (resources.managed.enabled && resources.unmanaged.enabled) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCES_DESC;
            if (!resources.managed.enabled && !resources.unmanaged.enabled) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCES_DESC;

            // Validate the resources
            ERTXGIStatus result = ERTXGIStatus::OK;
        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            result = ValidateManagedResourcesDesc(resources.managed);
        #else
            result = ValidateUnmanagedResourcesDesc(resources.unmanaged);
        #endif
            if (result != ERTXGIStatus::OK) return result;

            // Set the push constants offset (useful in bindless mode)
            m_pushConstantsOffset = resources.descriptorBindlessDesc.pushConstantsOffset;

            // Always stored (even in managed mode) for convenience. This is helpful when other parts of an application
            // (e.g. ray tracing passes) access resources bindlessly and use the volume to look up resource offsets.
            // See DDGI_D3D12.cpp::RayTraceVolume() for an example.
            m_descriptorBindlessUAVOffset = resources.descriptorBindlessDesc.uavOffset;
            m_descriptorBindlessSRVOffset = resources.descriptorBindlessDesc.srvOffset;

            // Store the constants structured buffer pointers
            if (resources.constantsBuffer) m_constantsBuffer = resources.constantsBuffer;
            if (resources.constantsBufferUpload) m_constantsBufferUpload = resources.constantsBufferUpload;
            if (resources.constantsBufferUploadMemory) m_constantsBufferUploadMemory = resources.constantsBufferUploadMemory;
            m_constantsBufferSizeInBytes = resources.constantsBufferSizeInBytes;

            // Allocate or store pointers to the texture and PSO resources
        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            result = CreateManagedResources(desc, resources.managed);
            if (result != ERTXGIStatus::OK) return result;
        #else
            StoreUnmanagedResourcesDesc(resources.unmanaged);
        #endif

            // Store the new volume descriptor
            m_desc = desc;

        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            // Transition textures for general use
            Transition(cmdBuffer);

            // Create the descriptor set
            if (!CreateDescriptorSet()) return ERTXGIStatus::ERROR_DDGI_VK_CREATE_FAILURE_DESCRIPTOR_SET;
        #endif

            // Store the volume rotation
            m_rotationMatrix = EulerAnglesToRotationMatrixYXZ(desc.eulerAngles);
            m_rotationQuaternion = RotationMatrixToQuaternion(m_rotationMatrix);

            // Set the default scroll anchor to the origin
            m_probeScrollAnchor = m_desc.origin;

            // Initialize the random number generator if a seed is provided,
            // otherwise the RNG uses the default std::random_device().
            if (desc.rngSeed != 0)
            {
                SeedRNG((int)desc.rngSeed);
            }
            else
            {
                std::random_device rd;
                SeedRNG((int)rd());
            }

            return ERTXGIStatus::OK;
        }

        ERTXGIStatus DDGIVolume::ClearProbes(VkCommandBuffer cmdBuffer)
        {
            if (bInsertPerfMarkers) AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, "Clear Probes");

            VkClearColorValue color = { { 0.f, 0.f, 0.f, 0.f } };
            VkImageSubresourceRange range;
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseMipLevel = 0;
            range.levelCount = 1;
            range.baseArrayLayer = 0;
            range.layerCount = 1;

            vkCmdClearColorImage(cmdBuffer, m_probeIrradiance, VK_IMAGE_LAYOUT_GENERAL, &color, 1, &range);
            vkCmdClearColorImage(cmdBuffer, m_probeDistance, VK_IMAGE_LAYOUT_GENERAL, &color, 1, &range);

            if (bInsertPerfMarkers) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);

            return ERTXGIStatus::OK;
        }

        void DDGIVolume::Destroy()
        {
            m_pushConstantsOffset = 0;
            m_descriptorBindlessUAVOffset = 0;
            m_descriptorBindlessSRVOffset = 0;

            m_constantsBuffer = nullptr;
            m_constantsBufferUpload = nullptr;
            m_constantsBufferUploadMemory = nullptr;
            m_constantsBufferSizeInBytes = 0;

            m_desc = {};

            m_rotationQuaternion = { 0.f, 0.f, 0.f, 1.f };
            m_rotationMatrix = {
                { 1.f, 0.f, 0.f },
                { 0.f, 1.f, 0.f },
                { 0.f, 0.f, 1.f }
            };
            m_probeRayRotationQuaternion = { 0.f, 0.f, 0.f, 1.f };
            m_probeRayRotationMatrix = {
                { 1.f, 0.f, 0.f },
                { 0.f, 1.f, 0.f },
                { 0.f, 0.f, 1.f },
            };

            m_probeScrollOffsets = {};

        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            // Layouts
            vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
            vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

            // Shader Modules
            vkDestroyShaderModule(m_device, m_probeBlendingIrradianceModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeBlendingDistanceModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeBorderRowUpdateIrradianceModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeBorderRowUpdateDistanceModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeBorderColumnUpdateIrradianceModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeBorderColumnUpdateDistanceModule, nullptr);

            vkDestroyShaderModule(m_device, m_probeRelocationModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeRelocationResetModule, nullptr);

            vkDestroyShaderModule(m_device, m_probeClassificationModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeClassificationResetModule, nullptr);

            // Pipelines
            vkDestroyPipeline(m_device, m_probeBlendingIrradiancePipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeBlendingDistancePipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeBorderRowUpdateIrradiancePipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeBorderRowUpdateDistancePipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeBorderColumnUpdateIrradiancePipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeBorderColumnUpdateDistancePipeline, nullptr);

            vkDestroyPipeline(m_device, m_probeRelocationPipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeRelocationResetPipeline, nullptr);

            vkDestroyPipeline(m_device, m_probeClassificationPipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeClassificationResetPipeline, nullptr);

            // Textures
            vkDestroyImage(m_device, m_probeRayData, nullptr);
            vkDestroyImageView(m_device, m_probeRayDataView, nullptr);
            vkFreeMemory(m_device, m_probeRayDataMemory, nullptr);

            vkDestroyImage(m_device, m_probeIrradiance, nullptr);
            vkDestroyImageView(m_device, m_probeIrradianceView, nullptr);
            vkFreeMemory(m_device, m_probeIrradianceMemory, nullptr);

            vkDestroyImage(m_device, m_probeDistance, nullptr);
            vkDestroyImageView(m_device, m_probeDistanceView, nullptr);
            vkFreeMemory(m_device, m_probeDistanceMemory, nullptr);

            vkDestroyImage(m_device, m_probeData, nullptr);
            vkDestroyImageView(m_device, m_probeDataView, nullptr);
            vkFreeMemory(m_device, m_probeDataMemory, nullptr);

            m_descriptorSetLayout = nullptr;
            m_descriptorPool = nullptr;
            m_device = nullptr;
            m_physicalDevice = nullptr;
        #endif

            m_descriptorSet = nullptr;
            m_pipelineLayout = nullptr;

            // Textures
            m_probeRayData = nullptr;
            m_probeRayDataMemory = nullptr;
            m_probeRayDataView = nullptr;
            m_probeIrradiance = nullptr;
            m_probeIrradianceMemory = nullptr;
            m_probeIrradianceView = nullptr;
            m_probeDistance = nullptr;
            m_probeDistanceMemory = nullptr;
            m_probeDistanceView = nullptr;
            m_probeData = nullptr;
            m_probeDataMemory = nullptr;
            m_probeDataView = nullptr;

            // Shader Modules
            m_probeBlendingIrradianceModule = nullptr;
            m_probeBlendingDistanceModule = nullptr;
            m_probeBorderRowUpdateIrradianceModule = nullptr;
            m_probeBorderRowUpdateDistanceModule = nullptr;
            m_probeBorderColumnUpdateIrradianceModule = nullptr;
            m_probeBorderColumnUpdateDistanceModule = nullptr;

            m_probeRelocationModule = nullptr;
            m_probeRelocationResetModule = nullptr;

            m_probeClassificationModule = nullptr;
            m_probeClassificationResetModule = nullptr;

            // Pipelines
            m_probeBlendingIrradiancePipeline = nullptr;
            m_probeBlendingDistancePipeline = nullptr;
            m_probeBorderRowUpdateIrradiancePipeline = nullptr;
            m_probeBorderRowUpdateDistancePipeline = nullptr;
            m_probeBorderColumnUpdateIrradiancePipeline = nullptr;
            m_probeBorderColumnUpdateDistancePipeline = nullptr;

            m_probeRelocationPipeline = nullptr;
            m_probeRelocationResetPipeline = nullptr;

            m_probeClassificationPipeline = nullptr;
            m_probeClassificationResetPipeline = nullptr;
        }

        //------------------------------------------------------------------------
        // Private Resource Allocation Helper Functions (Managed Resources)
        //------------------------------------------------------------------------

    #if RTXGI_DDGI_RESOURCE_MANAGEMENT
        void DDGIVolume::Transition(VkCommandBuffer cmdBuffer)
        {
            // Transition the textures for general use
            std::vector<VkImageMemoryBarrier> barriers;

            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            barrier.image = m_probeRayData;
            barriers.push_back(barrier);
            barrier.image = m_probeIrradiance;
            barriers.push_back(barrier);
            barrier.image = m_probeDistance;
            barriers.push_back(barrier);
            barrier.image = m_probeData;
            barriers.push_back(barrier);

            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, static_cast<uint32_t>(barriers.size()), barriers.data());
        }

        bool DDGIVolume::AllocateMemory(VkMemoryRequirements reqs, VkMemoryPropertyFlags props, VkMemoryAllocateFlags flags, VkDeviceMemory* memory)
        {
            // Get the memory properties of the physical device
            VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
            vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &physicalDeviceMemoryProperties);

            // Check to see if the device has the required memory
            uint32_t memTypeIndex = 0;
            while (memTypeIndex < physicalDeviceMemoryProperties.memoryTypeCount)
            {
                bool isRequiredType = reqs.memoryTypeBits & (1 << memTypeIndex);
                bool hasRequiredProperties = (physicalDeviceMemoryProperties.memoryTypes[memTypeIndex].propertyFlags & props) == props;
                if (isRequiredType && hasRequiredProperties) break;
                ++memTypeIndex;
            }

            // Early exit, memory type not found
            if (memTypeIndex == physicalDeviceMemoryProperties.memoryTypeCount) return false;

            // Describe the memory allocation
            VkMemoryAllocateFlagsInfo allocateFlagsInfo = {};
            allocateFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
            allocateFlagsInfo.flags = flags;

            VkMemoryAllocateInfo memoryAllocateInfo = {};
            memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            memoryAllocateInfo.pNext = &allocateFlagsInfo;
            memoryAllocateInfo.memoryTypeIndex = memTypeIndex;
            memoryAllocateInfo.allocationSize = reqs.size;

            // Allocate the device memory
            VkResult result = vkAllocateMemory(m_device, &memoryAllocateInfo, nullptr, memory);
            if(VKFAILED(result)) return false;

            return true;
        }

        bool DDGIVolume::CreateDescriptorSet()
        {
            // Describe the descriptor set allocation
            VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
            descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descriptorSetAllocateInfo.descriptorPool = m_descriptorPool;
            descriptorSetAllocateInfo.descriptorSetCount = 1;
            descriptorSetAllocateInfo.pSetLayouts = &m_descriptorSetLayout;

            // Allocate the descriptor set
            VkResult result = vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_descriptorSet);
            if (VKFAILED(result)) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::string name = "DDGIVolume[" + std::to_string(m_desc.index) + "], Descriptor Set";
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_descriptorSet), name.c_str(), VK_OBJECT_TYPE_DESCRIPTOR_SET);
        #endif

            // Store the data to be written to the descriptor set
            std::vector<VkWriteDescriptorSet> writeDescriptorSets;

            VkDescriptorBufferInfo constantsSTBInfo = { m_constantsBuffer, 0, VK_WHOLE_SIZE };

            // DDGIVolume constants structured buffer
            VkWriteDescriptorSet constantsSTBSet = {};
            constantsSTBSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            constantsSTBSet.dstSet = m_descriptorSet;
            constantsSTBSet.dstBinding = EDDGIVolumeLayoutBindings::Constants;
            constantsSTBSet.dstArrayElement = 0;
            constantsSTBSet.descriptorCount = 1;
            constantsSTBSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            constantsSTBSet.pBufferInfo = &constantsSTBInfo;

            writeDescriptorSets.push_back(constantsSTBSet);

            VkDescriptorImageInfo rayDataInfo = { VK_NULL_HANDLE, m_probeRayDataView, VK_IMAGE_LAYOUT_GENERAL };

            // Probe Ray Data
            VkWriteDescriptorSet rayDataSet = {};
            rayDataSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            rayDataSet.dstSet = m_descriptorSet;
            rayDataSet.dstBinding = EDDGIVolumeLayoutBindings::ProbeRayData;
            rayDataSet.dstArrayElement = 0;
            rayDataSet.descriptorCount = 1;
            rayDataSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            rayDataSet.pImageInfo = &rayDataInfo;

            writeDescriptorSets.push_back(rayDataSet);

            VkDescriptorImageInfo irradianceInfo = { VK_NULL_HANDLE, m_probeIrradianceView, VK_IMAGE_LAYOUT_GENERAL };

            // Probe Irradiance
            VkWriteDescriptorSet irradianceSet = {};
            irradianceSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            irradianceSet.dstSet = m_descriptorSet;
            irradianceSet.dstBinding = EDDGIVolumeLayoutBindings::ProbeIrradiance;
            irradianceSet.dstArrayElement = 0;
            irradianceSet.descriptorCount = 1;
            irradianceSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            irradianceSet.pImageInfo = &irradianceInfo;

            writeDescriptorSets.push_back(irradianceSet);

            VkDescriptorImageInfo distanceInfo = { VK_NULL_HANDLE, m_probeDistanceView, VK_IMAGE_LAYOUT_GENERAL };

            // Probe Distance
            VkWriteDescriptorSet distanceSet = {};
            distanceSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            distanceSet.dstSet = m_descriptorSet;
            distanceSet.dstBinding = EDDGIVolumeLayoutBindings::ProbeDistance;
            distanceSet.dstArrayElement = 0;
            distanceSet.descriptorCount = 1;
            distanceSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            distanceSet.pImageInfo = &distanceInfo;

            writeDescriptorSets.push_back(distanceSet);

            VkDescriptorImageInfo dataInfo = { VK_NULL_HANDLE, m_probeDataView, VK_IMAGE_LAYOUT_GENERAL };

            // Probe Data
            VkWriteDescriptorSet dataSet = {};
            dataSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            dataSet.dstSet = m_descriptorSet;
            dataSet.dstBinding = EDDGIVolumeLayoutBindings::ProbeData;
            dataSet.dstArrayElement = 0;
            dataSet.descriptorCount = 1;
            dataSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            dataSet.pImageInfo = &dataInfo;

            writeDescriptorSets.push_back(dataSet);

            // Update the descriptor set
            vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

            return true;
        }

        bool DDGIVolume::CreateLayouts()
        {
            // Get the layout descriptors
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
            VkPushConstantRange pushConstantRange = {};
            VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};

            GetDDGIVolumeLayoutDescs(bindings, descriptorSetLayoutCreateInfo, pushConstantRange, pipelineLayoutCreateInfo);

            // Create the descriptor set layout
            VkResult result = vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_descriptorSetLayout);
            if (VKFAILED(result)) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::string name = "DDGIVolume[" + std::to_string(m_desc.index) + "] Descriptor Set Layout";
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_descriptorSetLayout), name.c_str(), VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT);
        #endif

            // Set the descriptor set layout for the pipeline layout
            pipelineLayoutCreateInfo.pSetLayouts = &m_descriptorSetLayout;

            // Create the pipeline layout
            result = vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);
            if ((result)) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            name = "DDGIVolume[" + std::to_string(m_desc.index) + "] Pipeline Layout";
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_pipelineLayout), name.c_str(), VK_OBJECT_TYPE_PIPELINE_LAYOUT);
        #endif

            return true;
        }

        bool DDGIVolume::CreateComputePipeline(ShaderBytecode shader, std::string entryPoint, VkShaderModule* module, VkPipeline* pipeline, std::string debugName = "")
        {
            if (entryPoint.compare("") == 0) return false;

            // Create the shader module
            VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
            shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(shader.pData);
            shaderModuleCreateInfo.codeSize = shader.size;

            // Create the shader module
            VkResult result = vkCreateShaderModule(m_device, &shaderModuleCreateInfo, nullptr, module);
            if (VKFAILED(result)) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::string name = "DDGIVolume[" + std::to_string(m_desc.index) + "]," + debugName + " Shader Module";
            SetObjectName(m_device, reinterpret_cast<uint64_t>(*module), name.c_str(), VK_OBJECT_TYPE_SHADER_MODULE);
        #endif

            // Describe the pipeline
            VkComputePipelineCreateInfo computePipelineCreateInfo = {};
            computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            computePipelineCreateInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            computePipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            computePipelineCreateInfo.stage.module = *module;
            computePipelineCreateInfo.stage.pName = entryPoint.c_str();
            computePipelineCreateInfo.layout = m_pipelineLayout;

            // Create the pipeline
            result = vkCreateComputePipelines(m_device, nullptr, 1, &computePipelineCreateInfo, nullptr, pipeline);
            if (VKFAILED(result)) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            name = "DDGIVolume[" + std::to_string(m_desc.index) + "]," + debugName + " Pipeline";
            SetObjectName(m_device, reinterpret_cast<uint64_t>(*pipeline), name.c_str(), VK_OBJECT_TYPE_PIPELINE);
        #endif

            return true;
        }

        bool DDGIVolume::CreateTexture(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImage* image, VkDeviceMemory* imageMemory, VkImageView* imageView)
        {
            // Describe the texture
            VkImageCreateInfo imageCreateInfo = {};
            imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
            imageCreateInfo.format = format;
            imageCreateInfo.extent.width = width;
            imageCreateInfo.extent.height = height;
            imageCreateInfo.extent.depth = 1;
            imageCreateInfo.mipLevels = 1;
            imageCreateInfo.arrayLayers = 1;
            imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageCreateInfo.usage = usage;
            imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            // Create the texture
            VkResult result = vkCreateImage(m_device, &imageCreateInfo, nullptr, image);
            if (VKFAILED(result)) return false;

            // Get memory requirements
            VkMemoryRequirements reqs;
            vkGetImageMemoryRequirements(m_device, *image, &reqs);

            // Allocate memory
            VkMemoryAllocateFlags flags = 0;
            VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            if (!AllocateMemory(reqs, props, flags, imageMemory)) return false;

            // Bind the memory to the texture resource
            result = vkBindImageMemory(m_device, *image, *imageMemory, 0);
            if (VKFAILED(result)) return false;

            // Describe the texture's image view
            VkImageViewCreateInfo imageViewCreateInfo = {};
            imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            imageViewCreateInfo.format = imageCreateInfo.format;
            imageViewCreateInfo.image = *image;
            imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageViewCreateInfo.subresourceRange.levelCount = 1;
            imageViewCreateInfo.subresourceRange.layerCount = 1;
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

            // Create the texture's image view
            result = vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, imageView);
            if (VKFAILED(result)) return false;

            return true;
        }

        bool DDGIVolume::CreateProbeRayData(const DDGIVolumeDesc& desc)
        {
            vkDestroyImage(m_device, m_probeRayData, nullptr);
            vkDestroyImageView(m_device, m_probeRayDataView, nullptr);
            vkFreeMemory(m_device, m_probeRayDataMemory, nullptr);

            uint32_t width = 0;
            uint32_t height = 0;
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::RayData, width, height);

            // Check for problems
            if (width <= 0 || height <= 0) return false;

            VkFormat format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::RayData, desc.probeRayDataFormat);
            VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

            // Create the texture, allocate memory, and bind the memory
            bool result = CreateTexture(width, height, format, usage, &m_probeRayData, &m_probeRayDataMemory, &m_probeRayDataView);
            if (!result) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::string name = "DDGIVolume[" + std::to_string(desc.index) + "], Probe Ray Data";
            std::string memory = name + " Memory";
            std::string view = name + " View";
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeRayData), name.c_str(), VK_OBJECT_TYPE_IMAGE);
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeRayDataMemory), memory.c_str(), VK_OBJECT_TYPE_DEVICE_MEMORY);
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeRayDataView), view.c_str(), VK_OBJECT_TYPE_IMAGE_VIEW);
        #endif

            return true;
        }

        bool DDGIVolume::CreateProbeIrradiance(const DDGIVolumeDesc& desc)
        {
            vkDestroyImage(m_device, m_probeIrradiance, nullptr);
            vkDestroyImageView(m_device, m_probeIrradianceView, nullptr);
            vkFreeMemory(m_device, m_probeIrradianceMemory, nullptr);

            uint32_t width = 0;
            uint32_t height = 0;
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::Irradiance, width, height);

            // Check for problems
            if (width <= 0 || height <= 0) return false;

            VkFormat format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Irradiance, desc.probeIrradianceFormat);
            VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

            // Create the texture, allocate memory, and bind the memory
            bool result = CreateTexture(width, height, format, usage, &m_probeIrradiance, &m_probeIrradianceMemory, &m_probeIrradianceView);
            if (!result) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::string name = "DDGIVolume[" + std::to_string(desc.index) + "], Probe Irradiance";
            std::string memory = name + " Memory";
            std::string view = name + " View";
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeIrradiance), name.c_str(), VK_OBJECT_TYPE_IMAGE);
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeIrradianceMemory), memory.c_str(), VK_OBJECT_TYPE_DEVICE_MEMORY);
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeIrradianceView), view.c_str(), VK_OBJECT_TYPE_IMAGE_VIEW);
        #endif
            return true;
        }

        bool DDGIVolume::CreateProbeDistance(const DDGIVolumeDesc& desc)
        {
            vkDestroyImage(m_device, m_probeDistance, nullptr);
            vkDestroyImageView(m_device, m_probeDistanceView, nullptr);
            vkFreeMemory(m_device, m_probeDistanceMemory, nullptr);

            uint32_t width = 0;
            uint32_t height = 0;
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::Distance, width, height);

            // Check for problems
            if (width <= 0 || height <= 0) return false;

            VkFormat format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Distance, desc.probeDistanceFormat);
            VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

            // Create the texture, allocate memory, and bind the memory
            bool result = CreateTexture(width, height, format, usage, &m_probeDistance, &m_probeDistanceMemory, &m_probeDistanceView);
            if (!result) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::string name = "DDGIVolume[" + std::to_string(desc.index) + "], Probe Distance";
            std::string memory = name + " Memory";
            std::string view = name + " View";
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeDistance), name.c_str(), VK_OBJECT_TYPE_IMAGE);
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeDistanceMemory), memory.c_str(), VK_OBJECT_TYPE_DEVICE_MEMORY);
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeDistanceView), view.c_str(), VK_OBJECT_TYPE_IMAGE_VIEW);
        #endif
            return true;
        }

        bool DDGIVolume::CreateProbeData(const DDGIVolumeDesc& desc)
        {
            vkDestroyImage(m_device, m_probeData, nullptr);
            vkDestroyImageView(m_device, m_probeDataView, nullptr);
            vkFreeMemory(m_device, m_probeDataMemory, nullptr);

            uint32_t width = 0;
            uint32_t height = 0;
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::Data, width, height);

            // Check for problems
            if (width <= 0) return false;

            VkFormat format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Data, desc.probeDataFormat);
            VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

            // Create the texture, allocate memory, and bind the memory
            bool result = CreateTexture(width, height, format, usage, &m_probeData, &m_probeDataMemory, &m_probeDataView);
            if (!result) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::string name = "DDGIVolume[" + std::to_string(desc.index) + "], Probe Data";
            std::string memory = name + " Memory";
            std::string view = name + " View";
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeData), name.c_str(), VK_OBJECT_TYPE_IMAGE);
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeDataMemory), memory.c_str(), VK_OBJECT_TYPE_DEVICE_MEMORY);
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeDataView), view.c_str(), VK_OBJECT_TYPE_IMAGE_VIEW);
        #endif
            return true;
        }

    #endif // RTXGI_MANAGED_RESOURCES
    } // namespace vulkan
} // namespace rtxgi
