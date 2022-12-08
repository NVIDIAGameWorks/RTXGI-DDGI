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

#include <d3d12.h>

namespace rtxgi
{
    namespace d3d12
    {

        enum class EBindlessType
        {
            RESOURCE_ARRAYS = 0,  // Shader Model 6.5 and below bindless
            DESCRIPTOR_HEAP,      // Shader Model 6.6+ bindless
            COUNT
        };

        enum class EResourceViewType
        {
            UAV = 0,
            SRV,
            COUNT
        };

        enum class EDDGIExecutionStage
        {
            POST_PROBE_TRACE = 0,
            PRE_GATHER_CS,
            PRE_GATHER_PS,
            POST_GATHER_PS,
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

            ID3D12Device*                device = nullptr;                                   // D3D12 device pointer

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

        struct ProbeRelocationPSO
        {
            ID3D12PipelineState*        updatePSO = nullptr;                                // Probe relocation compute PSO
            ID3D12PipelineState*        resetPSO = nullptr;                                 // Probe relocation reset compute PSO
        };

        struct ProbeClassificationPSO
        {
            ID3D12PipelineState*        updatePSO = nullptr;                                // Probe classification compute PSO
            ID3D12PipelineState*        resetPSO = nullptr;                                 // Probe classification reset compute PSO
        };

        struct ProbeVariabilityPSO
        {
            ID3D12PipelineState*        reductionPSO = nullptr;                             // Probe variability averaging PSO
            ID3D12PipelineState*        extraReductionPSO = nullptr;                        // Probe variability extra reduction PSO
        };

        struct DDGIVolumeUnmanagedResourcesDesc
        {
            bool                        enabled = false;                                    // Enable or disable unmanaged resources mode

            ID3D12RootSignature*        rootSignature = nullptr;                            // Root signature for the shaders

            UINT                        rootParamSlotRootConstants = 0;                     // Root signature root parameter slot of the root constants. Set by GetDDGIVolumeRootSignatureDesc() in Managed Resource Mode.
            UINT                        rootParamSlotResourceDescriptorTable = 0;           // Root signature root parameter slot of the resources descriptor table. Set by GetDDGIVolumeRootSignatureDesc() in Managed Resource Mode.
            UINT                        rootParamSlotSamplerDescriptorTable = 0;            // [Optional] Root signature root parameter slot of the sampler descriptor table. Set by GetDDGIVolumeRootSignatureDesc() in Managed Resource Mode.

            D3D12_CPU_DESCRIPTOR_HANDLE probeIrradianceRTV = { 0 };                         // Probe irradiance render target view
            D3D12_CPU_DESCRIPTOR_HANDLE probeDistanceRTV = { 0 };                           // Probe distance render target view

            // Texture Resources
            ID3D12Resource*             probeRayData = nullptr;                             // Probe ray data texture array - RGB: radiance | A: hit distance
            ID3D12Resource*             probeIrradiance = nullptr;                          // Probe irradiance texture array - RGB irradiance, encoded with a high gamma curve
            ID3D12Resource*             probeDistance = nullptr;                            // Probe distance texture array - R: mean distance | G: mean distance^2
            ID3D12Resource*             probeData = nullptr;                                // Probe data texture array - XYZ: world-space relocation offsets | W: classification state
            ID3D12Resource*             probeVariability = nullptr;                         // Probe variability texture array
            ID3D12Resource*             probeVariabilityAverage = nullptr;                  // Average of Probe variability for whole volume
            ID3D12Resource*             probeVariabilityReadback = nullptr;                 // CPU-readable resource containing final Probe variability average

            // Pipeline State Objects
            ID3D12PipelineState*        probeBlendingIrradiancePSO = nullptr;               // Probe blending (irradiance) compute PSO
            ID3D12PipelineState*        probeBlendingDistancePSO = nullptr;                 // Probe blending (distance) compute PSO

            ProbeRelocationPSO          probeRelocation;                                    // Probe Relocation PSOs
            ProbeClassificationPSO      probeClassification;                                // Probe Classification PSOs
            ProbeVariabilityPSO         probeVariabilityPSOs;                               // Probe Variability PSOs
        };

