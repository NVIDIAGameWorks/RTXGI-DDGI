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

#include "rtxgi/Common.h"
#include "rtxgi/Defines.h"
#include "rtxgi/Math.h"

// --- Resource Allocation Mode -------------------------------------------------------------------

// Define RTXGI_DDGI_RESOURCE_MANAGEMENT to specify the resource management mode.
// 0: "Unmanaged", the application creates, allocates, deallocates, and destroys all graphics resources (default).
// 1: "Managed", the SDK creates, allocates, deallocates, and destroys all graphics
//     resources - *except* for the DDGIVolume constants structured buffer and the Descriptor Heap (D3D12) or Descriptor Pool (Vulkan).
#ifndef RTXGI_DDGI_RESOURCE_MANAGEMENT
#error RTXGI_DDGI_RESOURCE_MANAGEMENT is not defined!
#endif

namespace rtxgi
{
    #include "DDGIRootConstants.h"
    #include "DDGIVolumeDescGPU.h"

    enum class EDDGIVolumeTextureType
    {
        RayData = 0,
        Irradiance,
        Distance,
        Data,
        Variability,
        VariabilityAverage,
        Count
    };

    enum class EDDGIVolumeTextureFormat
    {
        U32   = 0,  // 32-bits per texel unsigned normalized integer format. 4 channels, 10-bits per RGB and 2 bits for alpha. Used with Irradiance.
        F16   = 1,  // 16-bits per texel half precision float format. 1 channel,  16-bits per channel. Used with Variability.
        F16x2 = 2,  // 32-bits per texel half precision float format. 2 channels, 16-bits per channel. Used with Distance.
        F16x4 = 3,  // 64-bits per texel half precision float format. 4 channels, 16-bits per channel. Used with Irradiance, Distance, and Data.
        F32   = 4,  // 32-bits per texel float format. 1 channel,  32-bits per channel. Used with Variability.
        F32x2 = 5,  // 64-bits per texel float format. 2 channels, 32-bits per channel. Used with RayData and Distance.
        F32x4 = 6,  // 128-bits per texel float format. 4 channels, 32-bits per channel. Used with RayData, Irradiance, and Data.
        Count = 7
    };

    enum class EDDGIVolumeMovementType
    {
        Default = 0,
        Scrolling,
        Count
    };

    enum class EDDGIVolumeProbeVisType
    {
        Default = 0,
        Hide_Inactive,
        Count
    };

    extern bool bInsertPerfMarkers;
    RTXGI_API void SetInsertPerfMarkers(bool value);

    /**
     * Get the number of descriptors required for various descriptor heap types.
     */
    RTXGI_API int GetDDGIVolumeNumRTVDescriptors();
    RTXGI_API int GetDDGIVolumeNumTex2DArrayDescriptors();
    RTXGI_API int GetDDGIVolumeNumResourceDescriptors();

    /**
     * Describes properties of a DDGIVolume.
     */
    struct DDGIVolumeDesc
    {
        char*           name = nullptr;                         // Name of the volume
        uint32_t        index = 0;                              // Index of the volume in the constants structured buffer
        uint32_t        rngSeed = 0;                            // A seed for the random number generator (optional). A non-zero value manually initializes the seed used for rotation generation. Leave as zero to use the default (based on system time).

        bool            showProbes = false;                     // A flag for toggling probe visualizations for this volume
        bool            insertPerfMarkers = false;              // A flag for toggling volume-specific perf markers in the graphics command list (for debugging and tools)

        float3          origin = {};                            // World-space origin of the volume
        float3          eulerAngles = {};                       // Euler rotation angles XYZ (in radians)
        float3          probeSpacing = {};                      // World-space distance between probes on each axis of the grid

        int3            probeCounts = { -1, -1, -1 };           // Number of probes on each axis

        int             probeNumRays = 256;                     // Number of rays cast per probe per frame. When using RTXGI_DDGI_BLEND_SHARED_MEMORY, make this a multiple of the irradiance/distance probe texel resolution for best behavior.
        int             probeNumIrradianceTexels = -1;          // Number of texels used in one dimension of the irradiance texture, *including* the 1-pixel border
        int             probeNumIrradianceInteriorTexels = -1;  // Number of texels used in one dimension of the irradiance texture, *excluding* the 1-pixel border
        int             probeNumDistanceTexels = -1;            // Number of texels used in one dimension of the distance texture, *including* the 1-pixel border
        int             probeNumDistanceInteriorTexels = -1;    // Number of texels used in one dimension of the distance texture, *excluding* the 1-pixel border

        // Controls the influence of new rays when updating each probe. A value close to 1 will
        // very slowly change the probe textures, improving stability but reducing accuracy when objects
        // move in the scene. Values closer to 0.9 or lower will rapidly react to scene changes,
        // but will exhibit flickering.
        float           probeHysteresis = 0.97f;

