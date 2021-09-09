/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

// UE4 public interfaces
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Components/SceneComponent.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RendererInterface.h"

#include "DDGIVolumeComponent.generated.h"

class FRDGBuilder;
class FRHICommandListImmediate;
class FScene;
class FSceneInterface;
class FSceneRenderTargets;
class FViewInfo;
class FGlobalIlluminationExperimentalPluginResources;

enum class EDDGIIrradianceBits : uint8;
enum class EDDGIDistanceBits : uint8;

// This needs to match the shader code in ProbeBlendingCS.usf
UENUM()
enum class EDDGIRaysPerProbe
{
	n144 = 144 UMETA(DisplayName = "144"),
	n288 = 288 UMETA(DisplayName = "288"),
	n432 = 432 UMETA(DisplayName = "432"),
	n576 = 576 UMETA(DisplayName = "576"),
	n720 = 720 UMETA(DisplayName = "720"),
	n864 = 864 UMETA(DisplayName = "864"),
	n1008 = 1008 UMETA(DisplayName = "1008")
};

UENUM()
enum class EDDGISkyLightType
{
	None UMETA(DisplayName = "None"),
	Raster UMETA(DisplayName = "Raster"),
	RayTracing UMETA(DisplayName = "Ray Tracing")
};

struct FDDGITexturePixels
{
	struct
	{
		uint32 Width = 0;
		uint32 Height = 0;
		uint32 Stride = 0;
		uint32 PixelFormat = 0;
	} Desc;
	TArray<uint8> Pixels;
	FTexture2DRHIRef Texture;
};

struct FDDGITextureLoadContext
{
	bool ReadyForLoad = false;
	FDDGITexturePixels Irradiance;
	FDDGITexturePixels Distance;
	FDDGITexturePixels Offsets;
	FDDGITexturePixels States;

	void Clear()
	{
		*this = FDDGITextureLoadContext();
	}
};

class RTXGI_API FDDGIVolumeSceneProxy
{
public:

	/** Initialization constructor. */
	FDDGIVolumeSceneProxy(FSceneInterface* InOwningScene)
		: OwningScene(InOwningScene)
	{
	}

	virtual ~FDDGIVolumeSceneProxy()
	{
		check(IsInRenderingThread() || IsInParallelRenderingThread());
		AllProxiesReadyForRender_RenderThread.Remove(this);
	}

	bool IntersectsViewFrustum(const FViewInfo& View);

	static void RenderDiffuseIndirectLight_RenderThread(
		const FScene& Scene,
		const FViewInfo& View,
		FRDGBuilder& GraphBuilder,
		FGlobalIlluminationExperimentalPluginResources& Resources);

	static void RenderDiffuseIndirectVisualizations_RenderThread(
		const FScene& Scene,
		const FViewInfo& View,
		FRDGBuilder& GraphBuilder,
		FGlobalIlluminationExperimentalPluginResources& Resources);

	void ReallocateSurfaces_RenderThread(FRHICommandListImmediate& RHICmdList, EDDGIIrradianceBits IrradianceBits, EDDGIDistanceBits DistanceBits);
	void ResetTextures_RenderThread(FRDGBuilder& GraphBuilder);

	static void OnIrradianceOrDistanceBitsChange();

	static FDelegateHandle RenderDiffuseIndirectLightHandle;
	static FDelegateHandle RenderDiffuseIndirectVisualizationsHandle;

	// data from the component
	struct FComponentData
	{
		// A shared location cpp side for operational defines
		static const bool c_RTXGI_DDGI_PROBE_CLASSIFICATION = true;

		// It considers this many volumes that pass frustum culling when sampling GI for the scene.
		static const int c_RTXGI_DDGI_MAX_SHADING_VOLUMES = 12;

		static const EPixelFormat c_pixelFormatRadianceLowBitDepth = EPixelFormat::PF_G32R32F;
		static const EPixelFormat c_pixelFormatRadianceHighBitDepth = EPixelFormat::PF_A32B32G32R32F;
		static const EPixelFormat c_pixelFormatIrradianceLowBitDepth = EPixelFormat::PF_A2B10G10R10;
		static const EPixelFormat c_pixelFormatIrradianceHighBitDepth = EPixelFormat::PF_A32B32G32R32F;
		static const EPixelFormat c_pixelFormatDistanceHighBitDepth = EPixelFormat::PF_G32R32F;
		static const EPixelFormat c_pixelFormatDistanceLowBitDepth = EPixelFormat::PF_G16R16F;
		static const EPixelFormat c_pixelFormatOffsets = EPixelFormat::PF_A16B16G16R16;
		static const EPixelFormat c_pixelFormatStates = EPixelFormat::PF_R8_UINT;

