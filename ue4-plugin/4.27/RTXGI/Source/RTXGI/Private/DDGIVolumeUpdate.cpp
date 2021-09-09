/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "DDGIVolumeUpdate.h"

// UE4 public interfaces
#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "SceneView.h"
#include "RenderGraph.h"
#include "RayGenShaderUtils.h"
#include "ShaderParameterStruct.h"
#include "GlobalShader.h"
#include "RTXGIPluginSettings.h"

// UE4 private interfaces
#include "ReflectionEnvironment.h"
#include "FogRendering.h"
#include "SceneRendering.h"
#include "SceneTextureParameters.h"
#include "RayTracing/RayTracingLighting.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"

#include <cmath>

// local includes
#include "DDGIVolumeComponent.h"
#include "DDGIVolumeDescGPU.h"

#define LOCTEXT_NAMESPACE "FRTXGIPlugin"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
static TAutoConsoleVariable<int> CVarDDGIProbesTextureVis(
	TEXT("r.RTXGI.DDGI.ProbesTextureVis"),
	0,
	TEXT("If 1, will render what the probes see. If 2, will show misses (blue), hits (green), backfaces (red). \'vis DDGIProbesTexure\' to see the output.\n"),
	ECVF_RenderThreadSafe);
#endif

#if RHI_RAYTRACING

static FMatrix ComputeRandomRotation()
{
	// This approach is based on James Arvo's implementation from Graphics Gems 3 (pg 117-120).
	// Also available at: http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.53.1357&rep=rep1&type=pdf

	// Setup a random rotation matrix using 3 uniform RVs
	float u1 = 2.f * 3.14159265359 * FMath::FRand();
	float cos1 = std::cosf(u1);
	float sin1 = std::sinf(u1);

	float u2 = 2.f * 3.14159265359 * FMath::FRand();
	float cos2 = std::cosf(u2);
	float sin2 = std::sinf(u2);

	float u3 = FMath::FRand();
	float sq3 = 2.f * std::sqrtf(u3 * (1.f - u3));

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

	return FMatrix(
		FPlane( _11, _12, _13, 0.f ),
		FPlane(_21, _22, _23, 0.f ),
		FPlane(_31, _32, _33, 0.f ),
		FPlane(0.f, 0.f, 0.f, 1.f )
	);
}

class FRayTracingRTXGIProbeUpdateRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingRTXGIProbeUpdateRGS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingRTXGIProbeUpdateRGS, FGlobalShader)

	class FEnableTwoSidedGeometryDim : SHADER_PERMUTATION_BOOL("ENABLE_TWO_SIDED_GEOMETRY"); // If false, it will cull back face triangles. We want this on for probe relocation and to stop light leak.
	class FEnableMaterialsDim : SHADER_PERMUTATION_BOOL("ENABLE_MATERIALS");                 // If false, forces the geo to opaque (no alpha test). We want this off for speed.
	class FEnableRelocation : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_PROBE_RELOCATION");
	class FFormatRadiance : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_FORMAT_RADIANCE");
	class FFormatIrradiance : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_FORMAT_IRRADIANCE");
	class FEnableScrolling : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_INFINITE_SCROLLING_VOLUME");
	class FSkyLight : SHADER_PERMUTATION_INT("RTXGI_DDGI_SKY_LIGHT_TYPE", 3);

	using FPermutationDomain = TShaderPermutationDomain<FEnableTwoSidedGeometryDim, FEnableMaterialsDim, FEnableRelocation, FFormatRadiance, FFormatIrradiance, FEnableScrolling, FSkyLight>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("RTXGI_DDGI_PROBE_CLASSIFICATION"), FDDGIVolumeSceneProxy::FComponentData::c_RTXGI_DDGI_PROBE_CLASSIFICATION ? 1 : 0);

		// Set to 1 to be able to visualize this in the editor by typing "vis DDGIVolumeUpdateDebug" and later "vis none" to make it go away.
		// Set to 0 to disable and deadstrip everything related
		OutEnvironment.SetDefine(TEXT("DDGIVolumeUpdateDebug"), 0);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)

		SHADER_PARAMETER(uint32, FrameRandomSeed)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DDGIVolume_ProbeIrradiance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DDGIVolume_ProbeDistance)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DDGIVolume_ProbeOffsets)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, DDGIVolume_ProbeStates)
		SHADER_PARAMETER_SAMPLER(SamplerState, DDGIVolume_LinearClampSampler)
		SHADER_PARAMETER(FVector, DDGIVolume_Radius)
		SHADER_PARAMETER(float, DDGIVolume_IrradianceScalar)
		SHADER_PARAMETER(float, DDGIVolume_EmissiveMultiplier)
		SHADER_PARAMETER(int, DDGIVolume_ProbeIndexStart)
		SHADER_PARAMETER(int, DDGIVolume_ProbeIndexCount)

		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FDDGIVolumeDescGPU, DDGIVolume)

		SHADER_PARAMETER(FVector, Sky_Color)
		SHADER_PARAMETER_TEXTURE(Texture2D, Sky_Texture)
		SHADER_PARAMETER_SAMPLER(SamplerState, Sky_TextureSampler)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RadianceOutput)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DebugOutput)  // Per unreal RDG presentation, this is deadstripped if the shader doesn't write to it

		// assorted things needed by material resolves, even though some don't make sense outside of screenspace
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SSProfilesTexture)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_REF(FRaytracingLightDataPacked, LightDataPacked)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FRayTracingRTXGIProbeUpdateRGS, "/Plugin/RTXGI/Private/ProbeUpdateRGS.usf", "ProbeUpdateRGS", SF_RayGen);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

class FRayTracingRTXGIProbeViewRGS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FRayTracingRTXGIProbeViewRGS)
	SHADER_USE_ROOT_PARAMETER_STRUCT(FRayTracingRTXGIProbeViewRGS, FGlobalShader)

	class FEnableTwoSidedGeometryDim : SHADER_PERMUTATION_BOOL("ENABLE_TWO_SIDED_GEOMETRY"); // If false, it will cull back face triangles. We want this on for probe relocation and to stop light leak.
	class FEnableMaterialsDim : SHADER_PERMUTATION_BOOL("ENABLE_MATERIALS");                 // If false, forces the geo to opaque (no alpha test). We want this off for speed.
	class FVolumeDebugView : SHADER_PERMUTATION_INT("VOLUME_DEBUG_VIEW", 2);

	using FPermutationDomain = TShaderPermutationDomain<FEnableTwoSidedGeometryDim, FEnableMaterialsDim, FVolumeDebugView>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("RTXGI_DDGI_PROBE_CLASSIFICATION"), FDDGIVolumeSceneProxy::FComponentData::c_RTXGI_DDGI_PROBE_CLASSIFICATION ? 1 : 0);
		OutEnvironment.SetDefine(TEXT("RTXGI_DDGI_PROBE_RELOCATION"), 0);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_SRV(RaytracingAccelerationStructure, TLAS)

		SHADER_PARAMETER(uint32, FrameRandomSeed)

		SHADER_PARAMETER(FVector, CameraPos)
		SHADER_PARAMETER(FMatrix, CameraMatrix)

		SHADER_PARAMETER(float, DDGIVolume_PreExposure)
		SHADER_PARAMETER(int32, DDGIVolume_ShouldUsePreExposure)

		SHADER_PARAMETER(FVector, Sky_Color)
		SHADER_PARAMETER_TEXTURE(Texture2D, Sky_Texture)
		SHADER_PARAMETER_SAMPLER(SamplerState, Sky_TextureSampler)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RadianceOutput)

		// assorted things needed by material resolves, even though some don't make sense outside of screenspace
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, SSProfilesTexture)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_REF(FRaytracingLightDataPacked, LightDataPacked)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FRayTracingRTXGIProbeViewRGS, "/Plugin/RTXGI/Private/ProbeViewRGS.usf", "ProbeViewRGS", SF_RayGen);

#endif // #if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

