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
        namespace DDGI
        {
            namespace Visualizations
            {

                struct Resources
                {
                    // Pipeline Layout and Descriptor Set
                    VkDescriptorSet                                 descriptorSet = nullptr;

                    // Flags
                    uint32_t                                        flags = 0;

                    // Shaders
                    Shaders::ShaderRTPipeline                       rtShaders;
                    RTShaderModules                                 rtShadersModule;
                    Shaders::ShaderRTPipeline                       rtShaders2;
                    RTShaderModules                                 rtShadersModule2;

                    Shaders::ShaderProgram                          textureVisCS;
                    VkShaderModule                                  textureVisModule = nullptr;
                    Shaders::ShaderProgram                          updateTlasCS;
                    VkShaderModule                                  updateTlasModule = nullptr;

                    // Ray Tracing
                    VkBuffer                                        shaderTable = nullptr;
                    VkBuffer                                        shaderTableUpload = nullptr;
                    VkDeviceMemory                                  shaderTableMemory = nullptr;
                    VkDeviceMemory                                  shaderTableUploadMemory = nullptr;

                    VkPipeline                                      rtPipeline = nullptr;
                    VkPipeline                                      rtPipeline2 = nullptr;
                    VkPipeline                                      textureVisPipeline = nullptr;
                    VkPipeline                                      updateTlasPipeline = nullptr;

                    uint32_t                                        shaderTableSize = 0;
                    uint32_t                                        shaderTableRecordSize = 0;
                    uint32_t                                        shaderTableMissTableSize = 0;
                    uint32_t                                        shaderTableHitGroupTableSize = 0;

                    VkDeviceAddress                                 shaderTableRGSStartAddress;
                    VkDeviceAddress                                 shaderTableMissTableStartAddress;
                    VkDeviceAddress                                 shaderTableHitGroupTableStartAddress;

                    VkDeviceAddress                                 shaderTableRGS2StartAddress;
                    VkDeviceAddress                                 shaderTableMissTable2StartAddress;
                    VkDeviceAddress                                 shaderTableHitGroupTable2StartAddress;

                    // Probe Sphere Resources
                    VkBuffer                                        probeVB = nullptr;
                    VkDeviceMemory                                  probeVBMemory = nullptr;
                    VkBuffer                                        probeVBUpload = nullptr;
                    VkDeviceMemory                                  probeVBUploadMemory = nullptr;

                    VkBuffer                                        probeIB = nullptr;
                    VkDeviceMemory                                  probeIBMemory = nullptr;
                    VkBuffer                                        probeIBUpload = nullptr;
                    VkDeviceMemory                                  probeIBUploadMemory = nullptr;

                    Scenes::Mesh                                    probe;
                    AccelerationStructure                           blas;
                    AccelerationStructure                           tlas;

                    uint32_t                                        maxProbeInstances = 0;
                    std::vector<VkAccelerationStructureInstanceKHR> probeInstances;

                    // DDGI Resources
                    uint32_t                                        selectedVolume = 0;
                    std::vector<rtxgi::DDGIVolumeBase*>*            volumes;

                    VkBuffer                                        volumeConstantsSTB = nullptr;
                    VkBuffer                                        volumeResourceIndicesSTB = nullptr;

                    Instrumentation::Stat*                          cpuStat = nullptr;
                    Instrumentation::Stat*                          gpuProbeStat = nullptr;
                    Instrumentation::Stat*                          gpuTextureStat = nullptr;

                    bool                                            enabled = false;
                };
            }
        }
    }

    namespace DDGI::Visualizations
    {
        using Resources = Graphics::Vulkan::DDGI::Visualizations::Resources;
    }
}
