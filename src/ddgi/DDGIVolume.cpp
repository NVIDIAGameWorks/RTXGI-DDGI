/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "rtxgi/ddgi/DDGIVolume.h"

#include <algorithm>
#include <assert.h>
#include <cmath>
#include <random>

namespace rtxgi
{
    //------------------------------------------------------------------------
    // Public RTXGI Namespace DDGI Functions
    //------------------------------------------------------------------------

    bool bInsertPerfMarkers = true;
    void SetInsertPerfMarkers(bool value) { bInsertPerfMarkers = value; }

    int GetDDGIVolumeNumRTVDescriptors() { return 2; }
    int GetDDGIVolumeNumTex2DArrayDescriptors() { return 6; }
    int GetDDGIVolumeNumResourceDescriptors() { return 2 * GetDDGIVolumeNumTex2DArrayDescriptors(); } // Multiplied by 2 to account for UAV *and* SRV descriptors

    bool ValidateShaderBytecode(const ShaderBytecode& bytecode)
    {
        if (bytecode.pData == nullptr || bytecode.size == 0) return false;
        return true;
    }

    void GetDDGIVolumeProbeCounts(const DDGIVolumeDesc& desc, uint32_t& probeCountX, uint32_t& probeCountY, uint32_t& probeCountZ)
    {
    #if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
        probeCountX = (uint32_t)desc.probeCounts.x;
        probeCountY = (uint32_t)desc.probeCounts.z;
        probeCountZ = (uint32_t)desc.probeCounts.y;
    #elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
        probeCountX = (uint32_t)desc.probeCounts.y;
        probeCountY = (uint32_t)desc.probeCounts.x;
        probeCountZ = (uint32_t)desc.probeCounts.z;
    #elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
        probeCountX = (uint32_t)desc.probeCounts.x;
        probeCountY = (uint32_t)desc.probeCounts.y;
        probeCountZ = (uint32_t)desc.probeCounts.z;
    #endif
    }

    /**
     * Get the number of texels in each dimension of the volume's texture resources.
     */
    void GetDDGIVolumeTextureDimensions(const DDGIVolumeDesc& desc, EDDGIVolumeTextureType type, uint32_t& width, uint32_t& height, uint32_t& arraySize)
    {
        GetDDGIVolumeProbeCounts(desc, width, height, arraySize);
        if (type == EDDGIVolumeTextureType::RayData)
        {
            height = (uint32_t)(width * height);
            width = (uint32_t)desc.probeNumRays;
        }
        else
        {
            if (type == EDDGIVolumeTextureType::Irradiance)
            {
                width *= (uint32_t)(desc.probeNumIrradianceTexels);
                height *= (uint32_t)(desc.probeNumIrradianceTexels);
            }
            else if (type == EDDGIVolumeTextureType::Distance)
            {
                width *= (uint32_t)(desc.probeNumDistanceTexels);
                height *= (uint32_t)(desc.probeNumDistanceTexels);
            }
            else if (type == EDDGIVolumeTextureType::Variability)
            {
                width *= (uint32_t)(desc.probeNumIrradianceInteriorTexels);
                height *= (uint32_t)(desc.probeNumIrradianceInteriorTexels);
            }
            else if (type == EDDGIVolumeTextureType::VariabilityAverage)
            {
                // Start with Probe variability texture dimensions
                width *= (uint32_t)(desc.probeNumIrradianceInteriorTexels);
                height *= (uint32_t)(desc.probeNumIrradianceInteriorTexels);
                // Divide into thread groups, should match NUM_THREADS_XYZ in ReductionCS.hlsl
                const uint3 NumThreadsInGroup = { 4, 8, 4 };
                // Also divide by sample footprint per-thread, should match ThreadSampleFootprint in ReductionCS.hlsl
                const uint3 DimensionScale = { NumThreadsInGroup.x * 4, NumThreadsInGroup.y * 2, NumThreadsInGroup.z };
                // Size of diff total texture is just diff divided by thread group dimensions, rounded up
                width = (width + DimensionScale.x - 1) / DimensionScale.x;
                height = (height + DimensionScale.y - 1) / DimensionScale.y;
                arraySize = (arraySize + DimensionScale.z - 1) / DimensionScale.z;
            }
        }
    }

