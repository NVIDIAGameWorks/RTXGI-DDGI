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

#include "../DDGIVolume.h"

#include <vulkan/vulkan.h>

namespace rtxgi
{
    namespace vulkan
    {
        //------------------------------------------------------------------------
        // Managed Resource Mode (SDK manages volume resources)
        //------------------------------------------------------------------------

        struct ProbeRelocationBytecode
        {
            ShaderBytecode              updateCS;                                           // Probe relocation compute shader bytecode
            ShaderBytecode              resetCS;                                            // Probe relocation reset compute shader bytecode
        };

        struct ProbeClassificationBytecode
        {
            ShaderBytecode              updateCS;                                           // Probe classification compute shader bytecode
            ShaderBytecode              resetCS;                                            // Probe classification reset compute shader bytecode
        };

        struct DDGIVolumeManagedResourcesDesc
        {
            bool                         enabled = false;                                    // Enable or disable managed resources mode

            VkDevice                     device = nullptr;                                   // Vulkan device handle
            VkPhysicalDevice             physicalDevice = nullptr;                           // Vulkan physical device handle
            VkDescriptorPool             descriptorPool = nullptr;                           // Vulkan descriptor pool

            // Shader bytecode
            ShaderBytecode               probeBlendingIrradianceCS;                          // Probe blending (irradiance) compute shader bytecode
            ShaderBytecode               probeBlendingDistanceCS;                            // Probe blending (distance) compute shader bytecode
            ShaderBytecode               probeBorderRowUpdateIrradianceCS;                   // Probe border row update (irradiance) compute shader bytecode
            ShaderBytecode               probeBorderRowUpdateDistanceCS;                     // Probe border row update (distance) compute shader bytecode
            ShaderBytecode               probeBorderColumnUpdateIrradianceCS;                // Probe border column update (irradiance) compute shader bytecode
            ShaderBytecode               probeBorderColumnUpdateDistanceCS;                  // Probe border column update (distance) compute shader bytecode

            ProbeRelocationBytecode      probeRelocation;                                    // [Optional] Probe Relocation bytecode
            ProbeClassificationBytecode  probeClassification;                                // [Optional] Probe Classification bytecode
        };

        //------------------------------------------------------------------------
        // Unmanaged Resource Mode (Application manages volume resources)
        //------------------------------------------------------------------------

        struct ProbeRelocationPipeline
        {
            VkShaderModule              updateModule = nullptr;                             // Probe relocation shader module
            VkShaderModule              resetModule = nullptr;                              // Probe relocation reset shader module

            VkPipeline                  updatePipeline = nullptr;                           // Probe relocation compute pipeline
            VkPipeline                  resetPipeline = nullptr;                            // Probe relocation reset compute pipeline
        };

        struct ProbeClassificationPipeline
        {
            VkShaderModule              updateModule = nullptr;                             // Probe classification shader module
            VkShaderModule              resetModule = nullptr;                              // Probe classification reset shader module

            VkPipeline                  updatePipeline = nullptr;                           // Probe classification compute pipeline
            VkPipeline                  resetPipeline = nullptr;                            // Probe classification reset compute pipeline
        };

        struct DDGIVolumeUnmanagedResourcesDesc
        {
            bool                        enabled = false;                                    // Enable or disable unmanaged resources mode

            // Pipeline Layout and Descriptor Set
            VkPipelineLayout            pipelineLayout = nullptr;                           // Pipeline layout
            VkDescriptorSet             descriptorSet = nullptr;                            // Descriptor set

            // Textures
            VkImage                     probeRayData = nullptr;                             // Probe ray data, radiance (RGB) and hit distance (A) texture
            VkImage                     probeIrradiance = nullptr;                          // Probe irradiance texture, encoded with a high gamma curve
            VkImage                     probeDistance = nullptr;                            // Probe distance texture, R: mean distance, G: mean distance^2
            VkImage                     probeData = nullptr;                                // Probe data, relocation world-space offsets (XYZ) and classification (W) states texture

            // Texture Memory
            VkDeviceMemory              probeRayDataMemory = nullptr;                       // Probe ray data device memory
            VkDeviceMemory              probeIrradianceMemory = nullptr;                    // Probe irradiance texture device memory
            VkDeviceMemory              probeDistanceMemory = nullptr;                      // Probe distance texture device memory
            VkDeviceMemory              probeDataMemory = nullptr;                          // Probe data texture device memory

