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
#include <rtxgi/ddgi/gfx/DDGIVolume_VK.h>

namespace Graphics
{
    namespace Vulkan
    {
        namespace DDGI
        {
            struct Resources
            {
                // Textures
                VkImage                         output = nullptr;
                VkDeviceMemory                  outputMemory = nullptr;
                VkImageView                     outputView = nullptr;

                // Shaders
                Shaders::ShaderRTPipeline       rtShaders;
                Shaders::ShaderProgram          indirectCS;

                // Shader Modules
                RTShaderModules                 rtShaderModules;
                VkShaderModule                  indirectShaderModule = nullptr;

                // Ray Tracing
                VkBuffer                        shaderTable = nullptr;
                VkBuffer                        shaderTableUpload = nullptr;
                VkDeviceMemory                  shaderTableMemory = nullptr;
                VkDeviceMemory                  shaderTableUploadMemory = nullptr;

                VkDescriptorSet                 descriptorSet = nullptr;
                VkPipeline                      rtPipeline = nullptr;
                VkPipeline                      indirectPipeline = nullptr;

                uint32_t                        shaderTableSize = 0;
                uint32_t                        shaderTableRecordSize = 0;
                uint32_t                        shaderTableMissTableSize = 0;
                uint32_t                        shaderTableHitGroupTableSize = 0;

                VkDeviceAddress                 shaderTableRGSStartAddress = 0;
                VkDeviceAddress                 shaderTableMissTableStartAddress = 0;
                VkDeviceAddress                 shaderTableHitGroupTableStartAddress = 0;

                // DDGI
                std::vector<rtxgi::DDGIVolumeDesc> volumeDescs;
                std::vector<rtxgi::DDGIVolumeBase*> volumes;
                std::vector<rtxgi::vulkan::DDGIVolume*> selectedVolumes;

            #if !RTXGI_DDGI_RESOURCE_MANAGEMENT && !RTXGI_DDGI_BINDLESS_RESOURCES
                VkPipelineLayout                volumePipelineLayout = nullptr;
                VkDescriptorSetLayout           volumeDescriptorSetLayout = nullptr;
                std::vector<VkDescriptorSet>    volumeDescriptorSets;
            #endif

                VkBuffer                        volumeResourceIndicesSTB = nullptr;
                VkBuffer                        volumeResourceIndicesSTBUpload = nullptr;
                VkDeviceMemory                  volumeResourceIndicesSTBMemory = nullptr;
                VkDeviceMemory                  volumeResourceIndicesSTBUploadMemory = nullptr;
                uint64_t                        volumeResourceIndicesSTBSizeInBytes = 0;

                VkBuffer                        volumeConstantsSTB = nullptr;
                VkBuffer                        volumeConstantsSTBUpload = nullptr;
                VkDeviceMemory                  volumeConstantsSTBMemory = nullptr;
                VkDeviceMemory                  volumeConstantsSTBUploadMemory = nullptr;
                uint64_t                        volumeConstantsSTBSizeInBytes = 0;

                // Variability Tracking
                std::vector<uint32_t>           numVolumeVariabilitySamples;

                Instrumentation::Stat*          cpuStat = nullptr;
                Instrumentation::Stat*          gpuStat = nullptr;

                Instrumentation::Stat*          classifyStat = nullptr;
                Instrumentation::Stat*          rtStat = nullptr;
                Instrumentation::Stat*          blendStat = nullptr;
                Instrumentation::Stat*          relocateStat = nullptr;
                Instrumentation::Stat*          lightingStat = nullptr;
                Instrumentation::Stat*          variabilityStat = nullptr;

                bool                            enabled = false;
            };
        }
    }

    namespace DDGI
    {
        using Resources = Graphics::Vulkan::DDGI::Resources;
    }
}