        //------------------------------------------------------------------------

        struct DDGIVolumeDescriptorHeapDesc
        {
            ID3D12DescriptorHeap*             resources = nullptr;                          // Resource descriptor heap pointer
            ID3D12DescriptorHeap*             samplers = nullptr;                           // [Optional] Sampler descriptor heap pointer (for when your root signature requires a sampler heap)

            UINT                              entrySize;                                    // Size (in bytes) of a descriptor heap entry

            UINT                              constantsIndex;                               // Index of the volume constants structured buffer on the descriptor heap
            UINT                              resourceIndicesIndex;                         // Index of the resource indices structured buffer on the descriptor heap
            DDGIVolumeResourceIndices         resourceIndices;                              // Indices of volume resources on the descriptor heap
        };

        struct DDGIVolumeBindlessResourcesDesc
        {
            bool                              enabled;                                      // Specifies if bindless resources are used
            EBindlessType                     type;                                         // Specifies the type of bindless implementation used

            DDGIVolumeResourceIndices         resourceIndices;                              // Indices of volume resources in bindless resource arrays

            ID3D12Resource*                   resourceIndicesBuffer = nullptr;              // Resource indices structured buffer pointer (device)

            // Provide these resources if you use UploadDDGIVolumeResourceIndices() to transfer volume resource indices to the GPU
            ID3D12Resource*                   resourceIndicesBufferUpload = nullptr;        // [Optional] Resource indices structured buffer resource pointer (upload)
            UINT64                            resourceIndicesBufferSizeInBytes = 0;         // [Optional] Size (in bytes) of the resource indices structured buffer
        };

        /**
         * Specifies the properties of the resources used by the DDGIVolume.
         */
        struct DDGIVolumeResources
        {
            DDGIVolumeDescriptorHeapDesc      descriptorHeap;                               // Provides a pointer to a descriptor heap resource and describes the location of resources on it
            DDGIVolumeBindlessResourcesDesc   bindless;                                     // [Optional] Specifies properties of bindless resources

            DDGIVolumeManagedResourcesDesc    managed;                                      // [Managed Resource Mode] Provides a D3D12 device handle and compiled shader bytecode
            DDGIVolumeUnmanagedResourcesDesc  unmanaged;                                    // [Unmanaged Resource Mode] Provides a root signature handle and pointers to texture resources and PSOs

            ID3D12Resource*                   constantsBuffer = nullptr;                    // Volume constants structured buffer resource pointer (device)

            // Provide these resources if you use UploadDDGIVolumeConstants() to transfer volume constants to the GPU
            ID3D12Resource*                   constantsBufferUpload = nullptr;              // [Optional] Constants structured buffer resource pointer (upload)
            UINT64                            constantsBufferSizeInBytes = 0;               // [Optional] Size (in bytes) of the constants structured buffer
        };

        //------------------------------------------------------------------------
        // Public RTXGI D3D12 namespace functions
        //------------------------------------------------------------------------

        /**
         * Get the DXGI_FORMAT type of the given texture resource.
         */
        RTXGI_API DXGI_FORMAT GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType type, EDDGIVolumeTextureFormat format);

        /**
         * Get the root signature descriptor blob for a DDGIVolume (when not using bindless resources).
         */
        RTXGI_API bool GetDDGIVolumeRootSignatureDesc(const DDGIVolumeDescriptorHeapDesc& heapDesc, ID3DBlob*& signature);

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
             * Performs initialization of the DDGIVolume
             * Validates resource pointers or allocates resources if resource management is enabled
             */
            ERTXGIStatus Create(const DDGIVolumeDesc& desc, const DDGIVolumeResources& resources);

            /**
             * Clears the volume's probe texture arrays
             */
            ERTXGIStatus ClearProbes(ID3D12GraphicsCommandList* cmdList);

            /**
             * Transitions volume resources to the appropriate state(s) for the given execution stage
             */
            void TransitionResources(ID3D12GraphicsCommandList* cmdList, EDDGIExecutionStage stage) const;

            /**
             * Releases resources owned by the volume
             */
            void Destroy();

