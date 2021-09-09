/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "DDGIVolumeComponent.h"
#include "DDGIVolume.h"
#include "DDGIVolumeUpdate.h"

#include "RTXGIPluginSettings.h"

// UE4 Public Interfaces
#include "ConvexVolume.h"
#include "RenderGraphBuilder.h"
#include "ShaderParameterStruct.h"
#include "ShaderParameterUtils.h"
#include "SystemTextures.h"

// UE4 Private Interfaces
#include "PostProcess/SceneRenderTargets.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"

#include "RenderGraphUtils.h"

DECLARE_GPU_STAT_NAMED(RTXGI_Update, TEXT("RTXGI Update"));
DECLARE_GPU_STAT_NAMED(RTXGI_ApplyLighting, TEXT("RTXGI Apply Lighting"));
DECLARE_GPU_STAT_NAMED(RTXGI_UpscaleLighting, TEXT("RTXGI Upscale Lighting"));

static TAutoConsoleVariable<bool> CVarUseDDGI(
	TEXT("r.RTXGI.DDGI"),
	true,
	TEXT("If false, this will disable the lighting contribution and functionality of DDGI volumes.\n"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarLightingPassScale(
	TEXT("r.RTXGI.DDGI.LightingPass.Scale"),
	1.0f,
	TEXT("Scale for the lighting pass resolution between 0.25 - 1.0 (value is clamped to this range).\n"),
	ECVF_RenderThreadSafe | ECVF_Cheat);

static TAutoConsoleVariable<float> CVarRelativeDistanceThreshold(
	TEXT("r.RTXGI.DDGI.LightingPass.RelativeDistanceThreshold"),
	0.01f,
	TEXT("Relative distance threshold for geometry test in the lighting upscaler.\n"),
	ECVF_RenderThreadSafe | ECVF_Cheat);

static TAutoConsoleVariable<float> CVarNormalPower(
	TEXT("r.RTXGI.DDGI.LightingPass.NormalPower"),
	1.f,
	TEXT("Normal power for geometry test in the lighting upscaler.\n"),
	ECVF_RenderThreadSafe | ECVF_Cheat);

BEGIN_SHADER_PARAMETER_STRUCT(FVolumeData, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ProbeIrradiance)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ProbeDistance)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ProbeOffsets)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ProbeStates)
	SHADER_PARAMETER(FVector, Position)
	SHADER_PARAMETER(FVector4, Rotation)
	SHADER_PARAMETER(FVector, Radius)
	SHADER_PARAMETER(FVector, ProbeGridSpacing)
	SHADER_PARAMETER(FIntVector, ProbeGridCounts)
	SHADER_PARAMETER(FIntVector, ProbeScrollOffsets)
	SHADER_PARAMETER(uint32, LightingChannelMask)
	SHADER_PARAMETER(int, ProbeNumIrradianceTexels)
	SHADER_PARAMETER(int, ProbeNumDistanceTexels)
	SHADER_PARAMETER(float, ProbeIrradianceEncodingGamma)
	SHADER_PARAMETER(float, NormalBias)
	SHADER_PARAMETER(float, ViewBias)
	SHADER_PARAMETER(float, BlendDistance)
	SHADER_PARAMETER(float, BlendDistanceBlack)
	SHADER_PARAMETER(float, ApplyLighting)
	SHADER_PARAMETER(float, IrradianceScalar)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FApplyLightingDeferredShaderParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, NormalTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BaseColorTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MetallicTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LightingChannelsTexture)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, LightingPassUAV)
	SHADER_PARAMETER_SAMPLER(SamplerState, PointClampSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, LinearClampSampler)
	SHADER_PARAMETER(FVector4, ScaledViewSizeAndInvSize)
	SHADER_PARAMETER(int32, ShouldUsePreExposure)
	SHADER_PARAMETER(int32, NumVolumes)
	// Volumes are sorted from densest probes to least dense probes
	SHADER_PARAMETER_STRUCT_ARRAY(FVolumeData, DDGIVolume, [FDDGIVolumeSceneProxy::FComponentData::c_RTXGI_DDGI_MAX_SHADING_VOLUMES])
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FUpscaleLightingShaderParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputGITexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, NormalTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, DepthTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, BaseColorTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, MetallicTexture)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, SceneColorOutput)
	SHADER_PARAMETER_SAMPLER(SamplerState, PointClampSampler)
	SHADER_PARAMETER_SAMPLER(SamplerState, LinearClampSampler)
	SHADER_PARAMETER(FVector4, InputViewSizeAndInvSize)
	SHADER_PARAMETER(float, RelativeDistanceThreshold)
	SHADER_PARAMETER(float, NormalPower)
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
END_SHADER_PARAMETER_STRUCT()

class FApplyLightingDeferredShaderCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FApplyLightingDeferredShaderCS);
	SHADER_USE_PARAMETER_STRUCT(FApplyLightingDeferredShaderCS, FGlobalShader);

	using FParameters = FApplyLightingDeferredShaderParameters;

	class FLightingChannelsDim : SHADER_PERMUTATION_BOOL("USE_LIGHTING_CHANNELS");
	class FEnableRelocation : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_PROBE_RELOCATION");
	class FEnableScrolling : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_INFINITE_SCROLLING_VOLUME");
	class FFormatRadiance : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_FORMAT_RADIANCE");
	class FFormatIrradiance : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_FORMAT_IRRADIANCE");

	using FPermutationDomain = TShaderPermutationDomain<FLightingChannelsDim, FEnableRelocation, FEnableScrolling, FFormatRadiance, FFormatIrradiance>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		FString volumeMacroList;
		for (int i = 0; i < FDDGIVolumeSceneProxy::FComponentData::c_RTXGI_DDGI_MAX_SHADING_VOLUMES; ++i)
			volumeMacroList += FString::Printf(TEXT(" VOLUME_ENTRY(%i)"), i);
		OutEnvironment.SetDefine(TEXT("VOLUME_LIST"), volumeMacroList.GetCharArray().GetData());

		OutEnvironment.SetDefine(TEXT("RTXGI_DDGI_PROBE_CLASSIFICATION"), FDDGIVolumeSceneProxy::FComponentData::c_RTXGI_DDGI_PROBE_CLASSIFICATION ? 1 : 0);

		// needed for a typed UAV load. This already assumes we are raytracing, so should be fine.
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

class FUpscaleLightingShaderCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FUpscaleLightingShaderCS);
	SHADER_USE_PARAMETER_STRUCT(FUpscaleLightingShaderCS, FGlobalShader);

	using FParameters = FUpscaleLightingShaderParameters;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		// needed for a typed UAV load. This already assumes we are raytracing, so should be fine.
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FApplyLightingDeferredShaderCS, "/Plugin/RTXGI/Private/ApplyLightingDeferred.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FUpscaleLightingShaderCS, "/Plugin/RTXGI/Private/UpscaleLighting.usf", "MainCS", SF_Compute);

// Delegate Handles
FDelegateHandle FDDGIVolumeSceneProxy::RenderDiffuseIndirectVisualizationsHandle;
FDelegateHandle FDDGIVolumeSceneProxy::RenderDiffuseIndirectLightHandle;

TSet<FDDGIVolumeSceneProxy*> FDDGIVolumeSceneProxy::AllProxiesReadyForRender_RenderThread;
TMap<const FSceneInterface*, float> FDDGIVolumeSceneProxy::SceneRoundRobinValue;

