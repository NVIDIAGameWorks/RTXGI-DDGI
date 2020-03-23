/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include <d3d12.h>
#include <dxcapi.h>
#include <string>

#include "rtxgi/Common.h"
#include "rtxgi/Defines.h"
#include "rtxgi/Math.h"
#include "rtxgi/Types.h"

namespace rtxgi
{
    #include "rtxgi/ddgi/DDGIVolumeDefines.h"
    #include "rtxgi/ddgi/DDGIVolumeDescGPU.h"

    enum class EDDGITextureType
    {
        RTRadiance = 0,
        Irradiance,
        Distance,
        Offsets,
        States,
        Count
    };

    /**
    * Resources provided by the host application to be used by the volume.
    */
    struct DDGIVolumeResources
    {
        ID3D12DescriptorHeap*   descriptorHeap = nullptr;               // Descriptor heap
        int                     descriptorHeapDescSize = 0;             // Size of each entry on the descriptor heap
        int                     descriptorHeapOffset = 0;               // Offset to the first free descriptor heap slot

#if RTXGI_DDGI_SDK_MANAGED_RESOURCES
        ID3D12Device*           device = nullptr;                       // D3D12 device
        ID3DBlob*               probeRadianceBlendingCS = nullptr;      // Probe radiance blending compute shader bytecode
        ID3DBlob*               probeDistanceBlendingCS = nullptr;      // Probe distance blending compute shader bytecode
        ID3DBlob*               probeBorderRowCS = nullptr;             // Probe irradiance or distance border row update compute shader bytecode
        ID3DBlob*               probeBorderColumnCS = nullptr;          // Probe irradiance or distance border column update compute shader bytecode
#if RTXGI_DDGI_PROBE_RELOCATION
        ID3DBlob*               probeRelocationCS = nullptr;            // Probe relocation compute shader bytecode
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        ID3DBlob*               probeStateClassifierCS = nullptr;               // Probe state classifier bytecode
        ID3DBlob*               probeStateClassifierActivateAllCS = nullptr;    // Probe state classifier activate all bytecode
#endif

        bool IsDeviceChanged(DDGIVolumeResources &resources)
        {
            if (resources.device != device) return true;
            return false;
        }

#else   /* !RTXGI_DDGI_SDK_MANAGED_RESOURCES */
        ID3D12RootSignature*    rootSignature = nullptr;                // Root signature for the compute shaders
        ID3D12Resource*         probeRTRadiance = nullptr;              // Probe radiance texture (from ray tracing)
        ID3D12Resource*         probeIrradiance = nullptr;              // Probe irradiance texture, encoded with a high gamma curve
        ID3D12Resource*         probeDistance = nullptr;                // Probe distance texture, R channel is mean distance, G channel is mean distance^2
#if RTXGI_DDGI_PROBE_RELOCATION
        ID3D12Resource*         probeOffsets = nullptr;                 // Probe relocation world-space offsets texture
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        ID3D12Resource*         probeStates = nullptr;                  // Probe state texture
#endif

        ID3D12PipelineState*    probeRadianceBlendingPSO = nullptr;     // Probe radiance blending compute PSO
        ID3D12PipelineState*    probeDistanceBlendingPSO = nullptr;     // Probe distance blending compute PSO
        ID3D12PipelineState*    probeBorderRowPSO = nullptr;            // Probe irradiance or distance border row update compute PSO
        ID3D12PipelineState*    probeBorderColumnPSO = nullptr;         // Probe irradiance or distance border column update compute PSO
#if RTXGI_DDGI_PROBE_RELOCATION
        ID3D12PipelineState*    probeRelocationPSO = nullptr;           // Probe relocation compute PSO
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        ID3D12PipelineState*    probeStateClassifierPSO = nullptr;              // Probe state classifier compute PSO
        ID3D12PipelineState*    probeStateClassifierActivateAllPSO = nullptr;   // Probe state classifier activate all compute PSO
#endif
#endif /* RTXGI_DDGI_SDK_MANAGED_RESOURCES */
    };

