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

namespace rtxgi
{
    //------------------------------------------------------------------------
    // Public RTXGI Namespace DDGI Functions
    //------------------------------------------------------------------------

    bool bInsertPerfMarkers = true;
    void SetInsertPerfMarkers(bool value) { bInsertPerfMarkers = value; }

    int GetDDGIVolumeNumRTVDescriptors() { return 2; }
    int GetDDGIVolumeNumSRVDescriptors() { return 4; }
    int GetDDGIVolumeNumUAVDescriptors() { return 4; }

    bool ValidateShaderBytecode(const ShaderBytecode& bytecode)
    {
        if (bytecode.pData == nullptr || bytecode.size == 0) return false;
        return true;
    }

    void GetDDGIVolumeProbeCounts(const DDGIVolumeDesc& desc, uint32_t& probeCountX, uint32_t& probeCountY)
    {
    #if RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT || RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT
        probeCountX = (uint32_t)(desc.probeCounts.x * desc.probeCounts.y);
        probeCountY = (uint32_t)desc.probeCounts.z;
    #elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_LEFT_Z_UP
        probeCountX = (uint32_t)(desc.probeCounts.y * desc.probeCounts.z);
        probeCountY = (uint32_t)desc.probeCounts.x;
    #elif RTXGI_COORDINATE_SYSTEM == RTXGI_COORDINATE_SYSTEM_RIGHT_Z_UP
        probeCountX = (uint32_t)(desc.probeCounts.x * desc.probeCounts.z);
        probeCountY = (uint32_t)desc.probeCounts.y;
    #endif
    }