class FDDGIIrradianceBlend : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDDGIIrradianceBlend)
	SHADER_USE_PARAMETER_STRUCT(FDDGIIrradianceBlend, FGlobalShader)

	class FRaysPerProbeEnum : SHADER_PERMUTATION_SPARSE_INT("RAYS_PER_PROBE",
		int32(EDDGIRaysPerProbe::n144),
		int32(EDDGIRaysPerProbe::n288),
		int32(EDDGIRaysPerProbe::n432),
		int32(EDDGIRaysPerProbe::n576),
		int32(EDDGIRaysPerProbe::n720),
		int32(EDDGIRaysPerProbe::n864),
		int32(EDDGIRaysPerProbe::n1008));
	class FEnableRelocation : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_PROBE_RELOCATION");
	class FFormatRadiance : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_FORMAT_RADIANCE");
	class FFormatIrradiance : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_FORMAT_IRRADIANCE");
	class FEnableScrolling : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_INFINITE_SCROLLING_VOLUME");

	using FPermutationDomain = TShaderPermutationDomain<FRaysPerProbeEnum, FEnableRelocation, FFormatRadiance, FFormatIrradiance, FEnableScrolling>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("RTXGI_DDGI_PROBE_CLASSIFICATION"), FDDGIVolumeSceneProxy::FComponentData::c_RTXGI_DDGI_PROBE_CLASSIFICATION ? 1 : 0);

		OutEnvironment.SetDefine(TEXT("PROBE_NUM_TEXELS"), FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsIrradiance);
		OutEnvironment.SetDefine(TEXT("RTXGI_DDGI_BLEND_RADIANCE"), 1);

		// Set to 1 to be able to visualize this in the editor by typing "vis DDGIIrradianceBlendDebug" and later "vis none" to make it go away.
		// Set to 0 to disable and deadstrip everything related
		OutEnvironment.SetDefine(TEXT("DDGIIrradianceBlendDebug"), 0);

		// needed for a typed UAV load. This already assumes we are raytracing, so should be fine.
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER(int, ProbeIndexStart)
		SHADER_PARAMETER(int, ProbeIndexCount)

		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FDDGIVolumeDescGPU, DDGIVolume)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DDGIVolumeRayDataUAV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DDGIVolumeProbeDataUAV)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, DDGIVolumeProbeStatesTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, DDGIProbeScrollSpace)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DebugOutput)  // Per unreal RDG presentation, this is deadstripped if the shader doesn't write to it

	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FDDGIIrradianceBlend, "/Plugin/RTXGI/Private/SDK/ddgi/ProbeBlendingCS.usf", "DDGIProbeBlendingCS", SF_Compute);

class FDDGIDistanceBlend : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDDGIDistanceBlend)
	SHADER_USE_PARAMETER_STRUCT(FDDGIDistanceBlend, FGlobalShader)

	class FRaysPerProbeEnum : SHADER_PERMUTATION_SPARSE_INT("RAYS_PER_PROBE",
		int32(EDDGIRaysPerProbe::n144),
		int32(EDDGIRaysPerProbe::n288),
		int32(EDDGIRaysPerProbe::n432),
		int32(EDDGIRaysPerProbe::n576),
		int32(EDDGIRaysPerProbe::n720),
		int32(EDDGIRaysPerProbe::n864),
		int32(EDDGIRaysPerProbe::n1008));
	class FEnableRelocation : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_PROBE_RELOCATION");
	class FFormatRadiance : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_FORMAT_RADIANCE");
	class FFormatIrradiance : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_FORMAT_IRRADIANCE");
	class FEnableScrolling : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_INFINITE_SCROLLING_VOLUME");

	using FPermutationDomain = TShaderPermutationDomain<FRaysPerProbeEnum, FEnableRelocation, FFormatRadiance, FFormatIrradiance, FEnableScrolling>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("RTXGI_DDGI_PROBE_CLASSIFICATION"), FDDGIVolumeSceneProxy::FComponentData::c_RTXGI_DDGI_PROBE_CLASSIFICATION ? 1 : 0);
		OutEnvironment.SetDefine(TEXT("PROBE_NUM_TEXELS"), FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsDistance);
		OutEnvironment.SetDefine(TEXT("RTXGI_DDGI_BLEND_RADIANCE"), 0);

		// Set to 1 to be able to visualize this in the editor by typing "vis DDGIDistanceBlendDebug" and later "vis none" to make it go away.
		// Set to 0 to disable and deadstrip everything related
		OutEnvironment.SetDefine(TEXT("DDGIDistanceBlendDebug"), 0);

		// needed for a typed UAV load. This already assumes we are raytracing, so should be fine.
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER(int, ProbeIndexStart)
		SHADER_PARAMETER(int, ProbeIndexCount)

		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FDDGIVolumeDescGPU, DDGIVolume)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DDGIVolumeRayDataUAV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DDGIVolumeProbeDataUAV)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, DDGIVolumeProbeStatesTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, DDGIProbeScrollSpace)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DebugOutput)  // Per unreal RDG presentation, this is deadstripped if the shader doesn't write to it

	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FDDGIDistanceBlend, "/Plugin/RTXGI/Private/SDK/ddgi/ProbeBlendingCS.usf", "DDGIProbeBlendingCS", SF_Compute);

class FDDGIBorderRowUpdate : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDDGIBorderRowUpdate)
	SHADER_USE_PARAMETER_STRUCT(FDDGIBorderRowUpdate, FGlobalShader)

	class FProbeNumTexels : SHADER_PERMUTATION_SPARSE_INT("PROBE_NUM_TEXELS",
		FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsIrradiance,
		FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsDistance);

	using FPermutationDomain = TShaderPermutationDomain<FProbeNumTexels>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// needed for a typed UAV load. This already assumes we are raytracing, so should be fine.
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DDGIVolumeProbeDataUAV)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FDDGIBorderRowUpdate, "/Plugin/RTXGI/Private/SDK/ddgi/ProbeBorderUpdateCS.usf", "DDGIProbeBorderRowUpdateCS", SF_Compute);

class FDDGIBorderColumnUpdate : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDDGIBorderColumnUpdate)
	SHADER_USE_PARAMETER_STRUCT(FDDGIBorderColumnUpdate, FGlobalShader)

	class FProbeNumTexels : SHADER_PERMUTATION_SPARSE_INT("PROBE_NUM_TEXELS",
		FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsIrradiance,
		FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsDistance);

	using FPermutationDomain = TShaderPermutationDomain<FProbeNumTexels>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// needed for a typed UAV load. This already assumes we are raytracing, so should be fine.
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DDGIVolumeProbeDataUAV)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FDDGIBorderColumnUpdate, "/Plugin/RTXGI/Private/SDK/ddgi/ProbeBorderUpdateCS.usf", "DDGIProbeBorderColumnUpdateCS", SF_Compute);

IMPLEMENT_UNIFORM_BUFFER_STRUCT(FDDGIVolumeDescGPU, "DDGIVolume");

class FDDGIProbesRelocate : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDDGIProbesRelocate)
	SHADER_USE_PARAMETER_STRUCT(FDDGIProbesRelocate, FGlobalShader)

	class FFormatRadiance : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_FORMAT_RADIANCE");
	class FFormatIrradiance : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_FORMAT_IRRADIANCE");
	class FEnableScrolling : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_INFINITE_SCROLLING_VOLUME");

	using FPermutationDomain = TShaderPermutationDomain<FFormatRadiance, FFormatIrradiance, FEnableScrolling>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("RTXGI_DDGI_PROBE_CLASSIFICATION"), FDDGIVolumeSceneProxy::FComponentData::c_RTXGI_DDGI_PROBE_CLASSIFICATION ? 1 : 0);
		OutEnvironment.SetDefine(TEXT("RTXGI_DDGI_PROBE_RELOCATION"), 1);

		// needed for a typed UAV load. This already assumes we are raytracing, so should be fine.
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(float, ProbeDistanceScale)
		SHADER_PARAMETER(int, ProbeIndexStart)
		SHADER_PARAMETER(int, ProbeIndexCount)

		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FDDGIVolumeDescGPU, DDGIVolume)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DDGIVolumeRayDataUAV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DDGIVolumeProbeOffsetsUAV)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FDDGIProbesRelocate, "/Plugin/RTXGI/Private/SDK/ddgi/ProbeRelocationCS.usf", "DDGIProbeRelocationCS", SF_Compute);

class FDDGIProbesClassify : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDDGIProbesClassify)
	SHADER_USE_PARAMETER_STRUCT(FDDGIProbesClassify, FGlobalShader)

	class FEnableRelocation : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_PROBE_RELOCATION");
	class FFormatRadiance : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_FORMAT_RADIANCE");
	class FFormatIrradiance : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_FORMAT_IRRADIANCE");
	class FEnableScrolling : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_INFINITE_SCROLLING_VOLUME");

	using FPermutationDomain = TShaderPermutationDomain<FEnableRelocation, FFormatRadiance, FFormatIrradiance, FEnableScrolling>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("RTXGI_DDGI_PROBE_CLASSIFICATION"), 1);

		// needed for a typed UAV load. This already assumes we are raytracing, so should be fine.
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(int, ProbeIndexStart)
		SHADER_PARAMETER(int, ProbeIndexCount)

		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FDDGIVolumeDescGPU, DDGIVolume)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, DDGIVolumeRayDataUAV)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, DDGIVolumeProbeStatesUAV)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FDDGIProbesClassify, "/Plugin/RTXGI/Private/SDK/ddgi/ProbeStateClassifierCS.usf", "DDGIProbeStateClassifierCS", SF_Compute);

#endif // RHI_RAYTRACING

namespace DDGIVolumeUpdate
{
#if RHI_RAYTRACING
	FDelegateHandle AnyRayTracingPassEnabledHandle;
	FDelegateHandle PrepareRayTracingHandle;

