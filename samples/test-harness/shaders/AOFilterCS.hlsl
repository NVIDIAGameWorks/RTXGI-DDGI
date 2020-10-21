/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "include/ComputeRS.hlsl"

static const int c_radius = 5;
static const int c_paddedPixelWidth = BLOCK_SIZE + c_radius * 2;
static const int c_paddedPixelCount = c_paddedPixelWidth * c_paddedPixelWidth;

groupshared float2 s_depthAO[c_paddedPixelWidth][c_paddedPixelWidth];

float FilterAO(int2 paddedPixelPos)
{
    if (AOFilterDistanceSigma <= 0.f || AOFilterDepthSigma <= 0.f)
    {
        return s_depthAO[paddedPixelPos.x][paddedPixelPos.y].y;
    }

    float distanceKernel[c_radius + 1];
    distanceKernel[0] = DistKernel0;
    distanceKernel[1] = DistKernel1;
    distanceKernel[2] = DistKernel2;
    distanceKernel[3] = DistKernel3;
    distanceKernel[4] = DistKernel4;
    distanceKernel[5] = DistKernel5;

    float totalWeight = 0.f;
    float sum = 0.f;
    float centerDepth = s_depthAO[paddedPixelPos.x][paddedPixelPos.y].x;

    for (int y = -c_radius; y <= c_radius; ++y)
    {
        for (int x = -c_radius; x <= c_radius; ++x)
        {
            float weight = distanceKernel[abs(x)] * distanceKernel[abs(y)];
            float depth = s_depthAO[paddedPixelPos.x + x][paddedPixelPos.y + y].x;
            float depthDifference = depth - centerDepth;

            weight *= exp(-(depthDifference * depthDifference) / (2.f * AOFilterDepthSigma * AOFilterDepthSigma));

            sum += s_depthAO[paddedPixelPos.x + x][paddedPixelPos.y + y].y * weight;
            totalWeight += weight;
        }
    }

    return sum / totalWeight;
}

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void CS(uint3 GroupID : SV_GroupID, uint GroupIndex : SV_GroupIndex, uint3 GroupThreadID : SV_GroupThreadID, uint3 DispatchThreadID : SV_DispatchThreadID)
{
    // Load depth and AO for this thread group so we can filter it quickly with group shared memory
    {
        int2 pixelBase = int2(GroupID.xy) * int2(BLOCK_SIZE, BLOCK_SIZE) - int2(c_radius, c_radius);
        int  pixelIndex = int(GroupIndex);

        static const int c_unpaddedPixelCount = BLOCK_SIZE * BLOCK_SIZE;
        static const int c_loopCount = (c_paddedPixelCount % c_unpaddedPixelCount) ? (1 + c_paddedPixelCount / c_unpaddedPixelCount) : (c_paddedPixelCount / c_unpaddedPixelCount);

        // Loop to fill the shared memory
        for (int i = 0; i < c_loopCount; ++i)
        {
            int2 paddedPixel = int2(pixelIndex % c_paddedPixelWidth, pixelIndex / c_paddedPixelWidth);

            if (paddedPixel.x < c_paddedPixelWidth && paddedPixel.y < c_paddedPixelWidth)
            {
                int2 srcPixel = paddedPixel + pixelBase;

                if (srcPixel.x < 0 || srcPixel.y < 0 || srcPixel.x >= GBufferWidth || srcPixel.y >= GBufferHeight)
                {
                    s_depthAO[paddedPixel.x][paddedPixel.y] = float2(0.f, 0.f);
                }
                else
                {
                    float depth = GBufferB.Load(srcPixel).w;
                    float AO = RTAORaw.Load(srcPixel);
                    s_depthAO[paddedPixel.x][paddedPixel.y] = float2(depth, AO);
                }
            }

            // Move to the next pixel
            pixelIndex += (BLOCK_SIZE*BLOCK_SIZE);
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // Filter using group shared memory
    {
        int2 globalPixelIndex = int2(DispatchThreadID.xy);
        int2 localPixelIndex = int2(GroupThreadID.xy) + int2(c_radius, c_radius);

        RTAOFiltered[globalPixelIndex] = FilterAO(localPixelIndex);
    }
}