            // Texture Views
            VkImageView                 probeRayDataView = nullptr;                         // Probe ray data texture view
            VkImageView                 probeIrradianceView = nullptr;                      // Probe irradiance texture view
            VkImageView                 probeDistanceView = nullptr;                        // Probe distance texture view
            VkImageView                 probeDataView = nullptr;                            // Probe data texture view

            // Shader Modules
            VkShaderModule              probeBlendingIrradianceModule = nullptr;             // Probe blending (irradiance) shader module
            VkShaderModule              probeBlendingDistanceModule = nullptr;               // Probe blending (distance) shader module
            VkShaderModule              probeBorderRowUpdateIrradianceModule = nullptr;      // Probe border row update (irradiance) shader module
            VkShaderModule              probeBorderRowUpdateDistanceModule = nullptr;        // Probe border row update (distance) shader module
            VkShaderModule              probeBorderColumnUpdateIrradianceModule = nullptr;   // Probe border column update (irradiance) shader module
            VkShaderModule              probeBorderColumnUpdateDistanceModule = nullptr;     // Probe border column update (distance) shader module

            // Pipelines
            VkPipeline                  probeBlendingIrradiancePipeline = nullptr;           // Probe blending (irradiance) compute pipeline
            VkPipeline                  probeBlendingDistancePipeline = nullptr;             // Probe blending (distance) compute pipeline
            VkPipeline                  probeBorderRowUpdateIrradiancePipeline = nullptr;    // Probe border row update (irradiance) compute pipeline
            VkPipeline                  probeBorderRowUpdateDistancePipeline = nullptr;      // Probe border row update (distance) compute pipeline
            VkPipeline                  probeBorderColumnUpdateIrradiancePipeline = nullptr; // Probe border column update (irradiance) compute pipeline
            VkPipeline                  probeBorderColumnUpdateDistancePipeline = nullptr;   // Probe border column update (distance) compute pipeline

            ProbeRelocationPipeline     probeRelocation;                                    // [Optional] Probe Relocation pipelines
            ProbeClassificationPipeline probeClassification;                                // [Optional] Probe Classification pipelines
        };

        //------------------------------------------------------------------------

        struct DDGIVolumeBindlessDescriptorDesc
        {
            uint32_t                    pushConstantsOffset = 0;                            // Offset to the DDGIConsts data in the push constants block
            uint32_t                    uavOffset = 0;                                      // Offset to the ray data UAV in a bindless RWTex2D descriptor range. Ignored when shaders are not in bindless mode.
            uint32_t                    srvOffset = 0;                                      // Offset to the ray data SRV in a bindless Tex2D descriptor range. Ignored when shaders are not in bindless mode.
        };

        /**
         * Specifies the resources used by the DDGIVolume.
         */
        struct DDGIVolumeResources
        {
            DDGIVolumeBindlessDescriptorDesc  descriptorBindlessDesc;                       // Provides offsets to the volume's resources in bindless resource arrays.
            DDGIVolumeManagedResourcesDesc    managed;                                      // [Managed Resource Mode] Provides Vulkan device handles and compiled shader bytecode.
            DDGIVolumeUnmanagedResourcesDesc  unmanaged;                                    // [Unmanaged Resource Mode] Provides a pipeline layout, descriptor set, and pointers to texture resources and pipelines.

            VkBuffer                constantsBuffer = nullptr;                              // Constants structured buffer (device)

            // Provide these resources if you use DDGIVolume::Upload() to transfer constants to the GPU
            VkBuffer                constantsBufferUpload = nullptr;                        // [Optional] Constants structured buffer (upload)
            VkDeviceMemory          constantsBufferUploadMemory = nullptr;                  // [Optional] Constants structured buffer memory (upload)
            uint64_t                constantsBufferSizeInBytes = 0;                         // [Optional] Size (in bytes) of the constants structured buffer
        };

        //------------------------------------------------------------------------
        // Public RTXGI Vulkan namespace functions
        //------------------------------------------------------------------------

        /**
         * Get the VkFormat type of the given texture resource.
         */
        VkFormat GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType type, uint32_t format);