		static const EPixelFormat c_pixelFormatScrollSpace = EPixelFormat::PF_R8_UINT;


		// ProbeBlendingCS (.hlsl in SDK, .usf in plugin) needs this as a define so is a hard coded constant right now.
		// We need that shader to not require that as a define. Then, we can make it a tuneable parameter on the volume.
		// There should be a task on the SDK about this.
		static const uint32 c_NumTexelsIrradiance = 6;
		static const uint32 c_NumTexelsDistance = 14;

		uint32 GetNumRaysPerProbe() const
		{
			switch (RaysPerProbe)
			{
				case EDDGIRaysPerProbe::n144: return 144;
				case EDDGIRaysPerProbe::n288: return 288;
				case EDDGIRaysPerProbe::n432: return 432;
				case EDDGIRaysPerProbe::n576: return 576;
				case EDDGIRaysPerProbe::n720: return 720;
				case EDDGIRaysPerProbe::n864: return 864;
				case EDDGIRaysPerProbe::n1008: return 1008;
			}
			check(false);
			return 144;
		}

		EDDGIRaysPerProbe RaysPerProbe = EDDGIRaysPerProbe::n144;
		float ProbeMaxRayDistance = 1000.0f;
		FTransform Transform = FTransform::Identity;
		FVector Origin = FVector(0);
		FLightingChannels LightingChannels;
		FIntVector ProbeCounts = FIntVector(0); // 0 = invalid, will be written with valid counts before use
		float ProbeDistanceExponent = 1.0f;
		float ProbeIrradianceEncodingGamma = 1.0f;
		int   LightingPriority = 0;
		float UpdatePriority = 1.0f;
		float ProbeHysteresis = 0.0f;
		float ProbeChangeThreshold = 0.0f;
		float ProbeBrightnessThreshold = 0.0f;
		float NormalBias = 0.0f;
		float ViewBias = 0.0f;
		float BlendDistance = 0.0f;
		float BlendDistanceBlack = 0.0f;
		float ProbeBackfaceThreshold = 0.0f;
		float ProbeMinFrontfaceDistance = 0.0f;
		bool EnableProbeRelocation = false;
		bool EnableProbeScrolling = false;
		bool EnableProbeVisulization = false;
		bool EnableVolume = true;
		FIntVector ProbeScrollOffsets = FIntVector{ 0, 0, 0 };
		float IrradianceScalar = 1.0f;
		float EmissiveMultiplier = 1.0f;
		float LightingMultiplier = 1.0f;
		bool RuntimeStatic = false; // If true, does not update during gameplay, only during editor.
		EDDGISkyLightType SkyLightTypeOnRayMiss = EDDGISkyLightType::Raster;

		// This is GetDDGIVolumeProbeCounts() from the SDK
		FIntPoint Get2DProbeCount() const
		{
			return FIntPoint(
				ProbeCounts.Y * ProbeCounts.Z,
				ProbeCounts.X
			);
		}

		int GetProbeCount() const
		{
			return ProbeCounts.X * ProbeCounts.Y * ProbeCounts.Z;
		}
	};
	FComponentData ComponentData;
	FDDGITextureLoadContext TextureLoadContext;

	TRefCountPtr<IPooledRenderTarget> ProbesIrradiance;
	TRefCountPtr<IPooledRenderTarget> ProbesDistance;
	TRefCountPtr<IPooledRenderTarget> ProbesOffsets;
	TRefCountPtr<IPooledRenderTarget> ProbesStates;
	TRefCountPtr<IPooledRenderTarget> ProbesSpace;


	// Where to start the probe update from, for updating a subset of probes
	int ProbeIndexStart = 0;
	int ProbeIndexCount = 0;

	static TSet<FDDGIVolumeSceneProxy*> AllProxiesReadyForRender_RenderThread;
	static TMap<const FSceneInterface*, float> SceneRoundRobinValue;

	// Only render volumes in the scenes they are present in
	FSceneInterface* OwningScene;
};

USTRUCT(BlueprintType)
struct FProbeRelocation
{
	GENERATED_USTRUCT_BODY()

	// If true, probes will attempt to relocate within their cell to leave geometry.
	UPROPERTY(EditAnywhere, Category = "GI Probes");
	bool AutomaticProbeRelocation = true;