bool FDDGIVolumeSceneProxy::IntersectsViewFrustum(const FViewInfo& View)
{
	// Get the volume position and scale
	FVector ProxyPosition = ComponentData.Origin;
	FQuat   ProxyRotation = ComponentData.Transform.GetRotation();
	FVector ProxyScale = ComponentData.Transform.GetScale3D();
	FVector ProxyExtent = ProxyScale * 100.0f;

	if (ProxyRotation.IsIdentity())
	{
		// This volume is not rotated, test it against the view frustum
		// Skip this volume if it doesn't intersect the view frustum
		return View.ViewFrustum.IntersectBox(ProxyPosition, ProxyExtent);
	}
	else
	{
		// TODO: optimize CPU performance for many volumes (100s to 1000s)

		// This volume is rotated, transform the view frustum so the volume's
		// oriented bounding box becomes an axis-aligned bounding box.
		FConvexVolume TransformedViewFrustum;
		FMatrix FrustumTransform = FTranslationMatrix::Make(-ProxyPosition)
			* FRotationMatrix::Make(ProxyRotation)
			* FTranslationMatrix::Make(ProxyPosition);

		// Based on SetupViewFrustum()
		if (View.SceneViewInitOptions.OverrideFarClippingPlaneDistance > 0.0f)
		{
			FVector PlaneBasePoint = FrustumTransform.TransformPosition(View.ViewMatrices.GetViewOrigin() + View.GetViewDirection() * View.SceneViewInitOptions.OverrideFarClippingPlaneDistance);
			FVector PlaneNormal = FrustumTransform.TransformVector(View.GetViewDirection());

			const FPlane FarPlane(PlaneBasePoint, PlaneNormal);
			// Derive the view frustum from the view projection matrix, overriding the far plane
			GetViewFrustumBounds(TransformedViewFrustum, FrustumTransform * View.ViewMatrices.GetViewProjectionMatrix(), FarPlane, true, false);
		}
		else
		{
			// Derive the view frustum from the view projection matrix.
			GetViewFrustumBounds(TransformedViewFrustum, FrustumTransform * View.ViewMatrices.GetViewProjectionMatrix(), false);
		}

		// Test the transformed view frustum against the volume
		// Skip this volume if it doesn't intersect the view frustum
		return TransformedViewFrustum.IntersectBox(ProxyPosition, ProxyExtent);
	}
}

void FDDGIVolumeSceneProxy::OnIrradianceOrDistanceBitsChange()
{
	EDDGIIrradianceBits IrradianceBits = GetDefault<URTXGIPluginSettings>()->IrradianceBits;
	EDDGIDistanceBits DistanceBits = GetDefault<URTXGIPluginSettings>()->DistanceBits;

	// tell all the proxies about the change
	ENQUEUE_RENDER_COMMAND(DDGIOnIrradianceBitsChange)(
		[IrradianceBits, DistanceBits](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);

			for (FDDGIVolumeSceneProxy* DDGIProxy : AllProxiesReadyForRender_RenderThread)
			{
				DDGIProxy->ReallocateSurfaces_RenderThread(RHICmdList, IrradianceBits, DistanceBits);
				DDGIProxy->ResetTextures_RenderThread(GraphBuilder);
			}

			GraphBuilder.Execute();
		}
	);
}

void FDDGIVolumeSceneProxy::ReallocateSurfaces_RenderThread(FRHICommandListImmediate& RHICmdList, EDDGIIrradianceBits IrradianceBits, EDDGIDistanceBits DistanceBits)
{
	FIntPoint ProxyDims = ComponentData.Get2DProbeCount();

	// Irradiance
	{
		int NumTexels = FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsIrradiance;
		FIntPoint ProxyTexDims = ProxyDims * (NumTexels + 2);
		EPixelFormat Format = (IrradianceBits == EDDGIIrradianceBits::n32 ) ? FDDGIVolumeSceneProxy::FComponentData::c_pixelFormatIrradianceHighBitDepth : FDDGIVolumeSceneProxy::FComponentData::c_pixelFormatIrradianceLowBitDepth;

		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(ProxyTexDims, Format, FClearValueBinding::Transparent, TexCreate_None, TexCreate_ShaderResource | TexCreate_UAV , false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ProbesIrradiance, TEXT("DDGIIrradiance"), ERenderTargetTransience::NonTransient);
	}

	// Distance
	{
		int NumTexels = FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsDistance;
		FIntPoint ProxyTexDims = ProxyDims * (NumTexels + 2);
		EPixelFormat Format = (DistanceBits == EDDGIDistanceBits::n32) ? FDDGIVolumeSceneProxy::FComponentData::c_pixelFormatDistanceHighBitDepth : FDDGIVolumeSceneProxy::FComponentData::c_pixelFormatDistanceLowBitDepth;

		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(ProxyTexDims, Format, FClearValueBinding::Transparent, TexCreate_None, TexCreate_ShaderResource | TexCreate_UAV, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ProbesDistance, TEXT("DDGIDistance"), ERenderTargetTransience::NonTransient);
	}

	// Offsets - only pay the cost of this resource if this volume is actually doing relocation
	if (ComponentData.EnableProbeRelocation)
	{
		EPixelFormat Format = FDDGIVolumeSceneProxy::FComponentData::c_pixelFormatOffsets;

		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(ProxyDims, Format, FClearValueBinding::Transparent, TexCreate_None, TexCreate_ShaderResource | TexCreate_UAV, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ProbesOffsets, TEXT("DDGIOffsets"), ERenderTargetTransience::NonTransient);
	}
	else
	{
		ProbesOffsets.SafeRelease();
	}

	// probe classifications
	if (FDDGIVolumeSceneProxy::FComponentData::c_RTXGI_DDGI_PROBE_CLASSIFICATION)
	{
		EPixelFormat Format = FDDGIVolumeSceneProxy::FComponentData::c_pixelFormatStates;

		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(ProxyDims, Format, FClearValueBinding::Transparent, TexCreate_None, TexCreate_ShaderResource | TexCreate_UAV, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ProbesStates, TEXT("DDGIStates"), ERenderTargetTransience::NonTransient);
	}
	else
	{
		ProbesStates.SafeRelease();
	}

	if (ComponentData.EnableProbeScrolling)
	{
		EPixelFormat Format = FDDGIVolumeSceneProxy::FComponentData::c_pixelFormatScrollSpace;

		FPooledRenderTargetDesc Desc(FPooledRenderTargetDesc::Create2DDesc(ProxyDims, Format, FClearValueBinding::Transparent, TexCreate_None, TexCreate_ShaderResource | TexCreate_UAV, false));
		GRenderTargetPool.FindFreeElement(RHICmdList, Desc, ProbesSpace, TEXT("DDGIScrollSpace"), ERenderTargetTransience::NonTransient);
	}
	else
	{
		ProbesSpace.SafeRelease();
	}
}

void FDDGIVolumeSceneProxy::ResetTextures_RenderThread(FRDGBuilder& GraphBuilder)
{
	float ClearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(ProbesIrradiance)), ClearColor);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(ProbesDistance)), ClearColor);

	if (ProbesOffsets)
	{
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(ProbesOffsets)), ClearColor);
	}

	if (ProbesStates)
	{
		uint32 StatesClearColor[] = { 0u, 0u, 0u, 0u };
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalTexture(ProbesStates)), StatesClearColor);
	}
}

