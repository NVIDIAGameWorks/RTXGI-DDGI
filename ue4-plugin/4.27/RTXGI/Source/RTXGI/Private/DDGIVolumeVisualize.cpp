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
#include "RenderGraphBuilder.h"
#include "ShaderParameterStruct.h"
#include "ShaderParameterUtils.h"
#include "SystemTextures.h"

// UE4 private interfaces
#include "PostProcess/SceneRenderTargets.h"
#include "SceneRendering.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

DECLARE_GPU_STAT_NAMED(RTXGI_Visualizations, TEXT("RTXGI Visualizations"));

BEGIN_SHADER_PARAMETER_STRUCT(FVolumeVisualizeShaderParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ProbeIrradianceTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ProbeDistanceTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ProbeOffsets)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ProbeStates)
	SHADER_PARAMETER_SAMPLER(SamplerState, ProbeSampler)
	SHADER_PARAMETER(int, Mode)
	SHADER_PARAMETER(float, ProbeRadius)
	SHADER_PARAMETER(float, DepthScale)
	SHADER_PARAMETER(int, VolumeProbeNumIrradianceTexels)
	SHADER_PARAMETER(int, VolumeProbeNumDistanceTexels)
	SHADER_PARAMETER(float, VolumeProbeIrradianceEncodingGamma)
	SHADER_PARAMETER(FVector, VolumePosition)
	SHADER_PARAMETER(FVector4, VolumeRotation)
	SHADER_PARAMETER(FVector, VolumeProbeGridSpacing)
	SHADER_PARAMETER(FIntVector, VolumeProbeGridCounts)
	SHADER_PARAMETER(FMatrix, WorldToClip)
	SHADER_PARAMETER(FVector, CameraPosition)
	SHADER_PARAMETER(float, PreExposure)
	SHADER_PARAMETER(int32, ShouldUsePreExposure)
	SHADER_PARAMETER(FIntVector, VolumeProbeScrollOffsets)
	SHADER_PARAMETER(float, IrradianceScalar)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FVolumeVisualizeShaderVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVolumeVisualizeShaderVS);
	SHADER_USE_PARAMETER_STRUCT(FVolumeVisualizeShaderVS, FGlobalShader);

	using FParameters = FVolumeVisualizeShaderParameters;

	class FEnableRelocation : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_PROBE_RELOCATION");
	class FEnableScrolling : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_INFINITE_SCROLLING_VOLUME");

	using FPermutationDomain = TShaderPermutationDomain<FEnableRelocation, FEnableScrolling>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("RTXGI_DDGI_PROBE_CLASSIFICATION"), FDDGIVolumeSceneProxy::FComponentData::c_RTXGI_DDGI_PROBE_CLASSIFICATION ? 1 : 0);

		// needed for a typed UAV load. This already assumes we are raytracing, so should be fine.
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

class FVolumeVisualizeShaderPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FVolumeVisualizeShaderPS);
	SHADER_USE_PARAMETER_STRUCT(FVolumeVisualizeShaderPS, FGlobalShader);

	using FParameters = FVolumeVisualizeShaderParameters;

	class FEnableRelocation : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_PROBE_RELOCATION");
	class FEnableScrolling : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_INFINITE_SCROLLING_VOLUME");
	class FFormatRadiance : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_FORMAT_RADIANCE");
	class FFormatIrradiance : SHADER_PERMUTATION_BOOL("RTXGI_DDGI_FORMAT_IRRADIANCE");

	using FPermutationDomain = TShaderPermutationDomain<FEnableRelocation, FEnableScrolling, FFormatRadiance, FFormatIrradiance>;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("RTXGI_DDGI_PROBE_CLASSIFICATION"), FDDGIVolumeSceneProxy::FComponentData::c_RTXGI_DDGI_PROBE_CLASSIFICATION ? 1 : 0);

		// needed for a typed UAV load. This already assumes we are raytracing, so should be fine.
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

