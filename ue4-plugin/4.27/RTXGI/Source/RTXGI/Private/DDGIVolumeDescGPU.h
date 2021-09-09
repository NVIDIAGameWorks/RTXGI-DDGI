/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#ifndef RTXGI_DDGI_VOLUME_DESC_GPU_H
#define RTXGI_DDGI_VOLUME_DESC_GPU_H

/**
 * DDGIVolumeDescGPU
 * The condensed DDGIVolume descriptor for use on the GPU.
 */
BEGIN_UNIFORM_BUFFER_STRUCT(FDDGIVolumeDescGPU, )
	SHADER_PARAMETER(FVector,       origin)
	SHADER_PARAMETER(int,           numRaysPerProbe)
	SHADER_PARAMETER(FVector4,      rotation)
	SHADER_PARAMETER(FVector,       probeGridSpacing)
	SHADER_PARAMETER(float,         probeMaxRayDistance)
	SHADER_PARAMETER(FIntVector,    probeGridCounts)
	SHADER_PARAMETER(float,         probeDistanceExponent)
	SHADER_PARAMETER(float,         probeHysteresis)
	SHADER_PARAMETER(float,         probeChangeThreshold)
	SHADER_PARAMETER(float,         probeBrightnessThreshold)
	SHADER_PARAMETER(float,         probeIrradianceEncodingGamma)
	SHADER_PARAMETER(float,         probeInverseIrradianceEncodingGamma)
	SHADER_PARAMETER(int,           probeNumIrradianceTexels)
	SHADER_PARAMETER(int,           probeNumDistanceTexels)
	SHADER_PARAMETER(float,         normalBias)
	SHADER_PARAMETER(float,         viewBias)
	SHADER_PARAMETER(FMatrix,       probeRayRotationTransform)

	SHADER_PARAMETER(int,           volumeMovementType)
	SHADER_PARAMETER(FIntVector,    probeScrollOffsets)

	SHADER_PARAMETER(float,         probeBackfaceThreshold)
	SHADER_PARAMETER(float,         probeMinFrontfaceDistance)
END_UNIFORM_BUFFER_STRUCT()

#endif /* RTXGI_DDGI_VOLUME_DESC_GPU_H */