        // Maximum world-space distance a probe ray may travel
        float           probeMaxRayDistance = 1e27f;

        // Exponent for depth testing. A high value will rapidly react to depth discontinuities, but risks causing banding
        float           probeDistanceExponent = 50.f;

        // Irradiance blending happens in post-tonemap space.
        float           probeIrradianceEncodingGamma = 5.f;

        // A threshold ratio used during probe radiance blending that determines if a large lighting change has happened
        // If the max color component difference is larger than this threshold, the hysteresis will be reduced
        float           probeIrradianceThreshold = 0.25f;

        // A threshold value used during probe radiance blending that determines the maximum allowed difference in brightness
        // between the previous and current irradiance values. This prevents impulses from drastically changing a
        // texel's irradiance in a single update cycle.
        float           probeBrightnessThreshold = 0.10f;

        // Probe blending assumes probes with more than this ratio of backface hits are inside of geometry
        float           probeRandomRayBackfaceThreshold = 0.1f;

        // Probe relocation and probe classification assume probes with more than this ratio of backface hits are inside of geometry
        float           probeFixedRayBackfaceThreshold = 0.25f;

        // Bias values for indirect lighting
        float           probeViewBias = 0.1f;                   // A small offset along the camera view ray applied to the shaded surface point to avoid numerical instabilities when determining visibility
        float           probeNormalBias = 0.1f;                 // A small offset along the surface normal applied to the shaded surface point to avoid numerical instabilities when determining visibility

        // Format type for probe texture atlases
        EDDGIVolumeTextureFormat probeRayDataFormat;            // Texel format for the ray data texture, used with GetDDGIVolumeTextureFormat()
        EDDGIVolumeTextureFormat probeIrradianceFormat;         // Texel format for the irradiance texture, used with GetDDGIVolumeTextureFormat()
        EDDGIVolumeTextureFormat probeDistanceFormat;           // Texel format for the distance texture, used with GetDDGIVolumeTextureFormat()
        EDDGIVolumeTextureFormat probeDataFormat;               // Texel format for the probe data texture, used with GetDDGIVolumeTextureFormat()
        EDDGIVolumeTextureFormat probeVariabilityFormat;        // Texel format index for the probe variability texture, used with GetDDGIVolumeTextureFormat()

        // Using shared memory for scroll tests in probe blending can be a performance win on some hardware by reducing the compute workload
        bool            probeBlendingUseScrollSharedMemory = false;

        // Probe relocation moves probes to more useful positions
        bool            probeRelocationEnabled = false;
        bool            probeRelocationNeedsReset = false;

        // Probe relocation will maintain a minimum world-space distance from front facing surfaces
        float           probeMinFrontfaceDistance = 1.f;

        // Probe classification marks probes with states to reduce the ray tracing and blending workloads
        bool            probeClassificationEnabled = false;
        bool            probeClassificationNeedsReset = false;

        // Probe variability tracks the change in probes between updates as a proxy for convergence
        bool            probeVariabilityEnabled = false;

        // The type of movement the volume supports
        EDDGIVolumeMovementType movementType = EDDGIVolumeMovementType::Default;

        // The type of visualization that should be used for this volume
        EDDGIVolumeProbeVisType probeVisType = EDDGIVolumeProbeVisType::Default;

    #if RTXGI_DDGI_RESOURCE_MANAGEMENT
        bool ShouldAllocateProbes(const DDGIVolumeDesc& desc)
        {
            // Probes haven't been allocated or number of probes has changed
            if (desc.probeCounts.x == -1 && desc.probeCounts.y == -1 && desc.probeCounts.z == -1) return true;
            if (probeCounts != desc.probeCounts) return true;
            return false;
        }

        bool ShouldAllocateRayData(const DDGIVolumeDesc& desc)
        {
            // The number of rays to trace per probe has changed
            if (probeNumRays != desc.probeNumRays) return true;
            return false;
        }

        bool ShouldAllocateIrradiance(const DDGIVolumeDesc& desc)
        {
            // The number of irradiance texels has changed
            if (probeNumIrradianceTexels != desc.probeNumIrradianceTexels) return true;
            return false;
        }

        bool ShouldAllocateDistance(const DDGIVolumeDesc& desc)
        {
            // The number of distance texels has changed
            if (probeNumDistanceTexels != desc.probeNumDistanceTexels) return true;
            return false;
        }
    #endif // RTXGI_DDGI_RESOURCE_MANAGEMENT
    };

    /**
     * Validate a shader bytecode blob.
     */
    RTXGI_API bool ValidateShaderBytecode(const ShaderBytecode& bytecode);