    //------------------------------------------------------------------------
    // Public DDGIVolume Functions
    //------------------------------------------------------------------------

    void DDGIVolumeBase::Update()
    {
        // Update the random probe ray rotation transform
        ComputeRandomRotation();

        // Update scrolling offsets and clear flags
        if(m_desc.movementType == EDDGIVolumeMovementType::Scrolling) ComputeScrolling();
    }

#if _DEBUG
    void DDGIVolumeBase::ValidatePackedData(const DDGIVolumeDescGPUPacked packed) const
    {
        DDGIVolumeDescGPU l = UnpackDDGIVolumeDescGPU(packed);
        DDGIVolumeDescGPU r = GetDescGPU();

        // Packed0
        assert(l.probeCounts.x == r.probeCounts.x);
        assert(l.probeCounts.y == r.probeCounts.y);
        assert(l.probeCounts.z == r.probeCounts.z);

        // Packed1, expect precision loss going from FP32->FP16->FP32
        assert(abs(l.probeRandomRayBackfaceThreshold - r.probeRandomRayBackfaceThreshold) <= (1.f / 65536.f));
        assert(abs(l.probeFixedRayBackfaceThreshold - r.probeFixedRayBackfaceThreshold) <= (1.f / 65536.f));

        // Packed2
        assert(l.probeNumRays == r.probeNumRays);
        assert(l.probeNumIrradianceInteriorTexels == r.probeNumIrradianceInteriorTexels);
        assert(l.probeNumDistanceInteriorTexels == r.probeNumDistanceInteriorTexels);

        // Packed3
        assert(l.probeScrollOffsets.x == r.probeScrollOffsets.x);
        assert(l.probeScrollOffsets.y == r.probeScrollOffsets.y);

        // Packed4
        assert(l.probeScrollOffsets.z == r.probeScrollOffsets.z);
        assert(l.movementType == r.movementType);
        assert(l.probeRayDataFormat == r.probeRayDataFormat);
        assert(l.probeIrradianceFormat == r.probeIrradianceFormat);
        assert(l.probeRelocationEnabled == r.probeRelocationEnabled);
        assert(l.probeClassificationEnabled == r.probeClassificationEnabled);
        assert(l.probeVariabilityEnabled == r.probeVariabilityEnabled);
        assert(l.probeScrollClear[0] == r.probeScrollClear[0]);
        assert(l.probeScrollClear[1] == r.probeScrollClear[1]);
        assert(l.probeScrollClear[2] == r.probeScrollClear[2]);
        assert(l.probeScrollDirections[0] == r.probeScrollDirections[0]);
        assert(l.probeScrollDirections[1] == r.probeScrollDirections[1]);
        assert(l.probeScrollDirections[2] == r.probeScrollDirections[2]);
    }
#endif

    //------------------------------------------------------------------------
    // Getters
    //------------------------------------------------------------------------