IMPLEMENT_GLOBAL_SHADER(FVolumeVisualizeShaderVS, "/Plugin/RTXGI/Private/VisualizeDDGIProbes.usf", "VisualizeDDGIProbesVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FVolumeVisualizeShaderPS, "/Plugin/RTXGI/Private/VisualizeDDGIProbes.usf", "VisualizeDDGIProbesPS", SF_Pixel);

/**
* Probe sphere vertex buffer. Defines a sphere of unit size.
*/
template<int32 NumSphereSides, int32 NumSphereRings, typename VectorType>
class TDDGIProbeSphereVertexBuffer : public FVertexBuffer
{
public:

	int32 GetNumRings() const
	{
		return NumSphereRings;
	}

	/**
	* Initialize the RHI for this rendering resource
	*/
	void InitRHI() override
	{
		const int32 NumSides = NumSphereSides;
		const int32 NumRings = NumSphereRings;
		const int32 NumVerts = (NumSides + 1) * (NumRings + 1);

		const float RadiansPerRingSegment = PI / (float)NumRings;
		float Radius = 1;

		TArray<VectorType, TInlineAllocator<NumRings + 1> > ArcVerts;
		ArcVerts.Empty(NumRings + 1);
		// Calculate verts for one arc
		for (int32 i = 0; i < NumRings + 1; i++)
		{
			const float Angle = i * RadiansPerRingSegment;
			ArcVerts.Add(FVector(0.0f, FMath::Sin(Angle), FMath::Cos(Angle)));
		}

		TResourceArray<VectorType, VERTEXBUFFER_ALIGNMENT> Verts;
		Verts.Empty(NumVerts);
		// Then rotate this arc NumSides + 1 times.
		const FVector Center = FVector(0, 0, 0);
		for (int32 s = 0; s < NumSides + 1; s++)
		{
			FRotator ArcRotator(0, 360.f * ((float)s / NumSides), 0);
			FRotationMatrix ArcRot(ArcRotator);

			for (int32 v = 0; v < NumRings + 1; v++)
			{
				const int32 VIx = (NumRings + 1) * s + v;
				Verts.Add(Center + Radius * ArcRot.TransformPosition(ArcVerts[v]));
			}
		}

		NumSphereVerts = Verts.Num();
		uint32 Size = Verts.GetResourceDataSize();

		// Create vertex buffer. Fill buffer with initial data upon creation
		FRHIResourceCreateInfo CreateInfo(&Verts);
		VertexBufferRHI = RHICreateVertexBuffer(Size, BUF_Static, CreateInfo);
	}

	int32 GetVertexCount() const { return NumSphereVerts; }

	/**
	* Calculates the world transform for a sphere.
	* @param OutTransform - The output world transform.
	* @param Sphere - The sphere to generate the transform for.
	* @param PreViewTranslation - The pre-view translation to apply to the transform.
	* @param bConservativelyBoundSphere - when true, the sphere that is drawn will contain all positions in the analytical sphere,
	*		 Otherwise the sphere vertices will lie on the analytical sphere and the positions on the faces will lie inside the sphere.
	*/
	void CalcTransform(FVector4& OutPosAndScale, const FSphere& Sphere, const FVector& PreViewTranslation, bool bConservativelyBoundSphere = true)
	{
		float Radius = Sphere.W;
		if (bConservativelyBoundSphere)
		{
			const int32 NumRings = NumSphereRings;
			const float RadiansPerRingSegment = PI / (float)NumRings;

			// Boost the effective radius so that the edges of the sphere approximation lie on the sphere, instead of the vertices
			Radius /= FMath::Cos(RadiansPerRingSegment);
		}

		const FVector Translate(Sphere.Center + PreViewTranslation);
		OutPosAndScale = FVector4(Translate, Radius);
	}

private:
	int32 NumSphereVerts;
};

/**
* Probe sphere index buffer.
*/
template<int32 NumSphereSides, int32 NumSphereRings>
class TDDGIProbeSphereIndexBuffer : public FIndexBuffer
{
public:
	/**
	* Initialize the RHI for this rendering resource
	*/
	void InitRHI() override
	{
		const int32 NumSides = NumSphereSides;
		const int32 NumRings = NumSphereRings;
		TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> Indices;

		// Add triangles for all the vertices generated
		for (int32 s = 0; s < NumSides; s++)
		{
			const int32 a0start = (s + 0) * (NumRings + 1);
			const int32 a1start = (s + 1) * (NumRings + 1);

			for (int32 r = 0; r < NumRings; r++)
			{
				Indices.Add(a0start + r + 0);
				Indices.Add(a1start + r + 0);
				Indices.Add(a0start + r + 1);
				Indices.Add(a1start + r + 0);
				Indices.Add(a1start + r + 1);
				Indices.Add(a0start + r + 1);
			}
		}

		NumIndices = Indices.Num();
		const uint32 Size = Indices.GetResourceDataSize();
		const uint32 Stride = sizeof(uint16);

		// Create index buffer. Fill buffer with initial data upon creation
		FRHIResourceCreateInfo CreateInfo(&Indices);
		IndexBufferRHI = RHICreateIndexBuffer(Stride, Size, BUF_Static, CreateInfo);
	}

	int32 GetIndexCount() const { return NumIndices; };

private:
	int32 NumIndices;
};

struct FVisualDDGIProbesVertex
{
	FVector4 Position;
	FVisualDDGIProbesVertex() {}
	FVisualDDGIProbesVertex(const FVector4& InPosition) : Position(InPosition) {}
};

class FVisualizeDDGIProbesVertexDeclaration : public FRenderResource
{
public:

	FVertexDeclarationRHIRef VertexDeclarationRHI;

	virtual ~FVisualizeDDGIProbesVertexDeclaration() {}

	virtual void InitRHI() override
	{
		FVertexDeclarationElementList Elements;
		uint16 Stride = sizeof(FVisualDDGIProbesVertex);
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FVisualDDGIProbesVertex, Position), VET_Float4, 0, Stride));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI() override
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

