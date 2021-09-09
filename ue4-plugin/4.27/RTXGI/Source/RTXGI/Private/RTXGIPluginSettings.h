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

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/DeveloperSettings.h"
#include "RTXGIPluginSettings.generated.h"

UENUM()
enum class EDDGIIrradianceBits : uint8
{
	n10 UMETA(DisplayName = "10 bit"),
	n32 UMETA(DisplayName = "32 bit (for bright lighting and extended luminance range rendering)")
};

UENUM()
enum class EDDGIDistanceBits : uint8
{
	n16 UMETA(DisplayName = "16 bit"),
	n32 UMETA(DisplayName = "32 bit (for larger distances)")
};

UENUM()
enum class EDDGIProbesVisulizationMode : uint8
{
	off UMETA(DisplayName = "Off"),
	irrad UMETA(DisplayName = "Irradiance"),
	distr UMETA(DisplayName = "Squared Hit Distance"),
	distg UMETA(DisplayName = "Hit Distance")
};

/**
 * Configure the RTXGI plug-in.
 */
UCLASS(config=Engine, defaultconfig)
class URTXGIPluginSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URTXGIPluginSettings();

#if WITH_EDITOR
	//~ UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	//~ UDeveloperSettings interface
	virtual FText GetSectionText() const override;
#endif

	/** Light clipping can occur when lighting values are too large due to bright lights or extended radiance.
	* With 10-bits texture format, clipping can be componsated through the irradiance scalar parameter on the DDGI volume.
	* 32-bit texture format shows no clipping but with higher memory cost and slower updates.
	*/
	UPROPERTY(config, EditAnywhere, Category=DDGI)
	EDDGIIrradianceBits IrradianceBits = EDDGIIrradianceBits::n10;

	/** Same story, but for probe distances and squared distances, used to prevent leaks.
	*/
	UPROPERTY(config, EditAnywhere, Category=DDGI)
	EDDGIDistanceBits DistanceBits = EDDGIDistanceBits::n16;

	/** The radius of the spheres that visualize the ddgi probes */
	UPROPERTY(config, EditAnywhere, Category=DDGI)
	float DebugProbeRadius = 5.0f;

	/** The number of rays per frame DDGI is allowed to use to update volumes at max. One volume is updated per frame
	* in a weighted round robin fashion, based on each volume's update priority. It will use this many rays maximum
	* when updating a volume.  A budget value of 0 means there is no budget, and all probes will be updated each time.
	* A default volume has 8x8x8 probes, and uses 288 rays per probe for updates. That means it takes 147,456 rays
	* to update all probes. If you set the budget to 50,000 rays, it would take 3 frames to update all the probes which
	* means the probes will be less responsive to lighting changes, but will take less processing time each frame to update.
	*/
	UPROPERTY(config, EditAnywhere, Category = DDGI, meta = (ClampMin = "0"))
	int ProbeUpdateRayBudget = 0;

	/** Probes visualization mode for all volumes.	*/
	UPROPERTY(config, EditAnywhere, Category = DDGI)
	EDDGIProbesVisulizationMode ProbesVisualization = EDDGIProbesVisulizationMode::irrad;

	/** The depth value is divided by this scale before being shown on the sphere. */
	UPROPERTY(config, EditAnywhere, Category = DDGI)
	float ProbesDepthScale = 1000.0f;

	/** Save probes data to map file. Disabling it will clear existing saved data */
	UPROPERTY(config, EditAnywhere, Category = DDGI)
	bool SerializeProbes = true;
};