    DDGIVolumeDescGPU DDGIVolumeBase::GetDescGPU() const
    {
        DDGIVolumeDescGPU descGPU = {};
        descGPU.origin = m_desc.origin;
        descGPU.rotation = m_rotationQuaternion;
        descGPU.probeRayRotation = m_probeRayRotationQuaternion;
        descGPU.movementType = static_cast<uint32_t>(m_desc.movementType);
        descGPU.probeSpacing = m_desc.probeSpacing;
        descGPU.probeCounts = m_desc.probeCounts;
        descGPU.probeNumRays = m_desc.probeNumRays;
        descGPU.probeNumIrradianceInteriorTexels = m_desc.probeNumIrradianceInteriorTexels;
        descGPU.probeNumDistanceInteriorTexels = m_desc.probeNumDistanceInteriorTexels;
        descGPU.probeHysteresis = m_desc.probeHysteresis;
        descGPU.probeMaxRayDistance = m_desc.probeMaxRayDistance;
        descGPU.probeNormalBias = m_desc.probeNormalBias;
        descGPU.probeViewBias = m_desc.probeViewBias;
        descGPU.probeDistanceExponent = m_desc.probeDistanceExponent;

        descGPU.probeIrradianceEncodingGamma = m_desc.probeIrradianceEncodingGamma;
        descGPU.probeIrradianceThreshold = m_desc.probeIrradianceThreshold;
        descGPU.probeBrightnessThreshold = m_desc.probeBrightnessThreshold;

        descGPU.probeRandomRayBackfaceThreshold = std::clamp(m_desc.probeRandomRayBackfaceThreshold, 0.f, 1.f);
        descGPU.probeFixedRayBackfaceThreshold = std::clamp(m_desc.probeFixedRayBackfaceThreshold, 0.f, 1.f);

        descGPU.probeMinFrontfaceDistance = m_desc.probeMinFrontfaceDistance;

        // 15-bits used for scroll offsets (plus 1 sign bit), maximum magnitude of 32,767
        descGPU.probeScrollOffsets.x = std::min(32767, abs(m_probeScrollOffsets.x)) * rtxgi::Sign(m_probeScrollOffsets.x);
        descGPU.probeScrollOffsets.y = std::min(32767, abs(m_probeScrollOffsets.y)) * rtxgi::Sign(m_probeScrollOffsets.y);
        descGPU.probeScrollOffsets.z = std::min(32767, abs(m_probeScrollOffsets.z)) * rtxgi::Sign(m_probeScrollOffsets.z);

        descGPU.probeRayDataFormat = static_cast<uint32_t>(m_desc.probeRayDataFormat);
        descGPU.probeIrradianceFormat = static_cast<uint32_t>(m_desc.probeIrradianceFormat);
        descGPU.probeRelocationEnabled = m_desc.probeRelocationEnabled;
        descGPU.probeClassificationEnabled = m_desc.probeClassificationEnabled;
        descGPU.probeVariabilityEnabled = m_desc.probeVariabilityEnabled;
        descGPU.probeScrollClear[0] = m_probeScrollClear[0];
        descGPU.probeScrollClear[1] = m_probeScrollClear[1];
        descGPU.probeScrollClear[2] = m_probeScrollClear[2];
        descGPU.probeScrollDirections[0] = (m_probeScrollDirections[0] > 0);
        descGPU.probeScrollDirections[1] = (m_probeScrollDirections[1] > 0);
        descGPU.probeScrollDirections[2] = (m_probeScrollDirections[2] > 0);

        return descGPU;
    }

    DDGIVolumeDescGPUPacked DDGIVolumeBase::GetDescGPUPacked() const { return PackDDGIVolumeDescGPU(GetDescGPU()); }

    void DDGIVolumeBase::GetRayDispatchDimensions(uint32_t& width, uint32_t& height, uint32_t& depth) const
    {
        GetDDGIVolumeTextureDimensions(m_desc, EDDGIVolumeTextureType::RayData, width, height, depth);
    }

    float3 DDGIVolumeBase::GetOrigin() const
    {
        if(m_desc.movementType == EDDGIVolumeMovementType::Default) return m_desc.origin;

        return { m_desc.origin.x + ((float)m_probeScrollOffsets.x * m_desc.probeSpacing.x),
                 m_desc.origin.y + ((float)m_probeScrollOffsets.y * m_desc.probeSpacing.y),
                 m_desc.origin.z + ((float)m_probeScrollOffsets.z * m_desc.probeSpacing.z) };
    }

    uint32_t DDGIVolumeBase::GetTexture2DArraySize() const
    {
    #if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
        return (uint32_t)m_desc.probeCounts.y;
    #elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
        return (uint32_t)m_desc.probeCounts.z;
    #endif
    }

    float3 DDGIVolumeBase::GetProbeWorldPosition(int probeIndex) const
    {
        // NOTE: If the probe relocation is enabled, the probe offset textures need to be read and added to this value.
        int3 probeCoords = GetProbeGridCoords(probeIndex);
        float3 probeGridWorldPosition = m_desc.probeSpacing * probeCoords;
        float3 probeGridShift = (m_desc.probeSpacing * (m_desc.probeCounts - 1)) / 2.f;

        return (m_desc.origin + probeGridWorldPosition - probeGridShift);
    }