	void DDGIUpdateVolume_RenderThread(const FScene& Scene, const FViewInfo& View, FRDGBuilder& GraphBuilder, FDDGIVolumeSceneProxy* VolProxy);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	void DDGIUpdateVolume_RenderThread_DDGIProbesTextureVis(const FScene& Scene, const FViewInfo& View, FRDGBuilder& GraphBuilder);
#endif 

	void DDGIUpdateVolume_RenderThread_RTRadiance(const FScene& Scene, const FViewInfo& View, FRDGBuilder& GraphBuilder, FDDGIVolumeSceneProxy* VolProxy, const FMatrix& ProbeRayRotationTransform, FRDGTextureRef ProbesRadianceTex, FRDGTextureUAVRef ProbesRadianceUAV, bool highBitCount);
	void DDGIUpdateVolume_RenderThread_IrradianceBlend(const FViewInfo& View, FRDGBuilder& GraphBuilder, FDDGIVolumeSceneProxy* VolProxy, const FMatrix& ProbeRayRotationTransform, FRDGTextureUAVRef ProbesRadianceUAV, bool highBitCount);
	void DDGIUpdateVolume_RenderThread_DistanceBlend(const FViewInfo& View, FRDGBuilder& GraphBuilder, FDDGIVolumeSceneProxy* VolProxy, const FMatrix& ProbeRayRotationTransform, FRDGTextureUAVRef ProbesRadianceUAV, bool highBitCount);
	void DDGIUpdateVolume_RenderThread_IrradianceBorderUpdate(const FViewInfo& View, FRDGBuilder& GraphBuilder, FDDGIVolumeSceneProxy* VolProxy);
	void DDGIUpdateVolume_RenderThread_DistanceBorderUpdate(const FViewInfo& View, FRDGBuilder& GraphBuilder, FDDGIVolumeSceneProxy* VolProxy);
	void DDGIUpdateVolume_RenderThread_RelocateProbes(FRDGBuilder& GraphBuilder, FDDGIVolumeSceneProxy* VolProxy, const FMatrix& ProbeRayRotationTransform, FRDGTextureUAVRef ProbesRadianceUAV, bool highBitCount);
	void DDGIUpdateVolume_RenderThread_ClassifyProbes(FRDGBuilder& GraphBuilder, FDDGIVolumeSceneProxy* VolProxy, FRDGTextureUAVRef ProbesRadianceUAV, bool highBitCount);

	void PrepareRayTracingShaders(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders);
#endif // RHI_RAYTRACING

	// ---------------------- IMPLEMENTATION ------------------

	void Startup()
	{
#if RHI_RAYTRACING
		FGlobalIlluminationExperimentalPluginDelegates::FPrepareRayTracing& PRTDelegate = FGlobalIlluminationExperimentalPluginDelegates::PrepareRayTracing();
		PrepareRayTracingHandle = PRTDelegate.AddStatic(PrepareRayTracingShaders);

		FGlobalIlluminationExperimentalPluginDelegates::FAnyRayTracingPassEnabled& ARTPEDelegate = FGlobalIlluminationExperimentalPluginDelegates::AnyRayTracingPassEnabled();
		AnyRayTracingPassEnabledHandle = ARTPEDelegate.AddStatic(
			[](bool& anyEnabled)
			{
				anyEnabled |= true;
			}
		);
#endif // RHI_RAYTRACING
	}

	void Shutdown()
	{
#if RHI_RAYTRACING
		FGlobalIlluminationExperimentalPluginDelegates::FPrepareRayTracing& PRTDelegate = FGlobalIlluminationExperimentalPluginDelegates::PrepareRayTracing();
		check(PrepareRayTracingHandle.IsValid());
		PRTDelegate.Remove(PrepareRayTracingHandle);

		FGlobalIlluminationExperimentalPluginDelegates::FAnyRayTracingPassEnabled& ARTPEDelegate = FGlobalIlluminationExperimentalPluginDelegates::AnyRayTracingPassEnabled();
		check(AnyRayTracingPassEnabledHandle.IsValid());
		ARTPEDelegate.Remove(AnyRayTracingPassEnabledHandle);
#endif // RHI_RAYTRACING
	}

	void DDGIUpdatePerFrame_RenderThread(const FScene& Scene, const FViewInfo& View, FRDGBuilder& GraphBuilder)
	{
		check(IsInRenderingThread() || IsInParallelRenderingThread());

		// Gather the list of volumes to update and load data if it's available.
		// Loading static data is the only thing that happens if ray tracing is not available.
		TArray<FDDGIVolumeSceneProxy*> sceneVolumes;
		float totalPriority = 0.0f;
		for (FDDGIVolumeSceneProxy* proxy : FDDGIVolumeSceneProxy::AllProxiesReadyForRender_RenderThread)
		{
			// Copy the volume's texture data to the GPU, if loading from disk has finished
			if (proxy->TextureLoadContext.ReadyForLoad)
			{
				if (proxy->TextureLoadContext.Irradiance.Texture)
				{
					TRefCountPtr<IPooledRenderTarget> IrradianceLoaded = CreateRenderTarget(proxy->TextureLoadContext.Irradiance.Texture.GetReference(), TEXT("DDGIIrradianceLoaded"));
					AddCopyTexturePass(GraphBuilder, GraphBuilder.RegisterExternalTexture(IrradianceLoaded), GraphBuilder.RegisterExternalTexture(proxy->ProbesIrradiance), FRHICopyTextureInfo{});
				}

				if (proxy->TextureLoadContext.Distance.Texture)
				{
					TRefCountPtr<IPooledRenderTarget> DistanceLoaded = CreateRenderTarget(proxy->TextureLoadContext.Distance.Texture.GetReference(), TEXT("DDGIDistanceLoaded"));
					AddCopyTexturePass(GraphBuilder, GraphBuilder.RegisterExternalTexture(DistanceLoaded), GraphBuilder.RegisterExternalTexture(proxy->ProbesDistance), FRHICopyTextureInfo{});
				}

				if (proxy->TextureLoadContext.Offsets.Texture && proxy->ProbesOffsets)
				{
					TRefCountPtr<IPooledRenderTarget> OffsetsLoaded = CreateRenderTarget(proxy->TextureLoadContext.Offsets.Texture.GetReference(), TEXT("DDGIOffsetsLoaded"));
					AddCopyTexturePass(GraphBuilder, GraphBuilder.RegisterExternalTexture(OffsetsLoaded), GraphBuilder.RegisterExternalTexture(proxy->ProbesOffsets), FRHICopyTextureInfo{});
				}

				if (proxy->TextureLoadContext.States.Texture && proxy->ProbesStates)
				{
					TRefCountPtr<IPooledRenderTarget> StatesLoaded = CreateRenderTarget(proxy->TextureLoadContext.States.Texture.GetReference(), TEXT("DDGIStatesLoaded"));
					AddCopyTexturePass(GraphBuilder, GraphBuilder.RegisterExternalTexture(StatesLoaded), GraphBuilder.RegisterExternalTexture(proxy->ProbesStates), FRHICopyTextureInfo{});
				}

				proxy->TextureLoadContext.Clear();
			}

			// Don't update the volume if it isn't part of the current scene
			if (proxy->OwningScene != &Scene) continue;

			// Don't update static runtime volumes during gameplay
			if (View.bIsGameView && proxy->ComponentData.RuntimeStatic) continue;

			// Don't update the volume if it is disabled
			if (!proxy->ComponentData.EnableVolume) continue;

			sceneVolumes.Add(proxy);
			totalPriority += proxy->ComponentData.UpdatePriority;
		}

#if RHI_RAYTRACING

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		DDGIUpdateVolume_RenderThread_DDGIProbesTextureVis(Scene, View, GraphBuilder);
#endif

		// Advance the scene's round robin value by the golden ratio (conjugate) and use that 
		// as a "random number" to give each volume a fair turn at recieving an update.
		float& value = FDDGIVolumeSceneProxy::SceneRoundRobinValue.FindOrAdd(&Scene);
		value += 0.61803398875f;
		value -= floor(value);

		// Update the relevant volumes with ray tracing
		float desiredPriority = totalPriority * value;
		for (int index = 0; index < sceneVolumes.Num(); ++index)
		{
			desiredPriority -= sceneVolumes[index]->ComponentData.UpdatePriority;
			if (desiredPriority <= 0.0f || index == sceneVolumes.Num() - 1)
			{
				DDGIUpdateVolume_RenderThread(Scene, View, GraphBuilder, sceneVolumes[index]);
				break;
			}
		}

#endif // RHI_RAYTRACING

	}

#if RHI_RAYTRACING