    /*
    * Describes properties of a DDGIVolume.
    */
    struct DDGIVolumeDesc
    {
        float3          origin = { 0.f, 0.f, 0.f };             // World-space origin of the volume.
        float3          probeGridSpacing = { 0.f, 0.f, 0.f };   // World-space distance between probes.

        int3            probeGridCounts = { -1, -1, -1 };       // Number of probes on each axis.

        int             numIrradianceTexels = -1;               // Number of texels used in one dimension of the irradiance texture, not including the 1-pixel border on each side.
        int             numDistanceTexels = -1;                 // Number of texels used in one dimension of the distance texture, not including the 1-pixel border on each side.
        int             numRaysPerProbe = 144;                  // Number of rays cast per probe per frame. When using RTXGI_DDGI_BLENDING_USE_SHARED_MEMORY, make this a multiple of the irradiance/distance probe texel resolution for best behavior.

        float           probeMaxRayDistance = 1e27f;            // Maximum distance a probe ray may travel.

        // Controls the influence of new rays when updating each probe. A value close to 1 will
        // very slowly change the probe textures, improving stability but reducing accuracy when objects
        // move in the scene. Values closer to 0.9 or lower will rapidly react to scene changes,
        // but will exhibit flickering.
        float           probeHysteresis = 0.97f;

        // Exponent for depth testing. A high value will rapidly react to depth discontinuities, 
        // but risks causing banding.
        float           probeDistanceExponent = 50.f;

        // Irradiance blending happens in post-tonemap space
        float           probeIrradianceEncodingGamma = 5.f;

        // A threshold ratio used during probe radiance blending that determines if a large lighting change has happened.
        // If the max color component difference is larger than this threshold, the hysteresis will be reduced.
        float           probeChangeThreshold = 0.25f;

        // A threshold value used during probe radiance blending that determines the maximum allowed difference in brightness
        // between the previous and current irradiance values. This prevents impulses from drastically changing a
        // texel's irradiance in a single update cycle.
        float           probeBrightnessThreshold = 0.10f;

        // Bias values for Indirect Lighting
        float           viewBias = 0.1f;
        float           normalBias = 0.1f;

#if RTXGI_DDGI_PROBE_RELOCATION
        // Probe relocation moves probes that see front facing triangles closer than this value
        float           probeMinFrontfaceDistance = 1.f;
#endif

#if RTXGI_DDGI_PROBE_RELOCATION || RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        // Probe relocation and state classifier assume probes with more than
        // this ratio of backface hits are inside of geometry.
        float           probeBackfaceThreshold = 0.25f;
#endif

#if RTXGI_DDGI_SDK_MANAGED_RESOURCES
        bool ShouldAllocateProbes(DDGIVolumeDesc &desc)
        {
            // Number of probes changed
            if (desc.probeGridCounts.x == -1 && desc.probeGridCounts.y == -1 && desc.probeGridCounts.z == -1) return true;
            if (probeGridCounts != desc.probeGridCounts) return true;
            
            // Volume moved
            if (origin != desc.origin) return true;
            if (probeGridSpacing != desc.probeGridSpacing) return true;
            return false;
        }

        bool ShouldAllocateRTRadiance(DDGIVolumeDesc &desc)
        {
            if (desc.numRaysPerProbe != numRaysPerProbe) return true;
            return false;
        }

        bool ShouldAllocateIrradiance(DDGIVolumeDesc &desc)
        {
            if (numIrradianceTexels != desc.numIrradianceTexels) return true;
            return false;
        }

        bool ShouldAllocateDistance(DDGIVolumeDesc &desc)
        {
            if (numDistanceTexels != desc.numDistanceTexels) return true;
            return false;
        }
#endif /* RTXGI_DDGI_SDK_MANAGED_RESOURCES */
    };

    /**
    * Get the size of the constant buffer required by a DDGIVolume.
    */
    int GetDDGIVolumeConstantBufferSize();

    /*
    * Get the number of CBV/SRV/UAV resource descriptors required by a DDGIVolume.
    */
    int GetDDGIVolumeNumDescriptors();