            //------------------------------------------------------------------------
            // Resource Getters
            //------------------------------------------------------------------------

            // Stats
            UINT GetGPUMemoryUsedInBytes() const;

            // Root Signature
            ID3D12RootSignature* GetRootSignature() const { return m_rootSignature; }
            UINT GetRootParamSlotRootConstants() const { return m_rootParamSlotRootConstants; };
            UINT GetRootParamSlotResourceDescriptorTable() const { return m_rootParamSlotResourceDescriptorTable; }
            UINT GetRootParamSlotSamplerDescriptorTable() const { return m_rootParamSlotSamplerDescriptorTable; }
            DDGIRootConstants GetRootConstants() const { return { m_desc.index, m_descriptorHeapDesc.constantsIndex, m_descriptorHeapDesc.resourceIndicesIndex, 0, 0, 0, 0 }; };
            bool GetBindlessEnabled() const { return m_bindlessResources.enabled; }
            EBindlessType GetBindlessType() const { return m_bindlessResources.type; }

            // Descriptors
            DDGIVolumeDescriptorHeapDesc GetDescriptorHeapDesc() const { return m_descriptorHeapDesc; }
            ID3D12DescriptorHeap* GetResourceDescriptorHeap() const { return m_descriptorHeapDesc.resources; }
            ID3D12DescriptorHeap* GetSamplerDescriptorHeap() const { return m_descriptorHeapDesc.samplers; }
            UINT GetResourceDescriptorHeapEntrySize() const { return m_descriptorHeapDesc.entrySize; }
            UINT GetResourceDescriptorHeapIndex(EDDGIVolumeTextureType type, EResourceViewType view) const;

            // Resource Indices (Bindless)
            DDGIVolumeResourceIndices GetResourceIndices() const;
            ID3D12Resource* GetResourceIndicesBuffer() const { return m_bindlessResources.resourceIndicesBuffer; }
            ID3D12Resource* GetResourceIndicesBufferUpload() const { return m_bindlessResources.resourceIndicesBufferUpload; }
            UINT64 GetResourceIndicesBufferSizeInBytes() const { return m_bindlessResources.resourceIndicesBufferSizeInBytes; }

            // Constants
            ID3D12Resource* GetConstantsBuffer() const { return m_constantsBuffer; }
            ID3D12Resource* GetConstantsBufferUpload() const { return m_constantsBufferUpload; }
            UINT64 GetConstantsBufferSizeInBytes() const { return m_constantsBufferSizeInBytes; }

            // Texture Arrays Format
            EDDGIVolumeTextureFormat GetRayDataFormat() const { return m_desc.probeRayDataFormat; }
            EDDGIVolumeTextureFormat GetIrradianceFormat() const { return m_desc.probeIrradianceFormat; }
            EDDGIVolumeTextureFormat GetDistanceFormat() const { return m_desc.probeDistanceFormat; }
            EDDGIVolumeTextureFormat GetProbeDataFormat() const { return m_desc.probeDataFormat; }
            EDDGIVolumeTextureFormat GetProbeVariabilityFormat() const { return m_desc.probeVariabilityFormat; }

            // Texture Arrays
            ID3D12Resource* GetProbeRayData() const { return m_probeRayData; }
            ID3D12Resource* GetProbeIrradiance() const { return m_probeIrradiance; }
            ID3D12Resource* GetProbeDistance() const { return m_probeDistance; }
            ID3D12Resource* GetProbeData() const { return m_probeData; }
            ID3D12Resource* GetProbeVariability() const { return m_probeVariability; }
            ID3D12Resource* GetProbeVariabilityAverage() const { return m_probeVariabilityAverage; }
            ID3D12Resource* GetProbeVariabilityReadback() const { return m_probeVariabilityReadback; }