	// Probe relocation moves probes that see front facing triangles closer than this value.
	UPROPERTY(EditAnywhere, Category = "GI Probes", meta = (ClampMin = "0"));
	float ProbeMinFrontfaceDistance = 10.0f;

	// Probe relocation and state classifier assume probes with more than this ratio of backface hits are inside of geometry.
	UPROPERTY(EditAnywhere, Category = "GI Probes", meta = (ClampMin = "0", ClampMax = "1"));
	float ProbeBackfaceThreshold = 0.25f;
};

UCLASS(HideCategories = (Tags, AssetUserData, Collision, Cooking, Transform, Rendering, Mobility, LOD))
class RTXGI_API UDDGIVolumeComponent : public USceneComponent, public FSelfRegisteringExec
{
	GENERATED_UCLASS_BODY()

protected:

	void InitializeComponent() override final;

	void Serialize(FArchive& Ar) override final;

	//~ Begin UActorComponent Interface
	virtual bool ShouldCreateRenderState() const override { return true; }
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;
	virtual void DestroyRenderState_Concurrent() override;
	virtual void SendRenderDynamicData_Concurrent() override;
	//~ Begin UActorComponent Interface

public:
	void UpdateRenderThreadData();
	void EnableVolumeComponent(bool enabled);

	static void Startup();
	static void Shutdown();

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
#endif // WITH_EDITOR

	/**
	 * FExec interface
	 */
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	// Clears the probe textures on all volumes
	UFUNCTION(exec)
	void DDGIClearVolumes();

public:
	// --- "GI Volume" properties

	// If true, the volume will be a candidate to be updated and render indirect light into the scene (if also in the view frustum).
	UPROPERTY(EditAnywhere, Category = "GI Volume");
	bool EnableVolume = true;

	// A priority value for scheduling updates to this volume's probes. Volumes with higher priority values get updated more often. Weighted round robin updating.
	UPROPERTY(EditAnywhere, Category = "GI Volume", meta = (ClampMin = "0.0001", ClampMax = "100.0"));
	float UpdatePriority = 1.0f;

	// A priority value used to select volumes when applying lighting. The volume with the lowest priority value is selected.
	// If volumes have the same priority, then volumes are selected based on probe density. The highest density volume is selected.
	UPROPERTY(EditAnywhere, Category = "GI Volume", meta = (ClampMin = "0", ClampMax = "10"));
	int32 LightingPriority = 0;

	// The distance in world units that this volume blends to a volume it overlaps, or fades out.
	UPROPERTY(EditAnywhere, Category = "GI Volume");
	float BlendingDistance = 20.0f;

	// The distance from the edge of a volume at which it has zero weighting (turns black or yields to an encompassing volume). Useful if you don't want a linear fade all the way to the edge, which can be useful for scrolling volumes, hiding probes that haven't converged yet.
	// Volume Blend Distance begins at this distance from the edge.
	UPROPERTY(EditAnywhere, Category = "GI Volume");
	float BlendingCutoffDistance = 0.0f;