    /**
    * Get the DXGI_FORMAT type of the given texture resource.
    */
    DXGI_FORMAT GetDDGIVolumeTextureFormat(EDDGITextureType type);

    /**
    * Get the number of probes on the X and Y dimensions of the irradiance and distance textures.
    */
    void GetDDGIVolumeProbeCounts(const DDGIVolumeDesc &desc, UINT &probeCountX, UINT &probeCountY);

    /**
    * Get the dimensions of the given texture type for a DDGIVolume with the given descriptor.
    */
    void GetDDGIVolumeTextureDimensions(const DDGIVolumeDesc &desc, EDDGITextureType type, UINT &width, UINT &height);

    /**
    * Get the root signature descriptor for a DDGIVolume.
    */
    bool GetDDGIVolumeRootSignatureDesc(int descriptorHeapOffset, ID3DBlob** signature);

    /**
     * Get the GPU version of the DDGIVolume descriptor.
     */
    DDGIVolumeDescGPU GetDDGIVolumeGPUDesc(DDGIVolumeDesc &desc);

    /*
    * A volume of space within which irradiance queries at arbitrary points are supported using
    * a grid of probes. A single DDGIVolume may cover the entire scene or some sub-volume of the scene.
    * The probe grid of the volume is centered around the provided origin. Grid probes are numbered
    * in ascending order from left to right, back to front in a left handed coordinate system.

    * If there are parts of a scene with very different geometric density or dimensions, instantiate
    * multiple DDGIVolumes.
    */
    class DDGIVolume
    {
    public:

        DDGIVolume(std::string name) : m_name(name) {}

        /**
         * Allocates resources for the volume if resource management is enabled.
         * Computes the bounding box and performs other initialization.
         */
        ERTXGIStatus Create(DDGIVolumeDesc &desc, DDGIVolumeResources &resources);

        /**
         * Move the volume's location by the given translation.
         */
        ERTXGIStatus Move(float3 translation);

        /**
         * Updates the volume's random rotation and constant buffer.
         */
        ERTXGIStatus Update(ID3D12Resource* constantBuffer, UINT64 offsetInBytes = 0);

        /**
         * Updates the volume's probes.
         */
        ERTXGIStatus UpdateProbes(ID3D12GraphicsCommandList4* cmdList);

#if RTXGI_DDGI_PROBE_RELOCATION
        /**
        * Adjusts probe positions to avoid being inside geometry.
        * The argument probeDistanceScale is a value between 0 and 1 indicating how far to move.
        * If using probe relocation in an iterative optimizer, start with values close to 1 and descend to 0.
        */
        ERTXGIStatus RelocateProbes(ID3D12GraphicsCommandList4* cmdList, float probeDistanceScale);
#endif /* RTXGI_DDGI_PROBE_RELOCATION */

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        /**
        * Classifies probes as active or inactive based on the current data in the RT radiance texture.
        */
        ERTXGIStatus ClassifyProbes(ID3D12GraphicsCommandList4* cmdList);

        /**
        * Forces all probes in the volume to the active state.
        */
        ERTXGIStatus ActivateAllProbes(ID3D12GraphicsCommandList4* cmdList);
#endif /* RTXGI_DDGI_PROBE_STATE_CLASSIFIER */

        /**
         * Releases resources owned by the volume.
         */
        void Destroy();

        //------------------------------------------------------------------------
        // Resources
        //------------------------------------------------------------------------

        ID3D12DescriptorHeap* GetDescriptorHeap() const { return m_descriptorHeap; }

        int GetDescriptorHeapOffset() const { return m_descriptorHeapOffset; }

        void SetDescriptorHeap(ID3D12DescriptorHeap* ptr, int offset) { m_descriptorHeap = ptr; m_descriptorHeapOffset = offset; }