            // Pipeline State Objects
            ID3D12PipelineState* GetProbeBlendingIrradiancePSO() const { return m_probeBlendingIrradiancePSO; }
            ID3D12PipelineState* GetProbeBlendingDistancePSO() const { return m_probeBlendingDistancePSO; }
            ID3D12PipelineState* GetProbeRelocationPSO() const { return m_probeRelocationPSO; }
            ID3D12PipelineState* GetProbeRelocationResetPSO() const { return m_probeRelocationResetPSO; }
            ID3D12PipelineState* GetProbeClassificationPSO() const { return m_probeClassificationPSO; }
            ID3D12PipelineState* GetProbeClassificationResetPSO() const { return m_probeClassificationResetPSO; }
            ID3D12PipelineState* GetProbeVariabilityReductionPSO() const { return m_probeVariabilityReductionPSO; }
            ID3D12PipelineState* GetProbeVariabilityExtraReductionPSO() const { return m_probeVariabilityExtraReductionPSO; }

            //------------------------------------------------------------------------
            // Resource Setters
            //------------------------------------------------------------------------

            // Root Signature
            void SetRootSignature(ID3D12RootSignature* ptr) { m_rootSignature = ptr; }
            void SetRootParamSlotRootConstants(UINT slot) { m_rootParamSlotRootConstants = slot; };
            void SetRootParamSlotResourceDescriptorTable(UINT slot) { m_rootParamSlotResourceDescriptorTable = slot; }
            void SetRootParamSlotSamplerDescriptorTable(UINT slot) { m_rootParamSlotSamplerDescriptorTable = slot; }
            void SetBindlessEnabled(bool value) { m_bindlessResources.enabled = value; }
            void SetBindlessType(EBindlessType type) { m_bindlessResources.type = type; }

            // Descriptor Heap
            void SetResourceDescriptorHeap(ID3D12DescriptorHeap* ptr) { m_descriptorHeapDesc.resources = ptr; }
            void SetSamplerDescriptorHeap(ID3D12DescriptorHeap* ptr) { m_descriptorHeapDesc.samplers = ptr; }
            void SetResourceDescriptorHeapEntrySize(UINT size) { m_descriptorHeapDesc.entrySize = size; }
            void SetResourceDescriptorHeapIndex(EDDGIVolumeTextureType type, EResourceViewType view, UINT index);

            // Resource Indices (Bindless)
            void SetResourceIndices(DDGIVolumeResourceIndices resourceIndices) { m_bindlessResources.resourceIndices = resourceIndices; }
            void SetResourceIndicesBuffer(ID3D12Resource* ptr) { m_bindlessResources.resourceIndicesBuffer = ptr; }
            void SetResourceIndicesBufferUpload(ID3D12Resource* ptr) { m_bindlessResources.resourceIndicesBufferUpload = ptr; }
            void SetResourceIndicesBufferSizeInBytes(UINT64 value) { m_bindlessResources.resourceIndicesBufferSizeInBytes = value; }

            // Constants
            void SetConstantsBuffer(ID3D12Resource* ptr) { m_constantsBuffer = ptr; }
            void SetConstantsBufferUpload(ID3D12Resource* ptr) { m_constantsBufferUpload = ptr; }
            void SetConstantsBufferSizeInBytes(UINT64 value) { m_constantsBufferSizeInBytes = value; }

            // Texture Array Format
            void SetRayDataFormat(EDDGIVolumeTextureFormat format) { m_desc.probeRayDataFormat = format; }
            void SetIrradianceFormat(EDDGIVolumeTextureFormat format) { m_desc.probeIrradianceFormat = format; }
            void SetDistanceFormat(EDDGIVolumeTextureFormat format) { m_desc.probeDistanceFormat = format; }
            void SetProbeDataFormat(EDDGIVolumeTextureFormat format) { m_desc.probeDataFormat = format; }
            void SetProbeVariabilityFormat(EDDGIVolumeTextureFormat format) { m_desc.probeVariabilityFormat = format; }

        #if !RTXGI_DDGI_RESOURCE_MANAGEMENT
            void SetProbeRayData(ID3D12Resource* ptr) { m_probeRayData = ptr; }
            void SetProbeIrradiance(ID3D12Resource* ptr) { m_probeIrradiance = ptr; }
            void SetProbeDistance(ID3D12Resource* ptr) { m_probeDistance = ptr; }
            void SetProbeData(ID3D12Resource* ptr) { m_probeData = ptr; }
            void SetProbeVariability(ID3D12Resource* ptr) { m_probeVariability = ptr; }
            void SetProbeVariabilityAverage(ID3D12Resource* ptr) { m_probeVariabilityAverage = ptr; }
        #endif

