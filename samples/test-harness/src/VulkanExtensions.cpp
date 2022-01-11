/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "VulkanExtensions.h"

// WARNING: This way of handling extensions works assuming one and only one device exists; do not call across multiple device objects

//----------------------------------------------------------------------------------------------------------
// Buffer Device Address Extension
//----------------------------------------------------------------------------------------------------------

PFN_vkGetBufferDeviceAddressKHR                       gvkGetBufferDeviceAddressKHR;

VKAPI_ATTR VkDeviceAddress VKAPI_CALL vkGetBufferDeviceAddressKHR(
    VkDevice device,
    const VkBufferDeviceAddressInfo* pInfo)
{
    return gvkGetBufferDeviceAddressKHR(device, pInfo);
}

//----------------------------------------------------------------------------------------------------------
// Acceleration Structure Extension
//----------------------------------------------------------------------------------------------------------

PFN_vkCreateAccelerationStructureKHR                  gvkCreateAccelerationStructureKHR;
PFN_vkDestroyAccelerationStructureKHR                 gvkDestroyAccelerationStructureKHR;
PFN_vkCmdBuildAccelerationStructuresKHR               gvkCmdBuildAccelerationStructuresKHR;
PFN_vkCmdBuildAccelerationStructuresIndirectKHR       gvkCmdBuildAccelerationStructuresIndirectKHR;
PFN_vkBuildAccelerationStructuresKHR                  gvkBuildAccelerationStructuresKHR;
PFN_vkCopyAccelerationStructureKHR                    gvkCopyAccelerationStructureKHR;
PFN_vkCopyAccelerationStructureToMemoryKHR            gvkCopyAccelerationStructureToMemoryKHR;
PFN_vkCopyMemoryToAccelerationStructureKHR            gvkCopyMemoryToAccelerationStructureKHR;
PFN_vkWriteAccelerationStructuresPropertiesKHR        gvkWriteAccelerationStructuresPropertiesKHR;
PFN_vkCmdCopyAccelerationStructureKHR                 gvkCmdCopyAccelerationStructureKHR;
PFN_vkCmdCopyAccelerationStructureToMemoryKHR         gvkCmdCopyAccelerationStructureToMemoryKHR;
PFN_vkCmdCopyMemoryToAccelerationStructureKHR         gvkCmdCopyMemoryToAccelerationStructureKHR;
PFN_vkGetAccelerationStructureDeviceAddressKHR        gvkGetAccelerationStructureDeviceAddressKHR;
PFN_vkCmdWriteAccelerationStructuresPropertiesKHR     gvkCmdWriteAccelerationStructuresPropertiesKHR;
PFN_vkGetDeviceAccelerationStructureCompatibilityKHR  gvkGetDeviceAccelerationStructureCompatibilityKHR;
PFN_vkGetAccelerationStructureBuildSizesKHR           gvkGetAccelerationStructureBuildSizesKHR;