TGlobalResource<FVisualizeDDGIProbesVertexDeclaration> GVisualizeDDGIProbesVertexDeclaration;
TGlobalResource<TDDGIProbeSphereVertexBuffer<36, 24, FVector4>> GDDGIProbeSphereVertexBuffer;
TGlobalResource<TDDGIProbeSphereIndexBuffer<36, 24>> GDDGIProbeSphereIndexBuffer;

void FDDGIVolumeSceneProxy::RenderDiffuseIndirectVisualizations_RenderThread(
	const FScene& Scene,
	const FViewInfo& View,
	FRDGBuilder& GraphBuilder,
	FGlobalIlluminationExperimentalPluginResources& Resources)
{
	check(IsInRenderingThread() || IsInParallelRenderingThread());

	int mode = static_cast<int>(GetDefault<URTXGIPluginSettings>()->ProbesVisualization);
	if (mode < 1 || mode > 3) return;

	RDG_GPU_STAT_SCOPE(GraphBuilder, RTXGI_Visualizations);
	RDG_EVENT_SCOPE(GraphBuilder, "RTXGI Visualizations");

	float probeRadius = GetDefault<URTXGIPluginSettings>()->DebugProbeRadius;
	float depthScale = GetDefault<URTXGIPluginSettings>()->ProbesDepthScale;

	// Get other things we'll need for all proxies
	FIntRect ViewRect = View.ViewRect;
	FRDGTextureRef SceneColorTexture = GraphBuilder.RegisterExternalTexture(Resources.SceneColor);
	FRDGTextureRef SceneDepthTexture = GraphBuilder.RegisterExternalTexture(Resources.SceneDepthZ);

	for (FDDGIVolumeSceneProxy* proxy : FDDGIVolumeSceneProxy::AllProxiesReadyForRender_RenderThread)
	{
		// Skip if the volume visualization is not enabled
		if (!proxy->ComponentData.EnableProbeVisulization) continue;

		// Skip this volume if it isn't part of the current scene
		if (proxy->OwningScene != &Scene) continue;

		// Skip this volume if it is not enabled
		if (!proxy->ComponentData.EnableVolume) continue;

		// Skip this volume if it doesn't intersect the view frustum
		if (!proxy->IntersectsViewFrustum(View)) continue;

		// Get the shader permutation
		FVolumeVisualizeShaderVS::FPermutationDomain PermutationVectorVS;
		PermutationVectorVS.Set<FVolumeVisualizeShaderVS::FEnableRelocation>(proxy->ComponentData.EnableProbeRelocation);
		PermutationVectorVS.Set<FVolumeVisualizeShaderVS::FEnableScrolling>(proxy->ComponentData.EnableProbeScrolling);

		FVolumeVisualizeShaderPS::FPermutationDomain PermutationVectorPS;
		PermutationVectorPS.Set<FVolumeVisualizeShaderPS::FEnableRelocation>(proxy->ComponentData.EnableProbeRelocation);
		PermutationVectorPS.Set<FVolumeVisualizeShaderPS::FEnableScrolling>(proxy->ComponentData.EnableProbeScrolling);
		bool highBitCount = (GetDefault<URTXGIPluginSettings>()->IrradianceBits == EDDGIIrradianceBits::n32);
		PermutationVectorPS.Set<FVolumeVisualizeShaderPS::FFormatRadiance>(highBitCount);
		PermutationVectorPS.Set<FVolumeVisualizeShaderPS::FFormatIrradiance>(highBitCount);

		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
		TShaderMapRef<FVolumeVisualizeShaderVS> VertexShader(GlobalShaderMap, PermutationVectorVS);
		TShaderMapRef<FVolumeVisualizeShaderPS> PixelShader(GlobalShaderMap, PermutationVectorPS);

		// Set shader pass parameters
		FVolumeVisualizeShaderParameters DefaultPassParameters;
		FVolumeVisualizeShaderParameters* PassParameters = GraphBuilder.AllocParameters<FVolumeVisualizeShaderParameters>();
		*PassParameters = DefaultPassParameters;

		PassParameters->ProbeIrradianceTexture = GraphBuilder.RegisterExternalTexture(proxy->ProbesIrradiance);
		PassParameters->ProbeDistanceTexture = GraphBuilder.RegisterExternalTexture(proxy->ProbesDistance);
		PassParameters->ProbeOffsets = RegisterExternalTextureWithFallback(GraphBuilder, proxy->ProbesOffsets, GSystemTextures.BlackDummy);
		PassParameters->ProbeStates = RegisterExternalTextureWithFallback(GraphBuilder, proxy->ProbesStates, GSystemTextures.BlackDummy);
		PassParameters->ProbeRadius = probeRadius;
		PassParameters->DepthScale = depthScale;
		PassParameters->WorldToClip = View.ViewMatrices.GetViewProjectionMatrix();
		PassParameters->CameraPosition = View.ViewLocation;

		PassParameters->ShouldUsePreExposure = View.Family->EngineShowFlags.Tonemapper;
		PassParameters->PreExposure = View.PreExposure;
		PassParameters->IrradianceScalar = proxy->ComponentData.IrradianceScalar;

		PassParameters->VolumeProbeScrollOffsets = proxy->ComponentData.ProbeScrollOffsets;

		PassParameters->ProbeSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->Mode = mode;
		PassParameters->VolumeProbeIrradianceEncodingGamma = proxy->ComponentData.ProbeIrradianceEncodingGamma;

		PassParameters->VolumePosition = proxy->ComponentData.Origin;
		FQuat rotation = proxy->ComponentData.Transform.GetRotation();
		PassParameters->VolumeRotation = FVector4{ rotation.X, rotation.Y, rotation.Z, rotation.W };

		FVector volumeSize = proxy->ComponentData.Transform.GetScale3D() * 200.0f;
		FVector probeGridSpacing;
		probeGridSpacing.X = volumeSize.X / float(proxy->ComponentData.ProbeCounts.X);
		probeGridSpacing.Y = volumeSize.Y / float(proxy->ComponentData.ProbeCounts.Y);
		probeGridSpacing.Z = volumeSize.Z / float(proxy->ComponentData.ProbeCounts.Z);
		PassParameters->VolumeProbeGridSpacing = probeGridSpacing;

		PassParameters->VolumeProbeGridCounts = proxy->ComponentData.ProbeCounts;

		PassParameters->VolumeProbeNumIrradianceTexels = FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsIrradiance;
		PassParameters->VolumeProbeNumDistanceTexels = FDDGIVolumeSceneProxy::FComponentData::c_NumTexelsDistance;

		PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneColorTexture, ERenderTargetLoadAction::ELoad);
		PassParameters->RenderTargets.DepthStencil = FDepthStencilBinding(SceneDepthTexture, ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ENoAction, FExclusiveDepthStencil::DepthWrite_StencilNop);

		uint32 NumInstances = uint32(proxy->ComponentData.ProbeCounts.X * proxy->ComponentData.ProbeCounts.Y * proxy->ComponentData.ProbeCounts.Z);

		GraphBuilder.AddPass(
			Forward<FRDGEventName>(RDG_EVENT_NAME("DDGI Visualize Probes")),
			PassParameters,
			ERDGPassFlags::Raster,
			[PassParameters, GlobalShaderMap, VertexShader, PixelShader, ViewRect, NumInstances](FRHICommandList& RHICmdList)
			{
				RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);

				FGraphicsPipelineStateInitializer GraphicsPSOInit;
				RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

				GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_DepthNearOrEqual>::GetRHI();
				GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<CW_RGB, CW_RGBA>::GetRHI();

				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GVisualizeDDGIProbesVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;
				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

				SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), *PassParameters);
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PassParameters);

				RHICmdList.SetStreamSource(0, GDDGIProbeSphereVertexBuffer.VertexBufferRHI, 0);
				RHICmdList.DrawIndexedPrimitive(GDDGIProbeSphereIndexBuffer.IndexBufferRHI, 0, 0, GDDGIProbeSphereVertexBuffer.GetVertexCount(), 0, GDDGIProbeSphereIndexBuffer.GetIndexCount() / 3, NumInstances);
			}
		);
	}
}
#else
void FDDGIVolumeSceneProxy::RenderDiffuseIndirectVisualizations_RenderThread(
	const FScene& Scene,
	const FViewInfo& View,
	FRDGBuilder& GraphBuilder,
	FGlobalIlluminationExperimentalPluginResources& Resources){}
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