        private:

        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            ID3D12Device*                   m_device = nullptr;                                 // D3D12 device pointer
        #endif

            // Volume Constants (if you use UploadDDGIVolumeConstants() to transfer constants to the GPU)
            ID3D12Resource*                 m_constantsBuffer = nullptr;                        // Structured buffer that stores the volume's constants (device)
            ID3D12Resource*                 m_constantsBufferUpload = nullptr;                  // Structured buffer that stores the volume's constants (upload)
            UINT64                          m_constantsBufferSizeInBytes = 0;                   // Size (in bytes) of the structured buffer that stores constants for *all* volumes

            // Texture Arrays
            ID3D12Resource*                 m_probeRayData = nullptr;                           // Probe ray data texture array - RGB: radiance | A: hit distance
            ID3D12Resource*                 m_probeIrradiance = nullptr;                        // Probe irradiance texture array - RGB: irradiance, encoded with a high gamma curve
            ID3D12Resource*                 m_probeDistance = nullptr;                          // Probe distance texture array - R: mean distance | G: mean distance^2
            ID3D12Resource*                 m_probeData = nullptr;                              // Probe data texture array - XYZ: world-space relocation offsets | W: classification state
            ID3D12Resource*                 m_probeVariability = nullptr;                       // Probe luminance difference from previous update
            ID3D12Resource*                 m_probeVariabilityAverage = nullptr;                // Average Probe variability for whole volume
            ID3D12Resource*                 m_probeVariabilityReadback = nullptr;               // CPU-readable buffer with average Probe variability

            // Render Target Views
            D3D12_CPU_DESCRIPTOR_HANDLE     m_probeIrradianceRTV = { 0 };                       // Probe irradiance render target view
            D3D12_CPU_DESCRIPTOR_HANDLE     m_probeDistanceRTV = { 0 };                         // Probe distance render target view

            // Root Signature
            ID3D12RootSignature*            m_rootSignature = nullptr;                          // Root signature to be used with the shaders
            UINT                            m_rootParamSlotRootConstants = 0;                   // Root signature root parameter slot of the volume's root constants
            UINT                            m_rootParamSlotResourceDescriptorTable = 0;         // Root signature root parameter slot of the resources descriptor table
            UINT                            m_rootParamSlotSamplerDescriptorTable = 0;          // Root signature root parameter slot of the samplers descriptor table

            // Descriptors
            DDGIVolumeDescriptorHeapDesc    m_descriptorHeapDesc = {};                          // Properties of the descriptor heap

            // Bindless
            DDGIVolumeBindlessResourcesDesc m_bindlessResources = {};                           // Properties associated with bindless resources

            // Pipeline State Objects
            ID3D12PipelineState*            m_probeBlendingIrradiancePSO = nullptr;             // Probe blending (irradiance) compute shader pipeline state object
            ID3D12PipelineState*            m_probeBlendingDistancePSO = nullptr;               // Probe blending (distance) compute shader pipeline state object
            ID3D12PipelineState*            m_probeRelocationPSO = nullptr;                     // Probe relocation compute shader pipeline state object
            ID3D12PipelineState*            m_probeRelocationResetPSO = nullptr;                // Probe relocation reset compute shader pipeline state object
            ID3D12PipelineState*            m_probeClassificationPSO = nullptr;                 // Probe classification compute shader pipeline state object
            ID3D12PipelineState*            m_probeClassificationResetPSO = nullptr;            // Probe classification reset compute shader pipeline state object
            ID3D12PipelineState*            m_probeVariabilityReductionPSO = nullptr;           // Probe variability reduction
            ID3D12PipelineState*            m_probeVariabilityExtraReductionPSO = nullptr;      // Probe variability extra reduction pass

        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            ID3D12DescriptorHeap*           m_rtvDescriptorHeap = nullptr;                      // Descriptor heap for render target views

            ERTXGIStatus CreateManagedResources(const DDGIVolumeDesc& desc, const DDGIVolumeManagedResourcesDesc& managed);
            void ReleaseManagedResources();