        /**
         * Get the DDGIVolume's descriptor set and pipeline layouts descriptors.
         */
        void GetDDGIVolumeLayoutDescs(
            std::vector<VkDescriptorSetLayoutBinding>& bindings,
            VkDescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo,
            VkPushConstantRange& pushConstantRange,
            VkPipelineLayoutCreateInfo& pipelineLayoutCreateInfo);

        //------------------------------------------------------------------------
        // DDGIVolume
        //------------------------------------------------------------------------

        /**
         * DDGIVolume
         * A volume within which irradiance queries at arbitrary points are supported using a grid
         * of probes. A single DDGIVolume may cover the entire scene or some sub-volume of the scene.
         *
         * The probe grid of the volume is centered around the provided origin. Grid probes are numbered
         * in ascending order from left to right, back to front (in a left handed coordinate system).
         *
         * If there are parts of a scene with very different geometric density or dimensions, use
         * multiple DDGIVolumes with varying probe densities.
         */
        class DDGIVolume : public DDGIVolumeBase
        {
        public:
            /**
             * Performs other initialization of the DDGIVolume.
             * Validates resource pointers or allocates resources if resource management is enabled.
             */
        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            ERTXGIStatus Create(VkCommandBuffer cmdBuffer, const DDGIVolumeDesc& desc, const DDGIVolumeResources& resources);
        #else
            ERTXGIStatus Create(const DDGIVolumeDesc& desc, const DDGIVolumeResources& resources);
        #endif

            /**
             * Clears the volume's probe texture atlases.
             */
            ERTXGIStatus ClearProbes(VkCommandBuffer cmdBuffer);

            /**
             * Releases resources owned by the volume.
             */
            void Destroy();

            //------------------------------------------------------------------------
            // Resource Getters
            //------------------------------------------------------------------------

            VkPipelineLayout GetPipelineLayout() const { return m_pipelineLayout; }

            // Descriptors
            const VkDescriptorSet* GetDescriptorSetConstPtr() const { return &m_descriptorSet; }
            VkDescriptorSet* GetDescriptorSetPtr() { return &m_descriptorSet; }
            VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_descriptorSetLayout; }
            uint32_t GetDescriptorBindlessUAVOffset() const { return m_descriptorBindlessUAVOffset; }
            uint32_t GetDescriptorBindlessSRVOffset() const { return m_descriptorBindlessSRVOffset; }

            // Constants
            uint32_t GetPushConstantsOffset() const { return m_pushConstantsOffset; }
            VkBuffer GetConstantsBuffer() const { return m_constantsBuffer; }
            VkBuffer GetConstantsBufferUpload() const { return m_constantsBufferUpload; }
            VkDeviceMemory GetConstantsBufferUploadMemory() const { return m_constantsBufferUploadMemory; }
            uint64_t GetConstantsBufferSizeInBytes() const { return m_constantsBufferSizeInBytes; }

            // Textures
            uint32_t GetRayDataFormat() const { return m_desc.probeRayDataFormat; }
            uint32_t GetIrradianceFormat() const { return m_desc.probeIrradianceFormat; }
            uint32_t GetDistanceFormat() const { return m_desc.probeDistanceFormat; }
            uint32_t GetProbeDataFormat() const { return m_desc.probeDataFormat; }

            VkImage GetProbeRayData() const { return m_probeRayData; }
            VkDeviceMemory GetProbeRayDataMemory() const { return m_probeRayDataMemory; }
            VkImageView GetProbeRayDataView() const { return m_probeRayDataView; }

            VkImage GetProbeIrradiance() const { return m_probeIrradiance; }
            VkDeviceMemory GetProbeIrradianceMemory() const { return m_probeIrradianceMemory; }
            VkImageView GetProbeIrradianceView() const { return m_probeIrradianceView; }

            VkImage GetProbeDistance() const { return m_probeDistance; }
            VkDeviceMemory GetProbeDistanceMemory() const { return m_probeDistanceMemory; }
            VkImageView GetProbeDistanceView() const { return m_probeDistanceView; }

            VkImage GetProbeData() const { return m_probeData; }
            VkDeviceMemory GetProbeDataMemory() const { return m_probeDataMemory; }
            VkImageView GetProbeDataView() const { return m_probeDataView; }