void FDDGIVolumeSceneProxy::RenderDiffuseIndirectLight_RenderThread(
	const FScene& Scene,
	const FViewInfo& View,
	FRDGBuilder& GraphBuilder,
	FGlobalIlluminationExperimentalPluginResources& Resources)
{
	// Early out if DDGI is disabled
	if (!CVarUseDDGI.GetValueOnRenderThread()) return;

	// Update DDGIVolumes when rendering a main view and when ray tracing is available.
	// Other views can use DDGIVolumes for lighting, but don't need to update the volumes.
	// This is especially true for situations like bIsSceneCapture, when bSceneCaptureUsesRayTracing is false, and it can make incorrect probe update results.
	if (!View.bIsSceneCapture && !View.bIsReflectionCapture && !View.bIsPlanarReflection)
	{
		RDG_GPU_STAT_SCOPE(GraphBuilder, RTXGI_Update);
		RDG_EVENT_SCOPE(GraphBuilder, "RTXGI Update");
		DDGIVolumeUpdate::DDGIUpdatePerFrame_RenderThread(Scene, View, GraphBuilder);
	}

	// Register the GBuffer textures with the render graph
	FRDGTextureRef GBufferATexture = GraphBuilder.RegisterExternalTexture(Resources.GBufferA);
	FRDGTextureRef GBufferBTexture = GraphBuilder.RegisterExternalTexture(Resources.GBufferB);
	FRDGTextureRef GBufferCTexture = GraphBuilder.RegisterExternalTexture(Resources.GBufferC);
	FRDGTextureRef SceneDepthTexture = GraphBuilder.RegisterExternalTexture(Resources.SceneDepthZ);
	FRDGTextureRef SceneColorTexture = GraphBuilder.RegisterExternalTexture(Resources.SceneColor);
	if (!View.bUsesLightingChannels) Resources.LightingChannelsTexture = nullptr;

	float ScreenScale = FMath::Clamp(CVarLightingPassScale.GetValueOnRenderThread(), 0.25f, 1.0f);
	uint32 ScaledViewSizeX = FMath::Max(1, FMath::CeilToInt(View.ViewRect.Size().X * ScreenScale));
	uint32 ScaledViewSizeY = FMath::Max(1, FMath::CeilToInt(View.ViewRect.Size().Y * ScreenScale));
	FIntPoint ScaledViewSize = FIntPoint(ScaledViewSizeX, ScaledViewSizeY);

	FRDGTextureDesc RTXGILightingPassOutputDesc = FRDGTextureDesc::Create2D(
		ScaledViewSize,
		SceneColorTexture->Desc.Format,
		FClearValueBinding::Transparent,
		TexCreate_ShaderResource | TexCreate_UAV
	);

	FRDGTextureRef LightingPassTex = GraphBuilder.CreateTexture(RTXGILightingPassOutputDesc, TEXT("RTXGILightingPassOutput"));
	FRDGTextureUAVRef LightingPassUAV = GraphBuilder.CreateUAV(LightingPassTex);

	{
		RDG_GPU_STAT_SCOPE(GraphBuilder, RTXGI_ApplyLighting);
		RDG_EVENT_SCOPE(GraphBuilder, "RTXGI Apply Lighting");

		// DDGIVolume and useful metadata
		struct FProxyEntry
		{
			FVector Position;
			FQuat Rotation;
			FVector Scale;
			float Density;
			uint32 lightingChannelMask;
			const FDDGIVolumeSceneProxy* proxy;
		};

		// Find all the volumes that intersect the view frustum
		TArray<FProxyEntry> volumes;
		for (FDDGIVolumeSceneProxy* volumeProxy : AllProxiesReadyForRender_RenderThread)
		{
			// Skip this volume if it belongs to another scene
			if (volumeProxy->OwningScene != &Scene) continue;

			// Skip this volume if it is not enabled
			if (!volumeProxy->ComponentData.EnableVolume) continue;

			// Skip this volume if it doesn't intersect the view frustum
			if (!volumeProxy->IntersectsViewFrustum(View)) continue;

			// Get the volume position, rotation, and scale
			FVector ProxyPosition = volumeProxy->ComponentData.Origin;
			FQuat   ProxyRotation = volumeProxy->ComponentData.Transform.GetRotation();
			FVector ProxyScale = volumeProxy->ComponentData.Transform.GetScale3D();

			float ProxyDensity = float(volumeProxy->ComponentData.ProbeCounts.X * volumeProxy->ComponentData.ProbeCounts.Y * volumeProxy->ComponentData.ProbeCounts.Z) / (ProxyScale.X * ProxyScale.Y * ProxyScale.Z);
			uint32 ProxyLightingChannelMask =
				(volumeProxy->ComponentData.LightingChannels.bChannel0 ? 1 : 0) |
				(volumeProxy->ComponentData.LightingChannels.bChannel1 ? 2 : 0) |
				(volumeProxy->ComponentData.LightingChannels.bChannel2 ? 4 : 0);

			// Add the current volume to the list of in-frustum volumes
			volumes.Add(FProxyEntry{ ProxyPosition, ProxyRotation, ProxyScale, ProxyDensity, ProxyLightingChannelMask, volumeProxy });
		}

		// Early out if no volumes contribute light to the current view
		if (volumes.Num() == 0) return;

		// TODO: manage in-frustum volumes in a more sophisticated way
		// Support a large number of volumes by culling volumes based on spatial data, projected view area, and/or other heuristics

		// Sort the in-frustum volumes by user specified priority and probe density
		Algo::Sort(volumes, [](const FProxyEntry& A, const FProxyEntry& B)
		{
			if (A.proxy->ComponentData.LightingPriority < B.proxy->ComponentData.LightingPriority) return true;
			if ((A.proxy->ComponentData.LightingPriority == B.proxy->ComponentData.LightingPriority) && (A.Density > B.Density)) return true;
			return false;
		});

		// Get the number of relevant in-frustum volumes
		int32 numVolumes = FMath::Min(volumes.Num(), FDDGIVolumeSceneProxy::FComponentData::c_RTXGI_DDGI_MAX_SHADING_VOLUMES);

		// Truncate the in-frustum volumes list to the maximum number of volumes supported
		volumes.SetNum(numVolumes, true);

		// Sort the final volume list by descending probe density
		Algo::Sort(volumes, [](const FProxyEntry& A, const FProxyEntry& B)
		{
			return (A.Density > B.Density);
		});

		if (CVarLightingPassScale.GetValueOnRenderThread() < 1.0f)
		{
			AddClearUAVPass(GraphBuilder, LightingPassUAV, FLinearColor::Transparent);
		}

		// Loop over the shader permutations to render indirect light from relevant volumes
		for (int permutationIndex = 0; permutationIndex < 4; ++permutationIndex)
		{
			// Render with the current shader permutation if there one (or more) volume that matches the permutation settings
			bool enableRelocation = (permutationIndex & 1) != 0;
			bool enableScrolling = (permutationIndex & 2) != 0;
			bool foundAMatch = false;
			for (int32 i = 0; i < volumes.Num(); ++i)
			{
				foundAMatch = true;
				foundAMatch = foundAMatch && (enableRelocation == volumes[i].proxy->ComponentData.EnableProbeRelocation);
				foundAMatch = foundAMatch && (enableScrolling == volumes[i].proxy->ComponentData.EnableProbeScrolling);
				if (foundAMatch) break;
			}

			// Skip this shader permutation if there are no volumes that match its feature set
			if (!foundAMatch) continue;

			// Get the shader permutation
			FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
			bool highBitCount = (GetDefault<URTXGIPluginSettings>()->IrradianceBits == EDDGIIrradianceBits::n32);
			FApplyLightingDeferredShaderCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FApplyLightingDeferredShaderCS::FLightingChannelsDim>(Resources.LightingChannelsTexture != nullptr);
			PermutationVector.Set<FApplyLightingDeferredShaderCS::FEnableRelocation>(enableRelocation);
			PermutationVector.Set<FApplyLightingDeferredShaderCS::FEnableScrolling>(enableScrolling);
			PermutationVector.Set<FApplyLightingDeferredShaderCS::FFormatRadiance>(highBitCount);
			PermutationVector.Set<FApplyLightingDeferredShaderCS::FFormatIrradiance>(highBitCount);
			TShaderMapRef<FApplyLightingDeferredShaderCS> ComputeShader(GlobalShaderMap, PermutationVector);

			// Set the shader parameters
			FApplyLightingDeferredShaderParameters DefaultPassParameters;
			FApplyLightingDeferredShaderParameters* PassParameters = GraphBuilder.AllocParameters<FApplyLightingDeferredShaderParameters>();
			*PassParameters = DefaultPassParameters;
			PassParameters->NormalTexture = GBufferATexture;
			PassParameters->DepthTexture = SceneDepthTexture;
			PassParameters->BaseColorTexture = GBufferCTexture;
			PassParameters->MetallicTexture = GBufferBTexture;
			PassParameters->LightingChannelsTexture = Resources.LightingChannelsTexture;
			PassParameters->PointClampSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			PassParameters->LinearClampSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			PassParameters->ShouldUsePreExposure = View.Family->EngineShowFlags.Tonemapper;
			PassParameters->NumVolumes = numVolumes;

			// Set the shader parameters for the relevant volumes
			for (int32 volumeIndex = 0; volumeIndex < numVolumes; ++volumeIndex)
			{
				FProxyEntry volume = volumes[volumeIndex];
				const FDDGIVolumeSceneProxy* volumeProxy = volume.proxy;

				// Set the volume textures
				PassParameters->DDGIVolume[volumeIndex].ProbeIrradiance = GraphBuilder.RegisterExternalTexture(volumeProxy->ProbesIrradiance);
				PassParameters->DDGIVolume[volumeIndex].ProbeDistance = GraphBuilder.RegisterExternalTexture(volumeProxy->ProbesDistance);
				PassParameters->DDGIVolume[volumeIndex].ProbeOffsets = RegisterExternalTextureWithFallback(GraphBuilder, volumeProxy->ProbesOffsets, GSystemTextures.BlackDummy);
				PassParameters->DDGIVolume[volumeIndex].ProbeStates = RegisterExternalTextureWithFallback(GraphBuilder, volumeProxy->ProbesStates, GSystemTextures.BlackDummy);

				// Set the volume parameters
				PassParameters->DDGIVolume[volumeIndex].Position = volumeProxy->ComponentData.Origin;
				PassParameters->DDGIVolume[volumeIndex].Rotation = FVector4(volume.Rotation.X, volume.Rotation.Y, volume.Rotation.Z, volume.Rotation.W);
				PassParameters->DDGIVolume[volumeIndex].Radius = volume.Scale * 100.0f;
				PassParameters->DDGIVolume[volumeIndex].LightingChannelMask = volume.lightingChannelMask;

				FVector volumeSize = volumeProxy->ComponentData.Transform.GetScale3D() * 200.0f;
				FVector probeGridSpacing;
				probeGridSpacing.X = volumeSize.X / float(volumeProxy->ComponentData.ProbeCounts.X);
				probeGridSpacing.Y = volumeSize.Y / float(volumeProxy->ComponentData.ProbeCounts.Y);
				probeGridSpacing.Z = volumeSize.Z / float(volumeProxy->ComponentData.ProbeCounts.Z);

				PassParameters->DDGIVolume[volumeIndex].ProbeGridSpacing = probeGridSpacing;
				PassParameters->DDGIVolume[volumeIndex].ProbeGridCounts = volumeProxy->ComponentData.ProbeCounts;
				PassParameters->DDGIVolume[volumeIndex].ProbeNumIrradianceTexels = FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsIrradiance;
				PassParameters->DDGIVolume[volumeIndex].ProbeNumDistanceTexels = FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsDistance;
				PassParameters->DDGIVolume[volumeIndex].ProbeIrradianceEncodingGamma = volumeProxy->ComponentData.ProbeIrradianceEncodingGamma;
				PassParameters->DDGIVolume[volumeIndex].NormalBias = volumeProxy->ComponentData.NormalBias;
				PassParameters->DDGIVolume[volumeIndex].ViewBias = volumeProxy->ComponentData.ViewBias;
				PassParameters->DDGIVolume[volumeIndex].BlendDistance = volumeProxy->ComponentData.BlendDistance;
				PassParameters->DDGIVolume[volumeIndex].BlendDistanceBlack = volumeProxy->ComponentData.BlendDistanceBlack;
				PassParameters->DDGIVolume[volumeIndex].ProbeScrollOffsets = volumeProxy->ComponentData.ProbeScrollOffsets;

				// Only apply lighting if this is the pass it should be applied in
				// The shader needs data for all of the volumes for blending purposes
				bool applyLighting = true;
				applyLighting = applyLighting && (enableRelocation == volumeProxy->ComponentData.EnableProbeRelocation);
				applyLighting = applyLighting && (enableScrolling == volumeProxy->ComponentData.EnableProbeScrolling);
				PassParameters->DDGIVolume[volumeIndex].ApplyLighting = applyLighting;
				PassParameters->DDGIVolume[volumeIndex].IrradianceScalar = volumeProxy->ComponentData.IrradianceScalar;

				// Apply the lighting multiplier to artificially lighten or darken the indirect light from the volume
				PassParameters->DDGIVolume[volumeIndex].IrradianceScalar /= volumeProxy->ComponentData.LightingMultiplier;
			}

			// When there are fewer relevant volumes than the maximum supported, set the empty volume texture slots to dummy values
			for (int32 volumeIndex = numVolumes; volumeIndex < FDDGIVolumeSceneProxy::FComponentData::c_RTXGI_DDGI_MAX_SHADING_VOLUMES; ++volumeIndex)
			{
				PassParameters->DDGIVolume[volumeIndex].ProbeIrradiance = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
				PassParameters->DDGIVolume[volumeIndex].ProbeDistance = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
				PassParameters->DDGIVolume[volumeIndex].ProbeOffsets = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
				PassParameters->DDGIVolume[volumeIndex].ProbeStates = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
			}

			if (CVarLightingPassScale.GetValueOnRenderThread() == 1.0f)
			{
				PassParameters->LightingPassUAV = GraphBuilder.CreateUAV(SceneColorTexture);
			}
			else
			{
				PassParameters->LightingPassUAV = LightingPassUAV;
			}

			PassParameters->ScaledViewSizeAndInvSize = FVector4(ScaledViewSize.X, ScaledViewSize.Y, 1.0f / ScaledViewSize.X, 1.0f / ScaledViewSize.Y);
			PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;

			// Currently hardcoded as 8
			const float groupSize = 8.f;
			uint32 numThreadsX = ScaledViewSizeX;
			uint32 numThreadsY = ScaledViewSizeY;

			uint32 numGroupsX = (uint32)ceil((float)numThreadsX / groupSize);
			uint32 numGroupsY = (uint32)ceil((float)numThreadsY / groupSize);

			// Dispatching the Downsampling CS
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("DDGI Apply Lighting"),
				ComputeShader,
				PassParameters,
				FIntVector(numGroupsX, numGroupsY, 1)
			);
		}
	}

	if (CVarLightingPassScale.GetValueOnRenderThread() < 1.0f)
	{
		RDG_GPU_STAT_SCOPE(GraphBuilder, RTXGI_UpscaleLighting);
		RDG_EVENT_SCOPE(GraphBuilder, "RTXGI Upscale Lighting");

		// Set parameters for the Upsampler CS
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
		TShaderMapRef<FUpscaleLightingShaderCS> ComputeShader(GlobalShaderMap);

		FUpscaleLightingShaderParameters DefaultPassParameters;
		FUpscaleLightingShaderParameters* PassParameters = GraphBuilder.AllocParameters<FUpscaleLightingShaderParameters>();
		*PassParameters = DefaultPassParameters;
		PassParameters->InputGITexture = LightingPassTex;
		PassParameters->NormalTexture = GBufferATexture;
		PassParameters->BaseColorTexture = GBufferCTexture;
		PassParameters->MetallicTexture = GBufferBTexture;
		PassParameters->DepthTexture = SceneDepthTexture;
		PassParameters->SceneColorOutput = GraphBuilder.CreateUAV(SceneColorTexture);
		PassParameters->PointClampSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->LinearClampSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->RelativeDistanceThreshold = CVarRelativeDistanceThreshold.GetValueOnRenderThread();
		PassParameters->InputViewSizeAndInvSize = FVector4(ScaledViewSize.X, ScaledViewSize.Y, 1.0f / ScaledViewSize.X, 1.0f / ScaledViewSize.Y);;
		PassParameters->NormalPower = CVarNormalPower.GetValueOnRenderThread();
		PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;

		// Currently hardcoded as 8
		const float groupSize = 8.f;
		uint32 numGroupsX = (uint32)ceil((float)View.ViewRect.Size().X / groupSize);
		uint32 numGroupsY = (uint32)ceil((float)View.ViewRect.Size().Y / groupSize);

		// Dispatching the Upsampling CS
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("DDGI Upscale Lighting"),
			ComputeShader,
			PassParameters,
			FIntVector(numGroupsX, numGroupsY, 1)
		);
	}
}

