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
#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleInterface.h"
#include "Styling/SlateStyle.h"

/**
* The public interface of the RTXGIPlacement
*/
class FRTXGIEditor : public IModuleInterface, public IModularFeature
{
public:
	static FString GetModularFeatureName()
	{
		static FString FeatureName = FString(TEXT("RTXGIEditor"));
		return FeatureName;
	}

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	static TSharedPtr<FSlateStyleSet> StyleSet;
	void OnPlacementModeRefresh(FName CategoryName);
};