            // Pipelines
            VkShaderModule GetProbeBlendingIrradianceModule() const { return m_probeBlendingIrradianceModule; }
            VkPipeline GetProbeBlendingIrradiancePipeline() const { return m_probeBlendingIrradiancePipeline; }

            VkShaderModule GetProbeBlendingDistanceModule() const { return m_probeBlendingDistanceModule; }
            VkPipeline GetProbeBlendingDistancePipeline() const { return m_probeBlendingDistancePipeline; }

            VkShaderModule GetProbeBorderRowUpdateIrradianceModule() const { return m_probeBorderRowUpdateIrradianceModule; }
            VkPipeline GetProbeBorderRowUpdateIrradiancePipeline() const { return m_probeBorderRowUpdateIrradiancePipeline; }

            VkShaderModule GetProbeBorderColumnUpdateIrradianceModule() const { return m_probeBorderColumnUpdateIrradianceModule; }
            VkPipeline GetProbeBorderColumnUpdateIrradiancePipeline() const { return m_probeBorderColumnUpdateIrradiancePipeline; }

            VkShaderModule GetProbeBorderRowUpdateDistanceModule() const { return m_probeBorderRowUpdateDistanceModule; }
            VkPipeline GetProbeBorderRowUpdateDistancePipeline() const { return m_probeBorderRowUpdateDistancePipeline; }

            VkShaderModule GetProbeBorderColumnUpdateDistanceModule() const { return m_probeBorderColumnUpdateDistanceModule; }
            VkPipeline GetProbeBorderColumnUpdateDistancePipeline() const { return m_probeBorderColumnUpdateDistancePipeline; }

            VkShaderModule GetProbeRelocationModule() const { return m_probeRelocationModule; }
            VkPipeline GetProbeRelocationPipeline() const { return m_probeRelocationPipeline; }

            VkShaderModule GetProbeRelocationResetModule() const { return m_probeRelocationResetModule; }
            VkPipeline GetProbeRelocationResetPipeline() const { return m_probeRelocationResetPipeline; }

            VkShaderModule GetProbeClassificationModule() const { return m_probeClassificationModule; }
            VkPipeline GetProbeClassificationPipeline() const { return m_probeClassificationPipeline; }

            VkShaderModule GetProbeClassificationResetModule() const { return m_probeClassificationResetModule; }
            VkPipeline GetProbeClassificationResetPipeline() const { return m_probeClassificationResetPipeline; }

            //------------------------------------------------------------------------
            // Resource Setters
            //------------------------------------------------------------------------

            // Descriptors
            void SetPushConstantsOffset(uint32_t offset) { m_pushConstantsOffset = offset; }
            void SetDescriptorBindlessUAVOffset(uint32_t offset) { m_descriptorBindlessUAVOffset = offset; }
            void SetDescriptorBindlessSRVOffset(uint32_t offset) { m_descriptorBindlessSRVOffset = offset; }

            // Constants
            void SetConstantsBuffer(VkBuffer ptr) { m_constantsBuffer = ptr; }
            void SetConstantsBufferUpload(VkBuffer ptr) { m_constantsBufferUpload = ptr; }
            void SetConstantsBufferUploadMemory(VkDeviceMemory ptr) { m_constantsBufferUploadMemory = ptr; }
            void SetConstantsBufferSizeInBytes(uint64_t value) { m_constantsBufferSizeInBytes = value; }

        #if !RTXGI_DDGI_RESOURCE_MANAGEMENT
            void SetProbeRayData(VkImage ptr, VkDeviceMemory memoryPtr, VkImageView viewPtr) { m_probeRayData = ptr; m_probeRayDataMemory = memoryPtr; m_probeRayDataView = viewPtr; }
            void SetProbeIrradiance(VkImage ptr, VkDeviceMemory memoryPtr, VkImageView viewPtr) { m_probeIrradiance = ptr; m_probeIrradianceMemory = memoryPtr; m_probeIrradianceView = viewPtr; }
            void SetProbeDistance(VkImage ptr, VkDeviceMemory memoryPtr, VkImageView viewPtr) { m_probeDistance = ptr; m_probeDistanceMemory = memoryPtr; m_probeDistanceView = viewPtr; }
            void SetProbeData(VkImage ptr, VkDeviceMemory memoryPtr, VkImageView viewPtr) { m_probeData = ptr; m_probeDataMemory = memoryPtr; m_probeDataView = viewPtr; }
        #endif