static bool MemoryUseExec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (!FParse::Command(&Cmd, TEXT("r.RTXGI.MemoryUsed")))
		return false;

	struct VolumeMemoryInfo
	{
		const FDDGIVolumeSceneProxy* proxy = nullptr;
		const AActor* actor = nullptr;
		int irradianceBytes = 0;
		int distanceBytes = 0;
		int offsetsBytes = 0;
		int statesBytes = 0;
	};

	TArray<VolumeMemoryInfo> memoryInfo;

	ENQUEUE_RENDER_COMMAND(MemoryUsage)(
		[&memoryInfo](FRHICommandListImmediate& RHICmdList)
		{
			for (FDDGIVolumeSceneProxy* proxy : FDDGIVolumeSceneProxy::AllProxiesReadyForRender_RenderThread)
			{
				VolumeMemoryInfo info;
				info.proxy = proxy;

				if (proxy->ProbesIrradiance)
				{
					FRHITexture2D* texture = proxy->ProbesIrradiance->GetShaderResourceRHI()->GetTexture2D();
					if (texture)
						info.irradianceBytes = texture->GetSizeX()* texture->GetSizeY()* GPixelFormats[texture->GetFormat()].BlockBytes;
				}

				if (proxy->ProbesDistance)
				{
					FRHITexture2D* texture = proxy->ProbesDistance->GetShaderResourceRHI()->GetTexture2D();
					if (texture)
						info.distanceBytes = texture->GetSizeX() * texture->GetSizeY() * GPixelFormats[texture->GetFormat()].BlockBytes;
				}

				if (proxy->ProbesOffsets)
				{
					FRHITexture2D* texture = proxy->ProbesOffsets->GetShaderResourceRHI()->GetTexture2D();
					if (texture)
						info.offsetsBytes = texture->GetSizeX() * texture->GetSizeY() * GPixelFormats[texture->GetFormat()].BlockBytes;
				}

				if (proxy->ProbesStates)
				{
					FRHITexture2D* texture = proxy->ProbesStates->GetShaderResourceRHI()->GetTexture2D();
					if (texture)
						info.statesBytes = texture->GetSizeX() * texture->GetSizeY() * GPixelFormats[texture->GetFormat()].BlockBytes;
				}

				memoryInfo.Push(info);
			}
		}
	);
	FlushRenderingCommands();

	ULevel* level = InWorld->GetCurrentLevel();

	for (AActor* actor : level->Actors)
	{
		if (!actor)
			continue;

		UDDGIVolumeComponent* volume = Cast<UDDGIVolumeComponent>(actor->GetComponentByClass(UDDGIVolumeComponent::StaticClass()));
		if (!volume)
			continue;

		for (VolumeMemoryInfo& info : memoryInfo)
		{
			if (info.proxy == volume->SceneProxy)
			{
				info.actor = actor;
				break;
			}
		}
	}

	UE_LOG(LogConsoleResponse, Log, TEXT("RTXGI Texture Memory Usage (NOTE: Does not include alignment padding, so actual memory usage could be higher):"));

	int totalBytes = 0;
	static const float c_oneMegabyte = 1024.0f * 1024.0f;

	for (VolumeMemoryInfo& info : memoryInfo)
	{
		int bytes = 0;
		bytes += info.irradianceBytes;
		bytes += info.distanceBytes;
		bytes += info.offsetsBytes;
		bytes += info.statesBytes;
		totalBytes += bytes;

		UE_LOG(LogConsoleResponse, Log, TEXT("  %s: %0.2f MB (%d B)"), (info.actor ? *info.actor->GetFullName() : *FString("<Unknown>")), float(bytes) / c_oneMegabyte, bytes);

		UE_LOG(LogConsoleResponse, Log, TEXT("    Irradiance: %0.2f MB (%d B)"), float(info.irradianceBytes) / c_oneMegabyte, info.irradianceBytes);
		UE_LOG(LogConsoleResponse, Log, TEXT("    Distance: %0.2f MB (%d B)"), float(info.distanceBytes) / c_oneMegabyte, info.distanceBytes);
		UE_LOG(LogConsoleResponse, Log, TEXT("    Offsets: %0.2f MB (%d B)"), float(info.offsetsBytes) / c_oneMegabyte, info.offsetsBytes);
		UE_LOG(LogConsoleResponse, Log, TEXT("    States: %0.2f MB (%d B)"), float(info.statesBytes) / c_oneMegabyte, info.statesBytes);
	}

	UE_LOG(LogConsoleResponse, Log, TEXT("Total: %0.2f MB (%d B)"), float(totalBytes) / c_oneMegabyte, totalBytes);

	return true;
}

static FStaticSelfRegisteringExec RendererExecRegistration(MemoryUseExec);

#endif  // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