	void PrepareRayTracingShaders(const FViewInfo& View, TArray<FRHIRayTracingShader*>& OutRayGenShaders)
	{
		const auto FeatureLevel = GMaxRHIFeatureLevel;
		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

		for (int i = 0; i < 8; ++i)
		{
			for (int j = 0; j < 3; ++j)
			{
				FRayTracingRTXGIProbeUpdateRGS::FPermutationDomain PermutationVector;
				PermutationVector.Set<FRayTracingRTXGIProbeUpdateRGS::FEnableTwoSidedGeometryDim>(true);
				PermutationVector.Set<FRayTracingRTXGIProbeUpdateRGS::FEnableMaterialsDim>(false);
				PermutationVector.Set<FRayTracingRTXGIProbeUpdateRGS::FEnableRelocation>((i & 1) != 0 ? true : false);
				PermutationVector.Set<FRayTracingRTXGIProbeUpdateRGS::FFormatRadiance>((i & 2) != 0 ? true : false);
				PermutationVector.Set<FRayTracingRTXGIProbeUpdateRGS::FFormatIrradiance>((i & 2) != 0 ? true : false);
				PermutationVector.Set<FRayTracingRTXGIProbeUpdateRGS::FEnableScrolling>((i & 4) != 0 ? true : false);
				PermutationVector.Set<FRayTracingRTXGIProbeUpdateRGS::FSkyLight>(j);
				TShaderMapRef<FRayTracingRTXGIProbeUpdateRGS> RayGenerationShader(ShaderMap, PermutationVector);

				OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
			}
		}

		#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		for (int i = 0; i < 2; ++i)
		{
			FRayTracingRTXGIProbeViewRGS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FRayTracingRTXGIProbeViewRGS::FEnableTwoSidedGeometryDim>(true);
			PermutationVector.Set<FRayTracingRTXGIProbeViewRGS::FEnableMaterialsDim>(false);
			PermutationVector.Set<FRayTracingRTXGIProbeViewRGS::FVolumeDebugView>((i & 1) != 0 ? true : false);
			TShaderMapRef<FRayTracingRTXGIProbeViewRGS> RayGenerationShader(ShaderMap, PermutationVector);

			OutRayGenShaders.Add(RayGenerationShader.GetRayTracingShader());
		}
		#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	}

	bool ShouldRenderRayTracingEffect(bool bEffectEnabled)
	{
		if (!IsRayTracingEnabled())
		{
			return false;
		}

		static auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.ForceAllRayTracingEffects"));
		const int32 OverrideMode = CVar != nullptr ? CVar->GetInt() : -1;

		if (OverrideMode >= 0)
		{
			return OverrideMode > 0;
		}
		else
		{
			return bEffectEnabled;
		}
	}

	bool ShouldDynamicUpdate(const FViewInfo& View)
	{
		return ShouldRenderRayTracingEffect(true) && View.RayTracingScene.RayTracingSceneRHI != nullptr;
	}

	void DDGIUpdateVolume_RenderThread(const FScene& Scene, const FViewInfo& View, FRDGBuilder& GraphBuilder, FDDGIVolumeSceneProxy* VolProxy)
	{
		// Early out if ray tracing is not enabled
		if (!ShouldDynamicUpdate(View)) return;

		bool highBitCount = (GetDefault<URTXGIPluginSettings>()->IrradianceBits == EDDGIIrradianceBits::n32);

		// ASSUMES RENDERTHREAD
		check(IsInRenderingThread() || IsInParallelRenderingThread());
		check(VolProxy);

		FMatrix ProbeRayRotationTransform = ComputeRandomRotation();

		// Create the temporary radiance texture & UAV
		FRDGTextureRef ProbesRadianceTex;
		FRDGTextureUAVRef ProbesRadianceUAV;
		{
			const FDDGIVolumeSceneProxy::FComponentData& ComponentData = VolProxy->ComponentData;
			FRDGTextureDesc DDGIDebugOutputDesc = FRDGTextureDesc::Create2D(
				FIntPoint
				{
					(int32)ComponentData.GetNumRaysPerProbe(),
					(int32)ComponentData.ProbeCounts.X * ComponentData.ProbeCounts.Y * ComponentData.ProbeCounts.Z,
				},
				// This texture stores both color and distance
				highBitCount ? FDDGIVolumeSceneProxy::FComponentData::c_pixelFormatRadianceHighBitDepth : FDDGIVolumeSceneProxy::FComponentData::c_pixelFormatRadianceLowBitDepth,
				FClearValueBinding::None,
				TexCreate_ShaderResource | TexCreate_UAV
			);

			ProbesRadianceTex = GraphBuilder.CreateTexture(DDGIDebugOutputDesc, TEXT("DDGIVolumeRadiance"));
			ProbesRadianceUAV = GraphBuilder.CreateUAV(ProbesRadianceTex);
		}

		DDGIUpdateVolume_RenderThread_RTRadiance(Scene, View, GraphBuilder, VolProxy, ProbeRayRotationTransform, ProbesRadianceTex, ProbesRadianceUAV, highBitCount);
		DDGIUpdateVolume_RenderThread_IrradianceBlend(View, GraphBuilder, VolProxy, ProbeRayRotationTransform, ProbesRadianceUAV, highBitCount);
		DDGIUpdateVolume_RenderThread_DistanceBlend(View, GraphBuilder, VolProxy, ProbeRayRotationTransform, ProbesRadianceUAV, highBitCount);
		DDGIUpdateVolume_RenderThread_IrradianceBorderUpdate(View, GraphBuilder, VolProxy);
		DDGIUpdateVolume_RenderThread_DistanceBorderUpdate(View, GraphBuilder, VolProxy);

		if (VolProxy->ComponentData.EnableProbeRelocation)
		{
			DDGIUpdateVolume_RenderThread_RelocateProbes(GraphBuilder, VolProxy, ProbeRayRotationTransform, ProbesRadianceUAV, highBitCount);
		}

		if (FDDGIVolumeSceneProxy::FComponentData::c_RTXGI_DDGI_PROBE_CLASSIFICATION)
		{
			DDGIUpdateVolume_RenderThread_ClassifyProbes(GraphBuilder, VolProxy, ProbesRadianceUAV, highBitCount);
		}
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	void DDGIUpdateVolume_RenderThread_DDGIProbesTextureVis(const FScene& Scene, const FViewInfo& View, FRDGBuilder& GraphBuilder)
	{
		// Early out if not visualizing probes
		int DDGIProbesTextureVis = FMath::Clamp(CVarDDGIProbesTextureVis.GetValueOnRenderThread(), 0, 2);
		if (DDGIProbesTextureVis == 0 || View.RayTracingScene.RayTracingSceneRHI == nullptr) return;

		static const int c_probeVisWidth = 800;
		static const int c_probeVisHeight = 600;

		// create the texture and uav being rendered to
		FRDGTextureDesc ProbeVisTex = FRDGTextureDesc::Create2D(
			FIntPoint(c_probeVisWidth, c_probeVisHeight),
			EPixelFormat::PF_A32B32G32R32F,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV
		);
		FRDGTextureUAVRef ProbeVisUAV = GraphBuilder.CreateUAV(GraphBuilder.CreateTexture(ProbeVisTex, TEXT("DDGIProbesTexture")));

		// get the shader
		const auto FeatureLevel = GMaxRHIFeatureLevel;
		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);
		FRayTracingRTXGIProbeViewRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FRayTracingRTXGIProbeViewRGS::FEnableTwoSidedGeometryDim>(true);
		PermutationVector.Set<FRayTracingRTXGIProbeViewRGS::FEnableMaterialsDim>(false);
		PermutationVector.Set<FRayTracingRTXGIProbeViewRGS::FVolumeDebugView>(DDGIProbesTextureVis - 1);
		TShaderMapRef<FRayTracingRTXGIProbeViewRGS> RayGenerationShader(ShaderMap, PermutationVector);

		// fill out shader parameters
		FRayTracingRTXGIProbeViewRGS::FParameters DefaultPassParameters;
		FRayTracingRTXGIProbeViewRGS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRayTracingRTXGIProbeViewRGS::FParameters>();
		*PassParameters = DefaultPassParameters;

		PassParameters->DDGIVolume_PreExposure = View.PreExposure;
		PassParameters->DDGIVolume_ShouldUsePreExposure = View.Family->EngineShowFlags.Tonemapper;

		PassParameters->CameraPos = View.ViewMatrices.GetViewOrigin();
		PassParameters->CameraMatrix = View.ViewMatrices.GetViewMatrix().Inverse();

		PassParameters->TLAS = View.RayTracingScene.RayTracingSceneRHI->GetShaderResourceView();
		check(PassParameters->TLAS);
		PassParameters->RadianceOutput = ProbeVisUAV;
		PassParameters->FrameRandomSeed = GFrameNumber;

		// skylight parameters
		if (Scene.SkyLight && Scene.SkyLight->ProcessedTexture)
		{
			PassParameters->Sky_Color = FVector(Scene.SkyLight->GetEffectiveLightColor());
			PassParameters->Sky_Texture = Scene.SkyLight->ProcessedTexture->TextureRHI;
			PassParameters->Sky_TextureSampler = Scene.SkyLight->ProcessedTexture->SamplerStateRHI;
		}
		else
		{
			PassParameters->Sky_Color = FVector(0.0);
			PassParameters->Sky_Texture = GBlackTextureCube->TextureRHI;
			PassParameters->Sky_TextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		}

		PassParameters->SSProfilesTexture = GraphBuilder.RegisterExternalTexture(View.RayTracingSubSurfaceProfileTexture);
		PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
		PassParameters->LightDataPacked = View.RayTracingLightData.UniformBuffer;

		FIntPoint DispatchSize(c_probeVisWidth, c_probeVisHeight);

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("DDGI RTRadiance %dx%d", DispatchSize.X, DispatchSize.Y),
			PassParameters,
			ERDGPassFlags::Compute,
			[PassParameters, &View, RayGenerationShader, DispatchSize](FRHICommandList& RHICmdList)
			{
				FRayTracingShaderBindingsWriter GlobalResources;
				SetShaderParameters(GlobalResources, RayGenerationShader, *PassParameters);

				FRHIRayTracingScene* RayTracingSceneRHI = View.RayTracingScene.RayTracingSceneRHI;
				RHICmdList.RayTraceDispatch(View.RayTracingMaterialPipeline, RayGenerationShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, DispatchSize.X, DispatchSize.Y);
			}
		);
	}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	void DDGIUpdateVolume_RenderThread_RTRadiance(const FScene& Scene, const FViewInfo& View, FRDGBuilder& GraphBuilder, FDDGIVolumeSceneProxy* VolProxy, const FMatrix& ProbeRayRotationTransform, FRDGTextureRef ProbesRadianceTex, FRDGTextureUAVRef ProbesRadianceUAV, bool highBitCount)
	{
		// Deal with probe ray budgets, and updating probes in a round robin fashion within the volume
		int ProbeUpdateRayBudget = GetDefault<URTXGIPluginSettings>()->ProbeUpdateRayBudget;
		if (ProbeUpdateRayBudget == 0)
		{
			VolProxy->ProbeIndexStart = 0;
			VolProxy->ProbeIndexCount = VolProxy->ComponentData.GetProbeCount();
		}
		else
		{
			int ProbeCount = VolProxy->ComponentData.GetProbeCount();
			int ProbeUpdateBudget = ProbeUpdateRayBudget / VolProxy->ComponentData.GetNumRaysPerProbe();
			if (ProbeUpdateBudget < 1)
				ProbeUpdateBudget = 1;
			if (ProbeUpdateBudget > ProbeCount)
				ProbeUpdateBudget = ProbeCount;
			VolProxy->ProbeIndexStart += ProbeUpdateBudget;
			VolProxy->ProbeIndexStart = VolProxy->ProbeIndexStart % ProbeCount;
			VolProxy->ProbeIndexCount = ProbeUpdateBudget;
		}

		const auto FeatureLevel = GMaxRHIFeatureLevel;
		auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

		FRayTracingRTXGIProbeUpdateRGS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FRayTracingRTXGIProbeUpdateRGS::FEnableTwoSidedGeometryDim>(true);
		PermutationVector.Set<FRayTracingRTXGIProbeUpdateRGS::FEnableMaterialsDim>(false);
		PermutationVector.Set<FRayTracingRTXGIProbeUpdateRGS::FEnableRelocation>(VolProxy->ComponentData.EnableProbeRelocation);
		PermutationVector.Set<FRayTracingRTXGIProbeUpdateRGS::FFormatRadiance>(highBitCount);
		PermutationVector.Set<FRayTracingRTXGIProbeUpdateRGS::FFormatIrradiance>(highBitCount);
		PermutationVector.Set<FRayTracingRTXGIProbeUpdateRGS::FEnableScrolling>(VolProxy->ComponentData.EnableProbeScrolling);
		PermutationVector.Set<FRayTracingRTXGIProbeUpdateRGS::FSkyLight>(int(VolProxy->ComponentData.SkyLightTypeOnRayMiss));
		TShaderMapRef<FRayTracingRTXGIProbeUpdateRGS> RayGenerationShader(ShaderMap, PermutationVector);

		FRayTracingRTXGIProbeUpdateRGS::FParameters DefaultPassParameters;
		FRayTracingRTXGIProbeUpdateRGS::FParameters* PassParameters = GraphBuilder.AllocParameters<FRayTracingRTXGIProbeUpdateRGS::FParameters>();
		*PassParameters = DefaultPassParameters;

		PassParameters->TLAS = View.RayTracingScene.RayTracingSceneRHI->GetShaderResourceView();
		check(PassParameters->TLAS);
		PassParameters->RadianceOutput = ProbesRadianceUAV;
		PassParameters->FrameRandomSeed = GFrameNumber;

		// skylight parameters
		if (Scene.SkyLight && Scene.SkyLight->ProcessedTexture)
		{
			PassParameters->Sky_Color = FVector(Scene.SkyLight->GetEffectiveLightColor());
			PassParameters->Sky_Texture = Scene.SkyLight->ProcessedTexture->TextureRHI;
			PassParameters->Sky_TextureSampler = Scene.SkyLight->ProcessedTexture->SamplerStateRHI;
		}
		else
		{
			PassParameters->Sky_Color = FVector(0.0);
			PassParameters->Sky_Texture = GBlackTextureCube->TextureRHI;
			PassParameters->Sky_TextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		}

		// DDGI Volume Parameters
		{
			PassParameters->DDGIVolume_ProbeIrradiance = GraphBuilder.RegisterExternalTexture(VolProxy->ProbesIrradiance);
			PassParameters->DDGIVolume_ProbeDistance = GraphBuilder.RegisterExternalTexture(VolProxy->ProbesDistance);
			PassParameters->DDGIVolume_ProbeOffsets = RegisterExternalTextureWithFallback(GraphBuilder, VolProxy->ProbesOffsets, GSystemTextures.BlackDummy);
			PassParameters->DDGIVolume_ProbeStates = RegisterExternalTextureWithFallback(GraphBuilder, VolProxy->ProbesStates, GSystemTextures.BlackDummy);
			PassParameters->DDGIVolume_LinearClampSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

			PassParameters->DDGIVolume_Radius = VolProxy->ComponentData.Transform.GetScale3D() * 100.0f;
			PassParameters->DDGIVolume_IrradianceScalar = VolProxy->ComponentData.IrradianceScalar;
			PassParameters->DDGIVolume_EmissiveMultiplier = VolProxy->ComponentData.EmissiveMultiplier;
			PassParameters->DDGIVolume_ProbeIndexStart = VolProxy->ProbeIndexStart;
			PassParameters->DDGIVolume_ProbeIndexCount = VolProxy->ProbeIndexCount;

			// calculate grid spacing based on size (scale) and probe count
			// regarding the *200: the scale is the radius so we need to double it. There is also an implict * 100 of the basic box.
			FVector volumeSize = VolProxy->ComponentData.Transform.GetScale3D() * 200.0f;
			FVector probeGridSpacing;
			probeGridSpacing.X = volumeSize.X / float(VolProxy->ComponentData.ProbeCounts.X);
			probeGridSpacing.Y = volumeSize.Y / float(VolProxy->ComponentData.ProbeCounts.Y);
			probeGridSpacing.Z = volumeSize.Z / float(VolProxy->ComponentData.ProbeCounts.Z);

			FDDGIVolumeDescGPU DefaultDDGIVolumeDescGPU;
			FDDGIVolumeDescGPU* DDGIVolumeDescGPU = GraphBuilder.AllocParameters<FDDGIVolumeDescGPU>();
			*DDGIVolumeDescGPU = DefaultDDGIVolumeDescGPU;
			DDGIVolumeDescGPU->origin = VolProxy->ComponentData.Origin;
			FQuat rotation = VolProxy->ComponentData.Transform.GetRotation();
			DDGIVolumeDescGPU->rotation = FVector4{ rotation.X, rotation.Y, rotation.Z, rotation.W };
			DDGIVolumeDescGPU->probeMaxRayDistance = VolProxy->ComponentData.ProbeMaxRayDistance;
			DDGIVolumeDescGPU->probeGridCounts = VolProxy->ComponentData.ProbeCounts;
			DDGIVolumeDescGPU->probeRayRotationTransform = ProbeRayRotationTransform;
			DDGIVolumeDescGPU->numRaysPerProbe = VolProxy->ComponentData.GetNumRaysPerProbe();
			DDGIVolumeDescGPU->probeGridSpacing = probeGridSpacing;
			DDGIVolumeDescGPU->probeNumIrradianceTexels = FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsIrradiance;
			DDGIVolumeDescGPU->probeNumDistanceTexels = FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsDistance;
			DDGIVolumeDescGPU->probeIrradianceEncodingGamma = VolProxy->ComponentData.ProbeIrradianceEncodingGamma;
			DDGIVolumeDescGPU->normalBias = VolProxy->ComponentData.NormalBias;
			DDGIVolumeDescGPU->viewBias = VolProxy->ComponentData.ViewBias;
			DDGIVolumeDescGPU->probeScrollOffsets = VolProxy->ComponentData.ProbeScrollOffsets;

			PassParameters->DDGIVolume = GraphBuilder.CreateUniformBuffer(DDGIVolumeDescGPU);
		}

		FRDGTextureDesc DDGIDebugOutputDesc = FRDGTextureDesc::Create2D(
			ProbesRadianceTex->Desc.Extent,
			ProbesRadianceTex->Desc.Format,
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV
		);
		PassParameters->DebugOutput = GraphBuilder.CreateUAV(GraphBuilder.CreateTexture(DDGIDebugOutputDesc, TEXT("DDGIVolumeUpdateDebug")));

		PassParameters->SSProfilesTexture = GraphBuilder.RegisterExternalTexture(View.RayTracingSubSurfaceProfileTexture);
		PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
		PassParameters->LightDataPacked = View.RayTracingLightData.UniformBuffer;

		FIntPoint DispatchSize = ProbesRadianceTex->Desc.Extent;

		GraphBuilder.AddPass(
			RDG_EVENT_NAME("DDGI RTRadiance %dx%d", DispatchSize.X, DispatchSize.Y),
			PassParameters,
			ERDGPassFlags::Compute,
			[PassParameters, &View, RayGenerationShader, DispatchSize, ProbesRadianceTex](FRHICommandList& RHICmdList)
			{
				FRayTracingShaderBindingsWriter GlobalResources;
				SetShaderParameters(GlobalResources, RayGenerationShader, *PassParameters);

				FRHIRayTracingScene* RayTracingSceneRHI = View.RayTracingScene.RayTracingSceneRHI;
				RHICmdList.RayTraceDispatch(View.RayTracingMaterialPipeline, RayGenerationShader.GetRayTracingShader(), RayTracingSceneRHI, GlobalResources, DispatchSize.X, DispatchSize.Y);
			}
		);
	}