UDDGIVolumeComponent::UDDGIVolumeComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
}

void UDDGIVolumeComponent::InitializeComponent()
{
	Super::InitializeComponent();

	MarkRenderDynamicDataDirty();

	TransformUpdated.AddLambda(
		[this](USceneComponent* /*UpdatedComponent*/, EUpdateTransformFlags /*UpdateTransformFlags*/, ETeleportType /*Teleport*/)
		{
			MarkRenderDynamicDataDirty();
		}
	);
}

// Serialization version for stored DDGIVolume data
struct RTXGI_API FDDGICustomVersion
{
	enum Type
	{
		AddingCustomVersion = 1,
		SaveLoadProbeTextures,     // save pixels and width/height
		SaveLoadProbeTexturesFmt,  // save texel format since the format can change in the project settings
		SaveLoadProbeDataIsOptional, // Probe data is optionally stored depending on project settings
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FDDGICustomVersion() {}
};
const FGuid FDDGICustomVersion::GUID(0xc12f0537, 0x7346d9c5, 0x336fbba3, 0x738ab145);

// Register the custom version with core
FCustomVersionRegistration GRegisterCustomVersion(FDDGICustomVersion::GUID, FDDGICustomVersion::SaveLoadProbeDataIsOptional, TEXT("DDGIVolCompVer"));

// Create a CPU accessible GPU texture and copy the provided GPU texture's contents to it
static FDDGITexturePixels GetTexturePixelsStep1_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* textureGPU)
{
	FDDGITexturePixels ret;

	// Early out if a GPU texture is not provided
	if (!textureGPU) return ret;

	ret.Desc.Width = textureGPU->GetTexture2D()->GetSizeX();
	ret.Desc.Height = textureGPU->GetTexture2D()->GetSizeY();
	ret.Desc.PixelFormat = (int32)textureGPU->GetFormat();

	// Create the texture
	FRHIResourceCreateInfo createInfo(TEXT("DDGIGetTexturePixelsSave"));
	ret.Texture = RHICreateTexture2D(
		textureGPU->GetTexture2D()->GetSizeX(),
		textureGPU->GetTexture2D()->GetSizeY(),
		textureGPU->GetFormat(),
		1,
		1,
		TexCreate_ShaderResource | TexCreate_Transient,
		ERHIAccess::CopyDest,
		createInfo);

	// Transition the GPU texture to a copy source
	RHICmdList.Transition(FRHITransitionInfo(textureGPU, ERHIAccess::SRVMask, ERHIAccess::CopySrc));

	// Schedule a copy of the GPU texture to the CPU accessible GPU texture
	RHICmdList.CopyTexture(textureGPU, ret.Texture, FRHICopyTextureInfo{});

	// Transition the GPU texture back to general
	RHICmdList.Transition(FRHITransitionInfo(textureGPU, ERHIAccess::CopySrc, ERHIAccess::SRVMask));

	return ret;
}