        private:

        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            VkDevice                    m_device = nullptr;                                    // Vulkan device handle
            VkPhysicalDevice            m_physicalDevice = nullptr;                            // Vulkan physical device handle
            VkDescriptorPool            m_descriptorPool = nullptr;                            // Vulkan descriptor pool handle
        #endif

            // Constants (if you use the DDGIVolume::Upload() to transfer constants to the GPU)
            VkBuffer                    m_constantsBuffer = nullptr;                           // Structured buffer that stores the volume's constants (device)
            VkBuffer                    m_constantsBufferUpload = nullptr;                     // Structured buffer that stores the volume's constants (upload)
            VkDeviceMemory              m_constantsBufferUploadMemory = nullptr;               // Memory for the volume's constants upload structured buffer
            uint64_t                    m_constantsBufferSizeInBytes = 0;                      // Size (in bytes) of the structured buffer that stores *all* volumes constants

            // Textures
            VkImage                     m_probeRayData = nullptr;                              // Probe ray data, radiance (RGB) and hit distance (A) texture
            VkDeviceMemory              m_probeRayDataMemory = nullptr;                        // Probe ray data memory
            VkImageView                 m_probeRayDataView = nullptr;                          // Probe ray data view

            VkImage                     m_probeIrradiance = nullptr;                           // Probe irradiance texture, encoded with a high gamma curve
            VkDeviceMemory              m_probeIrradianceMemory = nullptr;                     // Probe irradiance memory
            VkImageView                 m_probeIrradianceView = nullptr;                       // Probe irradiance view

            VkImage                     m_probeDistance = nullptr;                             // Probe distance texture, R: mean distance, G: mean distance^2
            VkDeviceMemory              m_probeDistanceMemory = nullptr;                       // Probe distance memory
            VkImageView                 m_probeDistanceView = nullptr;                         // Probe distance view

            VkImage                     m_probeData = nullptr;                                 // Probe relocation world-space offsets (XYZ) and classification (W) states texture
            VkDeviceMemory              m_probeDataMemory = nullptr;                           // Probe data memory
            VkImageView                 m_probeDataView = nullptr;                             // Probe data view

            // Pipeline Layout
            VkPipelineLayout            m_pipelineLayout = nullptr;                            // Pipeline layout, used for all update compute shaders

            // Descriptors
            VkDescriptorSet             m_descriptorSet = nullptr;                             // Descriptor set
            VkDescriptorSetLayout       m_descriptorSetLayout = nullptr;                       // Descriptor set layout

            uint32_t                    m_pushConstantsOffset = 0;                             // Offset to the DDGIConsts data in the push constants block
            uint32_t                    m_descriptorBindlessUAVOffset = 0;                     // Offset to the ray data UAV in the RWTex2D descriptor range
            uint32_t                    m_descriptorBindlessSRVOffset = 0;                     // Offset to the ray data SRV in the Tex2D descriptor range

            // Shader Modules
            VkShaderModule              m_probeBlendingIrradianceModule = nullptr;             // Probe blending (irradiance) shader module
            VkShaderModule              m_probeBlendingDistanceModule = nullptr;               // Probe blending (distance) shader module
            VkShaderModule              m_probeBorderRowUpdateIrradianceModule = nullptr;      // Probe border row update (irradiance) shader module
            VkShaderModule              m_probeBorderRowUpdateDistanceModule = nullptr;        // Probe border row update (distance) shader module
            VkShaderModule              m_probeBorderColumnUpdateIrradianceModule = nullptr;   // Probe border column update (irradiance) shader module
            VkShaderModule              m_probeBorderColumnUpdateDistanceModule = nullptr;     // Probe border column update (distance) shader module

            VkShaderModule              m_probeRelocationModule = nullptr;                     // Probe relocation shader module
            VkShaderModule              m_probeRelocationResetModule = nullptr;                // Probe relocation reset shader module

            VkShaderModule              m_probeClassificationModule = nullptr;                 // Probe classification shader module
            VkShaderModule              m_probeClassificationResetModule = nullptr;            // Probe classification reset shader module