    void GetDDGIVolumeTextureDimensions(const DDGIVolumeDesc& desc, EDDGIVolumeTextureType type, uint32_t& width, uint32_t& height)
    {
        if (type == EDDGIVolumeTextureType::RayData)
        {
            width = (uint32_t)desc.probeNumRays;
            height = (uint32_t)(desc.probeCounts.x * desc.probeCounts.y * desc.probeCounts.z);
        }
        else
        {
            GetDDGIVolumeProbeCounts(desc, width, height);

            if (type == EDDGIVolumeTextureType::Irradiance)
            {
                width *= (uint32_t)(desc.probeNumIrradianceTexels + 2);
                height *= (uint32_t)(desc.probeNumIrradianceTexels + 2);
            }
            else if (type == EDDGIVolumeTextureType::Distance)
            {
                width *= (uint32_t)(desc.probeNumDistanceTexels + 2);
                height *= (uint32_t)(desc.probeNumDistanceTexels + 2);
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
        descGPU.probeNumIrradianceTexels = m_desc.probeNumIrradianceTexels;
        descGPU.probeNumDistanceTexels = m_desc.probeNumDistanceTexels;
        descGPU.probeHysteresis = m_desc.probeHysteresis;
        descGPU.probeMaxRayDistance = m_desc.probeMaxRayDistance;
        descGPU.probeNormalBias = m_desc.probeNormalBias;
        descGPU.probeViewBias = m_desc.probeViewBias;
        descGPU.probeDistanceExponent = m_desc.probeDistanceExponent;
        descGPU.probeIrradianceThreshold = m_desc.probeIrradianceThreshold;
        descGPU.probeBrightnessThreshold = m_desc.probeBrightnessThreshold;
        descGPU.probeIrradianceEncodingGamma = m_desc.probeIrradianceEncodingGamma;
        descGPU.probeBackfaceThreshold = m_desc.probeBackfaceThreshold;
        descGPU.probeMinFrontfaceDistance = m_desc.probeMinFrontfaceDistance;

        // 15-bits used for scroll offsets (plus 1 sign bit), maximum magnitude of 32,767
        descGPU.probeScrollOffsets.x = std::min(32767, abs(m_probeScrollOffsets.x)) * rtxgi::Sign(m_probeScrollOffsets.x);
        descGPU.probeScrollOffsets.y = std::min(32767, abs(m_probeScrollOffsets.y)) * rtxgi::Sign(m_probeScrollOffsets.y);
        descGPU.probeScrollOffsets.z = std::min(32767, abs(m_probeScrollOffsets.z)) * rtxgi::Sign(m_probeScrollOffsets.z);

        descGPU.probeRayDataFormat = m_desc.probeRayDataFormat;
        descGPU.probeIrradianceFormat = m_desc.probeIrradianceFormat;
        descGPU.probeRelocationEnabled = m_desc.probeRelocationEnabled;
        descGPU.probeClassificationEnabled = m_desc.probeClassificationEnabled;
        descGPU.probeScrollClear[0] = m_probeScrollClear[0];
        descGPU.probeScrollClear[1] = m_probeScrollClear[1];
        descGPU.probeScrollClear[2] = m_probeScrollClear[2];

        return descGPU;
    }

    DDGIVolumeDescGPUPacked DDGIVolumeBase::GetDescGPUPacked() const
    {
        return GetDescGPU().GetPackedData();
    }

    float3 DDGIVolumeBase::GetOrigin() const
    {
        if(m_desc.movementType == EDDGIVolumeMovementType::Default) return m_desc.origin;

        return { m_desc.origin.x + ((float)m_probeScrollOffsets.x * m_desc.probeSpacing.x),
                 m_desc.origin.y + ((float)m_probeScrollOffsets.y * m_desc.probeSpacing.y),
                 m_desc.origin.z + ((float)m_probeScrollOffsets.z * m_desc.probeSpacing.z) };
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

        if (m_desc.eulerAngles == float3{ 0.f, 0.f, 0.f })
        {
            return a;
        }
        else
        {
            // Real-Time Collision Detection by Christer Ericson
            // 4.2.6 AABB Recomputed from Rotated AABB
            AABB b = {};
            for (size_t i = 0; i < 3; ++i)
            {
                for (size_t j = 0; j < 3; j++) {
                    float e = m_rotationMatrix[i][j] * a.min[j];
                    float f = m_rotationMatrix[i][j] * a.max[j];
                    if (e < f) {
                        b.min[i] += e;
                        b.max[i] += f;
                    }
                    else {
                        b.min[i] += f;
                        b.max[i] += e;
                    }
                }
            }
            return b;
        }
    }

    OBB DDGIVolumeBase::GetOrientedBoundingBox() const
    {
        OBB obb = {};
        obb.origin = m_desc.origin;
        obb.rotation = m_rotationQuaternion;
        obb.e = float3(m_desc.probeSpacing * (m_desc.probeCounts - 1)) / 2.f;

        return obb;
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
            m_rotationMatrix = EulerAnglesToRotationMatrixYXZ(eulerAngles);
            m_rotationQuaternion = RotationMatrixToQuaternion(m_rotationMatrix);
        }
    }

    //------------------------------------------------------------------------
    // Random number generation
    //------------------------------------------------------------------------

    static std::uniform_real_distribution<float> s_distribution(0.f, 1.f);

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

        // Get the world-space translation and direction between the (effective) origin and the scroll anchor
        float3 translation = m_probeScrollAnchor - GetOrigin();
        int3   translationDirection = { Sign(translation.x), Sign(translation.y), Sign(translation.z) };

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

            ScrollReset(0, translationDirection.x);
        }

        if (scroll.y != 0)
        {
            // Increment the scroll offsets
            m_probeScrollOffsets.y += scroll.y;
            m_probeScrollClear[1] = true;

            ScrollReset(1, translationDirection.y);
        }

        if (scroll.z != 0)
        {
            m_probeScrollOffsets.z += scroll.z;
            m_probeScrollClear[2] = true;

            ScrollReset(2, translationDirection.z);
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

    void DDGIVolumeBase::ScrollReset(int axis, int direction)
    {
        // Reset the volume's origin and scroll offsets (if necessary) for the given axis
        if (m_probeScrollOffsets[(size_t)axis] != 0 && (m_probeScrollOffsets[(size_t)axis] % m_desc.probeCounts[(size_t)axis] == 0))
        {
            m_desc.origin[(size_t)axis] += ((float)m_desc.probeCounts[(size_t)axis] * m_desc.probeSpacing[(size_t)axis]) * (float)direction;
            m_probeScrollOffsets[(size_t)axis] = 0;
        }
    }

}
