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
#include "GameFramework/Actor.h"

#include "DDGIVolume.generated.h"

class UBillboardComponent;
class UBoxComponent;
class UDDGIVolumeComponent;

UCLASS(HideCategories = (Navigation, Physics, Collision, Rendering, Tags, Cooking, Replication, Input, Actor, HLOD, Mobile, LOD))
class RTXGI_API ADDGIVolume : public AActor
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GI", meta = (AllowPrivateAccess = "true"));
	UDDGIVolumeComponent* DDGIVolumeComponent;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient);
	UBoxComponent* BoxComponent = nullptr;
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override final;
	void PostEditMove(bool bFinished) override final;
#endif
};