            // Pipelines
            VkPipeline                  m_probeBlendingIrradiancePipeline = nullptr;           // Probe blending (irradiance) compute shader pipeline
            VkPipeline                  m_probeBlendingDistancePipeline = nullptr;             // Probe blending (distance) compute shader pipeline
            VkPipeline                  m_probeBorderRowUpdateIrradiancePipeline = nullptr;    // Probe border row update (irradiance) compute shader pipeline
            VkPipeline                  m_probeBorderRowUpdateDistancePipeline = nullptr;      // Probe border row update (distance= compute shader pipeline
            VkPipeline                  m_probeBorderColumnUpdateIrradiancePipeline = nullptr; // Probe border column update (irradiance) compute shader pipeline
            VkPipeline                  m_probeBorderColumnUpdateDistancePipeline = nullptr;   // Probe border column update (distance) compute shader pipeline

            VkPipeline                  m_probeRelocationPipeline = nullptr;                   // Probe relocation compute shader pipeline
            VkPipeline                  m_probeRelocationResetPipeline = nullptr;              // Probe relocation reset compute shader pipeline

            VkPipeline                  m_probeClassificationPipeline = nullptr;               // Probe classification compute shader pipeline
            VkPipeline                  m_probeClassificationResetPipeline = nullptr;          // Probe classification reset compute shader pipeline

        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            ERTXGIStatus CreateManagedResources(const DDGIVolumeDesc& desc, const DDGIVolumeManagedResourcesDesc& managed);
            void ReleaseManagedResources();

            void Transition(VkCommandBuffer cmdBuffer);
            bool AllocateMemory(VkMemoryRequirements reqs, VkMemoryPropertyFlags props, VkMemoryAllocateFlags flags, VkDeviceMemory* memory);

            bool CreateDescriptorSet();
            bool CreateLayouts();
            bool CreateComputePipeline(ShaderBytecode shader, std::string entryPoint, VkShaderModule* module, VkPipeline* pipeline, std::string debugName);
            bool CreateTexture(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkImage* image, VkDeviceMemory* imageMemory, VkImageView* imageView);
            bool CreateProbeRayData(const DDGIVolumeDesc& desc);
            bool CreateProbeIrradiance(const DDGIVolumeDesc& desc);
            bool CreateProbeDistance(const DDGIVolumeDesc& desc);
            bool CreateProbeData(const DDGIVolumeDesc& desc);

            bool IsDeviceChanged(const DDGIVolumeManagedResourcesDesc& desc)
            {
                // Has the Vulkan device changed?
                if (desc.device != m_device) return true;
                return false;
            }
        #else
            void StoreUnmanagedResourcesDesc(const DDGIVolumeUnmanagedResourcesDesc& unmanaged);
        #endif
        }; // class DDGIVolume

        //------------------------------------------------------------------------
        // Public RTXGI Vulkan namespace DDGIVolume Functions
        //------------------------------------------------------------------------

        /**
         * Uploads constants for one or more volumes to the GPU.
         * This function is for convenience and isn't necessary if you upload volume constants yourself.
         */
        ERTXGIStatus UploadDDGIVolumeConstants(VkDevice device, VkCommandBuffer cmdBuffer, uint32_t bufferingIndex, uint32_t numVolumes, DDGIVolume** volumes);

        /**
         * Updates one or more volume's probes using data in the volume's radiance texture.
         * Probe blending and border update workloads are batched together for better performance.
         */
        ERTXGIStatus UpdateDDGIVolumeProbes(VkCommandBuffer cmdBuffer, uint32_t numVolumes, DDGIVolume** volumes);

        /**
         * Adjusts one or more volume's world-space probe positions to avoid them being too close to or inside of geometry.
         * If a volume has the reset flag set, all probe relocation offsets are set to zero before relocation occurs.
         */
        ERTXGIStatus RelocateDDGIVolumeProbes(VkCommandBuffer cmdBuffer, uint32_t numVolumes, DDGIVolume** volumes);

        /**
         * Classifies one or more volume's probes as active or inactive based on the hit distance data in the ray data texture.
         * If a volume has the reset flag set, all probes are set to active before classification occurs.
         */
        ERTXGIStatus ClassifyDDGIVolumeProbes(VkCommandBuffer cmdBuffer, uint32_t numVolumes, DDGIVolume** volumes);

    } // namespace vulkan
} // namespace rtxgi