// Read the CPU accessible GPU texture data into CPU memory
static void GetTexturePixelsStep2_RenderThread(FRHICommandListImmediate& RHICmdList, FDDGITexturePixels& texturePixels)
{
	// Early out if no texture is provided
	if (!texturePixels.Texture) return;

	// Get a pointer to the CPU memory
	uint8* mappedTextureMemory = (uint8*)RHILockTexture2D(texturePixels.Texture, 0, RLM_ReadOnly, texturePixels.Desc.Stride, false);

	// Copy the texture data to CPU memory
	texturePixels.Pixels.AddZeroed(texturePixels.Desc.Height * texturePixels.Desc.Stride);
	FMemory::Memcpy(&texturePixels.Pixels[0], mappedTextureMemory, texturePixels.Desc.Height * texturePixels.Desc.Stride);

	RHIUnlockTexture2D(texturePixels.Texture, 0, false);
}

static void SaveFDDGITexturePixels(FArchive& Ar, FDDGITexturePixels& texturePixels, bool bSaveFormat)
{
	check(Ar.IsSaving());

	Ar << texturePixels.Desc.Width;
	Ar << texturePixels.Desc.Height;
	Ar << texturePixels.Desc.Stride;
	Ar << texturePixels.Pixels;

	if (bSaveFormat) Ar << texturePixels.Desc.PixelFormat;
}

static void LoadFDDGITexturePixels(FArchive& Ar, FDDGITexturePixels& texturePixels, EPixelFormat expectedPixelFormat, bool bLoadFormat)
{
	check(Ar.IsLoading());

	// Load the texture data
	Ar << texturePixels.Desc.Width;
	Ar << texturePixels.Desc.Height;
	Ar << texturePixels.Desc.Stride;
	Ar << texturePixels.Pixels;

	if (bLoadFormat)
	{
		Ar << texturePixels.Desc.PixelFormat;

		// Early out if the loaded pixel format doesn't match our expected format
		if (texturePixels.Desc.PixelFormat != expectedPixelFormat) return;
	}

	// Early out if no data was loaded
	if (texturePixels.Desc.Width == 0 || texturePixels.Desc.Height == 0 || texturePixels.Desc.Stride == 0) return;

	// Create the texture resource
	FRHIResourceCreateInfo createInfo(TEXT("DDGITextureLoad"));
	texturePixels.Texture = RHICreateTexture2D(
		texturePixels.Desc.Width,
		texturePixels.Desc.Height,
		expectedPixelFormat,
		1,
		1,
		TexCreate_ShaderResource | TexCreate_Transient,
		createInfo);

	// Copy the texture's data to the staging buffer
	ENQUEUE_RENDER_COMMAND(DDGILoadTex)(
		[&texturePixels](FRHICommandListImmediate& RHICmdList)
		{
			if (texturePixels.Pixels.Num() == texturePixels.Desc.Height * texturePixels.Desc.Stride)
			{
				uint32 destStride;
				uint8* mappedTextureMemory = (uint8*)RHILockTexture2D(texturePixels.Texture, 0, RLM_WriteOnly, destStride, false);
				if (texturePixels.Desc.Stride == destStride)
				{
					// Loaded data has the same stride as expected by the runtime
					// Copy the entire texture at once
					FMemory::Memcpy(mappedTextureMemory, &texturePixels.Pixels[0], texturePixels.Desc.Height * texturePixels.Desc.Stride);
				}
				else
				{
					// Loaded data has a different stride than expected by the runtime
					// Texture data was stored with a different API than what is running now (D3D12->VK, VK->D3D12)
					// Copy each row of the source data to the texture
					const uint8* SourceBuffer = &texturePixels.Pixels[0];
					for (uint32 Row = 0; Row < texturePixels.Desc.Height; ++Row)
					{
						FMemory::Memcpy(mappedTextureMemory, SourceBuffer, FMath::Min(texturePixels.Desc.Stride, destStride));

						mappedTextureMemory += destStride;
						SourceBuffer += texturePixels.Desc.Stride;
					}
				}
				RHIUnlockTexture2D(texturePixels.Texture, 0, false);
			}

			// Only clear the texels when in a game.
			// Cooking needs this data to write textures to disk on save, after load, when headless etc.
		#if !WITH_EDITOR
			texturePixels.Pixels.Reset();
		#endif
		}
	);
}

void UDDGIVolumeComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FDDGICustomVersion::GUID);
	if (Ar.CustomVer(FDDGICustomVersion::GUID) < FDDGICustomVersion::AddingCustomVersion)
	{
		if (Ar.IsLoading())
		{
			uint32 w, h;
			TArray<float> pixels;
			Ar << w;
			Ar << h;
			Ar << pixels;
		}
	}
	else if (Ar.CustomVer(FDDGICustomVersion::GUID) >= FDDGICustomVersion::SaveLoadProbeTextures)
	{
		// Save and load DDGIVolume texture resources when entering a level
		// Also applicable when ray tracing is not available (DX11 and Vulkan RHI).
		bool bSaveFormat = Ar.CustomVer(FDDGICustomVersion::GUID) >= FDDGICustomVersion::SaveLoadProbeTexturesFmt;

		FDDGIVolumeSceneProxy* proxy = SceneProxy;

		if (Ar.IsSaving())
		{
			// Probe data can be optionally not saved depending on project settings.
			bool bSeralizeProbesIsOptional = Ar.CustomVer(FDDGICustomVersion::GUID) >= FDDGICustomVersion::SaveLoadProbeDataIsOptional;
			bool bProbesSerialized = bSeralizeProbesIsOptional ? GetDefault<URTXGIPluginSettings>()->SerializeProbes : true;
			if (bSeralizeProbesIsOptional)
				Ar << bProbesSerialized;

			if (bProbesSerialized)
			{
				FDDGITexturePixels Irradiance, Distance, Offsets, States;

				// When we are *not* cooking and ray tracing is available, copy the DDGIVolume probe texture resources
				// to CPU memory otherwise, write out the DDGIVolume texture resources acquired at load time
				if (!Ar.IsCooking() && IsRayTracingEnabled() && proxy)
				{
					// Copy textures to CPU accessible texture resources
					ENQUEUE_RENDER_COMMAND(DDGISaveTexStep1)(
						[&Irradiance, &Distance, &Offsets, &States, proxy](FRHICommandListImmediate& RHICmdList)
						{
							Irradiance = GetTexturePixelsStep1_RenderThread(RHICmdList, proxy->ProbesIrradiance->GetTargetableRHI());
							Distance = GetTexturePixelsStep1_RenderThread(RHICmdList, proxy->ProbesDistance->GetTargetableRHI());
							Offsets = GetTexturePixelsStep1_RenderThread(RHICmdList, proxy->ProbesOffsets ? proxy->ProbesOffsets->GetTargetableRHI() : nullptr);
							States = GetTexturePixelsStep1_RenderThread(RHICmdList, proxy->ProbesStates ? proxy->ProbesStates->GetTargetableRHI() : nullptr);
						}
					);
					FlushRenderingCommands();

					// Read the GPU texture data to CPU memory
					ENQUEUE_RENDER_COMMAND(DDGISaveTexStep2)(
						[&Irradiance, &Distance, &Offsets, &States](FRHICommandListImmediate& RHICmdList)
						{
							GetTexturePixelsStep2_RenderThread(RHICmdList, Irradiance);
							GetTexturePixelsStep2_RenderThread(RHICmdList, Distance);
							GetTexturePixelsStep2_RenderThread(RHICmdList, Offsets);
							GetTexturePixelsStep2_RenderThread(RHICmdList, States);
						}
					);
					FlushRenderingCommands();
				}
				else
				{
					Irradiance = LoadContext.Irradiance;
					Distance = LoadContext.Distance;
					Offsets = LoadContext.Offsets;
					States = LoadContext.States;
				}

				// Write the volume data
				SaveFDDGITexturePixels(Ar, Irradiance, bSaveFormat);
				SaveFDDGITexturePixels(Ar, Distance, bSaveFormat);
				SaveFDDGITexturePixels(Ar, Offsets, bSaveFormat);
				SaveFDDGITexturePixels(Ar, States, bSaveFormat);
			}
		}
		else if (Ar.IsLoading())
		{
			bool bSeralizeProbesIsOptional = Ar.CustomVer(FDDGICustomVersion::GUID) >= FDDGICustomVersion::SaveLoadProbeDataIsOptional;
			bool bProbesSerialized = true;
			if (bSeralizeProbesIsOptional)
				Ar << bProbesSerialized;

			if (bProbesSerialized)
			{
				EDDGIIrradianceBits IrradianceBits = GetDefault<URTXGIPluginSettings>()->IrradianceBits;
				EDDGIDistanceBits DistanceBits = GetDefault<URTXGIPluginSettings>()->DistanceBits;
				bool bLoadFormat = Ar.CustomVer(FDDGICustomVersion::GUID) >= FDDGICustomVersion::SaveLoadProbeTexturesFmt;

				// Read the volume texture data in and note that it's ready for load
				LoadFDDGITexturePixels(Ar, LoadContext.Irradiance, (IrradianceBits == EDDGIIrradianceBits::n32) ? FDDGIVolumeSceneProxy::FComponentData::c_pixelFormatIrradianceHighBitDepth : FDDGIVolumeSceneProxy::FComponentData::c_pixelFormatIrradianceLowBitDepth, bLoadFormat);
				LoadFDDGITexturePixels(Ar, LoadContext.Distance, (DistanceBits == EDDGIDistanceBits::n32) ? FDDGIVolumeSceneProxy::FComponentData::c_pixelFormatDistanceHighBitDepth : FDDGIVolumeSceneProxy::FComponentData::c_pixelFormatDistanceLowBitDepth, bLoadFormat);
				LoadFDDGITexturePixels(Ar, LoadContext.Offsets, FDDGIVolumeSceneProxy::FComponentData::c_pixelFormatOffsets, bLoadFormat);
				LoadFDDGITexturePixels(Ar, LoadContext.States, FDDGIVolumeSceneProxy::FComponentData::c_pixelFormatStates, bLoadFormat);

				bool& ReadyForLoad = LoadContext.ReadyForLoad;
				ENQUEUE_RENDER_COMMAND(DDGILoadReady)(
					[&ReadyForLoad](FRHICommandListImmediate& RHICmdList)
					{
						ReadyForLoad = true;
					}
				);
			}
		}
	}
}