    /**
     * Get the number of probes on each axis of the volume for use when specifying textures.
     * The returned Z dimension represents the up axis regardless of coordinate system.
     */
    RTXGI_API void GetDDGIVolumeProbeCounts(const DDGIVolumeDesc& desc, uint32_t& probeCountX, uint32_t& probeCountY, uint32_t& probeCountZ);

    /**
     * Get the dimensions (in texels) of the specified texture type.
     */
    RTXGI_API void GetDDGIVolumeTextureDimensions(const DDGIVolumeDesc& desc, EDDGIVolumeTextureType type, uint32_t& width, uint32_t& height, uint32_t& arraySize);

    /**
     * DDGIVolume abstract base class. Instantiate the API-specific subclass.
     */
    class RTXGI_API DDGIVolumeBase
    {
    public:

        // Update the volume's rotation matrices and scrolling
        virtual void Update();

        // Random numbers
        void  SeedRNG(const int seed);
        float GetRandomFloat();

        // Event Handlers
        virtual void OnGlobalLightChange() {}
        virtual void OnLargeObjectChange() {}
        virtual void OnSmallLightChange() {}

        // Releases resources owned by the volume
        virtual void Destroy() = 0;

    #if _DEBUG
        // Packed constant data validation
        void ValidatePackedData(const DDGIVolumeDescGPUPacked packed) const;
    #endif

        //------------------------------------------------------------------------
        // Setters
        //------------------------------------------------------------------------

        void SetName(char* name) { m_desc.name = name; }

        void SetIndex(uint32_t index) { m_desc.index = index; }

        void SetShowProbes(bool value) { m_desc.showProbes = value; }

        void SetInsertPerfMarkers(bool value) { m_desc.insertPerfMarkers = value; }

        void SetMovementType(EDDGIVolumeMovementType value);

        void SetProbeVisType(EDDGIVolumeProbeVisType value) { m_desc.probeVisType = value; }

        void SetOrigin(const float3& value) { m_desc.origin = value; }

        void SetScrollAnchor(const float3& value) { m_probeScrollAnchor = value; }

        void SetProbeSpacing(const float3& value) { m_desc.probeSpacing = value; }

        void SetEulerAngles(const float3& eulerAngles);

        void SetProbeHysteresis(float value) { m_desc.probeHysteresis = value; }

        void SetProbeMaxRayDistance(float value) { m_desc.probeMaxRayDistance = value; }

        void SetProbeNormalBias(float value) { m_desc.probeNormalBias = value; }

        void SetProbeViewBias(float value) { m_desc.probeViewBias = value; }

        void SetProbeDistanceExponent(float value) { m_desc.probeDistanceExponent = value; }

        void SetIrradianceEncodingGamma(float value) { m_desc.probeIrradianceEncodingGamma = value; }

        void SetProbeIrradianceThreshold(float value) { m_desc.probeIrradianceThreshold = value; }

        void SetProbeBrightnessThreshold(float value) { m_desc.probeBrightnessThreshold = value; }

        void SetProbeRandomRayBackfaceThreshold(float value) { m_desc.probeRandomRayBackfaceThreshold = value; }
        
        void SetProbeFixedRayBackfaceThreshold(float value) { m_desc.probeFixedRayBackfaceThreshold = value; }

        // Probe Relocation Setters
        void SetProbeRelocationEnabled(bool value) { m_desc.probeRelocationEnabled = value; }

        void SetProbeRelocationNeedsReset(bool value) { m_desc.probeRelocationNeedsReset = value; }

        void SetMinFrontFaceDistance(float value) { m_desc.probeMinFrontfaceDistance = value; }

        // Probe Classification Setters
        void SetProbeClassificationEnabled(bool value) { m_desc.probeClassificationEnabled = value; }

        void SetProbeClassificationNeedsReset(bool value) { m_desc.probeClassificationNeedsReset = value; }

        // Probe Variability Setters
        void SetProbeVariabilityEnabled(bool value) { m_desc.probeVariabilityEnabled = value; }

        void SetVolumeAverageVariability(float value) { m_averageVariability = value; };

        //------------------------------------------------------------------------
        // Getters
        //------------------------------------------------------------------------

        virtual uint32_t GetGPUMemoryUsedInBytes() const;

        DDGIVolumeDesc GetDesc() const { return m_desc; }

        DDGIVolumeDescGPU GetDescGPU() const;

        DDGIVolumeDescGPUPacked GetDescGPUPacked() const;

        const char* GetName() const { return m_desc.name; }

        uint32_t GetIndex() const { return m_desc.index; }

        float3 GetOrigin() const;

        bool GetShowProbes() const { return m_desc.showProbes; }

        bool GetInsertPerfMarkers() const { return m_desc.insertPerfMarkers; }