	// If true, the volume will not update at runtime, and will keep the lighting values seen when the level is saved.
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "GI Volume");
	bool RuntimeStatic = false;

	UPROPERTY(VisibleDefaultsOnly, AdvancedDisplay, Category = "GI Volume");
	FVector LastOrigin = FVector{ 0.0f, 0.0f, 0.0f };

	// --- "GI Probes" properties

	// Number of rays shot for each probe when updating probe data.
	UPROPERTY(EditAnywhere, Category = "GI Probes");
	EDDGIRaysPerProbe RaysPerProbe = EDDGIRaysPerProbe::n288;

	// Number of probes on each axis.
	UPROPERTY(EditAnywhere, Category = "GI Probes", meta = (ClampMin = "1"));
	FIntVector ProbeCounts = FIntVector(8, 8, 8);

	// Maximum distance a probe ray may travel. Shortening this can increase performance. If you shorten it too much, it can miss geometry.
	UPROPERTY(EditAnywhere, Category = "GI Probes", meta = (ClampMin = "0"));
	float ProbeMaxRayDistance = 100000.0f;

	// Controls the influence of new rays when updating each probe. Values towards 1 will keep history longer, while values towards 0 will be more responsive to current values.
	UPROPERTY(EditAnywhere, Category = "GI Probes", meta = (ClampMin = "0", ClampMax = "1"));
	float ProbeHistoryWeight = 0.97f;

	// Probes relocation.
	UPROPERTY(EditAnywhere, Category = "GI Probes");
	FProbeRelocation ProbeRelocation;

	// If true, probes will keep their same position in world space as the volume moves around. Useful for moving volumes to have more temporally stable probes.
	UPROPERTY(EditAnywhere, Category = "GI Probes");
	bool ScrollProbesInfinitely = false;

	// Toggle probes visualization, Probes visualization modes can be changed from Project Settings
	UPROPERTY(EditAnywhere, Category = "GI Probes");
	bool VisualizeProbes = false;

	UPROPERTY(VisibleDefaultsOnly, AdvancedDisplay, Category = "GI Probes");
	FIntVector ProbeScrollOffset = FIntVector{ 0, 0, 0 };

	// Exponent for depth testing. A high value will rapidly react to depth discontinuities, but risks causing banding.
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "GI Probes");
	float probeDistanceExponent = 50.f;

	// Irradiance blending happens in post-tonemap space
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "GI Probes");
	float probeIrradianceEncodingGamma = 5.f;

	// A threshold ratio used during probe radiance blending that determines if a large lighting change has happened.
	// If the max color component difference is larger than this threshold, the hysteresis will be reduced.
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "GI Probes");
	float probeChangeThreshold = 0.2f;

	// A threshold value used during probe radiance blending that determines the maximum allowed difference in brightness
	// between the previous and current irradiance values. This prevents impulses from drastically changing a
	// texel's irradiance in a single update cycle.
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = "GI Probes");
	float probeBrightnessThreshold = 2.0f;

	// --- "GI Lighting" properties

	// What type of skylight should contribute to GI
	UPROPERTY(EditAnywhere, Category = "GI Lighting");
	EDDGISkyLightType SkyLightTypeOnRayMiss = EDDGISkyLightType::Raster;

	// Bias values for Indirect Lighting
	UPROPERTY(EditAnywhere, Category = "GI Lighting", meta = (ClampMin = "0"));
	float ViewBias = 40.0f;

	// Bias values for Indirect Lighting
	UPROPERTY(EditAnywhere, Category = "GI Lighting", meta = (ClampMin = "0"));
	float NormalBias = 10.0f;

	// If you want to artificially increase the amount of lighting given by this volume, you can modify this lighting multiplier to do so.
	UPROPERTY(EditAnywhere, Category = "GI Lighting", meta = (ClampMin = "0"));
	float LightMultiplier = 1.0f;

	// Use this to artificially modify how much emissive lighting contributes to GI
	UPROPERTY(EditAnywhere, Category = "GI Lighting", meta = (ClampMin = "0"));
	float EmissiveMultiplier = 1.0f;

	// Multiplier to compensate for irradiance clipping that might happen in 10-bit mode (use smaller values for higher irradiance).
	// 32 - bit irradiance textures can be set from project settings to avoid clipping but will have higher memory cost and slower to update.
	UPROPERTY(EditAnywhere, DisplayName = "10-Bit Irradiance Scalar", Category = "GI Lighting", meta = (ClampMin = "0.001", ClampMax = "1"));
	float IrradianceScalar = 1.0f;

	// Objects with overlapping channel flags will receive lighting from this volume
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GI Lighting")
	FLightingChannels LightingChannels;

	// Blueprint Nodes
	UFUNCTION(BlueprintCallable, Category = "DDGI")
	void ClearProbeData();

	UFUNCTION(BlueprintCallable, Category = "DDGI")
	void ToggleVolume(bool IsVolumeEnabled);

	UFUNCTION(BlueprintCallable, Category = "DDGI")
	float GetIrradianceScalar() const;

	UFUNCTION(BlueprintCallable, Category = "DDGI")
	void SetIrradianceScalar(float NewIrradianceScalar);

	UFUNCTION(BlueprintCallable, Category = "DDGI")
	float GetEmissiveMultiplier() const;

	UFUNCTION(BlueprintCallable, Category = "DDGI")
	void SetEmissiveMultiplier(float NewEmissiveMultiplier);

	UFUNCTION(BlueprintCallable, Category = "DDGI")
	float GetLightMultiplier() const;

	UFUNCTION(BlueprintCallable, Category = "DDGI")
	void SetLightMultiplier(float NewLightMultiplier);

	FDDGIVolumeSceneProxy* SceneProxy;

	// When loading a volume we get data for it's textures but don't have a scene proxy yet.
	// This is where that data is stored until the scene proxy is ready to take it.
	FDDGITextureLoadContext LoadContext;
};