    AABB DDGIVolumeBase::GetAxisAlignedBoundingBox() const
    {
        float3 origin = m_desc.origin;
        float3 extent = float3(m_desc.probeSpacing * (m_desc.probeCounts - 1)) / 2.f;
        AABB a = { (origin - extent), (origin + extent) };

        // Early out: no rotation
        if (m_desc.eulerAngles == float3{ 0.f, 0.f, 0.f })
        {
            return a;
        }

        // Real-Time Collision Detection by Christer Ericson
        // 4.2.6 AABB Recomputed from Rotated AABB
        AABB b = {};
        for (size_t i = 0; i < 3; ++i)
        {
            for (size_t j = 0; j < 3; j++)
            {
                float e = m_rotationMatrix[i][j] * a.min[j];
                float f = m_rotationMatrix[i][j] * a.max[j];
                if (e < f)
                {
                    b.min[i] += e;
                    b.max[i] += f;
                }
                else
                {
                    b.min[i] += f;
                    b.max[i] += e;
                }
            }
        }
        return b;
    }

    OBB DDGIVolumeBase::GetOrientedBoundingBox() const
    {
        OBB obb = {};
        obb.origin = m_desc.origin;
        obb.rotation = m_rotationQuaternion;
        obb.e = float3(m_desc.probeSpacing * (m_desc.probeCounts - 1)) / 2.f;

        return obb;
    }

    uint32_t DDGIVolumeBase::GetGPUMemoryUsedInBytes() const
    {
        uint32_t bytesPerVolume = 0;

        uint32_t numRayDataBytesPerTexel = 0;
        uint32_t numIrradianceBytesPerTexel = 0;
        uint32_t numDistanceBytesPerTexel = 0;
        uint32_t numProbeDataBytesPerTexel = 0;
        uint32_t numProbeVariabilityBytesPerTexel = 0;
        uint32_t numProbeVariabilityAverageBytesPerTexel = 0;

        // Compute the number of irradiance and distance texels
        uint32_t numIrradianceTexelsPerProbe = (m_desc.probeNumIrradianceTexels * m_desc.probeNumIrradianceTexels);
        uint32_t numDistanceTexelsPerProbe = (m_desc.probeNumDistanceTexels * m_desc.probeNumDistanceTexels);

        // Get the number of bytes per ray data texel
        if (m_desc.probeRayDataFormat == EDDGIVolumeTextureFormat::F32x2) numRayDataBytesPerTexel = 8;
        else if (m_desc.probeRayDataFormat == EDDGIVolumeTextureFormat::F32x4) numRayDataBytesPerTexel = 16;

        // Get the number of bytes per irradiance texel
        if (m_desc.probeIrradianceFormat == EDDGIVolumeTextureFormat::U32) numIrradianceBytesPerTexel = 4;
        else if (m_desc.probeIrradianceFormat == EDDGIVolumeTextureFormat::F16x4) numIrradianceBytesPerTexel = 8;
        else if (m_desc.probeIrradianceFormat == EDDGIVolumeTextureFormat::F32x4) numIrradianceBytesPerTexel = 16;

        // Get the number of bytes per distance texel
        if (m_desc.probeDistanceFormat == EDDGIVolumeTextureFormat::F16x2) numDistanceBytesPerTexel = 4;
        else if (m_desc.probeIrradianceFormat == EDDGIVolumeTextureFormat::F32x2) numDistanceBytesPerTexel = 8;

        // Get the number of bytes per probe data texel
        if (m_desc.probeDataFormat == EDDGIVolumeTextureFormat::F16x4) numProbeDataBytesPerTexel = 8;
        else if (m_desc.probeDataFormat == EDDGIVolumeTextureFormat::F32x4) numProbeDataBytesPerTexel = 16;

        // Get the number of bytes per probe variability texel
        if (m_desc.probeVariabilityFormat == EDDGIVolumeTextureFormat::F16) numProbeVariabilityBytesPerTexel = 2;
        else if (m_desc.probeVariabilityFormat == EDDGIVolumeTextureFormat::F32) numProbeVariabilityBytesPerTexel = 4;

        // Variability average is always F32x2 (8 bytes)
        numProbeVariabilityAverageBytesPerTexel = 8;

        // Compute the number of bytes per probe
        uint32_t bytesPerProbe = 0;
        bytesPerProbe += GetNumRaysPerProbe() * numRayDataBytesPerTexel;
        bytesPerProbe += (numIrradianceTexelsPerProbe * numIrradianceBytesPerTexel);
        bytesPerProbe += (numDistanceTexelsPerProbe * numDistanceBytesPerTexel);
        bytesPerProbe += numProbeDataBytesPerTexel;
        bytesPerProbe += numProbeVariabilityBytesPerTexel;

        // Coefficient of variation average texture is different (smaller) dimensions from other textures
        uint32_t width, height, arraySize;
        GetDDGIVolumeTextureDimensions(m_desc, EDDGIVolumeTextureType::VariabilityAverage, width, height, arraySize);
        bytesPerVolume += width * height * arraySize * numProbeVariabilityAverageBytesPerTexel;

        // Add the per probe memory use
        bytesPerVolume += GetNumProbes() * bytesPerProbe;

        // Add the memory used for the GPU-side DDGIVolumeDescGPUPacked (128B)
        bytesPerVolume += (uint32_t)sizeof(DDGIVolumeDescGPUPacked);

        return bytesPerVolume;
    }