        uint32_t GetTexture2DArraySize() const;

        EDDGIVolumeMovementType GetMovementType() const { return m_desc.movementType; }

        EDDGIVolumeProbeVisType GetProbeVisType() const { return m_desc.probeVisType; }

        float3 GetScrollAnchor() const { return m_probeScrollAnchor; }

        int3 GetScrollOffsets() { return m_probeScrollOffsets; }

        float3 GetProbeSpacing() const { return m_desc.probeSpacing; }

        int3 GetProbeCounts() const { return m_desc.probeCounts; }

        int GetNumProbes() const { return (m_desc.probeCounts.x * m_desc.probeCounts.y * m_desc.probeCounts.z); }

        int GetNumRaysPerProbe() const { return m_desc.probeNumRays; }

        void GetRayDispatchDimensions(uint32_t& width, uint32_t& height, uint32_t& depth) const;

        float GetProbeHysteresis() const { return m_desc.probeHysteresis; }

        float GetProbeMaxRayDistance() const { return m_desc.probeMaxRayDistance; }

        float GetProbeNormalBias() const { return m_desc.probeNormalBias; }

        float GetProbeViewBias() const { return m_desc.probeViewBias; }

        float GetProbeDistanceExponent() const { return m_desc.probeDistanceExponent; }

        float GetProbeIrradianceEncodingGamma() const { return m_desc.probeIrradianceEncodingGamma; }

        float GetProbeIrradianceThreshold() const { return m_desc.probeIrradianceThreshold; }

        float GetProbeBrightnessThreshold() const { return m_desc.probeBrightnessThreshold; }

        float GetProbeRandomRayBackfaceThreshold() const { return m_desc.probeRandomRayBackfaceThreshold; }

        float GetProbeFixedRayBackfaceThreshold() const { return m_desc.probeFixedRayBackfaceThreshold; }

        float3 GetEulerAngles() const { return m_desc.eulerAngles; }

        float3 GetProbeWorldPosition(int probeIndex) const;

        AABB GetAxisAlignedBoundingBox() const;

        OBB GetOrientedBoundingBox() const;

        // Probe Relocation Getters
        bool GetProbeRelocationEnabled() const { return m_desc.probeRelocationEnabled; }

        bool GetProbeRelocationNeedsReset() const { return m_desc.probeRelocationNeedsReset; }

        float GetMinFrontFaceDistance() const { return m_desc.probeMinFrontfaceDistance; }

        // Probe Classification Getters
        bool GetProbeClassificationEnabled() const { return m_desc.probeClassificationEnabled; }

        bool GetProbeClassificationNeedsReset() const { return m_desc.probeClassificationNeedsReset; }

        // Probe Variability Getters
        bool GetProbeVariabilityEnabled() const { return m_desc.probeVariabilityEnabled; }

        float GetVolumeAverageVariability() const { return m_averageVariability; };

    protected:

        void ComputeRandomRotation();
        void ComputeScrolling();
        int3 GetProbeGridCoords(int probeIndex) const;

    protected:

        DDGIVolumeDesc m_desc;                                                 // Properties of the volume

        float4         m_rotationQuaternion = { 0.f, 0.f, 0.f, 1.f };          // Quaternion defining the orientation of the volume (constructed from m_rotationMatrix)
        float3x3       m_rotationMatrix = {                                    // Matrix defining the orientation of the volume
                        { 1.f, 0.f, 0.f },
                        { 0.f, 1.f, 0.f },
                        { 0.f, 0.f, 1.f }
        };

        float4         m_probeRayRotationQuaternion = { 0.f, 0.f, 0.f, 1.f };  // Quaternion defining the orientation of probe rays (constructed from m_probeRayRotationMatrix)
        float3x3       m_probeRayRotationMatrix = {                            // Matrix defining the orientation of probe rays, updated every time Update() is called
                        { 1.f, 0.f, 0.f },
                        { 0.f, 1.f, 0.f },
                        { 0.f, 0.f, 1.f },
        };

        float3         m_probeScrollAnchor = { 0.f, 0.f, 0.f };                // The anchor position for a scrolling volume to target for it's effective origin
        int3           m_probeScrollOffsets = { 0, 0, 0 };                     // Grid-space space offsets for scrolling movement
        int3           m_probeScrollDirections = { 0, 0, 0 };                  // Direction of scrolling movement
        bool           m_probeScrollClear[3] = { 0, 0, 0 };                    // If probes of a plane need to be cleared due to scrolling movement

        float          m_averageVariability = 0;                               // Average variability for last update's probe irradiance values

        bool           m_insertPerfMarkers = false;                            // Toggles whether the volume will insert performance markers in the graphics command list.

    private:

        void ScrollReset();

    };
}
