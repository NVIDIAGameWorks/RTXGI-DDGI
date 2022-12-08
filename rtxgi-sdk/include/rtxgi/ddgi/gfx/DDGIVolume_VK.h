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

        enum class EResourceViewType
        {
            UAV = 0,
            SRV,
            COUNT
        };

        enum class EDDGIVolumeBindings
        {
            Constants = 0,
            RayData,
            ProbeIrradiance,
            ProbeDistance,
            ProbeData,
            ProbeVariability,
            ProbeVariabilityAverage
        };

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

        struct ProbeVariabilityByteCode
        {
            ShaderBytecode               reductionCS;                                       // Probe variability reduction compute shader bytecode
            ShaderBytecode               extraReductionCS;                                  // Probe variability reduction extra passes compute shader bytecode
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

            ProbeRelocationBytecode      probeRelocation;                                    // Probe Relocation bytecode
            ProbeClassificationBytecode  probeClassification;                                // Probe Classification bytecode
            ProbeVariabilityByteCode     probeVariability;                                   // Probe Classification bytecode
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

        struct ProbeVariabilityPipeline
        {
            VkShaderModule              reductionModule = nullptr;                          // Probe variability reduction shader module
            VkShaderModule              extraReductionModule = nullptr;                     // Probe variability reduction extra passes shader module

            VkPipeline                  reductionPipeline = nullptr;                        // Probe variability reduction compute pipeline
            VkPipeline                  extraReductionPipeline = nullptr;                   // Probe variability extra reduction compute pipeline
        };

        struct DDGIVolumeUnmanagedResourcesDesc
        {
            bool                        enabled = false;                                    // Enable or disable unmanaged resources mode

            VkPipelineLayout            pipelineLayout = nullptr;                           // Pipeline layout
            VkDescriptorSet             descriptorSet = nullptr;                            // Descriptor set

            // Texture Resources
            VkImage                     probeRayData = nullptr;                             // Probe ray data texture array - RGB: radiance | A: hit distance
            VkImage                     probeIrradiance = nullptr;                          // Probe irradiance texture array - RGB: irradiance, encoded with a high gamma curve
            VkImage                     probeDistance = nullptr;                            // Probe distance texture array - R: mean distance | G: mean distance^2
            VkImage                     probeData = nullptr;                                // Probe data texture array - XYZ: world-space relocation offsets | W: classification state
            VkImage                     probeVariability = nullptr;                         // Probe variability texture array
            VkImage                     probeVariabilityAverage = nullptr;                  // Average of Probe variability for whole volume
            VkBuffer                    probeVariabilityReadback = nullptr;                 // CPU-readable resource containing final Probe variability average

            // Texture Memory
            VkDeviceMemory              probeRayDataMemory = nullptr;                       // Probe ray data texture array device memory
            VkDeviceMemory              probeIrradianceMemory = nullptr;                    // Probe irradiance texture array device memory
            VkDeviceMemory              probeDistanceMemory = nullptr;                      // Probe distance texture array device memory
            VkDeviceMemory              probeDataMemory = nullptr;                          // Probe data texture array device memory
            VkDeviceMemory              probeVariabilityMemory = nullptr;                   // Probe variability texture array device memory
            VkDeviceMemory              probeVariabilityAverageMemory = nullptr;            // Probe variability average texture device memory
            VkDeviceMemory              probeVariabilityReadbackMemory = nullptr;           // Probe variability readback texture device memory

            // Texture Views
            VkImageView                 probeRayDataView = nullptr;                         // Probe ray data texture array view
            VkImageView                 probeIrradianceView = nullptr;                      // Probe irradiance texture array view
            VkImageView                 probeDistanceView = nullptr;                        // Probe distance texture array view
            VkImageView                 probeDataView = nullptr;                            // Probe data texture array view
            VkImageView                 probeVariabilityView = nullptr;                     // Probe variability texture array view
            VkImageView                 probeVariabilityAverageView = nullptr;              // Probe variability average texture view

            // Shader Modules
            VkShaderModule              probeBlendingIrradianceModule = nullptr;             // Probe blending (irradiance) shader module
            VkShaderModule              probeBlendingDistanceModule = nullptr;               // Probe blending (distance) shader module

            // Pipelines
            VkPipeline                  probeBlendingIrradiancePipeline = nullptr;           // Probe blending (irradiance) compute pipeline
            VkPipeline                  probeBlendingDistancePipeline = nullptr;             // Probe blending (distance) compute pipeline

            ProbeRelocationPipeline     probeRelocation;                                     // Probe Relocation pipelines
            ProbeClassificationPipeline probeClassification;                                 // Probe Classification pipelines
            ProbeVariabilityPipeline    probeVariabilityPipelines;                           // Probe Variability pipelines
        };

        //------------------------------------------------------------------------

        struct DDGIVolumeBindlessResourcesDesc
        {
            bool                        enabled;                                      // Specifies if bindless resources are used

            uint32_t                    pushConstantsOffset = 0;                      // Offset to the DDGIConsts data in the push constants block

            DDGIVolumeResourceIndices   resourceIndices;                              // Indices of volume resources in bindless resource arrays

            VkBuffer                    resourceIndicesBuffer = nullptr;              // Resource indices structured buffer pointer (device)

            // Provide these resources if you use UploadDDGIVolumeResourceIndices() to transfer volume resource indices to the GPU
            VkBuffer                    resourceIndicesBufferUpload = nullptr;        // [Optional] Constants structured buffer (upload)
            VkDeviceMemory              resourceIndicesBufferUploadMemory = nullptr;  // [Optional] Constants structured buffer memory (upload)
            uint64_t                    resourceIndicesBufferSizeInBytes = 0;         // [Optional] Size (in bytes) of the constants structured buffer
        };

        /**
         * Specifies the resources used by the DDGIVolume.
         */
        struct DDGIVolumeResources
        {
            DDGIVolumeBindlessResourcesDesc   bindless;                                     // [Optional] Specifies properties of bindless resources
            DDGIVolumeManagedResourcesDesc    managed;                                      // [Managed Resource Mode] Provides Vulkan device handles and compiled shader bytecode.
            DDGIVolumeUnmanagedResourcesDesc  unmanaged;                                    // [Unmanaged Resource Mode] Provides a pipeline layout, descriptor set, and pointers to texture resources and pipelines.

            VkBuffer                constantsBuffer = nullptr;                              // Constants structured buffer (device)

            // Provide these resources if you use UploadDDGIVolumeConstants() to transfer constants to the GPU
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
        RTXGI_API VkFormat GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType type, EDDGIVolumeTextureFormat format);

        /**
         * Get the number of descriptor bindings used by the descriptor set.
         */
        RTXGI_API uint32_t GetDDGIVolumeLayoutBindingCount();

        /**
         * Get the DDGIVolume's descriptor set and pipeline layouts descriptors.
         */
        RTXGI_API void GetDDGIVolumeLayoutDescs(
            VkDescriptorSetLayoutCreateInfo& descriptorSetLayoutCreateInfo,
            VkPushConstantRange& pushConstantRange,
            VkPipelineLayoutCreateInfo& pipelineLayoutCreateInfo,
            VkDescriptorSetLayoutBinding* bindings);

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
        class RTXGI_API DDGIVolume : public DDGIVolumeBase
        {
        public:
            /**
             * Performs other initialization of the DDGIVolume
             * Validates resource pointers or allocates resources if resource management is enabled
             */
        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            ERTXGIStatus Create(VkCommandBuffer cmdBuffer, const DDGIVolumeDesc& desc, const DDGIVolumeResources& resources);
        #else
            ERTXGIStatus Create(const DDGIVolumeDesc& desc, const DDGIVolumeResources& resources);
        #endif

            /**
             * Clears the volume's probe texture arrays
             */
            ERTXGIStatus ClearProbes(VkCommandBuffer cmdBuffer);

            /**
             * Releases resources owned by the volume
             */
            void Destroy();

            //------------------------------------------------------------------------
            // Resource Getters
            //------------------------------------------------------------------------

            // Stats
            uint32_t GetGPUMemoryUsedInBytes() const;

            // Pipeline Layout
            VkPipelineLayout GetPipelineLayout() const { return m_pipelineLayout; }
            bool GetBindlessEnabled() const { return m_bindlessResources.enabled; }

            // Descriptors
            const VkDescriptorSet* GetDescriptorSetConstPtr() const { return &m_descriptorSet; }
            VkDescriptorSet* GetDescriptorSetPtr() { return &m_descriptorSet; }
            VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_descriptorSetLayout; }

            // Push Constants
            uint32_t GetPushConstantsOffset() const { return m_pushConstantsOffset; }
            DDGIRootConstants GetPushConstants() const { return { m_desc.index, 0, 0, 0, 0, 0 }; }

            // Resource Indices (Bindless)
            DDGIVolumeResourceIndices GetResourceIndices() const { return m_bindlessResources.resourceIndices; }
            VkBuffer GetResourceIndicesBuffer() const { return m_bindlessResources.resourceIndicesBuffer; }
            VkBuffer GetResourceIndicesBufferUpload() const { return m_bindlessResources.resourceIndicesBufferUpload; }
            VkDeviceMemory GetResourceIndicesBufferUploadMemory() const { return m_bindlessResources.resourceIndicesBufferUploadMemory; }
            uint64_t GetResourceIndicesBufferSizeInBytes() const { return m_bindlessResources.resourceIndicesBufferSizeInBytes; }

            // Constants
            VkBuffer GetConstantsBuffer() const { return m_constantsBuffer; }
            VkBuffer GetConstantsBufferUpload() const { return m_constantsBufferUpload; }
            VkDeviceMemory GetConstantsBufferUploadMemory() const { return m_constantsBufferUploadMemory; }
            uint64_t GetConstantsBufferSizeInBytes() const { return m_constantsBufferSizeInBytes; }

            // Texture Arrays Format
            EDDGIVolumeTextureFormat GetRayDataFormat() const { return m_desc.probeRayDataFormat; }
            EDDGIVolumeTextureFormat GetIrradianceFormat() const { return m_desc.probeIrradianceFormat; }
            EDDGIVolumeTextureFormat GetDistanceFormat() const { return m_desc.probeDistanceFormat; }
            EDDGIVolumeTextureFormat GetProbeDataFormat() const { return m_desc.probeDataFormat; }
            EDDGIVolumeTextureFormat GetProbeVariabilityFormat() const { return m_desc.probeVariabilityFormat; }

            // Texture Arrays
            VkImage GetProbeRayData() const { return m_probeRayData; }
            VkImage GetProbeIrradiance() const { return m_probeIrradiance; }
            VkImage GetProbeDistance() const { return m_probeDistance; }
            VkImage GetProbeData() const { return m_probeData; }
            VkImage GetProbeVariability() const { return m_probeVariability; }
            VkImage GetProbeVariabilityAverage() const { return m_probeVariabilityAverage; }
            VkBuffer GetProbeVariabilityReadback() const { return m_probeVariabilityReadback; }

            // Texture Array Memory
            VkDeviceMemory GetProbeRayDataMemory() const { return m_probeRayDataMemory; }
            VkDeviceMemory GetProbeIrradianceMemory() const { return m_probeIrradianceMemory; }
            VkDeviceMemory GetProbeDistanceMemory() const { return m_probeDistanceMemory; }
            VkDeviceMemory GetProbeDataMemory() const { return m_probeDataMemory; }
            VkDeviceMemory GetProbeVariabilityMemory() const { return m_probeVariabilityMemory; }
            VkDeviceMemory GetProbeVariabilityAverageMemory() const { return m_probeVariabilityAverageMemory; }
            VkDeviceMemory GetProbeVariabilityReadbackMemory() const { return m_probeVariabilityReadbackMemory; }

            // Texture Array Views
            VkImageView GetProbeRayDataView() const { return m_probeRayDataView; }
            VkImageView GetProbeIrradianceView() const { return m_probeIrradianceView; }
            VkImageView GetProbeDistanceView() const { return m_probeDistanceView; }
            VkImageView GetProbeDataView() const { return m_probeDataView; }
            VkImageView GetProbeVariabilityView() const { return m_probeVariabilityView; }
            VkImageView GetProbeVariabilityAverageView() const { return m_probeVariabilityAverageView; }

            // Shader Modules
            VkShaderModule GetProbeBlendingIrradianceModule() const { return m_probeBlendingIrradianceModule; }
            VkShaderModule GetProbeBlendingDistanceModule() const { return m_probeBlendingDistanceModule; }
            VkShaderModule GetProbeRelocationModule() const { return m_probeRelocationModule; }
            VkShaderModule GetProbeRelocationResetModule() const { return m_probeRelocationResetModule; }
            VkShaderModule GetProbeClassificationModule() const { return m_probeClassificationModule; }
            VkShaderModule GetProbeClassificationResetModule() const { return m_probeClassificationResetModule; }
            VkShaderModule GetProbeVariabilityReductionModule() const { return m_probeVariabilityReductionModule; }
            VkShaderModule GetProbeVariabilityExtraReductionModule() const { return m_probeVariabilityExtraReductionModule; }

            // Pipelines
            VkPipeline GetProbeBlendingIrradiancePipeline() const { return m_probeBlendingIrradiancePipeline; }
            VkPipeline GetProbeBlendingDistancePipeline() const { return m_probeBlendingDistancePipeline; }
            VkPipeline GetProbeRelocationPipeline() const { return m_probeRelocationPipeline; }
            VkPipeline GetProbeRelocationResetPipeline() const { return m_probeRelocationResetPipeline; }
            VkPipeline GetProbeClassificationPipeline() const { return m_probeClassificationPipeline; }
            VkPipeline GetProbeClassificationResetPipeline() const { return m_probeClassificationResetPipeline; }
            VkPipeline GetProbeVariabilityReductionPipeline() const { return m_probeVariabilityReductionPipeline; }
            VkPipeline GetProbeVariabilityExtraReductionPipeline() const { return m_probeVariabilityExtraReductionPipeline; }

            //------------------------------------------------------------------------
            // Resource Setters
            //------------------------------------------------------------------------

            // Push Constants
            void SetPushConstantsOffset(uint32_t offset) { m_pushConstantsOffset = offset; }

            // Resource Indices (Bindless)
            void SetResourceIndices(DDGIVolumeResourceIndices resourceIndices) { m_bindlessResources.resourceIndices = resourceIndices; }
            void SetResourceIndicesBuffer(VkBuffer ptr) { m_bindlessResources.resourceIndicesBuffer = ptr; }
            void SetResourceIndicesBufferUpload(VkBuffer ptr) { m_bindlessResources.resourceIndicesBufferUpload = ptr; }
            void SetResourceIndicesBufferUploadMemory(VkDeviceMemory ptr) { m_bindlessResources.resourceIndicesBufferUploadMemory = ptr; }
            void SetResourceIndicesBufferSizeInBytes(uint64_t size) { m_bindlessResources.resourceIndicesBufferSizeInBytes = size; }

            // Constants
            void SetConstantsBuffer(VkBuffer ptr) { m_constantsBuffer = ptr; }
            void SetConstantsBufferUpload(VkBuffer ptr) { m_constantsBufferUpload = ptr; }
            void SetConstantsBufferUploadMemory(VkDeviceMemory ptr) { m_constantsBufferUploadMemory = ptr; }
            void SetConstantsBufferSizeInBytes(uint64_t value) { m_constantsBufferSizeInBytes = value; }

            // Texture Array Format
            void SetRayDataFormat(EDDGIVolumeTextureFormat format) { m_desc.probeRayDataFormat = format; }
            void SetIrradianceFormat(EDDGIVolumeTextureFormat format) { m_desc.probeIrradianceFormat = format; }
            void SetDistanceFormat(EDDGIVolumeTextureFormat format) { m_desc.probeDistanceFormat = format; }
            void SetProbeDataFormat(EDDGIVolumeTextureFormat format) { m_desc.probeDataFormat = format; }
            void SetProbeVariabilityFromat(EDDGIVolumeTextureFormat format) { m_desc.probeVariabilityFormat = format; }

        #if !RTXGI_DDGI_RESOURCE_MANAGEMENT
            void SetProbeRayData(VkImage ptr, VkDeviceMemory memoryPtr, VkImageView viewPtr) { m_probeRayData = ptr; m_probeRayDataMemory = memoryPtr; m_probeRayDataView = viewPtr; }
            void SetProbeIrradiance(VkImage ptr, VkDeviceMemory memoryPtr, VkImageView viewPtr) { m_probeIrradiance = ptr; m_probeIrradianceMemory = memoryPtr; m_probeIrradianceView = viewPtr; }
            void SetProbeDistance(VkImage ptr, VkDeviceMemory memoryPtr, VkImageView viewPtr) { m_probeDistance = ptr; m_probeDistanceMemory = memoryPtr; m_probeDistanceView = viewPtr; }
            void SetProbeData(VkImage ptr, VkDeviceMemory memoryPtr, VkImageView viewPtr) { m_probeData = ptr; m_probeDataMemory = memoryPtr; m_probeDataView = viewPtr; }
            void SetProbeVariability(VkImage ptr, VkDeviceMemory memoryPtr, VkImageView viewPtr) { m_probeVariability = ptr; m_probeVariabilityMemory = memoryPtr; m_probeVariabilityView = viewPtr; }
            void SetProbeVariabilityAverage(VkImage ptr, VkDeviceMemory memoryPtr, VkImageView viewPtr) { m_probeVariabilityAverage = ptr; m_probeVariabilityAverageMemory = memoryPtr; m_probeVariabilityAverageView = viewPtr; }
            void SetProbeVariabilityReadback(VkBuffer ptr, VkDeviceMemory memoryPtr) { m_probeVariabilityReadback = ptr; m_probeVariabilityReadbackMemory = memoryPtr; }
        #endif

        private:

        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            VkDevice                        m_device = nullptr;                                 // Vulkan device handle
            VkPhysicalDevice                m_physicalDevice = nullptr;                         // Vulkan physical device handle
            VkDescriptorPool                m_descriptorPool = nullptr;                         // Vulkan descriptor pool handle
        #endif

            // Volume Constants (if you use UploadDDGIVolumeConstants() to transfer constants to the GPU)
            VkBuffer                        m_constantsBuffer = nullptr;                        // Structured buffer that stores the volume's constants (device)
            VkBuffer                        m_constantsBufferUpload = nullptr;                  // Structured buffer that stores the volume's constants (upload)
            VkDeviceMemory                  m_constantsBufferUploadMemory = nullptr;            // Memory for the volume's constants upload structured buffer
            uint64_t                        m_constantsBufferSizeInBytes = 0;                   // Size (in bytes) of the structured buffer that stores constants for *all* volumes

            // Texture Arrays
            VkImage                         m_probeRayData = nullptr;                           // Probe ray data texture array - RGB: radiance | A: hit distance
            VkImage                         m_probeIrradiance = nullptr;                        // Probe irradiance texture array - RGB: irradiance, encoded with a high gamma curve
            VkImage                         m_probeDistance = nullptr;                          // Probe distance texture array - R: mean distance | G: mean distance^2
            VkImage                         m_probeData = nullptr;                              // Probe data texture array - XYZ: world-space relocation offsets | W: classification state
            VkImage                         m_probeVariability = nullptr;                       // Probe variability texture
            VkImage                         m_probeVariabilityAverage = nullptr;                // Probe variability average texture
            VkBuffer                        m_probeVariabilityReadback = nullptr;               // Probe variability readback texture

            // Texture Array Memory
            VkDeviceMemory                  m_probeRayDataMemory = nullptr;                     // Probe ray data memory
            VkDeviceMemory                  m_probeIrradianceMemory = nullptr;                  // Probe irradiance memory
            VkDeviceMemory                  m_probeDistanceMemory = nullptr;                    // Probe distance memory
            VkDeviceMemory                  m_probeDataMemory = nullptr;                        // Probe data memory
            VkDeviceMemory                  m_probeVariabilityMemory = nullptr;                 // Probe variability memory
            VkDeviceMemory                  m_probeVariabilityAverageMemory = nullptr;          // Probe variability average memory
            VkDeviceMemory                  m_probeVariabilityReadbackMemory = nullptr;         // Probe variability readback memory

            // Texture Array Views
            VkImageView                     m_probeRayDataView = nullptr;                       // Probe ray data view
            VkImageView                     m_probeIrradianceView = nullptr;                    // Probe irradiance view
            VkImageView                     m_probeDistanceView = nullptr;                      // Probe distance view
            VkImageView                     m_probeDataView = nullptr;                          // Probe data view
            VkImageView                     m_probeVariabilityView = nullptr;                   // Probe variability view
            VkImageView                     m_probeVariabilityAverageView = nullptr;            // Probe variability average view

            // Pipeline Layout
            VkPipelineLayout                m_pipelineLayout = nullptr;                         // Pipeline layout, used for all update compute shaders

            // Descriptors
            VkDescriptorSet                 m_descriptorSet = nullptr;                          // Descriptor set
            VkDescriptorSetLayout           m_descriptorSetLayout = nullptr;                    // Descriptor set layout

            // Push Constants
            uint32_t                        m_pushConstantsOffset = 0;                          // Offset in the push constants block to DDGIRootConstants

            // Bindless
            DDGIVolumeBindlessResourcesDesc m_bindlessResources = {};                           // Properties associated with bindless resources

            // Shader Modules
            VkShaderModule                  m_probeBlendingIrradianceModule = nullptr;          // Probe blending (irradiance) shader module
            VkShaderModule                  m_probeBlendingDistanceModule = nullptr;            // Probe blending (distance) shader module
            VkShaderModule                  m_probeRelocationModule = nullptr;                  // Probe relocation shader module
            VkShaderModule                  m_probeRelocationResetModule = nullptr;             // Probe relocation reset shader module
            VkShaderModule                  m_probeClassificationModule = nullptr;              // Probe classification shader module
            VkShaderModule                  m_probeClassificationResetModule = nullptr;         // Probe classification reset shader module
            VkShaderModule                  m_probeVariabilityReductionModule = nullptr;        // Probe variability reduction shader module
            VkShaderModule                  m_probeVariabilityExtraReductionModule = nullptr;   // Probe variability reduction extra passes shader module

            // Pipelines
            VkPipeline                      m_probeBlendingIrradiancePipeline = nullptr;         // Probe blending (irradiance) compute shader pipeline
            VkPipeline                      m_probeBlendingDistancePipeline = nullptr;           // Probe blending (distance) compute shader pipeline
            VkPipeline                      m_probeRelocationPipeline = nullptr;                 // Probe relocation compute shader pipeline
            VkPipeline                      m_probeRelocationResetPipeline = nullptr;            // Probe relocation reset compute shader pipeline
            VkPipeline                      m_probeClassificationPipeline = nullptr;             // Probe classification compute shader pipeline
            VkPipeline                      m_probeClassificationResetPipeline = nullptr;        // Probe classification reset compute shader pipeline
            VkPipeline                      m_probeVariabilityReductionPipeline = nullptr;       // Probe variability reduction compute shader pipeline
            VkPipeline                      m_probeVariabilityExtraReductionPipeline = nullptr;  // Probe variability reduction extra passes compute shader pipeline

        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            ERTXGIStatus CreateManagedResources(const DDGIVolumeDesc& desc, const DDGIVolumeManagedResourcesDesc& managed);
            void ReleaseManagedResources();

            void Transition(VkCommandBuffer cmdBuffer);
            bool AllocateMemory(VkMemoryRequirements reqs, VkMemoryPropertyFlags props, VkMemoryAllocateFlags flags, VkDeviceMemory* memory);

            bool CreateDescriptorSet();
            bool CreateLayouts();
            bool CreateComputePipeline(ShaderBytecode shader, const char* entryPoint, VkShaderModule* module, VkPipeline* pipeline, const char* debugName);
            bool CreateTexture(uint32_t width, uint32_t height, uint32_t arraySize, VkFormat format, VkImageUsageFlags usage, VkImage* image, VkDeviceMemory* imageMemory, VkImageView* imageView);
            bool CreateProbeRayData(const DDGIVolumeDesc& desc);
            bool CreateProbeIrradiance(const DDGIVolumeDesc& desc);
            bool CreateProbeDistance(const DDGIVolumeDesc& desc);
            bool CreateProbeData(const DDGIVolumeDesc& desc);
            bool CreateProbeVariability(const DDGIVolumeDesc& desc);
            bool CreateProbeVariabilityAverage(const DDGIVolumeDesc& desc);

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
         * Uploads resource indices for one or more volumes to the GPU.
         * This function is for convenience and isn't necessary if you upload volume resource indices yourself.
         */
        RTXGI_API ERTXGIStatus UploadDDGIVolumeResourceIndices(VkDevice device, VkCommandBuffer cmdBuffer, uint32_t bufferingIndex, uint32_t numVolumes, DDGIVolume** volumes);

        /**
         * Uploads constants for one or more volumes to the GPU.
         * This function is for convenience and isn't necessary if you upload volume constants yourself.
         */
        RTXGI_API ERTXGIStatus UploadDDGIVolumeConstants(VkDevice device, VkCommandBuffer cmdBuffer, uint32_t bufferingIndex, uint32_t numVolumes, DDGIVolume** volumes);

        /**
         * Updates one or more volume's probes using data in the volume's radiance texture.
         * Probe blending and border update workloads are batched together for better performance.
         */
        RTXGI_API ERTXGIStatus UpdateDDGIVolumeProbes(VkCommandBuffer cmdBuffer, uint32_t numVolumes, DDGIVolume** volumes);

        /**
         * Adjusts one or more volume's world-space probe positions to avoid them being too close to or inside of geometry.
         * If a volume has the reset flag set, all probe relocation offsets are set to zero before relocation occurs.
         */
        RTXGI_API ERTXGIStatus RelocateDDGIVolumeProbes(VkCommandBuffer cmdBuffer, uint32_t numVolumes, DDGIVolume** volumes);

        /**
         * Classifies one or more volume's probes as active or inactive based on the hit distance data in the ray data texture.
         * If a volume has the reset flag set, all probes are set to active before classification occurs.
         */
        RTXGI_API ERTXGIStatus ClassifyDDGIVolumeProbes(VkCommandBuffer cmdBuffer, uint32_t numVolumes, DDGIVolume** volumes);

        /**
         * Calculates average variability for all probes in each provided volume
         */
        RTXGI_API ERTXGIStatus CalculateDDGIVolumeVariability(VkCommandBuffer cmdBuffer, uint32_t numVolumes, DDGIVolume** volumes);

        /**
         * Reads back average variability for each provided volume, at the time of the call
         */
        RTXGI_API ERTXGIStatus ReadbackDDGIVolumeVariability(VkDevice device, uint32_t numVolumes, DDGIVolume** volumes);
    } // namespace vulkan
} // namespace rtxgi
