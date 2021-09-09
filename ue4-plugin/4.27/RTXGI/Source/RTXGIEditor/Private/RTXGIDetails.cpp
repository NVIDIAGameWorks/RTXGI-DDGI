/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "RTXGIDetails.h"
#include "DDGIVolume.h"
#include "DDGIVolumeComponent.h"
#include "Widgets/Input/SButton.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "IDetailsView.h"

#define LOCTEXT_NAMESPACE "RTXGIDetails"

TSharedRef<IDetailCustomization> FRTXGIDetails::MakeInstance()
{
	return MakeShareable(new FRTXGIDetails);
}

void FRTXGIDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = DetailLayout.GetSelectedObjects();
	for (int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex)
	{
		const TWeakObjectPtr<UObject>& CurrentObject = SelectedObjects[ObjectIndex];
		if (CurrentObject.IsValid())
		{
			ADDGIVolume* CurrentDDGIVolumeActor = Cast<ADDGIVolume>(CurrentObject.Get());
			if (CurrentDDGIVolumeActor != NULL)
			{
				DDGIVolume = CurrentDDGIVolumeActor;
				break;
			}
		}
	}

	DetailLayout.EditCategory("GI Volume")
		.AddCustomRow(FText::FromString("Clear Probes Row"), true)
		.ValueContent()
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.OnClicked(this, &FRTXGIDetails::OnClearProbes)
			[ SNew(STextBlock).Text(FText::FromString("Clear Probes")) ]
		];
}

void FRTXGIDetails::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder)
{
	CachedDetailBuilder = DetailBuilder;
	CustomizeDetails(*DetailBuilder);
}

FReply FRTXGIDetails::OnClearProbes()
{
	UDDGIVolumeComponent* DDGIComponent = DDGIVolume.IsValid() ? DDGIVolume->DDGIVolumeComponent : nullptr;
	if (DDGIComponent != nullptr)
	{
		DDGIComponent->ClearProbeData();
	}
	return FReply::Handled();
}

void FRTXGIDetails::OnSourceTypeChanged()
{
	IDetailLayoutBuilder* DetailBuilder = CachedDetailBuilder.Pin().Get();
	if (DetailBuilder)
	{
		DetailBuilder->ForceRefreshDetails();
	}
}

#undef LOCTEXT_NAMESPACE
