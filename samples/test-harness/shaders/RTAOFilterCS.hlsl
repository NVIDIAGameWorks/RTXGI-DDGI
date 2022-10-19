/*
* Copyright (c) 2019-2022, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "include/Descriptors.hlsl"

static const int c_radius = 5;
static const int c_paddedPixelWidth = BLOCK_SIZE + c_radius * 2;
static const int c_paddedPixelCount = c_paddedPixelWidth * c_paddedPixelWidth;

groupshared float2 DistanceAndAO[c_paddedPixelWidth][c_paddedPixelWidth];

float FilterAO(int2 paddedPixelPos)
{
    if (GetGlobalConst(rtao, filterDistanceSigma) <= 0.f || GetGlobalConst(rtao, filterDepthSigma) <= 0.f)
    {
        return DistanceAndAO[paddedPixelPos.x][paddedPixelPos.y].y;
    }

    float distanceKernel[c_radius + 1];
    distanceKernel[0] = GetGlobalConst(rtao, filterDistKernel0);
    distanceKernel[1] = GetGlobalConst(rtao, filterDistKernel1);
    distanceKernel[2] = GetGlobalConst(rtao, filterDistKernel2);
    distanceKernel[3] = GetGlobalConst(rtao, filterDistKernel3);
    distanceKernel[4] = GetGlobalConst(rtao, filterDistKernel4);
    distanceKernel[5] = GetGlobalConst(rtao, filterDistKernel5);

    float totalWeight = 0.f;
    float sum = 0.f;
    float centerDepth = DistanceAndAO[paddedPixelPos.x][paddedPixelPos.y].x;

    for (int y = -c_radius; y <= c_radius; ++y)
    {
        for (int x = -c_radius; x <= c_radius; ++x)
        {
            float weight = distanceKernel[abs(x)] * distanceKernel[abs(y)];
            float depth = DistanceAndAO[paddedPixelPos.x + x][paddedPixelPos.y + y].x;
            float depthDifference = depth - centerDepth;
            float depthSigma = GetGlobalConst(rtao, filterDepthSigma);

            weight *= exp(-(depthDifference * depthDifference) / (2.f * depthSigma * depthSigma));

            sum += DistanceAndAO[paddedPixelPos.x + x][paddedPixelPos.y + y].y * weight;
            totalWeight += weight;
        }
    }
    return sum / totalWeight;
}

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void CS(uint3 GroupID : SV_GroupID, uint GroupIndex : SV_GroupIndex, uint3 GroupThreadID : SV_GroupThreadID, uint3 DispatchThreadID : SV_DispatchThreadID)
{
    // Get the (bindless) resources
    RWTexture2D<float4> GBufferB = GetRWTex2D(GBUFFERB_INDEX);
    RWTexture2D<float4> RTAOOutput = GetRWTex2D(RTAO_OUTPUT_INDEX);
    RWTexture2D<float4> RTAORaw = GetRWTex2D(RTAO_RAW_INDEX);

    // Load hit distance and ambient occlusion for this pixel and share it with the thread group
    {
        int2 pixelBase = int2(GroupID.xy) * int2(BLOCK_SIZE, BLOCK_SIZE) - int2(c_radius, c_radius);
        int  pixelIndex = int(GroupIndex);

        static const int c_unpaddedPixelCount = BLOCK_SIZE * BLOCK_SIZE;
        static const int c_loopCount = (c_paddedPixelCount % c_unpaddedPixelCount) ? (1 + c_paddedPixelCount / c_unpaddedPixelCount) : (c_paddedPixelCount / c_unpaddedPixelCount);

        // Cooperatively load the data into shared memory
        for (int i = 0; i < c_loopCount; ++i)
        {
            int2 paddedPixel = int2(pixelIndex % c_paddedPixelWidth, pixelIndex / c_paddedPixelWidth);
            if (paddedPixel.x < c_paddedPixelWidth && paddedPixel.y < c_paddedPixelWidth)
            {
                int2 srcPixel = paddedPixel + pixelBase;
                if (srcPixel.x < 0 || srcPixel.y < 0 || srcPixel.x >= GetGlobalConst(rtao, filterBufferWidth) || srcPixel.y >= GetGlobalConst(rtao, filterBufferHeight))
                {
                    DistanceAndAO[paddedPixel.x][paddedPixel.y] = float2(0.f, 0.f);
                }
                else
                {
                    float distance = GBufferB.Load(srcPixel).w;
                    float occlusion = RTAORaw.Load(srcPixel).x;
                    DistanceAndAO[paddedPixel.x][paddedPixel.y] = float2(distance, occlusion);
                }
            }

            // Move to the next pixel
            pixelIndex += (BLOCK_SIZE * BLOCK_SIZE);
        }
    }

    // Wait for the thread group to sync
    GroupMemoryBarrierWithGroupSync();

    // Filter using group shared memory
    {
        int2 pixelIndex = int2(GroupThreadID.xy) + int2(c_radius, c_radius);
        RTAOOutput[DispatchThreadID.xy] = FilterAO(pixelIndex);
    }
}