    //------------------------------------------------------------------------
    // Setters
    //------------------------------------------------------------------------

    void DDGIVolumeBase::SetMovementType(EDDGIVolumeMovementType value)
    {
        if(m_desc.movementType != value)
        {
            if (m_desc.movementType == EDDGIVolumeMovementType::Scrolling)
            {
                m_desc.origin = GetOrigin();
            }
            else if (m_desc.movementType == EDDGIVolumeMovementType::Default)
            {
                m_probeScrollAnchor = m_desc.origin;
            }

            m_desc.movementType = value;
            m_probeScrollOffsets = { 0, 0, 0 };
        }
    }

    void DDGIVolumeBase::SetEulerAngles(const float3& eulerAngles)
    {
        if(m_desc.movementType == EDDGIVolumeMovementType::Default)
        {
            m_desc.eulerAngles = eulerAngles;
            m_rotationMatrix = EulerAnglesToRotationMatrix(eulerAngles);
            m_rotationQuaternion = RotationMatrixToQuaternion(m_rotationMatrix);
        }
    }

    //------------------------------------------------------------------------
    // Random number generation
    //------------------------------------------------------------------------

    static std::uniform_real_distribution<float> s_distribution(0.f, 1.f);
    static std::mt19937 m_rng;

    void DDGIVolumeBase::SeedRNG(const int seed)
    {
        m_rng.seed((uint32_t)seed);
    }

    float DDGIVolumeBase::GetRandomFloat()
    {
        return s_distribution(m_rng);
    }

    //------------------------------------------------------------------------
    // Protected Helper Functions
    //------------------------------------------------------------------------

    void DDGIVolumeBase::ComputeScrolling()
    {
        // Reset plane clear flags
        m_probeScrollClear[0] = false;
        m_probeScrollClear[1] = false;
        m_probeScrollClear[2] = false;

        // Reset scroll offsets to not overflow (eventually)
        ScrollReset();

        // Get the world-space translation and direction between the (effective) origin and the scroll anchor
        float3 translation = m_probeScrollAnchor - GetOrigin();
        m_probeScrollDirections = { Sign(translation.x), Sign(translation.y), Sign(translation.z) };

        // Get the number of grid cells between the (effective) origin and the scroll anchor
        int3 scroll =
        {
            rtxgi::AbsFloor(translation.x / m_desc.probeSpacing.x),
            rtxgi::AbsFloor(translation.y / m_desc.probeSpacing.y),
            rtxgi::AbsFloor(translation.z / m_desc.probeSpacing.z),
        };

        if (scroll.x != 0)
        {
            m_probeScrollOffsets.x += scroll.x;
            m_probeScrollClear[0] = true;
        }

        if (scroll.y != 0)
        {
            m_probeScrollOffsets.y += scroll.y;
            m_probeScrollClear[1] = true;
        }

        if (scroll.z != 0)
        {
            m_probeScrollOffsets.z += scroll.z;
            m_probeScrollClear[2] = true;
        }
    }