	void DDGIUpdateVolume_RenderThread_IrradianceBlend(const FViewInfo& View, FRDGBuilder& GraphBuilder, FDDGIVolumeSceneProxy* VolProxy, const FMatrix& ProbeRayRotationTransform, FRDGTextureUAVRef ProbesRadianceUAV, bool highBitCount)
	{
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
		FDDGIIrradianceBlend::FPermutationDomain PermutationVector;
		PermutationVector.Set<FDDGIIrradianceBlend::FRaysPerProbeEnum>(int(VolProxy->ComponentData.RaysPerProbe));
		PermutationVector.Set<FDDGIIrradianceBlend::FEnableRelocation>(VolProxy->ComponentData.EnableProbeRelocation);
		PermutationVector.Set<FDDGIIrradianceBlend::FFormatRadiance>(highBitCount);
		PermutationVector.Set<FDDGIIrradianceBlend::FFormatIrradiance>(highBitCount);
		PermutationVector.Set<FDDGIIrradianceBlend::FEnableScrolling>(VolProxy->ComponentData.EnableProbeScrolling);
		TShaderMapRef<FDDGIIrradianceBlend> ComputeShader(ShaderMap, PermutationVector);

		// calculate grid spacing based on size (scale) and probe count
		// regarding the *200: the scale is the radius so we need to double it. There is also an implict * 100 of the basic box.
		FVector volumeSize = VolProxy->ComponentData.Transform.GetScale3D() * 200.0f;
		FVector probeGridSpacing;
		probeGridSpacing.X = volumeSize.X / float(VolProxy->ComponentData.ProbeCounts.X);
		probeGridSpacing.Y = volumeSize.Y / float(VolProxy->ComponentData.ProbeCounts.Y);
		probeGridSpacing.Z = volumeSize.Z / float(VolProxy->ComponentData.ProbeCounts.Z);

		// set up the shader parameters
		FDDGIVolumeDescGPU DefaultDDGIVolumeDescGPU;
		FDDGIVolumeDescGPU* DDGIVolumeDescGPU = GraphBuilder.AllocParameters<FDDGIVolumeDescGPU>();
		*DDGIVolumeDescGPU = DefaultDDGIVolumeDescGPU;
		DDGIVolumeDescGPU->probeGridSpacing = probeGridSpacing;
		DDGIVolumeDescGPU->probeGridCounts = VolProxy->ComponentData.ProbeCounts;
		DDGIVolumeDescGPU->numRaysPerProbe = VolProxy->ComponentData.GetNumRaysPerProbe();
		DDGIVolumeDescGPU->probeRayRotationTransform = ProbeRayRotationTransform;
		DDGIVolumeDescGPU->probeDistanceExponent = VolProxy->ComponentData.ProbeDistanceExponent;
		DDGIVolumeDescGPU->probeInverseIrradianceEncodingGamma = 1.0f / VolProxy->ComponentData.ProbeIrradianceEncodingGamma;
		DDGIVolumeDescGPU->probeHysteresis = VolProxy->ComponentData.ProbeHysteresis;
		DDGIVolumeDescGPU->probeChangeThreshold = VolProxy->ComponentData.ProbeChangeThreshold;
		DDGIVolumeDescGPU->probeBrightnessThreshold = VolProxy->ComponentData.ProbeBrightnessThreshold;
		DDGIVolumeDescGPU->probeScrollOffsets = VolProxy->ComponentData.ProbeScrollOffsets;

		FDDGIIrradianceBlend::FParameters DefaultPassParameters;
		FDDGIIrradianceBlend::FParameters* PassParameters = GraphBuilder.AllocParameters<FDDGIIrradianceBlend::FParameters>();
		*PassParameters = DefaultPassParameters;

		PassParameters->ProbeIndexStart = VolProxy->ProbeIndexStart;
		PassParameters->ProbeIndexCount = VolProxy->ProbeIndexCount;

		PassParameters->DDGIVolume = GraphBuilder.CreateUniformBuffer(DDGIVolumeDescGPU);

		PassParameters->DDGIVolumeRayDataUAV = ProbesRadianceUAV;
		PassParameters->DDGIVolumeProbeDataUAV = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(VolProxy->ProbesIrradiance));
		PassParameters->DDGIVolumeProbeStatesTexture = RegisterExternalTextureWithFallback(GraphBuilder, VolProxy->ProbesStates, GSystemTextures.BlackDummy);

