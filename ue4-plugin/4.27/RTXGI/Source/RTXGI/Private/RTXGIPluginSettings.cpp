/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "RTXGIPluginSettings.h"
#include "RTXGIPlugin.h"
#include "DDGIVolumeComponent.h"

#define LOCTEXT_NAMESPACE "RTXGIPlugin"

#if WITH_EDITOR

void URTXGIPluginSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// tell the scene proxies about the bit depth change
	if (PropertyChangedEvent.MemberProperty)
	{
		if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(URTXGIPluginSettings, IrradianceBits) ||
			PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(URTXGIPluginSettings, DistanceBits))
		{
			FDDGIVolumeSceneProxy::OnIrradianceOrDistanceBitsChange();
		}
	}
}

FText URTXGIPluginSettings::GetSectionText() const
{
	return LOCTEXT("SettingsDisplayName", "RTXGI");
}

#endif	// WITH_EDITOR

URTXGIPluginSettings::URTXGIPluginSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName  = TEXT("RTXGI");
}

#undef LOCTEXT_NAMESPACE