        ID3D12RootSignature* GetRootSignature() const { return m_rootSignature; }
        ID3D12Resource* GetConstantBuffer() const { return m_volumeCB; }
        ID3D12Resource* GetProbeRTRadianceTexture() const { return m_probeRTRadiance; }
        ID3D12Resource* GetProbeIrradianceTexture() const { return m_probeIrradiance; }
        ID3D12Resource* GetProbeDistanceTexture() const { return m_probeDistance; }
#if RTXGI_DDGI_PROBE_RELOCATION
        ID3D12Resource* GetProbeOffsetsTexture() const { return m_probeOffsets; }
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        ID3D12Resource* GetProbeStatesTexture() const { return m_probeStates; }
#endif

#if !RTXGI_DDGI_SDK_MANAGED_RESOURCES
        ID3D12Resource* SetRTRadianceTexture(ID3D12Resource* ptr) { m_probeRTRadiance = ptr; }
        ID3D12Resource* SetIrradianceTexture(ID3D12Resource* ptr) { m_probeIrradiance = ptr; }
        ID3D12Resource* SetDistanceTexture(ID3D12Resource* ptr) { m_probeDistance = ptr; }
#if RTXGI_DDGI_PROBE_RELOCATION
        ID3D12Resource* SetProbeOffsetsTexture(ID3D12Resource* ptr) { m_probeOffsets = ptr; }
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        ID3D12Resource* SetProbeStatesTexture(ID3D12Resource* ptr) { m_probeStates = ptr; }
#endif
#endif

        //------------------------------------------------------------------------
        // Setters
        //------------------------------------------------------------------------

        void SetName(std::string name) { m_name = name; }

        void SetViewBias(float value) { m_desc.viewBias = value; }

        void SetNormalBias(float value) { m_desc.normalBias = value; }

        void SetProbeHysteresis(float value) { m_desc.probeHysteresis = value; }

        void SetProbeMaxRayDistance(float value) { m_desc.probeMaxRayDistance = value; }

        void SetProbeDistanceExponent(float value) { m_desc.probeDistanceExponent = value; }

        void SetProbeChangeThreshold(float value) { m_desc.probeChangeThreshold = value; }

        void SetProbeBrightnessThreshold(float value) { m_desc.probeBrightnessThreshold = value; }

        //------------------------------------------------------------------------
        // Getters
        //------------------------------------------------------------------------

        std::string GetName() const { return m_name; }
        
        float GetViewBias() const { return m_desc.viewBias; }

        float GetNormalBias() const { return m_desc.normalBias; }

        float GetProbeHysteresis() const { return m_desc.probeHysteresis; }

        float GetProbeMaxRayDistance() const { return m_desc.probeMaxRayDistance; }

        float GetProbeDistanceExponent() const { return m_desc.probeDistanceExponent; }

        float GetProbeChangeThreshold() const { return m_desc.probeChangeThreshold; }

        float GetProbeBrightnessThreshold() const { return m_desc.probeBrightnessThreshold; }

        DDGIVolumeDesc GetDesc() const { return m_desc; }

        DDGIVolumeResources GetResources() const { return m_resources; }

        int GetNumProbes() const {  return (m_desc.probeGridCounts.x * m_desc.probeGridCounts.y * m_desc.probeGridCounts.z); }      

        int GetNumRaysPerProbe() const { return static_cast<int>(m_desc.numRaysPerProbe); }

        float4x4 GetRotationTransform() const { return m_rotationTransform; }

        float3 GetProbeWorldPosition(int probeIndex) const;

        AABB GetBoundingBox() const;

        //------------------------------------------------------------------------
        // Event Handlers
        //------------------------------------------------------------------------

        virtual void OnGlobalLightChange() {}
        virtual void OnLargeObjectChange() {}
        virtual void OnSmallLightChange() {}

    private:
        std::string                 m_name;                                             // Name of the volume
        DDGIVolumeDesc              m_desc;                                             // Properties of the volume provided by the client
        DDGIVolumeResources         m_resources;                                        // Resources passed in by the host application
        float3                      m_origin;                                           // Origin of the volume
        AABB                        m_boundingBox;                                      // Axis aligned bounding box of the volume, centered on the origin

