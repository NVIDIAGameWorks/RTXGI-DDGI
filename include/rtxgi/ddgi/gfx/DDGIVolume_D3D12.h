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

            ID3D12Device*                device = nullptr;                                   // D3D12 device pointer

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

        struct DDGIVolumeUnmanagedResourcesDesc
        {
            bool                        enabled = false;                                    // Enable or disable unmanaged resources mode

            ID3D12RootSignature*        rootSignature = nullptr;                            // Root signature for the shaders

            uint32_t                    rootParamSlotRootConstants = 0;                     // Root signature root parameter slot of the root constants. Set by GetDDGIVolumeRootSignatureDesc() in Managed Resource Mode.
            uint32_t                    rootParamSlotDescriptorTable = 0;                   // Root signature root parameter slot of the descriptor table. Set by GetDDGIVolumeRootSignatureDesc() in Managed Resource Mode.

            D3D12_CPU_DESCRIPTOR_HANDLE probeIrradianceRTV = { 0 };                         // Probe irradiance render target view
            D3D12_CPU_DESCRIPTOR_HANDLE probeDistanceRTV = { 0 };                           // Probe distance render target view

            // Texture resources
            ID3D12Resource*             probeRayData = nullptr;                             // Probe ray data, radiance (RGB) and hit distance (A) texture
            ID3D12Resource*             probeIrradiance = nullptr;                          // Probe irradiance texture, encoded with a high gamma curve
            ID3D12Resource*             probeDistance = nullptr;                            // Probe distance texture, R: mean distance, G: mean distance^2
            ID3D12Resource*             probeData = nullptr;                                // Probe relocation world-space offsets (XYZ) and classification (W) states texture

            // Pipeline State Objects
            ID3D12PipelineState*        probeBlendingIrradiancePSO = nullptr;               // Probe blending (irradiance) compute PSO
            ID3D12PipelineState*        probeBlendingDistancePSO = nullptr;                 // Probe blending (distance) compute PSO
            ID3D12PipelineState*        probeBorderRowUpdateIrradiancePSO = nullptr;        // Probe border row update (irradiance) compute PSO
            ID3D12PipelineState*        probeBorderRowUpdateDistancePSO = nullptr;          // Probe border row update (distance) compute PSO
            ID3D12PipelineState*        probeBorderColumnUpdateIrradiancePSO = nullptr;     // Probe border column update (irradiance) compute PSO
            ID3D12PipelineState*        probeBorderColumnUpdateDistancePSO = nullptr;       // Probe column update (distance) border compute PSO

            ProbeRelocationPSO          probeRelocation;                                    // [Optional] Probe Relocation PSOs
            ProbeClassificationPSO      probeClassification;                                // [Optional] Probe Classification PSOs
        };

        //------------------------------------------------------------------------

        struct DDGIVolumeDescriptorHeapDesc
        {
            ID3D12DescriptorHeap*       heap = nullptr;                                     // Descriptor heap pointer

            uint32_t                    constsOffset = 0;                                   // Offset to the constants structured buffer SRV on the descriptor heap
            uint32_t                    uavOffset = 0;                                      // Offset to the first descriptor heap slot for the volume's UAVs
            uint32_t                    srvOffset = 0;                                      // Offset to the first descriptor heap slot for the volume's SRVs
        };

        struct DDGIVolumeBindlessDescriptorDesc
        {
            uint32_t                    uavOffset = 0;                                      // Offset to the ray data UAV in a bindless RWTex2D descriptor range. Ignored when shaders are not in bindless mode.
            uint32_t                    srvOffset = 0;                                      // Offset to the ray data SRV in a bindless Tex2D descriptor range. Ignored when shaders are not in bindless mode.
        };

        /**
         * Specifies the resources used by the DDGIVolume.
         */
        struct DDGIVolumeResources
        {
            DDGIVolumeDescriptorHeapDesc      descriptorHeapDesc;                           // Provides a descriptor heap handle and heap offsets/indices for volume resources.
            DDGIVolumeBindlessDescriptorDesc  descriptorBindlessDesc;                       // Provides offsets to the volume's resources in bindless resource arrays.
            DDGIVolumeManagedResourcesDesc    managed;                                      // [Managed Resource Mode] Provides a D3D12 device handle and compiled shader bytecode.
            DDGIVolumeUnmanagedResourcesDesc  unmanaged;                                    // [Unmanaged Resource Mode] Provides a root signature handle and pointers to texture resources and PSOs.

            ID3D12Resource*                   constantsBuffer = nullptr;                    // Constants structured buffer resource pointer (device)

            // Provide these resources if you use UploadDDGIVolumeConstants() to transfer constants to the GPU
            ID3D12Resource*                   constantsBufferUpload = nullptr;              // [Optional] Constants structured buffer resource pointer (upload)
            UINT64                            constantsBufferSizeInBytes = 0;               // [Optional] Size (in bytes) of the constants structured buffer
        };

        //------------------------------------------------------------------------
        // Public RTXGI D3D12 namespace functions
        //------------------------------------------------------------------------

        /**
         * Get the DXGI_FORMAT type of the given texture resource.
         */
        DXGI_FORMAT GetDDGIVolumeTextureFormat(EDDGIVolumeTextureType type, UINT format);

        /**
         * Get the root signature descriptor for a DDGIVolume (when not using bindless resource access).
         */
        bool GetDDGIVolumeRootSignatureDesc(UINT constsOffset, UINT uavOffset, ID3DBlob*& signature);

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
             * Performs initialization of the DDGIVolume.
             * Validates resource pointers or allocates resources if resource management is enabled.
             */
            ERTXGIStatus Create(const DDGIVolumeDesc& desc, const DDGIVolumeResources& resources);

            /**
             * Clears the volume's probe texture atlases.
             */
            ERTXGIStatus ClearProbes(ID3D12GraphicsCommandList* cmdList);

            /**
             * Releases resources owned by the volume.
             */
            void Destroy();

            //------------------------------------------------------------------------
            // Resource Getters
            //------------------------------------------------------------------------

            // Root Signature
            ID3D12RootSignature* GetRootSignature() const { return m_rootSignature; }
            UINT GetRootParamSlotRootConstants() const { return m_rootParamSlotRootConstants; };
            UINT GetRootParamSlotDescriptorTable() const { return m_rootParamSlotDescriptorTable; }

            // Descriptors
            ID3D12DescriptorHeap* GetDescriptorHeap() const { return m_descriptorHeap; }
            UINT GetDescriptorHeapConstsOffset() const { return m_descriptorHeapConstsOffset; }
            UINT GetDescriptorHeapUAVOffset() const { return m_descriptorHeapUAVOffset; }
            UINT GetDescriptorHeapSRVOffset() const { return m_descriptorHeapSRVOffset; }
            UINT GetDescriptorBindlessUAVOffset() const { return m_descriptorBindlessUAVOffset; }
            UINT GetDescriptorBindlessSRVOffset() const { return m_descriptorBindlessSRVOffset; }

            // Constants
            ID3D12Resource* GetConstantsBuffer() const { return m_constantsBuffer; }
            ID3D12Resource* GetConstantsBufferUpload() const { return m_constantsBufferUpload; }
            UINT64 GetConstantsBufferSizeInBytes() const { return m_constantsBufferSizeInBytes; }

            // Textures
            uint32_t GetRayDataFormat() const { return m_desc.probeRayDataFormat; }
            uint32_t GetIrradianceFormat() const { return m_desc.probeIrradianceFormat; }
            uint32_t GetDistanceFormat() const { return m_desc.probeDistanceFormat; }
            uint32_t GetProbeDataFormat() const { return m_desc.probeDataFormat; }

            ID3D12Resource* GetProbeRayData() const { return m_probeRayData; }
            ID3D12Resource* GetProbeIrradiance() const { return m_probeIrradiance; }
            ID3D12Resource* GetProbeDistance() const { return m_probeDistance; }
            ID3D12Resource* GetProbeData() const { return m_probeData; }

            // Pipeline State Objects
            ID3D12PipelineState* GetProbeBlendingIrradiancePSO() const { return m_probeBlendingIrradiancePSO; }
            ID3D12PipelineState* GetProbeBlendingDistancePSO() const { return m_probeBlendingDistancePSO; }
            ID3D12PipelineState* GetProbeBorderRowUpdateIrradiancePSO() const { return m_probeBorderRowUpdateIrradiancePSO; }
            ID3D12PipelineState* GetProbeBorderColumnUpdateIrradiancePSO() const { return m_probeBorderColumnUpdateIrradiancePSO; }
            ID3D12PipelineState* GetProbeBorderRowUpdateDistancePSO() const { return m_probeBorderRowUpdateDistancePSO; }
            ID3D12PipelineState* GetProbeBorderColumnUpdateDistancePSO() const { return m_probeBorderColumnUpdateDistancePSO; }
            ID3D12PipelineState* GetProbeRelocationPSO() const { return m_probeRelocationPSO; }
            ID3D12PipelineState* GetProbeRelocationResetPSO() const { return m_probeRelocationResetPSO; }
            ID3D12PipelineState* GetProbeClassificationPSO() const { return m_probeClassificationPSO; }
            ID3D12PipelineState* GetProbeClassificationResetPSO() const { return m_probeClassificationResetPSO; }

            //------------------------------------------------------------------------
            // Resource Setters
            //------------------------------------------------------------------------

            // Root Signature
            void SetRootSignature(ID3D12RootSignature* ptr) { m_rootSignature = ptr; }

            // Descriptors
            void SetDescriptorHeap(ID3D12DescriptorHeap* ptr) { m_descriptorHeap = ptr; }
            void SetDescriptorHeapDescSize(UINT size) { m_descriptorHeapDescSize = size; }
            void SetDescriptorHeapConstsOffset(UINT offset) { m_descriptorHeapConstsOffset = offset; }
            void SetDescriptorHeapUAVOffset(UINT offset) { m_descriptorHeapUAVOffset = offset; }
            void SetDescriptorHeapSRVOffset(UINT offset) { m_descriptorHeapSRVOffset = offset; }
            void SetDescriptorBindlessUAVOffset(UINT offset) { m_descriptorBindlessUAVOffset = offset; }
            void SetDescriptorBindlessSRVOffset(UINT offset) { m_descriptorBindlessSRVOffset = offset; }

            // Constants
            void SetConstantsBuffer(ID3D12Resource* ptr) { m_constantsBuffer = ptr; }
            void SetConstantsBufferUpload(ID3D12Resource* ptr) { m_constantsBufferUpload = ptr; }
            void SetConstantsBufferSizeInBytes(UINT64 value) { m_constantsBufferSizeInBytes = value; }

            // Textures
        #if !RTXGI_DDGI_RESOURCE_MANAGEMENT
            void SetProbeRayData(ID3D12Resource* ptr) { m_probeRayData = ptr; }
            void SetProbeIrradiance(ID3D12Resource* ptr) { m_probeIrradiance = ptr; }
            void SetProbeDistance(ID3D12Resource* ptr) { m_probeDistance = ptr; }
            void SetProbeData(ID3D12Resource* ptr) { m_probeData = ptr; }
        #endif

        private:

        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            ID3D12Device*               m_device = nullptr;                                 // D3D12 device pointer
        #endif

            // Constants (if you use UploadDDGIVolumeConstants() to transfer constants to the GPU)
            ID3D12Resource*             m_constantsBuffer = nullptr;                        // Structured buffer that stores the volume's constants (device)
            ID3D12Resource*             m_constantsBufferUpload = nullptr;                  // Structured buffer that stores the volume's constants (upload)
            UINT64                      m_constantsBufferSizeInBytes = 0;                   // Size (in bytes) of the structured buffer that stores *all* volumes constants

            // Textures
            ID3D12Resource*             m_probeRayData = nullptr;                           // Probe ray data, radiance (RGB) and hit distance (A) texture
            ID3D12Resource*             m_probeIrradiance = nullptr;                        // Probe irradiance texture, encoded with a high gamma curve
            ID3D12Resource*             m_probeDistance = nullptr;                          // Probe distance texture, R: mean distance, G: mean distance^2
            ID3D12Resource*             m_probeData = nullptr;                              // Probe relocation world-space offsets (XYZ) and classification (W) states texture

            // Render Target Views
            D3D12_CPU_DESCRIPTOR_HANDLE m_probeIrradianceRTV = { 0 };                       // Probe irradiance render target view
            D3D12_CPU_DESCRIPTOR_HANDLE m_probeDistanceRTV = { 0 };                         // Probe distance render target view

            // Root Signature
            ID3D12RootSignature*        m_rootSignature = nullptr;                          // Root signature to be used with the shaders
            UINT                        m_rootParamSlotRootConstants = 0;                   // Root signature root parameter slot of the volume's root constants
            UINT                        m_rootParamSlotDescriptorTable = 0;                 // Root signature root parameter slot of the descriptor table

            // Descriptors
            ID3D12DescriptorHeap*       m_descriptorHeap = nullptr;                         // Descriptor heap
            UINT                        m_descriptorHeapDescSize = 0;                       // Size of each descriptor heap entry
            UINT                        m_descriptorHeapConstsOffset = 0;                   // Offset to the constants structured buffer SRV on the descriptor heap
            UINT                        m_descriptorHeapUAVOffset = 0;                      // Offset to the first descriptor heap slot for volume UAVs
            UINT                        m_descriptorHeapSRVOffset = 0;                      // Offset to the first descriptor heap slot for volume SRVs

            UINT                        m_descriptorBindlessUAVOffset = 0;                  // Offset to the ray data UAV in the RWTex2D descriptor range
            UINT                        m_descriptorBindlessSRVOffset = 0;                  // Offset to the ray data SRV in the Tex2D descriptor range

            // Pipeline State Objects
            ID3D12PipelineState*        m_probeBlendingIrradiancePSO = nullptr;             // Probe blending (irradiance) compute shader pipeline state object
            ID3D12PipelineState*        m_probeBlendingDistancePSO = nullptr;               // Probe blending (distance) compute shader pipeline state object
            ID3D12PipelineState*        m_probeBorderRowUpdateIrradiancePSO = nullptr;      // Probe border row update (irradiance) compute shader pipeline state object
            ID3D12PipelineState*        m_probeBorderRowUpdateDistancePSO = nullptr;        // Probe border row update (distance) compute shader pipeline state object
            ID3D12PipelineState*        m_probeBorderColumnUpdateIrradiancePSO = nullptr;   // Probe border column update (irradiance) compute shader pipeline state object
            ID3D12PipelineState*        m_probeBorderColumnUpdateDistancePSO = nullptr;     // Probe border column update (distance) compute shader pipeline state object

            ID3D12PipelineState*        m_probeRelocationPSO = nullptr;                     // Probe relocation compute shader pipeline state object
            ID3D12PipelineState*        m_probeRelocationResetPSO = nullptr;                // Probe relocation reset compute shader pipeline state object

            ID3D12PipelineState*        m_probeClassificationPSO = nullptr;                 // Probe classification compute shader pipeline state object
            ID3D12PipelineState*        m_probeClassificationResetPSO = nullptr;            // Probe classification reset compute shader pipeline state object

        #if RTXGI_DDGI_RESOURCE_MANAGEMENT
            ID3D12DescriptorHeap*       m_rtvDescriptorHeap = nullptr;                      // Descriptor heap for render target views

            ERTXGIStatus CreateManagedResources(const DDGIVolumeDesc& desc, const DDGIVolumeManagedResourcesDesc& managed);
            void ReleaseManagedResources();

            bool CreateDescriptors();
            bool CreateRootSignature();
            bool CreateComputePSO(ShaderBytecode shader, ID3D12PipelineState** pipeline, std::wstring debugName);
            bool CreateTexture(UINT64 width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_STATES state, ID3D12Resource** resource);
            bool CreateProbeRayData(const DDGIVolumeDesc& desc);
            bool CreateProbeIrradiance(const DDGIVolumeDesc& desc);
            bool CreateProbeDistance(const DDGIVolumeDesc& desc);
            bool CreateProbeData(const DDGIVolumeDesc& desc);

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
         * Uploads constants for one or more volumes to the GPU.
         * This function is for convenience and isn't necessary if you upload volume constants yourself.
         */
        ERTXGIStatus UploadDDGIVolumeConstants(ID3D12GraphicsCommandList* cmdList, UINT bufferingIndex, UINT numVolumes, DDGIVolume** volumes);

        /**
         * Updates one or more volume's probes using data in the volume's radiance texture.
         * Probe blending and border update workloads are batched together for better performance.
         */
        ERTXGIStatus UpdateDDGIVolumeProbes(ID3D12GraphicsCommandList* cmdList, UINT numVolumes, DDGIVolume** volumes);

        /**
         * Adjusts one or more volume's world-space probe positions to avoid them being too close to or inside of geometry.
         * If a volume has the reset flag set, all probe relocation offsets are set to zero before relocation occurs.
         */
        ERTXGIStatus RelocateDDGIVolumeProbes(ID3D12GraphicsCommandList* cmdList, UINT numVolumes, DDGIVolume** volumes);

        /**
         * Classifies one or more volume's probes as active or inactive based on the hit distance data in the ray data texture.
         * If a volume has the reset flag set, all probes are set to active before classification occurs.
         */
        ERTXGIStatus ClassifyDDGIVolumeProbes(ID3D12GraphicsCommandList* cmdList, UINT numVolumes, DDGIVolume** volumes);

    } // namespace d3d12
} // namespace rtxgi
