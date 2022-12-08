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

#include "rtxgi/VulkanExtensions.h"

#include <cstring>
#include <random>
#include <string>
#include <vector>

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
            if (!ValidateShaderBytecode(desc.probeRelocation.updateCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_RELOCATION;
            if (!ValidateShaderBytecode(desc.probeRelocation.resetCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_RELOCATION_RESET;
            if (!ValidateShaderBytecode(desc.probeClassification.updateCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_CLASSIFICATION;
            if (!ValidateShaderBytecode(desc.probeClassification.resetCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_CLASSIFICATION_RESET;
            if (!ValidateShaderBytecode(desc.probeVariability.reductionCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_CLASSIFICATION_RESET;
            if (!ValidateShaderBytecode(desc.probeVariability.extraReductionCS)) return ERTXGIStatus::ERROR_DDGI_INVALID_BYTECODE_PROBE_VARIABILITY_EXTRA_REDUCTION;

            return ERTXGIStatus::OK;
        }

        ERTXGIStatus ValidateUnmanagedResourcesDesc(const DDGIVolumeUnmanagedResourcesDesc& desc)
        {
            // Pipeline Layout and Descriptor Set
            if (desc.pipelineLayout == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_LAYOUT;
            if (desc.descriptorSet == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_DESCRIPTOR_SET;

            // Texture Arrays
            if (desc.probeRayData == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_RAY_DATA;
            if (desc.probeIrradiance == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_IRRADIANCE;
            if (desc.probeDistance == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_DISTANCE;
            if (desc.probeData == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_DATA;
            if (desc.probeVariability == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_VARIABILITY;
            if (desc.probeVariabilityAverage == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_VARIABILITY_AVERAGE;
            if (desc.probeVariabilityReadback == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_TEXTURE_PROBE_VARIABILITY_READBACK;

            // Texture Array Memory
            if (desc.probeRayDataMemory == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_MEMORY_PROBE_RAY_DATA;
            if (desc.probeIrradianceMemory == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_MEMORY_PROBE_IRRADIANCE;
            if (desc.probeDistanceMemory == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_MEMORY_PROBE_DISTANCE;
            if (desc.probeDataMemory == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_MEMORY_PROBE_DATA;
            if (desc.probeVariabilityMemory == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_MEMORY_PROBE_VARIABILITY;
            if (desc.probeVariabilityAverageMemory == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_MEMORY_PROBE_VARIABILITY_AVERAGE;
            if (desc.probeVariabilityReadbackMemory == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_MEMORY_PROBE_VARIABILITY_READBACK;

            // Texture Array Views
            if (desc.probeRayDataView == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_VIEW_PROBE_RAY_DATA;
            if (desc.probeIrradianceView == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_VIEW_PROBE_IRRADIANCE;
            if (desc.probeDistanceView == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_VIEW_PROBE_DISTANCE;
            if (desc.probeDataView == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_VIEW_PROBE_DATA;
            if (desc.probeVariabilityView == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_VIEW_PROBE_VARIABILITY;
            if (desc.probeVariabilityAverageView == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_IMAGE_VIEW_PROBE_VARIABILITY_AVERAGE;

            // Shader Modules
            if (desc.probeBlendingIrradianceModule == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_SHADER_MODULE_PROBE_BLENDING_IRRADIANCE;
            if (desc.probeBlendingDistanceModule == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_SHADER_MODULE_PROBE_BLENDING_DISTANCE;
            if (desc.probeRelocation.updateModule == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_SHADER_MODULE_PROBE_RELOCATION;
            if (desc.probeRelocation.resetModule == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_SHADER_MODULE_PROBE_RELOCATION_RESET;
            if (desc.probeClassification.updateModule == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_SHADER_MODULE_PROBE_CLASSIFICATION;
            if (desc.probeClassification.resetModule == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_SHADER_MODULE_PROBE_CLASSIFICATION_RESET;
            if (desc.probeVariabilityPipelines.reductionModule == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_SHADER_MODULE_PROBE_VARIABILITY_REDUCTION;
            if (desc.probeVariabilityPipelines.extraReductionModule == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_SHADER_MODULE_PROBE_VARIABILITY_EXTRA_REDUCTION;

            // Pipelines
            if (desc.probeBlendingIrradiancePipeline == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_PROBE_BLENDING_IRRADIANCE;
            if (desc.probeBlendingDistancePipeline == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_PROBE_BLENDING_DISTANCE;
            if (desc.probeRelocation.updatePipeline == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_PROBE_RELOCATION;
            if (desc.probeRelocation.resetPipeline == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_PROBE_RELOCATION_RESET;
            if (desc.probeClassification.updatePipeline == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_PROBE_CLASSIFICATION;
            if (desc.probeClassification.resetPipeline == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_PROBE_CLASSIFICATION_RESET;
            if (desc.probeVariabilityPipelines.reductionPipeline == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_PROBE_VARIABILITY_REDUCTION;
            if (desc.probeVariabilityPipelines.extraReductionPipeline == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_PIPELINE_PROBE_VARIABILITY_EXTRA_REDUCTION;

            return ERTXGIStatus::OK;
        }

        //------------------------------------------------------------------------
        // Public RTXGI Namespace DDGI Functions
        //------------------------------------------------------------------------

        VkFormat GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType type, EDDGIVolumeTextureFormat format)
        {
            if (type == EDDGIVolumeTextureType::RayData)
            {
                if (format == EDDGIVolumeTextureFormat::F32x2) return VK_FORMAT_R32G32_SFLOAT;
                else if (format == EDDGIVolumeTextureFormat::F32x4) return VK_FORMAT_R32G32B32A32_SFLOAT;
            }
            else if (type == EDDGIVolumeTextureType::Irradiance)
            {
                if (format == EDDGIVolumeTextureFormat::U32) return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
                else if (format == EDDGIVolumeTextureFormat::F16x4) return VK_FORMAT_R16G16B16A16_SFLOAT;
                else if (format == EDDGIVolumeTextureFormat::F32x4) return VK_FORMAT_R32G32B32A32_SFLOAT;
            }
            else if (type == EDDGIVolumeTextureType::Distance)
            {
                if (format == EDDGIVolumeTextureFormat::F16x2) return VK_FORMAT_R16G16_SFLOAT;  // Note: in large environments FP16 may not be sufficient
                else if (format == EDDGIVolumeTextureFormat::F32x2) return VK_FORMAT_R32G32_SFLOAT;
            }
            else if (type == EDDGIVolumeTextureType::Data)
            {
                if (format == EDDGIVolumeTextureFormat::F16x4) return VK_FORMAT_R16G16B16A16_SFLOAT;
                else if (format == EDDGIVolumeTextureFormat::F32x4) return VK_FORMAT_R32G32B32A32_SFLOAT;
            }
            else if (type == EDDGIVolumeTextureType::Variability)
            {
                if (format == EDDGIVolumeTextureFormat::F16) return VK_FORMAT_R16_SFLOAT;
                else if (format == EDDGIVolumeTextureFormat::F32) return VK_FORMAT_R32_SFLOAT;
            }
            else if (type == EDDGIVolumeTextureType::VariabilityAverage)
            {
                return VK_FORMAT_R32G32_SFLOAT;
            }
            return VK_FORMAT_UNDEFINED;
        }

        uint32_t GetDDGIVolumeLayoutBindingCount() { return 7; }

        void GetDDGIVolumeLayoutDescs(
            VkDescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo,
            VkPushConstantRange& pushConstantRange,
            VkPipelineLayoutCreateInfo& pipelineLayoutCreateInfo,
            VkDescriptorSetLayoutBinding* bindings)
        {
            // Descriptor set layout bindings
            // 1 SRV constants structured buffer       (0)
            // 1 UAV for ray data texture array        (1)
            // 1 UAV probe irradiance texture array    (2)
            // 1 UAV probe distance texture array      (3)
            // 1 UAV probe data texture array          (4)
            // 1 UAV probe variation texture array     (5)
            // 1 UAV probe variation average array     (6)

            // 0: Volume Constants Structured Buffer
            VkDescriptorSetLayoutBinding& bind0 = bindings[0];
            bind0.binding = static_cast<uint32_t>(EDDGIVolumeBindings::Constants);
            bind0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bind0.descriptorCount = 1;
            bind0.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            // 1: Ray Data Texture Array UAV
            VkDescriptorSetLayoutBinding& bind1 = bindings[1];
            bind1.binding = static_cast<uint32_t>(EDDGIVolumeBindings::RayData);
            bind1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            bind1.descriptorCount = 1;
            bind1.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            // 2: Probe Irradiance Texture Array UAV
            VkDescriptorSetLayoutBinding& bind2 = bindings[2];
            bind2.binding = static_cast<uint32_t>(EDDGIVolumeBindings::ProbeIrradiance);
            bind2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            bind2.descriptorCount = 1;
            bind2.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            // 3: Probe Distance Texture Array UAV
            VkDescriptorSetLayoutBinding& bind3 = bindings[3];
            bind3.binding = static_cast<uint32_t>(EDDGIVolumeBindings::ProbeDistance);
            bind3.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            bind3.descriptorCount = 1;
            bind3.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            // 4: Probe Data Texture Array UAV
            VkDescriptorSetLayoutBinding& bind4 = bindings[4];
            bind4.binding = static_cast<uint32_t>(EDDGIVolumeBindings::ProbeData);
            bind4.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            bind4.descriptorCount = 1;
            bind4.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            // 5: Probe Variability
            VkDescriptorSetLayoutBinding& bind5 = bindings[5];
            bind5.binding = static_cast<uint32_t>(EDDGIVolumeBindings::ProbeVariability);
            bind5.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            bind5.descriptorCount = 1;
            bind5.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            // 6: Probe Variability
            VkDescriptorSetLayoutBinding& bind6 = bindings[6];
            bind6.binding = static_cast<uint32_t>(EDDGIVolumeBindings::ProbeVariabilityAverage);
            bind6.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            bind6.descriptorCount = 1;
            bind6.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

            // Describe the descriptor set layout
            descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorSetLayoutCreateInfo.bindingCount = GetDDGIVolumeLayoutBindingCount();
            descriptorSetLayoutCreateInfo.pBindings = bindings;

            // Describe the push constants
            pushConstantRange.stageFlags = VK_SHADER_STAGE_ALL;
            pushConstantRange.offset = 0;
            pushConstantRange.size = DDGIRootConstants::GetAlignedSizeInBytes();

            // Describe the pipeline layout
            pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutCreateInfo.setLayoutCount = 1;
            pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
            pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
        }

        ERTXGIStatus UploadDDGIVolumeResourceIndices(VkDevice device, VkCommandBuffer cmdBuffer, uint32_t bufferingIndex, uint32_t numVolumes, DDGIVolume** volumes)
        {
            // Copy the resource indices for each volume
            for (uint32_t volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                // Get the volume
                const DDGIVolume* volume = volumes[volumeIndex];

                // Validate the upload and device buffers
                if (volume->GetResourceIndicesBuffer() == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_INDICES_BUFFER;
                if (volume->GetResourceIndicesBufferUpload() == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_INDICES_UPLOAD_BUFFER;
                if (volume->GetResourceIndicesBufferUploadMemory() == nullptr) return ERTXGIStatus::ERROR_DDGI_VK_INVALID_RESOURCE_INDICES_UPLOAD_MEMORY;

                // Offset to the resource indices data to write to (e.g. double buffering)
                uint64_t bufferOffset = volume->GetResourceIndicesBufferSizeInBytes() * bufferingIndex;

                // Offset to the volume in current resource indices buffer
                uint32_t volumeOffset = (volume->GetIndex() * (uint32_t)sizeof(DDGIVolumeResourceIndices));

                // Offset to the volume resource indices in the upload buffer
                uint64_t srcOffset = (bufferOffset + volumeOffset);

                // Map the resource indices buffer and update it
                void* pData = nullptr;
                VkResult result = vkMapMemory(device, volume->GetResourceIndicesBufferUploadMemory(), srcOffset, sizeof(DDGIVolumeResourceIndices), 0, &pData);
                if (VKFAILED(result)) return ERTXGIStatus::ERROR_DDGI_MAP_FAILURE_RESOURCE_INDICES_UPLOAD_BUFFER;

                // Get the DDGIVolume's bindless resource indices
                const DDGIVolumeResourceIndices gpuDesc = volume->GetResourceIndices();

                memcpy(pData, &gpuDesc, sizeof(DDGIVolumeResourceIndices));

                vkUnmapMemory(device, volume->GetResourceIndicesBufferUploadMemory());

                // Schedule a copy of the upload buffer to the device buffer
                VkBufferCopy bufferCopy = {};
                bufferCopy.size = sizeof(DDGIVolumeResourceIndices);
                bufferCopy.srcOffset = srcOffset;
                bufferCopy.dstOffset = volumeOffset;
                vkCmdCopyBuffer(cmdBuffer, volume->GetResourceIndicesBufferUpload(), volume->GetResourceIndicesBuffer(), 1, &bufferCopy);
            }

            return ERTXGIStatus::OK;
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

                // Map the constants buffer and update it
                void* pData = nullptr;
                VkResult result = vkMapMemory(device, volume->GetConstantsBufferUploadMemory(), srcOffset, sizeof(DDGIVolumeDescGPUPacked), 0, &pData);
                if (VKFAILED(result)) return ERTXGIStatus::ERROR_DDGI_MAP_FAILURE_CONSTANTS_UPLOAD_BUFFER;

                // Get the packed DDGIVolume GPU descriptor
                const DDGIVolumeDescGPUPacked gpuDesc = volume->GetDescGPUPacked();

            #if _DEBUG
                volume->ValidatePackedData(gpuDesc);
            #endif

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
            if (bInsertPerfMarkers) AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, "RTXGI DDGI Update Probes");

            uint32_t volumeIndex;
            std::vector<VkImageMemoryBarrier> barriers;

            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

            // Irradiance Blending
            if (bInsertPerfMarkers) AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, "Probe Irradiance");
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                // Get the volume
                const DDGIVolume* volume = volumes[volumeIndex];

                // Bind the descriptor set and push constants
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetPipelineLayout(), 0, 1, volume->GetDescriptorSetConstPtr(), 0, nullptr);

                // Update the push constants
                vkCmdPushConstants(cmdBuffer, volume->GetPipelineLayout(), VK_SHADER_STAGE_ALL, volume->GetPushConstantsOffset(), DDGIRootConstants::GetSizeInBytes(), volume->GetPushConstants().GetData());

                // Get the number of probes on each axis
                uint32_t probeCountX, probeCountY, probeCountZ;
                GetDDGIVolumeProbeCounts(volume->GetDesc(), probeCountX, probeCountY, probeCountZ);

                // Probe irradiance blending
                {
                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers())
                    {
                        std::string msg = "Irradiance, DDGIVolume[" + std::to_string(volume->GetIndex()) + "] - \"" + volume->GetName() + "\"";
                        AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, msg.c_str());
                    }

                    // Bind the pipeline and dispatch threads
                    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetProbeBlendingIrradiancePipeline());
                    vkCmdDispatch(cmdBuffer, probeCountX, probeCountY, probeCountZ);

                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers()) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);
                }

                // Add a barrier
                barrier.image = volume->GetProbeIrradiance();
                barriers.push_back(barrier);
                barrier.image = volume->GetProbeVariability();
                barriers.push_back(barrier);
            }
            if (bInsertPerfMarkers) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);

            // Distance Blending
            if (bInsertPerfMarkers) AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, "Probe Distance");
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                // Get the volume
                const DDGIVolume* volume = volumes[volumeIndex];

                // Bind the descriptor set and push constants
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetPipelineLayout(), 0, 1, volume->GetDescriptorSetConstPtr(), 0, nullptr);

                // Update the push constants
                vkCmdPushConstants(cmdBuffer, volume->GetPipelineLayout(), VK_SHADER_STAGE_ALL, volume->GetPushConstantsOffset(), DDGIRootConstants::GetSizeInBytes(), volume->GetPushConstants().GetData());

                // Get the number of probes on the X and Y dimensions of the texture
                uint32_t probeCountX, probeCountY, probeCountZ;
                GetDDGIVolumeProbeCounts(volume->GetDesc(), probeCountX, probeCountY, probeCountZ);

                // Probe distance blending
                {
                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers())
                    {
                        std::string msg = "Distance, DDGIVolume[" + std::to_string(volume->GetIndex()) + "] - \"" + volume->GetName() + "\"";
                        AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, msg.c_str());
                    }

                    // Bind the pipeline and dispatch threads
                    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetProbeBlendingDistancePipeline());
                    vkCmdDispatch(cmdBuffer, probeCountX, probeCountY, probeCountZ);

                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers()) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);
                }

                // Add a barrier
                barrier.image = volume->GetProbeDistance();
                barriers.push_back(barrier);
            }
            if (bInsertPerfMarkers) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);

            // Irradiance pass must finish generating variability before possible reduction pass
            // Also ensures that irradiance and distance complete before border update after reduction
            if (!barriers.empty())
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

            // Remove previous barriers
            barriers.clear();

            if (bInsertPerfMarkers) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);

            return ERTXGIStatus::OK;
        }

        ERTXGIStatus RelocateDDGIVolumeProbes(VkCommandBuffer cmdBuffer, uint32_t numVolumes, DDGIVolume** volumes)
        {
            if (bInsertPerfMarkers) AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, "RTXGI DDGI Relocate Probes");

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
                // Get the volume
                const DDGIVolume* volume = volumes[volumeIndex];
                if (!volume->GetProbeRelocationNeedsReset()) continue;  // Skip if the volume doesn't need to be reset

                // Bind descriptor set and push constants
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetPipelineLayout(), 0, 1, volume->GetDescriptorSetConstPtr(), 0, nullptr);

                // Update the push constants
                vkCmdPushConstants(cmdBuffer, volume->GetPipelineLayout(), VK_SHADER_STAGE_ALL, volume->GetPushConstantsOffset(), DDGIRootConstants::GetSizeInBytes(), volume->GetPushConstants().GetData());

                // Reset all probe offsets to zero
                const float groupSizeX = 32.f;
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
                // Get the volume
                const DDGIVolume* volume = volumes[volumeIndex];
                if (!volume->GetProbeRelocationEnabled()) continue;  // Skip if relocation is not enabled for this volume

                // Bind descriptor set and push constants
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetPipelineLayout(), 0, 1, volume->GetDescriptorSetConstPtr(), 0, nullptr);

                // Update the push constants
                vkCmdPushConstants(cmdBuffer, volume->GetPipelineLayout(), VK_SHADER_STAGE_ALL, volume->GetPushConstantsOffset(), DDGIRootConstants::GetSizeInBytes(), volume->GetPushConstants().GetData());

                // Probe relocation
                float groupSizeX = 32.f;
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
            if (bInsertPerfMarkers) AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, "RTXGI DDGI Classify Probes");

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
                // Get the volume
                const DDGIVolume* volume = volumes[volumeIndex];
                if (!volume->GetProbeClassificationNeedsReset()) continue;  // Skip if the volume doesn't need to be reset

                // Bind descriptor set and push constants
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetPipelineLayout(), 0, 1, volume->GetDescriptorSetConstPtr(), 0, nullptr);

                // Update the push constants
                vkCmdPushConstants(cmdBuffer, volume->GetPipelineLayout(), VK_SHADER_STAGE_ALL, volume->GetPushConstantsOffset(), DDGIRootConstants::GetSizeInBytes(), volume->GetPushConstants().GetData());

                // Reset all probe states to the ACTIVE state
                const float groupSizeX = 32.f;
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
                // Get the volume
                const DDGIVolume* volume = volumes[volumeIndex];
                if (!volume->GetProbeClassificationEnabled()) continue;  // Skip if classification is not enabled for this volume

                // Bind descriptor set and push constants
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetPipelineLayout(), 0, 1, volume->GetDescriptorSetConstPtr(), 0, nullptr);

                // Update the push constants
                vkCmdPushConstants(cmdBuffer, volume->GetPipelineLayout(), VK_SHADER_STAGE_ALL, volume->GetPushConstantsOffset(), DDGIRootConstants::GetSizeInBytes(), volume->GetPushConstants().GetData());

                // Probe classification
                const float groupSizeX = 32.f;
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

        ERTXGIStatus CalculateDDGIVolumeVariability(VkCommandBuffer cmdBuffer, uint32_t numVolumes, DDGIVolume** volumes)
        {
            if (bInsertPerfMarkers) AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, "Probe Variability Calculation");

            uint32_t volumeIndex;
            std::vector<VkImageMemoryBarrier> barriers;

            // Reduction
            for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                const DDGIVolume* volume = volumes[volumeIndex];
                if (!volume->GetProbeVariabilityEnabled()) continue;  // Skip if the volume is not calculating variability

                // Bind the descriptor set and push constants
                vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetPipelineLayout(), 0, 1, volume->GetDescriptorSetConstPtr(), 0, nullptr);

                // Get the number of probes on the XYZ dimensions of the texture
                uint32_t probeCountX, probeCountY, probeCountZ;
                GetDDGIVolumeProbeCounts(volume->GetDesc(), probeCountX, probeCountY, probeCountZ);

                // Initially, the reduction input is the full variability size (same as irradiance texture)
                uint32_t inputTexelsX = probeCountX * volume->GetDesc().probeNumIrradianceInteriorTexels;
                uint32_t inputTexelsY = probeCountY * volume->GetDesc().probeNumIrradianceInteriorTexels;
                uint32_t inputTexelsZ = probeCountZ;

                const uint3 NumThreadsInGroup = { 4, 8, 4 }; // Each thread group will have 8x8x8 threads
                constexpr uint2 ThreadSampleFootprint = { 4, 2 }; // Each thread will sample 4x2 texels

                // Set push constants
                DDGIRootConstants consts = volume->GetPushConstants();

                // First pass reduction takes probe irradiance data and calculates variability, reduces as much as possible
                {
                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers())
                    {
                        std::string msg = "Reduction, DDGIVolume[" + std::to_string(volume->GetIndex()) + "] - \"" + volume->GetName() + "\"";
                        AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, msg.c_str());
                    }

                    // Set the PSO and dispatch threads
                    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetProbeVariabilityReductionPipeline());

                    // One thread group per output texel
                    uint32_t outputTexelsX = (uint32_t)ceil((float)inputTexelsX / (float)(NumThreadsInGroup.x * ThreadSampleFootprint.x));
                    uint32_t outputTexelsY = (uint32_t)ceil((float)inputTexelsY / (float)(NumThreadsInGroup.y * ThreadSampleFootprint.y));
                    uint32_t outputTexelsZ = (uint32_t)ceil((float)inputTexelsZ / (float)NumThreadsInGroup.z);

                    consts.reductionInputSizeX = inputTexelsX;
                    consts.reductionInputSizeY = inputTexelsY;
                    consts.reductionInputSizeZ = inputTexelsZ;
                    vkCmdPushConstants(cmdBuffer, volume->GetPipelineLayout(), VK_SHADER_STAGE_ALL, volume->GetPushConstantsOffset(), DDGIRootConstants::GetSizeInBytes(), consts.GetData());

                    vkCmdDispatch(cmdBuffer, outputTexelsX, outputTexelsY, outputTexelsZ);

                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers()) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);

                    // Each thread group will write out a value to the averaging texture
                    // If there is more than one thread group, we will need to do extra averaging passes
                    inputTexelsX = outputTexelsX;
                    inputTexelsY = outputTexelsY;
                    inputTexelsZ = outputTexelsZ;
                }

                // UAV barrier needed after each reduction pass
                VkImageMemoryBarrier reductionBarrier = {};
                reductionBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                reductionBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                reductionBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                reductionBarrier.oldLayout = reductionBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                reductionBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                reductionBarrier.image = volume->GetProbeVariabilityAverage();
                vkCmdPipelineBarrier(
                    cmdBuffer,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &reductionBarrier);

                // Future extra passes (if they run) will re-use the reductionBarrier struct, so update srcAcessMask to match
                reductionBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

                // Extra reduction passes average values in variability texture down to single value
                while (inputTexelsX > 1 || inputTexelsY > 1 || inputTexelsZ > 1)
                {
                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers())
                    {
                        std::string msg = "Extra Reduction, DDGIVolume[" + std::to_string(volume->GetIndex()) + "] - \"" + volume->GetName() + "\"";
                        AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, msg.c_str());
                    }

                    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, volume->GetProbeVariabilityExtraReductionPipeline());

                    // One thread group per output texel
                    uint32_t outputTexelsX = (uint32_t)ceil((float)inputTexelsX / (float)(NumThreadsInGroup.x * ThreadSampleFootprint.x));
                    uint32_t outputTexelsY = (uint32_t)ceil((float)inputTexelsY / (float)(NumThreadsInGroup.y * ThreadSampleFootprint.y));
                    uint32_t outputTexelsZ = (uint32_t)ceil((float)inputTexelsZ / (float)NumThreadsInGroup.z);

                    consts.reductionInputSizeX = inputTexelsX;
                    consts.reductionInputSizeY = inputTexelsY;
                    consts.reductionInputSizeZ = inputTexelsZ;
                    vkCmdPushConstants(cmdBuffer, volume->GetPipelineLayout(), VK_SHADER_STAGE_ALL, volume->GetPushConstantsOffset(), DDGIRootConstants::GetSizeInBytes(), consts.GetData());

                    vkCmdDispatch(cmdBuffer, outputTexelsX, outputTexelsY, outputTexelsZ);

                    if (bInsertPerfMarkers && volume->GetInsertPerfMarkers()) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);

                    inputTexelsX = outputTexelsX;
                    inputTexelsY = outputTexelsY;
                    inputTexelsZ = outputTexelsZ;

                    // Need a barrier in between each reduction pass
                    vkCmdPipelineBarrier(
                        cmdBuffer,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        0,
                        0, nullptr,
                        0, nullptr,
                        1, &reductionBarrier);
                }
            }

            if (bInsertPerfMarkers) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);

            // Copy readback buffer
            if (bInsertPerfMarkers) AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, "Probe Variability Readback");

            {
                VkImageMemoryBarrier beforeBarrier = {};
                beforeBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                beforeBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
                beforeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                beforeBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                beforeBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                beforeBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

                VkImageMemoryBarrier afterBarrier = beforeBarrier;
                afterBarrier.srcAccessMask = beforeBarrier.dstAccessMask;
                afterBarrier.dstAccessMask = beforeBarrier.srcAccessMask;
                afterBarrier.oldLayout = beforeBarrier.newLayout;
                afterBarrier.newLayout = beforeBarrier.oldLayout;

                for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
                {
                    const DDGIVolume* volume = volumes[volumeIndex];
                    if (!volume->GetProbeVariabilityEnabled()) continue;  // Skip if the volume is not calculating variability

                    beforeBarrier.image = volume->GetProbeVariabilityAverage();
                    barriers.push_back(beforeBarrier);
                }

                if (!barriers.empty())
                {
                    vkCmdPipelineBarrier(
                        cmdBuffer,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0,
                        0, nullptr,
                        0, nullptr,
                        static_cast<uint32_t>(barriers.size()), barriers.data());

                    barriers.clear();
                }

                for (volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
                {
                    const DDGIVolume* volume = volumes[volumeIndex];
                    if (!volume->GetProbeVariabilityEnabled()) continue;  // Skip if the volume is not calculating variability

                    VkBufferImageCopy copy = {};
                    copy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
                    copy.imageExtent = { 1, 1, 1 };
                    vkCmdCopyImageToBuffer(cmdBuffer,
                        volume->GetProbeVariabilityAverage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        volume->GetProbeVariabilityReadback(),
                        1, &copy);

                    afterBarrier.image = volume->GetProbeVariabilityAverage();
                    barriers.push_back(afterBarrier);
                }

                if (!barriers.empty())
                {
                    vkCmdPipelineBarrier(
                        cmdBuffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        0,
                        0, nullptr,
                        0, nullptr,
                        static_cast<uint32_t>(barriers.size()), barriers.data());
                    barriers.clear();
                }
            }

            if (bInsertPerfMarkers) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);

            return ERTXGIStatus::OK;
        }

        ERTXGIStatus ReadbackDDGIVolumeVariability(VkDevice device, uint32_t numVolumes, DDGIVolume** volumes)
        {
            for (uint32_t volumeIndex = 0; volumeIndex < numVolumes; volumeIndex++)
            {
                // Get the volume
                DDGIVolume* volume = volumes[volumeIndex];
                if (!volume->GetProbeVariabilityEnabled()) continue;  // Skip if the volume is not calculating variability

                // Get the probe variability readback buffer
                VkDeviceMemory readback = volume->GetProbeVariabilityReadbackMemory();

                // Read the first 32-bits of the readback buffer
                float* pMappedMemory = nullptr;
                VkResult result = vkMapMemory(device, readback, 0, sizeof(float), 0, (void**)&pMappedMemory);
                if (VKFAILED(result)) return ERTXGIStatus::ERROR_DDGI_MAP_FAILURE_VARIABILITY_READBACK_BUFFER;
                float value = pMappedMemory[0];
                vkUnmapMemory(device, readback);

                volume->SetVolumeAverageVariability(value);
            }
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
            vkDestroyShaderModule(m_device, m_probeRelocationModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeRelocationResetModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeClassificationModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeClassificationResetModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeVariabilityReductionModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeVariabilityExtraReductionModule, nullptr);

            // Release the existing compute pipelines
            vkDestroyPipeline(m_device, m_probeBlendingIrradiancePipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeBlendingDistancePipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeRelocationPipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeRelocationResetPipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeClassificationPipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeClassificationResetPipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeVariabilityReductionPipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeVariabilityExtraReductionPipeline, nullptr);
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

                if (!CreateComputePipeline(
                    managed.probeVariability.reductionCS,
                    "DDGIReductionCS",
                    &m_probeVariabilityReductionModule,
                    &m_probeVariabilityReductionPipeline,
                    "Probe Variability Reduction")) return ERTXGIStatus::ERROR_DDGI_VK_CREATE_FAILURE_PIPELINE;

                if (!CreateComputePipeline(
                    managed.probeVariability.extraReductionCS,
                    "DDGIExtraReductionCS",
                    &m_probeVariabilityExtraReductionModule,
                    &m_probeVariabilityExtraReductionPipeline,
                    "Probe Variability Extra Reduction")) return ERTXGIStatus::ERROR_DDGI_VK_CREATE_FAILURE_PIPELINE;
            }

            // Create the textures
            if (deviceChanged || m_desc.ShouldAllocateProbes(desc))
            {
                // Probe counts have changed. The textures are the wrong size or aren't allocated yet.
                // (Re)allocate the probe ray data, irradiance, distance, and data texture arrays.
                if (!CreateProbeRayData(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_RAY_DATA;
                if (!CreateProbeIrradiance(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_IRRADIANCE;
                if (!CreateProbeDistance(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_DISTANCE;
                if (!CreateProbeData(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_DATA;
                if (!CreateProbeVariability(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_VARIABILITY;
                if (!CreateProbeVariabilityAverage(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_VARIABILITY_AVERAGE;
            }
            else
            {
                if (m_desc.ShouldAllocateRayData(desc))
                {
                    // The number of probe rays to trace per frame has changed. Reallocate the ray data texture array.
                    if (!CreateProbeRayData(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_RAY_DATA;
                }

                if (m_desc.ShouldAllocateIrradiance(desc))
                {
                    // The number of irradiance texels per probe has changed. Reallocate the irradiance texture array.
                    if (!CreateProbeIrradiance(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_IRRADIANCE;
                }

                if (m_desc.ShouldAllocateDistance(desc))
                {
                    // The number of distance texels per probe has changed. Reallocate the distance texture array.
                    if (!CreateProbeDistance(desc)) return ERTXGIStatus::ERROR_DDGI_ALLOCATE_FAILURE_TEXTURE_PROBE_DISTANCE;
                }
            }

            return ERTXGIStatus::OK;
        }
    #else
        void DDGIVolume::StoreUnmanagedResourcesDesc(const DDGIVolumeUnmanagedResourcesDesc& unmanaged)
        {
            // Pipeline Layout and Descriptor Set
            m_pipelineLayout = unmanaged.pipelineLayout;
            m_descriptorSet = unmanaged.descriptorSet;

            // Texture Arrays
            m_probeRayData = unmanaged.probeRayData;
            m_probeIrradiance = unmanaged.probeIrradiance;
            m_probeDistance = unmanaged.probeDistance;
            m_probeData = unmanaged.probeData;
            m_probeVariability = unmanaged.probeVariability;
            m_probeVariabilityAverage = unmanaged.probeVariabilityAverage;
            m_probeVariabilityReadback = unmanaged.probeVariabilityReadback;

            // Texture Array Memory
            m_probeRayDataMemory = unmanaged.probeRayDataMemory;
            m_probeIrradianceMemory = unmanaged.probeIrradianceMemory;
            m_probeDistanceMemory = unmanaged.probeDistanceMemory;
            m_probeDataMemory = unmanaged.probeDataMemory;
            m_probeVariabilityMemory = unmanaged.probeVariabilityMemory;
            m_probeVariabilityAverageMemory = unmanaged.probeVariabilityAverageMemory;
            m_probeVariabilityReadbackMemory = unmanaged.probeVariabilityReadbackMemory;

            // Texture Array Views
            m_probeRayDataView = unmanaged.probeRayDataView;
            m_probeIrradianceView = unmanaged.probeIrradianceView;
            m_probeDistanceView = unmanaged.probeDistanceView;
            m_probeDataView = unmanaged.probeDataView;
            m_probeVariabilityView = unmanaged.probeVariabilityView;
            m_probeVariabilityAverageView = unmanaged.probeVariabilityAverageView;

            // Shader Modules
            m_probeBlendingIrradianceModule = unmanaged.probeBlendingIrradianceModule;
            m_probeBlendingDistanceModule = unmanaged.probeBlendingDistanceModule;
            m_probeRelocationModule = unmanaged.probeRelocation.updateModule;
            m_probeRelocationResetModule = unmanaged.probeRelocation.resetModule;
            m_probeClassificationModule = unmanaged.probeClassification.updateModule;
            m_probeClassificationResetModule = unmanaged.probeClassification.resetModule;
            m_probeVariabilityReductionModule = unmanaged.probeVariabilityPipelines.reductionModule;
            m_probeVariabilityExtraReductionModule = unmanaged.probeVariabilityPipelines.extraReductionModule;

            // Pipelines
            m_probeBlendingIrradiancePipeline = unmanaged.probeBlendingIrradiancePipeline;
            m_probeBlendingDistancePipeline = unmanaged.probeBlendingDistancePipeline;
            m_probeRelocationPipeline = unmanaged.probeRelocation.updatePipeline;
            m_probeRelocationResetPipeline = unmanaged.probeRelocation.resetPipeline;
            m_probeClassificationPipeline = unmanaged.probeClassification.updatePipeline;
            m_probeClassificationResetPipeline = unmanaged.probeClassification.resetPipeline;
            m_probeVariabilityReductionPipeline = unmanaged.probeVariabilityPipelines.reductionPipeline;
            m_probeVariabilityExtraReductionPipeline = unmanaged.probeVariabilityPipelines.extraReductionPipeline;
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
            if (desc.probeCounts.x <= 0 || desc.probeCounts.y <= 0 || desc.probeCounts.z <= 0) return ERTXGIStatus::ERROR_DDGI_INVALID_PROBE_COUNTS;

            // Validate the resource indices buffer (when necessary)
            if(resources.bindless.enabled)
            {
                if(resources.bindless.resourceIndicesBuffer == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_RESOURCE_INDICES_BUFFER;
            }

            // Validate the constants buffer
            if (resources.constantsBuffer == nullptr) return ERTXGIStatus::ERROR_DDGI_INVALID_CONSTANTS_BUFFER;

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

            // Store the bindless resources descriptor
            m_bindlessResources = resources.bindless;

            // Store the push constants offset
            m_pushConstantsOffset = resources.bindless.pushConstantsOffset;

            // Store the bindless resources descriptor
            m_bindlessResources = resources.bindless;

            // Store the constants structured buffer pointers and size
            if (resources.constantsBuffer) m_constantsBuffer = resources.constantsBuffer;
            if (resources.constantsBufferUpload) m_constantsBufferUpload = resources.constantsBufferUpload;
            if (resources.constantsBufferUploadMemory) m_constantsBufferUploadMemory = resources.constantsBufferUploadMemory;
            m_constantsBufferSizeInBytes = resources.constantsBufferSizeInBytes;

            // Allocate or store pointers to the pipeline layout, descriptor set, textures, and pipelines
        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            result = CreateManagedResources(desc, resources.managed);
            if (result != ERTXGIStatus::OK) return result;
        #else
            StoreUnmanagedResourcesDesc(resources.unmanaged);
        #endif

            // Store the new volume descriptor
            m_desc = desc;

            // Vulkan only: Force relocation reset in case the allocated memory isn't zeroed
            if(m_desc.probeRelocationEnabled) m_desc.probeRelocationNeedsReset = true;

        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            Transition(cmdBuffer); // Transition texture arrays for general use

            // Create the descriptor set
            if (!CreateDescriptorSet()) return ERTXGIStatus::ERROR_DDGI_VK_CREATE_FAILURE_DESCRIPTOR_SET;
        #endif

            // Store the volume rotation
            m_rotationMatrix = EulerAnglesToRotationMatrix(desc.eulerAngles);
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
            if (bInsertPerfMarkers) AddPerfMarker(cmdBuffer, RTXGI_PERF_MARKER_GREEN, "RTXGI DDGI Clear Probes");

            uint32_t width, height, arraySize;
            GetDDGIVolumeProbeCounts(m_desc, width, height, arraySize);

            VkClearColorValue color = { { 0.f, 0.f, 0.f, 1.f } };
            VkImageSubresourceRange range;
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseMipLevel = 0;
            range.levelCount = 1;
            range.baseArrayLayer = 0;
            range.layerCount = arraySize;

            vkCmdClearColorImage(cmdBuffer, m_probeIrradiance, VK_IMAGE_LAYOUT_GENERAL, &color, 1, &range);
            vkCmdClearColorImage(cmdBuffer, m_probeDistance, VK_IMAGE_LAYOUT_GENERAL, &color, 1, &range);

            if (bInsertPerfMarkers) vkCmdEndDebugUtilsLabelEXT(cmdBuffer);

            return ERTXGIStatus::OK;
        }

        void DDGIVolume::Destroy()
        {
            m_bindlessResources = {};

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
            vkDestroyShaderModule(m_device, m_probeRelocationModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeRelocationResetModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeClassificationModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeClassificationResetModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeVariabilityReductionModule, nullptr);
            vkDestroyShaderModule(m_device, m_probeVariabilityExtraReductionModule, nullptr);

            // Pipelines
            vkDestroyPipeline(m_device, m_probeBlendingIrradiancePipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeBlendingDistancePipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeRelocationPipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeRelocationResetPipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeClassificationPipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeClassificationResetPipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeVariabilityReductionPipeline, nullptr);
            vkDestroyPipeline(m_device, m_probeVariabilityExtraReductionPipeline, nullptr);

            // Texture Arrays
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

            vkDestroyImage(m_device, m_probeVariability, nullptr);
            vkDestroyImageView(m_device, m_probeVariabilityView, nullptr);
            vkFreeMemory(m_device, m_probeVariabilityMemory, nullptr);

            vkDestroyImage(m_device, m_probeVariabilityAverage, nullptr);
            vkDestroyImageView(m_device, m_probeVariabilityAverageView, nullptr);
            vkFreeMemory(m_device, m_probeVariabilityAverageMemory, nullptr);

            vkDestroyBuffer(m_device, m_probeVariabilityReadback, nullptr);
            vkFreeMemory(m_device, m_probeVariabilityReadbackMemory, nullptr);

            m_descriptorSetLayout = nullptr;
            m_descriptorPool = nullptr;
            m_device = nullptr;
            m_physicalDevice = nullptr;
        #endif

            m_descriptorSet = nullptr;
            m_pipelineLayout = nullptr;

            // Texture Arrays
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
            m_probeVariability = nullptr;
            m_probeVariabilityMemory = nullptr;
            m_probeVariabilityView = nullptr;
            m_probeVariabilityAverage = nullptr;
            m_probeVariabilityAverageMemory = nullptr;
            m_probeVariabilityAverageView = nullptr;
            m_probeVariabilityReadback = nullptr;
            m_probeVariabilityReadbackMemory = nullptr;

            // Shader Modules
            m_probeBlendingIrradianceModule = nullptr;
            m_probeBlendingDistanceModule = nullptr;
            m_probeRelocationModule = nullptr;
            m_probeRelocationResetModule = nullptr;
            m_probeClassificationModule = nullptr;
            m_probeClassificationResetModule = nullptr;
            m_probeVariabilityReductionModule = nullptr;
            m_probeVariabilityExtraReductionModule = nullptr;

            // Pipelines
            m_probeBlendingIrradiancePipeline = nullptr;
            m_probeBlendingDistancePipeline = nullptr;
            m_probeRelocationPipeline = nullptr;
            m_probeRelocationResetPipeline = nullptr;
            m_probeClassificationPipeline = nullptr;
            m_probeClassificationResetPipeline = nullptr;
            m_probeVariabilityReductionPipeline = nullptr;
            m_probeVariabilityExtraReductionPipeline = nullptr;
        }

        uint32_t DDGIVolume::GetGPUMemoryUsedInBytes() const
        {
            uint32_t bytesPerVolume = DDGIVolumeBase::GetGPUMemoryUsedInBytes();

            if (m_bindlessResources.enabled)
            {
                // Add the memory used for the GPU-side DDGIVolumeResourceIndices (32B)
                bytesPerVolume += (uint32_t)sizeof(DDGIVolumeResourceIndices);
            }

            return bytesPerVolume;
        }

        //------------------------------------------------------------------------
        // Private Resource Allocation Helper Functions (Managed Resources)
        //------------------------------------------------------------------------

    #if RTXGI_DDGI_RESOURCE_MANAGEMENT
        void DDGIVolume::Transition(VkCommandBuffer cmdBuffer)
        {
            uint32_t width, height, arraySize;
            GetDDGIVolumeProbeCounts(m_desc, width, height, arraySize);

            // Transition the texture arrays for general use
            std::vector<VkImageMemoryBarrier> barriers;

            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, arraySize };

            barrier.image = m_probeRayData;
            barriers.push_back(barrier);
            barrier.image = m_probeIrradiance;
            barriers.push_back(barrier);
            barrier.image = m_probeDistance;
            barriers.push_back(barrier);
            barrier.image = m_probeData;
            barriers.push_back(barrier);
            barrier.image = m_probeVariability;
            barriers.push_back(barrier);

            GetDDGIVolumeTextureDimensions(m_desc, EDDGIVolumeTextureType::VariabilityAverage, width, height, arraySize);
            barrier.image = m_probeVariabilityAverage;
            barrier.subresourceRange.layerCount = arraySize;
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
            VkWriteDescriptorSet* descriptor = nullptr;
            std::vector<VkWriteDescriptorSet> descriptors;

            // 0: Volume Constants StructuredBuffer
            VkDescriptorBufferInfo volumeConstants = { m_constantsBuffer, 0, VK_WHOLE_SIZE };

            descriptor = &descriptors.emplace_back();
            descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor->dstSet = m_descriptorSet;
            descriptor->dstBinding = static_cast<uint32_t>(rtxgi::vulkan::EDDGIVolumeBindings::Constants);
            descriptor->dstArrayElement = 0;
            descriptor->descriptorCount = 1;
            descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptor->pBufferInfo = &volumeConstants;

            // 1-6: Volume Texture Array UAVs
            VkDescriptorImageInfo rwTex2D[] =
            {
                { VK_NULL_HANDLE, m_probeRayDataView, VK_IMAGE_LAYOUT_GENERAL },
                { VK_NULL_HANDLE, m_probeIrradianceView, VK_IMAGE_LAYOUT_GENERAL },
                { VK_NULL_HANDLE, m_probeDistanceView, VK_IMAGE_LAYOUT_GENERAL },
                { VK_NULL_HANDLE, m_probeDataView, VK_IMAGE_LAYOUT_GENERAL },
                { VK_NULL_HANDLE, m_probeVariabilityView, VK_IMAGE_LAYOUT_GENERAL },
                { VK_NULL_HANDLE, m_probeVariabilityAverageView, VK_IMAGE_LAYOUT_GENERAL }
            };

            descriptor = &descriptors.emplace_back();
            descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor->dstSet = m_descriptorSet;
            descriptor->dstBinding = static_cast<uint32_t>(EDDGIVolumeBindings::RayData);
            descriptor->dstArrayElement = 0;
            descriptor->descriptorCount = _countof(rwTex2D);
            descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptor->pImageInfo = rwTex2D;

            VkDescriptorImageInfo variabilityInfo = { VK_NULL_HANDLE, m_probeVariabilityView, VK_IMAGE_LAYOUT_GENERAL };

            // Probe Variability
            descriptor = &descriptors.emplace_back();
            descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor->dstSet = m_descriptorSet;
            descriptor->dstBinding = static_cast<uint32_t>(EDDGIVolumeBindings::ProbeVariability);
            descriptor->dstArrayElement = 0;
            descriptor->descriptorCount = 1;
            descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptor->pImageInfo = &variabilityInfo;

            VkDescriptorImageInfo variabilityAverageInfo = { VK_NULL_HANDLE, m_probeVariabilityAverageView, VK_IMAGE_LAYOUT_GENERAL };

            // Probe Variability Average
            descriptor = &descriptors.emplace_back();
            descriptor->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptor->dstSet = m_descriptorSet;
            descriptor->dstBinding = static_cast<uint32_t>(EDDGIVolumeBindings::ProbeVariabilityAverage);
            descriptor->dstArrayElement = 0;
            descriptor->descriptorCount = 1;
            descriptor->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            descriptor->pImageInfo = &variabilityAverageInfo;

            // Update the descriptor set
            vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(descriptors.size()), descriptors.data(), 0, nullptr);

            return true;
        }

        bool DDGIVolume::CreateLayouts()
        {
            // Get the layout descriptors
            VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
            VkPushConstantRange pushConstantRange = {};
            VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            bindings.resize(GetDDGIVolumeLayoutBindingCount());

            GetDDGIVolumeLayoutDescs(descriptorSetLayoutCreateInfo, pushConstantRange, pipelineLayoutCreateInfo, bindings.data());

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

        bool DDGIVolume::CreateComputePipeline(ShaderBytecode shader, const char* entryPoint, VkShaderModule* module, VkPipeline* pipeline, const char* debugName = "")
        {
            if (std::string(entryPoint).compare("") == 0) return false;

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
            computePipelineCreateInfo.stage.pName = entryPoint;
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

        bool DDGIVolume::CreateTexture(uint32_t width, uint32_t height, uint32_t arraySize, VkFormat format, VkImageUsageFlags usage, VkImage* image, VkDeviceMemory* imageMemory, VkImageView* imageView)
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
            imageCreateInfo.arrayLayers = arraySize;
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
            imageViewCreateInfo.subresourceRange.layerCount = arraySize;
            if(arraySize > 1) imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            else imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

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
            uint32_t arraySize = 0;
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::RayData, width, height, arraySize);

            // Check for problems
            if (width <= 0 || height <= 0 || arraySize <= 0) return false;

            VkFormat format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::RayData, desc.probeRayDataFormat);
            VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

            // Create the texture, allocate memory, and bind the memory
            bool result = CreateTexture(width, height, arraySize, format, usage, &m_probeRayData, &m_probeRayDataMemory, &m_probeRayDataView);
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
            uint32_t arraySize = 0;
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::Irradiance, width, height, arraySize);

            // Check for problems
            if (width <= 0 || height <= 0 || arraySize <= 0) return false;

            VkFormat format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Irradiance, desc.probeIrradianceFormat);
            VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

            // Create the texture, allocate memory, and bind the memory
            bool result = CreateTexture(width, height, arraySize, format, usage, &m_probeIrradiance, &m_probeIrradianceMemory, &m_probeIrradianceView);
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
            uint32_t arraySize = 0;
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::Distance, width, height, arraySize);

            // Check for problems
            if (width <= 0 || height <= 0 || arraySize <= 0) return false;

            VkFormat format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Distance, desc.probeDistanceFormat);
            VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

            // Create the texture, allocate memory, and bind the memory
            bool result = CreateTexture(width, height, arraySize, format, usage, &m_probeDistance, &m_probeDistanceMemory, &m_probeDistanceView);
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
            uint32_t arraySize = 0;
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::Data, width, height, arraySize);

            // Check for problems
            if (width <= 0 || height <= 0 || arraySize <= 0) return false;

            VkFormat format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Data, desc.probeDataFormat);
            VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

            // Create the texture, allocate memory, and bind the memory
            bool result = CreateTexture(width, height, arraySize, format, usage, &m_probeData, &m_probeDataMemory, &m_probeDataView);
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

        bool DDGIVolume::CreateProbeVariability(const DDGIVolumeDesc& desc)
        {
            vkDestroyImage(m_device, m_probeVariability, nullptr);
            vkDestroyImageView(m_device, m_probeVariabilityView, nullptr);
            vkFreeMemory(m_device, m_probeVariabilityMemory, nullptr);

            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t arraySize = 0;
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::Variability, width, height, arraySize);

            // Check for problems
            if (width <= 0 || height <= 0 || arraySize <= 0) return false;

            VkFormat format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::Variability, desc.probeVariabilityFormat);
            VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

            // Create the texture, allocate memory, and bind the memory
            bool result = CreateTexture(width, height, arraySize, format, usage, &m_probeVariability, &m_probeVariabilityMemory, &m_probeVariabilityView);
            if (!result) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::string name = "DDGIVolume[" + std::to_string(desc.index) + "], Probe Variability";
            std::string memory = name + " Memory";
            std::string view = name + " View";
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeVariability), name.c_str(), VK_OBJECT_TYPE_IMAGE);
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeVariabilityMemory), memory.c_str(), VK_OBJECT_TYPE_DEVICE_MEMORY);
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeVariabilityView), view.c_str(), VK_OBJECT_TYPE_IMAGE_VIEW);
        #endif
            return true;
        }

        bool DDGIVolume::CreateProbeVariabilityAverage(const DDGIVolumeDesc& desc)
        {
            vkDestroyImage(m_device, m_probeVariabilityAverage, nullptr);
            vkDestroyImageView(m_device, m_probeVariabilityAverageView, nullptr);
            vkFreeMemory(m_device, m_probeVariabilityAverageMemory, nullptr);

            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t arraySize = 0;
            GetDDGIVolumeTextureDimensions(desc, EDDGIVolumeTextureType::VariabilityAverage, width, height, arraySize);

            // Check for problems
            if (width <= 0 || height <= 0 || arraySize <= 0) return false;

            VkFormat format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::VariabilityAverage, desc.probeVariabilityFormat);
            VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

            // Create the texture, allocate memory, and bind the memory
            bool result = CreateTexture(width, height, arraySize, format, usage, &m_probeVariabilityAverage, &m_probeVariabilityAverageMemory, &m_probeVariabilityAverageView);
            if (!result) return false;
        #ifdef RTXGI_GFX_NAME_OBJECTS
            std::string name = "DDGIVolume[" + std::to_string(desc.index) + "], Probe Variability Average";
            std::string memory = name + " Memory";
            std::string view = name + " View";
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeVariabilityAverage), name.c_str(), VK_OBJECT_TYPE_IMAGE);
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeVariabilityAverageMemory), memory.c_str(), VK_OBJECT_TYPE_DEVICE_MEMORY);
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeVariabilityAverageView), view.c_str(), VK_OBJECT_TYPE_IMAGE_VIEW);
        #endif

            // Create the readback texture
            vkDestroyBuffer(m_device, m_probeVariabilityReadback, nullptr);

            // Readback texture is always in "full" format (R32G32F)
            format = GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType::VariabilityAverage, desc.probeVariabilityFormat);
            {
                VkBufferCreateInfo bufferCreateInfo = {};
                bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                bufferCreateInfo.size = sizeof(float) * 2;
                bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

                // Create the buffer
                VkResult result = vkCreateBuffer(m_device, &bufferCreateInfo, nullptr, &m_probeVariabilityReadback);
                if (VKFAILED(result)) return false;

                // Get memory requirements
                VkMemoryRequirements reqs;
                vkGetBufferMemoryRequirements(m_device, m_probeVariabilityReadback, &reqs);

                // Allocate memory
                VkMemoryAllocateFlags flags = 0;
                VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                if (!AllocateMemory(reqs, props, flags, &m_probeVariabilityReadbackMemory)) return false;

                vkBindBufferMemory(m_device, m_probeVariabilityReadback, m_probeVariabilityReadbackMemory, 0);
            }
        #ifdef RTXGI_GFX_NAME_OBJECTS
            name = "DDGIVolume[" + std::to_string(desc.index) + "], Probe Variability Readback";
            memory = name + " Memory";
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeVariabilityReadback), name.c_str(), VK_OBJECT_TYPE_BUFFER);
            SetObjectName(m_device, reinterpret_cast<uint64_t>(m_probeVariabilityReadbackMemory), memory.c_str(), VK_OBJECT_TYPE_DEVICE_MEMORY);
        #endif

            return true;
        }

    #endif // RTXGI_MANAGED_RESOURCES
    } // namespace vulkan
} // namespace rtxgi