void UDDGIVolumeComponent::UpdateRenderThreadData()
{
	// Send command to the rendering thread to update the transform and other parameters
	if (SceneProxy)
	{
		// Update the volume component's data
		FDDGIVolumeSceneProxy::FComponentData ComponentData;
		ComponentData.RaysPerProbe = RaysPerProbe;
		ComponentData.ProbeMaxRayDistance = ProbeMaxRayDistance;
		ComponentData.LightingChannels = LightingChannels;
		ComponentData.ProbeCounts = ProbeCounts;
		ComponentData.ProbeDistanceExponent = probeDistanceExponent;
		ComponentData.ProbeIrradianceEncodingGamma = probeIrradianceEncodingGamma;
		ComponentData.LightingPriority = LightingPriority;
		ComponentData.UpdatePriority = UpdatePriority;
		ComponentData.ProbeHysteresis = ProbeHistoryWeight;
		ComponentData.ProbeChangeThreshold = probeChangeThreshold;
		ComponentData.ProbeBrightnessThreshold = probeBrightnessThreshold;
		ComponentData.NormalBias = NormalBias;
		ComponentData.ViewBias = ViewBias;
		ComponentData.BlendDistance = BlendingDistance;
		ComponentData.BlendDistanceBlack = BlendingCutoffDistance;
		ComponentData.ProbeBackfaceThreshold = ProbeRelocation.ProbeBackfaceThreshold;
		ComponentData.ProbeMinFrontfaceDistance = ProbeRelocation.ProbeMinFrontfaceDistance;
		ComponentData.EnableProbeRelocation = ProbeRelocation.AutomaticProbeRelocation;
		ComponentData.EnableProbeScrolling = ScrollProbesInfinitely;
		ComponentData.EnableProbeVisulization = VisualizeProbes;
		ComponentData.EnableVolume = EnableVolume;
		ComponentData.IrradianceScalar = IrradianceScalar;
		ComponentData.EmissiveMultiplier = EmissiveMultiplier;
		ComponentData.LightingMultiplier = LightMultiplier;
		ComponentData.RuntimeStatic = RuntimeStatic;
		ComponentData.SkyLightTypeOnRayMiss = SkyLightTypeOnRayMiss;

		if (ScrollProbesInfinitely)
		{
			// Infinite Scrolling Volume
			// Disable volume transformations and instead move the volume by "scrolling" the probes over an infinite space.
			// Offset "planes" of probes from one end of the volume to the other (in the direction  of movement).
			// Useful for computing GI around a moving object, e.g. characters.
			// NB: scrolling probes can be disruptive when recursive probe sampling is enabled and the volume is small. Sudden changes in scrolled probes will propogate to nearby probes!
			FVector CurrentOrigin = GetOwner()->GetTransform().GetLocation();
			FVector MovementDelta = CurrentOrigin - LastOrigin;

			FVector ProbeGridSpacing;
			FVector VolumeSize = GetOwner()->GetTransform().GetScale3D() * 200.f;
			ProbeGridSpacing.X = VolumeSize.X / float(ProbeCounts.X);
			ProbeGridSpacing.Y = VolumeSize.Y / float(ProbeCounts.Y);
			ProbeGridSpacing.Z = VolumeSize.Z / float(ProbeCounts.Z);

			if (FMath::Abs(MovementDelta.X) >= ProbeGridSpacing.X || FMath::Abs(MovementDelta.Y) >= ProbeGridSpacing.Y || FMath::Abs(MovementDelta.Z) >= ProbeGridSpacing.Z)
			{
				auto absFloor = [](float f)
				{
					return f >= 0.f ? int(floor(f)) : int(ceil(f));
				};

				// Calculate the number of grid cells that have been moved
				FIntVector Translation;
				Translation.X = int(absFloor(MovementDelta.X / ProbeGridSpacing.X));
				Translation.Y = int(absFloor(MovementDelta.Y / ProbeGridSpacing.Y));
				Translation.Z = int(absFloor(MovementDelta.Z / ProbeGridSpacing.Z));

				// Move the volume origin the number of grid cells * the distance between cells
				LastOrigin.X += float(Translation.X) * ProbeGridSpacing.X;
				LastOrigin.Y += float(Translation.Y) * ProbeGridSpacing.Y;
				LastOrigin.Z += float(Translation.Z) * ProbeGridSpacing.Z;

				// Update the probe scroll offset count
				ProbeScrollOffset.X += Translation.X;
				ProbeScrollOffset.Y += Translation.Y;
				ProbeScrollOffset.X += Translation.Z;
			}

			// Set the probe scroll offsets.
			// It is required that the offset will be positive and we need to be able to distinguish between various offsets
			// to reset probes when they are moved to the other side of the volume in Infinite Scrolling Volume case.
			// Therefore, instead of having negative offsets that we roll into positive ones, we center offset around INT_MAX / 2.
			int32 HalfIntMax = INT_MAX / 2;
			FIntVector RoundedHalfIntMax = FIntVector(
				(HalfIntMax / ProbeCounts.X) * ProbeCounts.X,
				(HalfIntMax / ProbeCounts.Y) * ProbeCounts.Y,
				(HalfIntMax / ProbeCounts.Z) * ProbeCounts.Z
			);
			FIntVector ProbeScrollOffsetPositive = ProbeScrollOffset + RoundedHalfIntMax;

			ComponentData.ProbeScrollOffsets.X = (ProbeScrollOffsetPositive.X % ProbeCounts.X) + (ProbeScrollOffsetPositive.X / ProbeCounts.X) * ProbeCounts.X;
			ComponentData.ProbeScrollOffsets.Y = (ProbeScrollOffsetPositive.Y % ProbeCounts.Y) + (ProbeScrollOffsetPositive.Y / ProbeCounts.Y) * ProbeCounts.Y;
			ComponentData.ProbeScrollOffsets.Z = (ProbeScrollOffsetPositive.Z % ProbeCounts.Z) + (ProbeScrollOffsetPositive.Z / ProbeCounts.Z) * ProbeCounts.Z;

			// Set the volume origin and scale (rotation not allowed)
			ComponentData.Origin = LastOrigin;
			ComponentData.Transform.SetScale3D(GetOwner()->GetTransform().GetScale3D());
		}
		else
		{
			// Finite moveable volume
			// Transform the volume to stay aligned with its parent.
			// Useful for spaces that move, e.g. a ship or train car.
			ComponentData.Transform = GetOwner()->GetTransform();
			ComponentData.Origin = LastOrigin = GetOwner()->GetTransform().GetLocation();
			ComponentData.ProbeScrollOffsets = FIntVector{ 0, 0, 0 };
		}

		// If the ProbeCounts are too large to make textures, let's not update the render thread data to avoid a crash.
		// Everything is ok with not getting an update, ever, so this is safe.
		{
			volatile uint32 maxTextureSize = GetMax2DTextureDimension();

			// DDGIRadiance
			if (uint32(ProbeCounts.X * ProbeCounts.Y * ProbeCounts.Z) > maxTextureSize)
				return;

			FIntPoint ProxyDims = ComponentData.Get2DProbeCount();

			// DDGIIrradiance
			{
				int numTexels = FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsIrradiance;
				FIntPoint ProxyTexDims = ProxyDims * (numTexels + 2);
				if (uint32(ProxyTexDims.X) > maxTextureSize || uint32(ProxyTexDims.Y) > maxTextureSize)
					return;
			}

			// DDGIDistance
			{
				int numTexels = FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsDistance;
				FIntPoint ProxyTexDims = ProxyDims * (numTexels + 2);
				if (uint32(ProxyTexDims.X) > maxTextureSize || uint32(ProxyTexDims.Y) > maxTextureSize)
					return;
			}
		}

		FDDGIVolumeSceneProxy* DDGIProxy = SceneProxy;
		EDDGIIrradianceBits IrradianceBits = GetDefault<URTXGIPluginSettings>()->IrradianceBits;
		EDDGIDistanceBits DistanceBits = GetDefault<URTXGIPluginSettings>()->DistanceBits;

		FDDGITextureLoadContext TextureLoadContext = LoadContext;
		LoadContext.ReadyForLoad = false;

		ENQUEUE_RENDER_COMMAND(UpdateGIVolumeTransformCommand)(
			[DDGIProxy, ComponentData, TextureLoadContext, IrradianceBits, DistanceBits](FRHICommandListImmediate& RHICmdList)
			{
				FRDGBuilder GraphBuilder(RHICmdList);

				bool needReallocate =
					DDGIProxy->ComponentData.ProbeCounts != ComponentData.ProbeCounts ||
					DDGIProxy->ComponentData.RaysPerProbe != ComponentData.RaysPerProbe ||
					DDGIProxy->ComponentData.EnableProbeRelocation != ComponentData.EnableProbeRelocation;

				// set the data
				DDGIProxy->ComponentData = ComponentData;

				// handle state textures ready to load from serialization
				if (TextureLoadContext.ReadyForLoad)
					DDGIProxy->TextureLoadContext = TextureLoadContext;

				if (needReallocate)
				{
					DDGIProxy->ReallocateSurfaces_RenderThread(RHICmdList, IrradianceBits, DistanceBits);
					DDGIProxy->ResetTextures_RenderThread(GraphBuilder);
					FDDGIVolumeSceneProxy::AllProxiesReadyForRender_RenderThread.Add(DDGIProxy);
				}

				GraphBuilder.Execute();
			}
		);
	}
}