            bool CreateDescriptors();
            bool CreateRootSignature();
            bool CreateComputePSO(ShaderBytecode shader, ID3D12PipelineState** pipeline, const char* debugName = nullptr);
            bool CreateTexture(UINT64 width, UINT height, UINT arraySize, DXGI_FORMAT format, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_FLAGS flags, ID3D12Resource** resource);
            bool CreateProbeRayData(const DDGIVolumeDesc& desc);
            bool CreateProbeIrradiance(const DDGIVolumeDesc& desc);
            bool CreateProbeDistance(const DDGIVolumeDesc& desc);
            bool CreateProbeData(const DDGIVolumeDesc& desc);
            bool CreateProbeVariability(const DDGIVolumeDesc& desc);
            bool CreateProbeVariabilityAverage(const DDGIVolumeDesc& desc);

            bool IsDeviceChanged(const DDGIVolumeManagedResourcesDesc& desc)
            {
                // Has the D3D12 device changed?
                if (desc.device != m_device) return true;
                return false;
            }
        #else
            void StoreUnmanagedResourcesDesc(const DDGIVolumeUnmanagedResourcesDesc& unmanaged);
        #endif
        }; // class DDGIVolume

        //------------------------------------------------------------------------
        // Public RTXGI D3D12 namespace DDGIVolume Functions
        //------------------------------------------------------------------------

        /**
         * Uploads resource indices for one or more volumes to the GPU.
         * This function is for convenience and isn't necessary if you upload volume resource indices yourself.
         */
        RTXGI_API ERTXGIStatus UploadDDGIVolumeResourceIndices(ID3D12GraphicsCommandList* cmdList, UINT bufferingIndex, UINT numVolumes, DDGIVolume** volumes);

        /**
         * Uploads constants for one or more volumes to the GPU.
         * This function is for convenience and isn't necessary if you upload volume constants yourself.
         */
        RTXGI_API ERTXGIStatus UploadDDGIVolumeConstants(ID3D12GraphicsCommandList* cmdList, UINT bufferingIndex, UINT numVolumes, DDGIVolume** volumes);

        /**
         * Updates one or more volume's probes using data in the volume's radiance texture.
         * Probe blending and border update workloads are batched together for better performance.
         * Volume resources are expected to be in the D3D12_RESOURCE_STATE_UNORDERED_ACCESS state.
         */
        RTXGI_API ERTXGIStatus UpdateDDGIVolumeProbes(ID3D12GraphicsCommandList* cmdList, UINT numVolumes, DDGIVolume** volumes);

        /**
         * Adjusts one or more volume's world-space probe positions to avoid them being too close to or inside of geometry.
         * If a volume has the reset flag set, all probe relocation offsets are set to zero before relocation occurs.
         * Volume resources are expected to be in the D3D12_RESOURCE_STATE_UNORDERED_ACCESS state.
         */
        RTXGI_API ERTXGIStatus RelocateDDGIVolumeProbes(ID3D12GraphicsCommandList* cmdList, UINT numVolumes, DDGIVolume** volumes);

        /**
         * Classifies one or more volume's probes as active or inactive based on the hit distance data in the ray data texture.
         * If a volume has the reset flag set, all probes are set to active before classification occurs.
         * Volume resources are expected to be in the D3D12_RESOURCE_STATE_UNORDERED_ACCESS state.
         */
        RTXGI_API ERTXGIStatus ClassifyDDGIVolumeProbes(ID3D12GraphicsCommandList* cmdList, UINT numVolumes, DDGIVolume** volumes);

        /**
         * Calculates average variability for all probes in each provided volume
         */
        RTXGI_API ERTXGIStatus CalculateDDGIVolumeVariability(ID3D12GraphicsCommandList* cmdList, UINT numVolumes, DDGIVolume** volumes);

        /**
         * Reads back average variability for each provided volume, at the time of the call
         */
        RTXGI_API ERTXGIStatus ReadbackDDGIVolumeVariability(UINT numVolumes, DDGIVolume** volumes);
    } // namespace d3d12
} // namespace rtxgi