        ID3D12Resource*             m_volumeCB;                                         // Constant data for the DDGIVolume
        UINT64                      m_volumeCBOffsetInBytes;                            // Offset into the constant buffer, in bytes

        ID3D12Resource*             m_probeRTRadiance = nullptr;                        // Probe radiance (from ray tracing)
        ID3D12Resource*             m_probeIrradiance = nullptr;                        // Probe irradiance, encoded with a high gamma curve
        ID3D12Resource*             m_probeDistance = nullptr;                          // Probe distance, R channel is mean distance, G channel is mean distance^2

        ID3D12DescriptorHeap*       m_descriptorHeap = nullptr;                         // Descriptor heap
        ID3D12RootSignature*        m_rootSignature = nullptr;                          // Root signature, used for all the update compute shaders
        ID3D12PipelineState*        m_probeRadianceBlendingPSO = nullptr;               // Probe radiance blending compute shader pipeline state object
        ID3D12PipelineState*        m_probeDistanceBlendingPSO = nullptr;               // Probe distance blending compute shader pipeline state object
        ID3D12PipelineState*        m_probeBorderRowPSO = nullptr;                      // Probe irradiance or distance border row update compute shader pipeline state object
        ID3D12PipelineState*        m_probeBorderColumnPSO = nullptr;                   // Probe irradiance or distance border column update compute shader pipeline state object

#if RTXGI_DDGI_PROBE_RELOCATION
        ID3D12Resource*             m_probeOffsets = nullptr;                           // Probe offsets texture, world-space offsets to relocate probes at runtime
        ID3D12PipelineState*        m_probeRelocationPSO = nullptr;                     // Probe relocation compute shader pipeline state object
#endif

#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        ID3D12Resource*             m_probeStates = nullptr;
        ID3D12PipelineState*        m_probeClassifierPSO = nullptr;
        ID3D12PipelineState*        m_probeClassifierActivateAllPSO = nullptr;
#endif

        D3D12_ROOT_SIGNATURE_DESC   m_rootSignatureDesc;                                // The root signature descriptor for the volume's compute shaders

        D3D12_CPU_DESCRIPTOR_HANDLE m_descriptorHeapStart;                              // Start location of the descriptor heap
        UINT                        m_descriptorDescSize = 0;                           // Size of each descriptor heap entry
        UINT                        m_descriptorHeapOffset = 0;                         // Offset to the first available slot in the descriptor heap

        float4x4                    m_rotationTransform;                                // A random rotation transform, updated each frame for computing probe ray directions

        DDGIVolume() {}
        
#if RTXGI_DDGI_SDK_MANAGED_RESOURCES
        void CreateDescriptors(ID3D12Device* device);
        bool CreateRootSignature(ID3D12Device* device);
        bool CreateComputePSO(ID3D12Device* device, ID3DBlob* shader, ID3D12PipelineState** pipeline);
        bool CreateTexture(UINT64 width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_STATES state, ID3D12Resource** resource, ID3D12Device* device);
        bool CreateConstantBuffer(ID3D12Device* device);
        bool CreateProbeRTRadianceTexture(DDGIVolumeDesc &desc, ID3D12Device* device);
        bool CreateProbeIrradianceTexture(DDGIVolumeDesc &desc, ID3D12Device* device);
        bool CreateProbeDistanceTexture(DDGIVolumeDesc &desc, ID3D12Device* device);
#if RTXGI_DDGI_PROBE_RELOCATION
        bool CreateProbeOffsetTexture(DDGIVolumeDesc &desc, ID3D12Device* device);
#endif
#if RTXGI_DDGI_PROBE_STATE_CLASSIFIER
        bool CreateProbeStatesTexture(DDGIVolumeDesc &desc, ID3D12Device* device);
#endif
#endif

        // Helper Functions
        void ComputeRandomRotation();                               // Computes the random rotation transformation
        int3 GetProbeGridCoords(int probeIndex) const;              // Computes the 3D grid coordinates for the probe at the given probe index
    };
}