		if (VolProxy->ComponentData.EnableProbeScrolling)
			PassParameters->DDGIProbeScrollSpace = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(VolProxy->ProbesSpace));

		FRDGTextureDesc DDGIDebugOutputDesc = FRDGTextureDesc::Create2D(
			VolProxy->ProbesIrradiance->GetTargetableRHI()->GetTexture2D()->GetSizeXY(),
			VolProxy->ProbesIrradiance->GetTargetableRHI()->GetFormat(),
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV
		);
		PassParameters->DebugOutput = GraphBuilder.CreateUAV(GraphBuilder.CreateTexture(DDGIDebugOutputDesc, TEXT("DDGIIrradianceBlendDebug")));

		FIntPoint ProbeCount2D = VolProxy->ComponentData.Get2DProbeCount();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("DDGI Radiance Blend"),
			ComputeShader,
			PassParameters,
			FIntVector(ProbeCount2D.X, ProbeCount2D.Y, 1)
		);
	}

	void DDGIUpdateVolume_RenderThread_DistanceBlend(const FViewInfo& View, FRDGBuilder& GraphBuilder, FDDGIVolumeSceneProxy* VolProxy, const FMatrix& ProbeRayRotationTransform, FRDGTextureUAVRef ProbesRadianceUAV, bool highBitCount)
	{
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
		FDDGIDistanceBlend::FPermutationDomain PermutationVector;
		PermutationVector.Set<FDDGIDistanceBlend::FRaysPerProbeEnum>(int(VolProxy->ComponentData.RaysPerProbe));
		PermutationVector.Set<FDDGIDistanceBlend::FEnableRelocation>(int(VolProxy->ComponentData.EnableProbeRelocation));
		PermutationVector.Set<FDDGIDistanceBlend::FFormatRadiance>(highBitCount);
		PermutationVector.Set<FDDGIDistanceBlend::FFormatIrradiance>(highBitCount);
		PermutationVector.Set<FDDGIDistanceBlend::FEnableScrolling>(int(VolProxy->ComponentData.EnableProbeScrolling));
		TShaderMapRef<FDDGIDistanceBlend> ComputeShader(ShaderMap, PermutationVector);

		// calculate grid spacing based on size (scale) and probe count
		// regarding the *200: the scale is the radius so we need to double it. There is also an implict * 100 of the basic box.
		FVector volumeSize = VolProxy->ComponentData.Transform.GetScale3D() * 200.0f;
		FVector probeGridSpacing;
		probeGridSpacing.X = volumeSize.X / float(VolProxy->ComponentData.ProbeCounts.X);
		probeGridSpacing.Y = volumeSize.Y / float(VolProxy->ComponentData.ProbeCounts.Y);
		probeGridSpacing.Z = volumeSize.Z / float(VolProxy->ComponentData.ProbeCounts.Z);

		FDDGIVolumeDescGPU DefaultDDGIVolumeDescGPU;
		FDDGIVolumeDescGPU* DDGIVolumeDescGPU = GraphBuilder.AllocParameters<FDDGIVolumeDescGPU>();
		*DDGIVolumeDescGPU = DefaultDDGIVolumeDescGPU;
		DDGIVolumeDescGPU->probeGridSpacing = probeGridSpacing;
		DDGIVolumeDescGPU->probeGridCounts = VolProxy->ComponentData.ProbeCounts;
		DDGIVolumeDescGPU->numRaysPerProbe = VolProxy->ComponentData.GetNumRaysPerProbe();
		DDGIVolumeDescGPU->probeRayRotationTransform = ProbeRayRotationTransform;
		DDGIVolumeDescGPU->probeDistanceExponent = VolProxy->ComponentData.ProbeDistanceExponent;
		DDGIVolumeDescGPU->probeInverseIrradianceEncodingGamma = 1.0f / VolProxy->ComponentData.ProbeIrradianceEncodingGamma;
		DDGIVolumeDescGPU->probeHysteresis = VolProxy->ComponentData.ProbeHysteresis;
		DDGIVolumeDescGPU->probeChangeThreshold = VolProxy->ComponentData.ProbeChangeThreshold;
		DDGIVolumeDescGPU->probeBrightnessThreshold = VolProxy->ComponentData.ProbeBrightnessThreshold;
		DDGIVolumeDescGPU->probeScrollOffsets = VolProxy->ComponentData.ProbeScrollOffsets;

		FDDGIDistanceBlend::FParameters DefaultPassParameters;
		FDDGIDistanceBlend::FParameters* PassParameters = GraphBuilder.AllocParameters<FDDGIDistanceBlend::FParameters>();
		*PassParameters = DefaultPassParameters;

		PassParameters->ProbeIndexStart = VolProxy->ProbeIndexStart;
		PassParameters->ProbeIndexCount = VolProxy->ProbeIndexCount;

		PassParameters->DDGIVolume = GraphBuilder.CreateUniformBuffer(DDGIVolumeDescGPU);

		PassParameters->DDGIVolumeRayDataUAV = ProbesRadianceUAV;
		PassParameters->DDGIVolumeProbeDataUAV = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(VolProxy->ProbesDistance));
		PassParameters->DDGIVolumeProbeStatesTexture = RegisterExternalTextureWithFallback(GraphBuilder, VolProxy->ProbesStates, GSystemTextures.BlackDummy);

		if (VolProxy->ComponentData.EnableProbeScrolling)
			PassParameters->DDGIProbeScrollSpace = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(VolProxy->ProbesSpace));

		FRDGTextureDesc DDGIDebugOutputDesc = FRDGTextureDesc::Create2D(
			VolProxy->ProbesDistance->GetTargetableRHI()->GetTexture2D()->GetSizeXY(),
			VolProxy->ProbesDistance->GetTargetableRHI()->GetFormat(),
			FClearValueBinding::None,
			TexCreate_ShaderResource | TexCreate_UAV
		);
		PassParameters->DebugOutput = GraphBuilder.CreateUAV(GraphBuilder.CreateTexture(DDGIDebugOutputDesc, TEXT("DDGIDistanceBlendDebug")));

		FIntPoint ProbeCount2D = VolProxy->ComponentData.Get2DProbeCount();
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("DDGI Distance Blend"),
			ComputeShader,
			PassParameters,
			FIntVector(ProbeCount2D.X, ProbeCount2D.Y, 1)
		);
	}

	void DDGIUpdateVolume_RenderThread_IrradianceBorderUpdate(const FViewInfo& View, FRDGBuilder& GraphBuilder, FDDGIVolumeSceneProxy* VolProxy)
	{
		float groupSize = 8.0f;
		FIntPoint ProbeCount2D = VolProxy->ComponentData.Get2DProbeCount();

		// Row
		{
			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
			FDDGIBorderRowUpdate::FPermutationDomain PermutationVector;
			PermutationVector.Set<FDDGIBorderRowUpdate::FProbeNumTexels>(FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsIrradiance);
			TShaderMapRef<FDDGIBorderRowUpdate> ComputeShader(ShaderMap, PermutationVector);

			FDDGIBorderRowUpdate::FParameters DefaultPassParameters;
			FDDGIBorderRowUpdate::FParameters* PassParameters = GraphBuilder.AllocParameters<FDDGIBorderRowUpdate::FParameters>();
			*PassParameters = DefaultPassParameters;

			PassParameters->DDGIVolumeProbeDataUAV = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(VolProxy->ProbesIrradiance));

			uint32 numThreadsX = (ProbeCount2D.X * (FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsIrradiance + 2));
			uint32 numThreadsY = ProbeCount2D.Y;
			uint32 numGroupsX = (uint32)ceil((float)numThreadsX / groupSize);
			uint32 numGroupsY = (uint32)ceil((float)numThreadsY / groupSize);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("DDGI Irradiance Border Update Row"),
				ComputeShader,
				PassParameters,
				FIntVector(numGroupsX, numGroupsY, 1)
			);
		}

		// Column
		{
			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
			FDDGIBorderColumnUpdate::FPermutationDomain PermutationVector;
			PermutationVector.Set<FDDGIBorderColumnUpdate::FProbeNumTexels>(FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsIrradiance);
			TShaderMapRef<FDDGIBorderColumnUpdate> ComputeShader(ShaderMap, PermutationVector);

			FDDGIBorderColumnUpdate::FParameters DefaultPassParameters;
			FDDGIBorderColumnUpdate::FParameters* PassParameters = GraphBuilder.AllocParameters<FDDGIBorderColumnUpdate::FParameters>();
			*PassParameters = DefaultPassParameters;

			PassParameters->DDGIVolumeProbeDataUAV = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(VolProxy->ProbesIrradiance));

			uint32 numThreadsX = (ProbeCount2D.X * 2);
			uint32 numThreadsY = (ProbeCount2D.Y * (FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsIrradiance + 2));
			uint32 numGroupsX = (uint32)ceil((float)numThreadsX / groupSize);
			uint32 numGroupsY = (uint32)ceil((float)numThreadsY / groupSize);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("DDGI Irradiance Border Update Column"),
				ComputeShader,
				PassParameters,
				FIntVector(numGroupsX, numGroupsY, 1)
			);
		}
	}

	void DDGIUpdateVolume_RenderThread_DistanceBorderUpdate(const FViewInfo& View, FRDGBuilder& GraphBuilder, FDDGIVolumeSceneProxy* VolProxy)
	{
		float groupSize = 8.0f;
		FIntPoint ProbeCount2D = VolProxy->ComponentData.Get2DProbeCount();

		// Row
		{
			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
			FDDGIBorderRowUpdate::FPermutationDomain PermutationVector;
			PermutationVector.Set<FDDGIBorderRowUpdate::FProbeNumTexels>(FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsDistance);
			TShaderMapRef<FDDGIBorderRowUpdate> ComputeShader(ShaderMap, PermutationVector);

			FDDGIBorderRowUpdate::FParameters DefaultPassParameters;
			FDDGIBorderRowUpdate::FParameters* PassParameters = GraphBuilder.AllocParameters<FDDGIBorderRowUpdate::FParameters>();
			*PassParameters = DefaultPassParameters;

			PassParameters->DDGIVolumeProbeDataUAV = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(VolProxy->ProbesDistance));

			uint32 numThreadsX = (ProbeCount2D.X * (FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsDistance + 2));
			uint32 numThreadsY = ProbeCount2D.Y;
			uint32 numGroupsX = (uint32)ceil((float)numThreadsX / groupSize);
			uint32 numGroupsY = (uint32)ceil((float)numThreadsY / groupSize);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("DDGI Distance Border Update Row"),
				ComputeShader,
				PassParameters,
				FIntVector(numGroupsX, numGroupsY, 1)
			);
		}

		// Column
		{
			FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
			FDDGIBorderColumnUpdate::FPermutationDomain PermutationVector;
			PermutationVector.Set<FDDGIBorderColumnUpdate::FProbeNumTexels>(FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsDistance);
			TShaderMapRef<FDDGIBorderColumnUpdate> ComputeShader(ShaderMap, PermutationVector);

			FDDGIBorderColumnUpdate::FParameters DefaultPassParameters;
			FDDGIBorderColumnUpdate::FParameters* PassParameters = GraphBuilder.AllocParameters<FDDGIBorderColumnUpdate::FParameters>();
			*PassParameters = DefaultPassParameters;

			PassParameters->DDGIVolumeProbeDataUAV = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(VolProxy->ProbesDistance));

			uint32 numThreadsX = (ProbeCount2D.X * 2);
			uint32 numThreadsY = (ProbeCount2D.Y * (FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsDistance + 2));
			uint32 numGroupsX = (uint32)ceil((float)numThreadsX / groupSize);
			uint32 numGroupsY = (uint32)ceil((float)numThreadsY / groupSize);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("DDGI Distance Border Update Column"),
				ComputeShader,
				PassParameters,
				FIntVector(numGroupsX, numGroupsY, 1)
			);
		}
	}

	void DDGIUpdateVolume_RenderThread_RelocateProbes(FRDGBuilder& GraphBuilder, FDDGIVolumeSceneProxy* VolProxy, const FMatrix& ProbeRayRotationTransform, FRDGTextureUAVRef ProbesRadianceUAV, bool highBitCount)
	{
		FDDGIProbesRelocate::FPermutationDomain PermutationVector;
		PermutationVector.Set<FDDGIProbesRelocate::FFormatRadiance>(highBitCount);
		PermutationVector.Set<FDDGIProbesRelocate::FFormatIrradiance>(highBitCount);
		PermutationVector.Set<FDDGIProbesRelocate::FEnableScrolling>(VolProxy->ComponentData.EnableProbeScrolling);
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
		TShaderMapRef<FDDGIProbesRelocate> ComputeShader(ShaderMap, PermutationVector);

		// calculate grid spacing based on size (scale) and probe count
		// regarding the *200: the scale is the radius so we need to double it. There is also an implict * 100 of the basic box.
		FVector volumeSize = VolProxy->ComponentData.Transform.GetScale3D() * 200.0f;
		FVector probeGridSpacing;
		probeGridSpacing.X = volumeSize.X / float(VolProxy->ComponentData.ProbeCounts.X);
		probeGridSpacing.Y = volumeSize.Y / float(VolProxy->ComponentData.ProbeCounts.Y);
		probeGridSpacing.Z = volumeSize.Z / float(VolProxy->ComponentData.ProbeCounts.Z);

		FDDGIVolumeDescGPU DefaultDDGIVolumeDescGPU;
		FDDGIVolumeDescGPU* DDGIVolumeDescGPU = GraphBuilder.AllocParameters<FDDGIVolumeDescGPU>();
		*DDGIVolumeDescGPU = DefaultDDGIVolumeDescGPU;
		DDGIVolumeDescGPU->probeGridSpacing = probeGridSpacing;
		DDGIVolumeDescGPU->probeGridCounts = VolProxy->ComponentData.ProbeCounts;
		DDGIVolumeDescGPU->numRaysPerProbe = VolProxy->ComponentData.GetNumRaysPerProbe();
		DDGIVolumeDescGPU->probeScrollOffsets = VolProxy->ComponentData.ProbeScrollOffsets;
		DDGIVolumeDescGPU->probeBackfaceThreshold = VolProxy->ComponentData.ProbeBackfaceThreshold;
		DDGIVolumeDescGPU->probeRayRotationTransform = ProbeRayRotationTransform;
		DDGIVolumeDescGPU->probeMinFrontfaceDistance = VolProxy->ComponentData.ProbeMinFrontfaceDistance;

		FDDGIProbesRelocate::FParameters DefaultPassParameters;
		FDDGIProbesRelocate::FParameters* PassParameters = GraphBuilder.AllocParameters<FDDGIProbesRelocate::FParameters>();
		*PassParameters = DefaultPassParameters;

		// run every frame with full distance scale value for continuous relocation
		PassParameters->ProbeDistanceScale = 1.0f;

		PassParameters->ProbeIndexStart = VolProxy->ProbeIndexStart;
		PassParameters->ProbeIndexCount = VolProxy->ProbeIndexCount;

		PassParameters->DDGIVolume = GraphBuilder.CreateUniformBuffer(DDGIVolumeDescGPU);

		PassParameters->DDGIVolumeRayDataUAV = ProbesRadianceUAV;
		// This resource is required if this method was called.
		check(VolProxy->ProbesOffsets);
		PassParameters->DDGIVolumeProbeOffsetsUAV = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(VolProxy->ProbesOffsets));

		float groupSizeX = 8.f;
		float groupSizeY = 4.f;

		FIntPoint ProbeCount2D = VolProxy->ComponentData.Get2DProbeCount();
		uint32 numThreadsX = ProbeCount2D.X;
		uint32 numThreadsY = ProbeCount2D.Y;
		uint32 numGroupsX = (uint32)ceil((float)numThreadsX / groupSizeX);
		uint32 numGroupsY = (uint32)ceil((float)numThreadsY / groupSizeY);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("DDGI Probe Relocation"),
			ComputeShader,
			PassParameters,
			FIntVector(numGroupsX, numGroupsY, 1)
		);
	}

	void DDGIUpdateVolume_RenderThread_ClassifyProbes(FRDGBuilder& GraphBuilder, FDDGIVolumeSceneProxy* VolProxy, FRDGTextureUAVRef ProbesRadianceUAV, bool highBitCount)
	{
		// get the permuted shader
		FDDGIProbesClassify::FPermutationDomain PermutationVector;
		PermutationVector.Set<FDDGIProbesClassify::FEnableRelocation>(VolProxy->ComponentData.EnableProbeRelocation);
		PermutationVector.Set <FDDGIProbesClassify::FFormatRadiance>(highBitCount);
		PermutationVector.Set <FDDGIProbesClassify::FFormatIrradiance>(highBitCount);
		PermutationVector.Set <FDDGIProbesClassify::FEnableScrolling>(VolProxy->ComponentData.EnableProbeScrolling);
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
		TShaderMapRef<FDDGIProbesClassify> ComputeShader(ShaderMap, PermutationVector);

		// calculate grid spacing based on size (scale) and probe count
		// regarding the *200: the scale is the radius so we need to double it. There is also an implict * 100 of the basic box.
		FVector volumeSize = VolProxy->ComponentData.Transform.GetScale3D() * 200.0f;
		FVector probeGridSpacing;
		probeGridSpacing.X = volumeSize.X / float(VolProxy->ComponentData.ProbeCounts.X);
		probeGridSpacing.Y = volumeSize.Y / float(VolProxy->ComponentData.ProbeCounts.Y);
		probeGridSpacing.Z = volumeSize.Z / float(VolProxy->ComponentData.ProbeCounts.Z);

		// set up the shader parameters
		FDDGIVolumeDescGPU DefaultDDGIVolumeDescGPU;
		FDDGIVolumeDescGPU* DDGIVolumeDescGPU = GraphBuilder.AllocParameters<FDDGIVolumeDescGPU>();
		*DDGIVolumeDescGPU = DefaultDDGIVolumeDescGPU;
		DDGIVolumeDescGPU->probeGridSpacing = probeGridSpacing;
		DDGIVolumeDescGPU->probeGridCounts = VolProxy->ComponentData.ProbeCounts;
		DDGIVolumeDescGPU->numRaysPerProbe = VolProxy->ComponentData.GetNumRaysPerProbe();
		DDGIVolumeDescGPU->probeBackfaceThreshold = VolProxy->ComponentData.ProbeBackfaceThreshold;
		DDGIVolumeDescGPU->probeScrollOffsets = VolProxy->ComponentData.ProbeScrollOffsets;

		FDDGIProbesClassify::FParameters DefaultPassParameters;
		FDDGIProbesClassify::FParameters* PassParameters = GraphBuilder.AllocParameters<FDDGIProbesClassify::FParameters>();
		*PassParameters = DefaultPassParameters;

		PassParameters->ProbeIndexStart = VolProxy->ProbeIndexStart;
		PassParameters->ProbeIndexCount = VolProxy->ProbeIndexCount;

		PassParameters->DDGIVolume = GraphBuilder.CreateUniformBuffer(DDGIVolumeDescGPU);

		PassParameters->DDGIVolumeRayDataUAV = ProbesRadianceUAV;
		// This resource is required if this method was called.
		check(VolProxy->ProbesStates);
		PassParameters->DDGIVolumeProbeStatesUAV = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(VolProxy->ProbesStates));

		// Dispatch the compute shader
		float groupSizeX = 8.f;
		float groupSizeY = 4.f;

		FIntPoint ProbeCount2D = VolProxy->ComponentData.Get2DProbeCount();
		uint32 numThreadsX = ProbeCount2D.X;
		uint32 numThreadsY = ProbeCount2D.Y;
		uint32 numGroupsX = (uint32)ceil((float)numThreadsX / groupSizeX);
		uint32 numGroupsY = (uint32)ceil((float)numThreadsY / groupSizeY);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("DDGI Probe Classification"),
			ComputeShader,
			PassParameters,
			FIntVector(numGroupsX, numGroupsY, 1)
		);
	}

#endif // RHI_RAYTRACING

} // namespace DDGIVolumeUpdate

#undef LOCTEXT_NAMESPACE
