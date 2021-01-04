/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef COMPUTE_ROOT_SIGNATURE_HLSL
#define COMPUTE_ROOT_SIGNATURE_HLSL

// ---- CBV/SRV/UAV Descriptor Heap ------------------------------------------------

RWTexture2D<float4>                    GBufferA                  : register(u0);
RWTexture2D<float4>                    GBufferB                  : register(u1);
RWTexture2D<float4>                    GBufferC                  : register(u2);
RWTexture2D<float4>                    GBufferD                  : register(u3);
RWTexture2D<float>                     RTAORaw                   : register(u4);
RWTexture2D<float>                     RTAOFiltered              : register(u5);

// ---- Root Constants -------------------------------------------------------------

cbuffer ComputeRootConstants : register(b0)
{
    float AOFilterDistanceSigma;
    float AOFilterDepthSigma;
    uint  GBufferWidth;
    uint  GBufferHeight;
    float DistKernel0;
    float DistKernel1;
    float DistKernel2;
    float DistKernel3;
    float DistKernel4;
    float DistKernel5;
    uint2 AOPad;
};

#endif /* COMPUTE_ROOT_SIGNATURE_HLSL */