void UDDGIVolumeComponent::EnableVolumeComponent(bool enabled)
{
	EnableVolume = enabled;
	MarkRenderDynamicDataDirty();
}

void UDDGIVolumeComponent::Startup()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FGlobalIlluminationExperimentalPluginDelegates::FRenderDiffuseIndirectVisualizations& RVDelegate = FGlobalIlluminationExperimentalPluginDelegates::RenderDiffuseIndirectVisualizations();
	FDDGIVolumeSceneProxy::RenderDiffuseIndirectVisualizationsHandle = RVDelegate.AddStatic(FDDGIVolumeSceneProxy::RenderDiffuseIndirectVisualizations_RenderThread);
#endif

	FGlobalIlluminationExperimentalPluginDelegates::FRenderDiffuseIndirectLight& RDILDelegate = FGlobalIlluminationExperimentalPluginDelegates::RenderDiffuseIndirectLight();
	FDDGIVolumeSceneProxy::RenderDiffuseIndirectLightHandle = RDILDelegate.AddStatic(FDDGIVolumeSceneProxy::RenderDiffuseIndirectLight_RenderThread);
}

void UDDGIVolumeComponent::Shutdown()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FGlobalIlluminationExperimentalPluginDelegates::FRenderDiffuseIndirectVisualizations& RVDelegate = FGlobalIlluminationExperimentalPluginDelegates::RenderDiffuseIndirectVisualizations();
	check(FDDGIVolumeSceneProxy::RenderDiffuseIndirectVisualizationsHandle.IsValid());
	RVDelegate.Remove(FDDGIVolumeSceneProxy::RenderDiffuseIndirectVisualizationsHandle);
#endif

	FGlobalIlluminationExperimentalPluginDelegates::FRenderDiffuseIndirectLight& RDILDelegate = FGlobalIlluminationExperimentalPluginDelegates::RenderDiffuseIndirectLight();
	check(FDDGIVolumeSceneProxy::RenderDiffuseIndirectLightHandle.IsValid());
	RDILDelegate.Remove(FDDGIVolumeSceneProxy::RenderDiffuseIndirectLightHandle);
}

bool UDDGIVolumeComponent::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	return ProcessConsoleExec(Cmd, Ar, NULL);
}

#if WITH_EDITOR
bool UDDGIVolumeComponent::CanEditChange(const FProperty* InProperty) const
{
	if (InProperty)
	{
		const FString PropertyName = InProperty->GetName();
		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FProbeRelocation, ProbeMinFrontfaceDistance))
		{
			if (!ProbeRelocation.AutomaticProbeRelocation)
			{
				return false;
			}
		}
		else if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FProbeRelocation, ProbeBackfaceThreshold))
		{
			if (!ProbeRelocation.AutomaticProbeRelocation)
			{
				return false;
			}
		}
	}

	return Super::CanEditChange(InProperty);
}
#endif

void UDDGIVolumeComponent::DDGIClearVolumes()
{
	ENQUEUE_RENDER_COMMAND(DDGIClearVolumesCommand)(
		[](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);

			for (FDDGIVolumeSceneProxy* DDGIProxy : FDDGIVolumeSceneProxy::AllProxiesReadyForRender_RenderThread)
			{
				DDGIProxy->ResetTextures_RenderThread(GraphBuilder);
			}

			GraphBuilder.Execute();
		}
	);
}

void UDDGIVolumeComponent::SendRenderDynamicData_Concurrent()
{
	Super::SendRenderDynamicData_Concurrent();
	UpdateRenderThreadData();
}

void UDDGIVolumeComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	Super::CreateRenderState_Concurrent(Context);
	check(SceneProxy == nullptr);

#if WITH_EDITOR
	if (!GetOwner()->IsTemporarilyHiddenInEditor())
#endif
	{
		SceneProxy = new FDDGIVolumeSceneProxy(GetScene());
		UpdateRenderThreadData();
	}
}

void UDDGIVolumeComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();

	if (SceneProxy)
	{
		FDDGITextureLoadContext& ComponentLoadContext = LoadContext;

		FDDGIVolumeSceneProxy* DDGIProxy = SceneProxy;
		ENQUEUE_RENDER_COMMAND(DeleteProxy)(
			[DDGIProxy, &ComponentLoadContext](FRHICommandListImmediate& RHICmdList)
			{
				// If the component has textures pending load, nothing to do here. Those are the most authoritative.
				if (!ComponentLoadContext.ReadyForLoad)
				{
					// If the proxy has textures pending load which haven't been serviced yet, the component should take those
					// in case it creates another proxy.
					if (DDGIProxy->TextureLoadContext.ReadyForLoad)
					{
						ComponentLoadContext = DDGIProxy->TextureLoadContext;
					}
					// otherwise, we should copy the textures from this proxy into textures for the TextureLoadContext
					// to make them survive to the next proxy for this component if one is created.
					else
					{
						ComponentLoadContext.ReadyForLoad = true;
						ComponentLoadContext.Irradiance = GetTexturePixelsStep1_RenderThread(RHICmdList, DDGIProxy->ProbesIrradiance->GetTargetableRHI());
						ComponentLoadContext.Distance = GetTexturePixelsStep1_RenderThread(RHICmdList, DDGIProxy->ProbesDistance->GetTargetableRHI());
						ComponentLoadContext.Offsets = GetTexturePixelsStep1_RenderThread(RHICmdList, DDGIProxy->ProbesOffsets ? DDGIProxy->ProbesOffsets->GetTargetableRHI() : nullptr);
						ComponentLoadContext.States = GetTexturePixelsStep1_RenderThread(RHICmdList, DDGIProxy->ProbesStates ? DDGIProxy->ProbesStates->GetTargetableRHI() : nullptr);
					}
				}

				delete DDGIProxy;
			}
		);

		// wait for the above command to finish, so we know we got the load context if present
		FlushRenderingCommands();

		SceneProxy = nullptr;
	}
}

void UDDGIVolumeComponent::ClearProbeData()
{
	FDDGIVolumeSceneProxy* DDGIProxy = SceneProxy;

	ENQUEUE_RENDER_COMMAND(DDGIClearProbeData)(
		[DDGIProxy](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);

			DDGIProxy->ResetTextures_RenderThread(GraphBuilder);

			GraphBuilder.Execute();
		}
	);
}

void UDDGIVolumeComponent::ToggleVolume(bool IsVolumeEnabled)
{
	EnableVolumeComponent(IsVolumeEnabled);
}

float UDDGIVolumeComponent::GetIrradianceScalar() const
{
	return IrradianceScalar;
}

void UDDGIVolumeComponent::SetIrradianceScalar(float NewIrradianceScalar)
{
	IrradianceScalar = NewIrradianceScalar;
	MarkRenderDynamicDataDirty();
}

float UDDGIVolumeComponent::GetEmissiveMultiplier() const
{
	return EmissiveMultiplier;
}

void UDDGIVolumeComponent::SetEmissiveMultiplier(float NewEmissiveMultiplier)
{
	EmissiveMultiplier = NewEmissiveMultiplier;
	MarkRenderDynamicDataDirty();
}

float UDDGIVolumeComponent::GetLightMultiplier() const
{
	return LightMultiplier;
}

void UDDGIVolumeComponent::SetLightMultiplier(float NewLightMultiplier)
{
	LightMultiplier = NewLightMultiplier;
	MarkRenderDynamicDataDirty();
}