VKAPI_ATTR VkResult VKAPI_CALL vkCreateAccelerationStructureKHR(
    VkDevice device,
    const VkAccelerationStructureCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkAccelerationStructureKHR* pAccelerationStructure)
{
    return gvkCreateAccelerationStructureKHR(device, pCreateInfo, pAllocator, pAccelerationStructure);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyAccelerationStructureKHR(
    VkDevice device,
    VkAccelerationStructureKHR accelerationStructure,
    const VkAllocationCallbacks* pAllocator)
{
    return gvkDestroyAccelerationStructureKHR(device, accelerationStructure, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL vkCmdBuildAccelerationStructuresKHR(
    VkCommandBuffer  commandBuffer,
    uint32_t infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR* pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos)
{
    gvkCmdBuildAccelerationStructuresKHR(commandBuffer, infoCount, pInfos, ppBuildRangeInfos);
}

VKAPI_ATTR void VKAPI_CALL vkCmdBuildAccelerationStructuresIndirectKHR(
    VkCommandBuffer commandBuffer,
    uint32_t infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR* pInfos,
    const VkDeviceAddress* pIndirectDeviceAddresses,
    const uint32_t* pIndirectStrides,
    const uint32_t* const* ppMaxPrimitiveCounts)
{
    gvkCmdBuildAccelerationStructuresIndirectKHR(commandBuffer, infoCount, pInfos, pIndirectDeviceAddresses, pIndirectStrides, ppMaxPrimitiveCounts);
}

VKAPI_ATTR VkResult VKAPI_CALL vkBuildAccelerationStructuresKHR(
    VkDevice device,
    VkDeferredOperationKHR deferredOperation,
    uint32_t infoCount,
    const VkAccelerationStructureBuildGeometryInfoKHR* pInfos,
    const VkAccelerationStructureBuildRangeInfoKHR* const* ppOffsetInfos)
{
    return gvkBuildAccelerationStructuresKHR(device, deferredOperation, infoCount, pInfos, ppOffsetInfos);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCopyAccelerationStructureKHR(
    VkDevice device,
    VkDeferredOperationKHR deferredOperation,
    const VkCopyAccelerationStructureInfoKHR* pInfo)
{
    return gvkCopyAccelerationStructureKHR(device, deferredOperation, pInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCopyAccelerationStructureToMemoryKHR(
    VkDevice device,
    VkDeferredOperationKHR deferredOperation,
    const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo)
{
    return gvkCopyAccelerationStructureToMemoryKHR(device, deferredOperation, pInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCopyMemoryToAccelerationStructureKHR(
    VkDevice device,
    VkDeferredOperationKHR deferredOperation,
    const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo)
{
    return gvkCopyMemoryToAccelerationStructureKHR(device, deferredOperation, pInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL vkWriteAccelerationStructuresPropertiesKHR(
    VkDevice device,
    uint32_t accelerationStructureCount,
    const VkAccelerationStructureKHR* pAccelerationStructures,
    VkQueryType queryType,
    size_t dataSize,
    void* pData,
    size_t stride)
{
    return gvkWriteAccelerationStructuresPropertiesKHR(device, accelerationStructureCount, pAccelerationStructures, queryType, dataSize, pData, stride);
}

VKAPI_ATTR void VKAPI_CALL vkCmdCopyAccelerationStructureKHR(
    VkCommandBuffer commandBuffer,
    const VkCopyAccelerationStructureInfoKHR* pInfo)
{
    gvkCmdCopyAccelerationStructureKHR(commandBuffer, pInfo);
}

VKAPI_ATTR void VKAPI_CALL vkCmdCopyAccelerationStructureToMemoryKHR(
    VkCommandBuffer commandBuffer,
    const VkCopyAccelerationStructureToMemoryInfoKHR* pInfo)
{
    gvkCmdCopyAccelerationStructureToMemoryKHR(commandBuffer, pInfo);
}

VKAPI_ATTR void VKAPI_CALL vkCmdCopyMemoryToAccelerationStructureKHR(
    VkCommandBuffer commandBuffer,
    const VkCopyMemoryToAccelerationStructureInfoKHR* pInfo)
{
    gvkCmdCopyMemoryToAccelerationStructureKHR(commandBuffer, pInfo);
}

VKAPI_ATTR VkDeviceAddress VKAPI_CALL vkGetAccelerationStructureDeviceAddressKHR(
    VkDevice device,
    const VkAccelerationStructureDeviceAddressInfoKHR* pInfo)
{
    return gvkGetAccelerationStructureDeviceAddressKHR(device, pInfo);
}

VKAPI_ATTR void VKAPI_CALL vkCmdWriteAccelerationStructuresPropertiesKHR(
    VkCommandBuffer commandBuffer,
    uint32_t  accelerationStructureCount,
    const VkAccelerationStructureKHR* pAccelerationStructures,
    VkQueryType queryType,
    VkQueryPool queryPool,
    uint32_t firstQuery)
{
    return gvkCmdWriteAccelerationStructuresPropertiesKHR(commandBuffer, accelerationStructureCount, pAccelerationStructures, queryType, queryPool, firstQuery);
}

VKAPI_ATTR void VKAPI_CALL vkGetDeviceAccelerationStructureCompatibilityKHR(
    VkDevice device,
    const VkAccelerationStructureVersionInfoKHR* pVersionInfo,
    VkAccelerationStructureCompatibilityKHR* pCompatibility)
{
    return gvkGetDeviceAccelerationStructureCompatibilityKHR(device, pVersionInfo, pCompatibility);
}

VKAPI_ATTR void VKAPI_CALL vkGetAccelerationStructureBuildSizesKHR(
    VkDevice device,
    VkAccelerationStructureBuildTypeKHR buildType,
    const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo,
    const uint32_t* pMaxPrimitiveCounts,
    VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo)
{
    gvkGetAccelerationStructureBuildSizesKHR(device, buildType, pBuildInfo, pMaxPrimitiveCounts, pSizeInfo);
}

//----------------------------------------------------------------------------------------------------------
// Ray Tracing Pipeline Extension
//----------------------------------------------------------------------------------------------------------

PFN_vkCmdTraceRaysKHR                                 gvkCmdTraceRaysKHR;
PFN_vkCreateRayTracingPipelinesKHR                    gvkCreateRayTracingPipelinesKHR;
PFN_vkGetRayTracingShaderGroupHandlesKHR              gvkGetRayTracingShaderGroupHandlesKHR;
PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR gvkGetRayTracingCaptureReplayShaderGroupHandlesKHR;
PFN_vkCmdTraceRaysIndirectKHR                         gvkCmdTraceRaysIndirectKHR;
PFN_vkGetRayTracingShaderGroupStackSizeKHR            gvkGetRayTracingShaderGroupStackSizeKHR;
PFN_vkCmdSetRayTracingPipelineStackSizeKHR            gvkCmdSetRayTracingPipelineStackSizeKHR;

VKAPI_ATTR void VKAPI_CALL vkCmdTraceRaysKHR(
    VkCommandBuffer commandBuffer,
    const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable,
    uint32_t width,
    uint32_t height,
    uint32_t depth)
{
    gvkCmdTraceRaysKHR(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, width, height, depth);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateRayTracingPipelinesKHR(
    VkDevice device,
    VkDeferredOperationKHR deferredOperation,
    VkPipelineCache pipelineCache,
    uint32_t createInfoCount,
    const VkRayTracingPipelineCreateInfoKHR* pCreateInfos,
    const VkAllocationCallbacks* pAllocator,
    VkPipeline* pPipelines)
{
    return gvkCreateRayTracingPipelinesKHR(device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines);
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetRayTracingShaderGroupHandlesKHR(
    VkDevice device,
    VkPipeline pipeline,
    uint32_t firstGroup,
    uint32_t groupCount,
    size_t dataSize,
    void* pData)
{
    return gvkGetRayTracingShaderGroupHandlesKHR(device, pipeline, firstGroup, groupCount, dataSize, pData);
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetRayTracingCaptureReplayShaderGroupHandlesKHR(
    VkDevice device,
    VkPipeline pipeline,
    uint32_t firstGroup,
    uint32_t groupCount,
    size_t dataSize,
    void* pData)
{
    return gvkGetRayTracingCaptureReplayShaderGroupHandlesKHR(device, pipeline, firstGroup, groupCount, dataSize, pData);
}

VKAPI_ATTR void VKAPI_CALL vkCmdTraceRaysIndirectKHR(
    VkCommandBuffer commandBuffer,
    const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable,
    const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable,
    VkDeviceAddress indirectDeviceAddress)
{
    gvkCmdTraceRaysIndirectKHR(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, indirectDeviceAddress);
}

VKAPI_ATTR VkDeviceSize VKAPI_CALL vkGetRayTracingShaderGroupStackSizeKHR(
    VkDevice device,
    VkPipeline pipeline,
    uint32_t group,
    VkShaderGroupShaderKHR groupShader)
{
    return gvkGetRayTracingShaderGroupStackSizeKHR(device, pipeline, group, groupShader);
}

VKAPI_ATTR void VKAPI_CALL vkCmdSetRayTracingPipelineStackSizeKHR(
    VkCommandBuffer commandBuffer,
    uint32_t pipelineStackSize)
{
    return gvkCmdSetRayTracingPipelineStackSizeKHR(commandBuffer, pipelineStackSize);
}

//----------------------------------------------------------------------------------------------------------
// Debug Utils Extension
//----------------------------------------------------------------------------------------------------------

// Debug Utils
PFN_vkSetDebugUtilsObjectNameEXT                      gvkSetDebugUtilsObjectNameEXT;
PFN_vkSetDebugUtilsObjectTagEXT                       gvkSetDebugUtilsObjectTagEXT;
PFN_vkQueueBeginDebugUtilsLabelEXT                    gvkQueueBeginDebugUtilsLabelEXT;
PFN_vkQueueEndDebugUtilsLabelEXT                      gvkQueueEndDebugUtilsLabelEXT;
PFN_vkQueueInsertDebugUtilsLabelEXT                   gvkQueueInsertDebugUtilsLabelEXT;
PFN_vkCmdBeginDebugUtilsLabelEXT                      gvkCmdBeginDebugUtilsLabelEXT;
PFN_vkCmdEndDebugUtilsLabelEXT                        gvkCmdEndDebugUtilsLabelEXT;
PFN_vkCmdInsertDebugUtilsLabelEXT                     gvkCmdInsertDebugUtilsLabelEXT;
PFN_vkCreateDebugUtilsMessengerEXT                    gvkCreateDebugUtilsMessengerEXT;
PFN_vkDestroyDebugUtilsMessengerEXT                   gvkDestroyDebugUtilsMessengerEXT;
PFN_vkSubmitDebugUtilsMessageEXT                      gvkSubmitDebugUtilsMessageEXT;

VKAPI_ATTR VkResult VKAPI_CALL vkSetDebugUtilsObjectNameEXT(VkDevice device, const VkDebugUtilsObjectNameInfoEXT* pNameInfo)
{
    return gvkSetDebugUtilsObjectNameEXT(device, pNameInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL vkSetDebugUtilsObjectTagEXT(VkDevice device, const VkDebugUtilsObjectTagInfoEXT* pTagInfo)
{
    return gvkSetDebugUtilsObjectTagEXT(device, pTagInfo);
}

VKAPI_ATTR void VKAPI_CALL vkQueueBeginDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo)
{
    return gvkQueueBeginDebugUtilsLabelEXT(queue, pLabelInfo);
}

VKAPI_ATTR void VKAPI_CALL vkQueueEndDebugUtilsLabelEXT(VkQueue queue)
{
    return gvkQueueEndDebugUtilsLabelEXT(queue);
}

VKAPI_ATTR void VKAPI_CALL vkQueueInsertDebugUtilsLabelEXT(VkQueue queue, const VkDebugUtilsLabelEXT* pLabelInfo)
{
    return gvkQueueInsertDebugUtilsLabelEXT(queue, pLabelInfo);
}

VKAPI_ATTR void VKAPI_CALL vkCmdBeginDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo)
{
    return gvkCmdBeginDebugUtilsLabelEXT(commandBuffer, pLabelInfo);
}

VKAPI_ATTR void VKAPI_CALL vkCmdEndDebugUtilsLabelEXT(VkCommandBuffer commandBuffer)
{
    return gvkCmdEndDebugUtilsLabelEXT(commandBuffer);
}

VKAPI_ATTR void VKAPI_CALL vkCmdInsertDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo)
{
    return gvkCmdInsertDebugUtilsLabelEXT(commandBuffer, pLabelInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pMessenger)
{
    return gvkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* pAllocator)
{
    return gvkDestroyDebugUtilsMessengerEXT(instance, messenger, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL vkSubmitDebugUtilsMessageEXT(
    VkInstance instance,
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData)
{
    return gvkSubmitDebugUtilsMessageEXT(instance, messageSeverity, messageTypes, pCallbackData);
}

//----------------------------------------------------------------------------------------------------------
// Public Functions
//----------------------------------------------------------------------------------------------------------

#define LOAD_INSTANCE_PROC(NAME) g##NAME = (PFN_##NAME)vkGetInstanceProcAddr(instance, #NAME);
#define LOAD_DEVICE_PROC(NAME) g##NAME = (PFN_##NAME)vkGetDeviceProcAddr(device, #NAME);

void LoadInstanceExtensions(VkInstance instance)
{
    // Debug utils extension entry points
    LOAD_INSTANCE_PROC(vkCreateDebugUtilsMessengerEXT);
    LOAD_INSTANCE_PROC(vkDestroyDebugUtilsMessengerEXT);
    LOAD_INSTANCE_PROC(vkSubmitDebugUtilsMessageEXT);
}

void LoadDeviceExtensions(VkDevice device)
{
    // Buffer Device Address extension entry points
    LOAD_DEVICE_PROC(vkGetBufferDeviceAddressKHR)

    // Acceleration Structure extension entry points
    LOAD_DEVICE_PROC(vkCreateAccelerationStructureKHR)
    LOAD_DEVICE_PROC(vkDestroyAccelerationStructureKHR)
    LOAD_DEVICE_PROC(vkCmdBuildAccelerationStructuresKHR)
    LOAD_DEVICE_PROC(vkCmdBuildAccelerationStructuresIndirectKHR)
    LOAD_DEVICE_PROC(vkBuildAccelerationStructuresKHR)
    LOAD_DEVICE_PROC(vkCopyAccelerationStructureKHR)
    LOAD_DEVICE_PROC(vkCopyAccelerationStructureToMemoryKHR)
    LOAD_DEVICE_PROC(vkCopyMemoryToAccelerationStructureKHR)
    LOAD_DEVICE_PROC(vkWriteAccelerationStructuresPropertiesKHR)
    LOAD_DEVICE_PROC(vkCmdCopyAccelerationStructureKHR)
    LOAD_DEVICE_PROC(vkCmdCopyAccelerationStructureToMemoryKHR)
    LOAD_DEVICE_PROC(vkCmdCopyMemoryToAccelerationStructureKHR)
    LOAD_DEVICE_PROC(vkGetAccelerationStructureDeviceAddressKHR)
    LOAD_DEVICE_PROC(vkCmdWriteAccelerationStructuresPropertiesKHR)
    LOAD_DEVICE_PROC(vkGetDeviceAccelerationStructureCompatibilityKHR)
    LOAD_DEVICE_PROC(vkGetAccelerationStructureBuildSizesKHR)

    // Ray Tracing Pipeline extension entry points
    LOAD_DEVICE_PROC(vkCmdTraceRaysKHR)
    LOAD_DEVICE_PROC(vkCreateRayTracingPipelinesKHR)
    LOAD_DEVICE_PROC(vkGetRayTracingShaderGroupHandlesKHR)
    LOAD_DEVICE_PROC(vkGetRayTracingCaptureReplayShaderGroupHandlesKHR)
    LOAD_DEVICE_PROC(vkCmdTraceRaysIndirectKHR)
    LOAD_DEVICE_PROC(vkGetRayTracingShaderGroupStackSizeKHR)
    LOAD_DEVICE_PROC(vkCmdSetRayTracingPipelineStackSizeKHR)

    // Debug Utils extension entry points
    LOAD_DEVICE_PROC(vkSetDebugUtilsObjectNameEXT)
    LOAD_DEVICE_PROC(vkSetDebugUtilsObjectTagEXT)
    LOAD_DEVICE_PROC(vkQueueBeginDebugUtilsLabelEXT)
    LOAD_DEVICE_PROC(vkQueueEndDebugUtilsLabelEXT)
    LOAD_DEVICE_PROC(vkQueueInsertDebugUtilsLabelEXT)
    LOAD_DEVICE_PROC(vkCmdBeginDebugUtilsLabelEXT)
    LOAD_DEVICE_PROC(vkCmdEndDebugUtilsLabelEXT)
    LOAD_DEVICE_PROC(vkCmdInsertDebugUtilsLabelEXT)
}
