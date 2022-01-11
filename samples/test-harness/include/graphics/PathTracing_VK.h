/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include "Graphics.h"

namespace Graphics
{
    namespace Vulkan
    {
        namespace PathTracing
        {
            struct Resources
            {
                VkImage                        PTOutput = nullptr;
                VkDeviceMemory                 PTOutputMemory = nullptr;
                VkImageView                    PTOutputView = nullptr;

                VkImage                        PTAccumulation = nullptr;
                VkDeviceMemory                 PTAccumulationMemory = nullptr;
                VkImageView                    PTAccumulationView = nullptr;

                VkBuffer                       shaderTable = nullptr;
                VkBuffer                       shaderTableUpload = nullptr;
                VkDeviceMemory                 shaderTableMemory = nullptr;
                VkDeviceMemory                 shaderTableUploadMemory = nullptr;

                Shaders::ShaderRTPipeline      shaders;
                RTShaderModules                modules;
                VkDescriptorSet                descriptorSet = nullptr;
                VkPipeline                     pipeline = nullptr;

                uint32_t                       shaderTableSize = 0;
                uint32_t                       shaderTableRecordSize = 0;
                uint32_t                       shaderTableMissTableSize = 0;
                uint32_t                       shaderTableHitGroupTableSize = 0;

                VkDeviceAddress                shaderTableRGSStartAddress = 0;
                VkDeviceAddress                shaderTableMissTableStartAddress = 0;
                VkDeviceAddress                shaderTableHitGroupTableStartAddress = 0;

                Instrumentation::Stat*         cpuStat = nullptr;
                Instrumentation::Stat*         gpuStat = nullptr;
            };
        }
    }

    namespace PathTracing
    {
        using Resources = Graphics::Vulkan::PathTracing::Resources;
    }
}