    void DDGIVolumeBase::ComputeRandomRotation()
    {
        // This approach is based on James Arvo's implementation from Graphics Gems 3 (pg 117-120).
        // Also available at: http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.53.1357&rep=rep1&type=pdf

        // Setup a random rotation matrix using 3 uniform RVs
        float u1 = RTXGI_2PI * GetRandomFloat();
        float cos1 = cosf(u1);
        float sin1 = sinf(u1);

        float u2 = RTXGI_2PI * GetRandomFloat();
        float cos2 = cosf(u2);
        float sin2 = sinf(u2);

        float u3 = GetRandomFloat();
        float sq3 = 2.f * sqrtf(u3 * (1.f - u3));

        float s2 = 2.f * u3 * sin2 * sin2 - 1.f;
        float c2 = 2.f * u3 * cos2 * cos2 - 1.f;
        float sc = 2.f * u3 * sin2 * cos2;

        // Create the random rotation matrix
        float _11 = cos1 * c2 - sin1 * sc;
        float _12 = sin1 * c2 + cos1 * sc;
        float _13 = sq3 * cos2;

        float _21 = cos1 * sc - sin1 * s2;
        float _22 = sin1 * sc + cos1 * s2;
        float _23 = sq3 * sin2;

        float _31 = cos1 * (sq3 * cos2) - sin1 * (sq3 * sin2);
        float _32 = sin1 * (sq3 * cos2) + cos1 * (sq3 * sin2);
        float _33 = 1.f - 2.f * u3;

        // HLSL is column-major
        float3x3 transform;
        transform.r0 = { _11, _12, _13 };
        transform.r1 = { _21, _22, _23 };
        transform.r2 = { _31, _32, _33 };

        m_probeRayRotationMatrix = transform;
        m_probeRayRotationQuaternion = RotationMatrixToQuaternion(m_probeRayRotationMatrix);
    }

    int3 DDGIVolumeBase::GetProbeGridCoords(int probeIndex) const
    {
    #if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
        int x = probeIndex % m_desc.probeCounts.x;
        int y = probeIndex / (m_desc.probeCounts.x * m_desc.probeCounts.z);
        int z = (probeIndex / m_desc.probeCounts.x) % m_desc.probeCounts.z;
    #elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
        int x = (probeIndex / m_desc.probeCounts.y) % m_desc.probeCounts.x;
        int y = probeIndex % m_desc.probeCounts.y;
        int z = probeIndex / (m_desc.probeCounts.x * m_desc.probeCounts.y);
    #elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
        int x = probeIndex % m_desc.probeCounts.x;
        int y = (probeIndex / m_desc.probeCounts.x) % m_desc.probeCounts.y;
        int z = probeIndex / (m_desc.probeCounts.x * m_desc.probeCounts.y);
    #endif
        return { x, y, z };
    }

    //------------------------------------------------------------------------
    // Private Helper Functions
    //------------------------------------------------------------------------

    void DDGIVolumeBase::ScrollReset()
    {
        // Reset the volume's origin and scroll offsets (if necessary) for each axis
        for(int planeIndex = 0; planeIndex < 3; planeIndex++)
        {
            if (m_probeScrollOffsets[planeIndex] != 0 && (m_probeScrollOffsets[planeIndex] % m_desc.probeCounts[planeIndex] == 0))
            {
                m_desc.origin[planeIndex] += ((float)m_desc.probeCounts[planeIndex] * m_desc.probeSpacing[planeIndex]) * (float)m_probeScrollDirections[planeIndex];
                m_probeScrollOffsets[planeIndex] = 0;
            }
        }
    }

}
