/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "rtxgi/VulkanExtensions.h"

// WARNING: This way of handling extensions works assuming one and only one device exists; do not call across multiple device objects

//----------------------------------------------------------------------------------------------------------
// Debug Util Extensions
//----------------------------------------------------------------------------------------------------------

PFN_vkSetDebugUtilsObjectNameEXT gvkSetDebugUtilsObjectNameEXT;
PFN_vkCmdBeginDebugUtilsLabelEXT gvkCmdBeginDebugUtilsLabelEXT;
PFN_vkCmdEndDebugUtilsLabelEXT   gvkCmdEndDebugUtilsLabelEXT;

VKAPI_ATTR VkResult VKAPI_CALL vkSetDebugUtilsObjectNameEXT(VkDevice device, const VkDebugUtilsObjectNameInfoEXT* pNameInfo)
{
    return gvkSetDebugUtilsObjectNameEXT(device, pNameInfo);
}

VKAPI_ATTR void VKAPI_CALL vkCmdBeginDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo)
{
    return gvkCmdBeginDebugUtilsLabelEXT(commandBuffer, pLabelInfo);
}

VKAPI_ATTR void VKAPI_CALL vkCmdEndDebugUtilsLabelEXT(VkCommandBuffer commandBuffer)
{
    return gvkCmdEndDebugUtilsLabelEXT(commandBuffer);
}

//----------------------------------------------------------------------------------------------------------
// Public Functions
//----------------------------------------------------------------------------------------------------------

#define LOAD_DEVICE_PROC(NAME) g##NAME = (PFN_##NAME)vkGetDeviceProcAddr(device, #NAME);

namespace rtxgi
{
    namespace vulkan
    {
        void LoadExtensions(VkDevice device)
        {
            LOAD_DEVICE_PROC(vkSetDebugUtilsObjectNameEXT)
            LOAD_DEVICE_PROC(vkCmdBeginDebugUtilsLabelEXT)
            LOAD_DEVICE_PROC(vkCmdEndDebugUtilsLabelEXT)
        }
    }
}
