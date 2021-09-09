/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "RTXGIEditor.h"
#include "DDGIVolume.h"
#include "IPlacementModeModule.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"
#include "RTXGIDetails.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "FRTXGIEditor"

TSharedPtr<FSlateStyleSet> FRTXGIEditor::StyleSet;

void FRTXGIEditor::StartupModule()
{
	if (!StyleSet.IsValid())
	{
		StyleSet = MakeShared<FSlateStyleSet>(FName("RTXGIPlacementStyle"));
		FString IconPath = IPluginManager::Get().FindPlugin(TEXT("RTXGI"))->GetBaseDir() + TEXT("/Resources/Icon40.png");
		StyleSet->Set("RTXGIPlacement.ModesIcon", new FSlateImageBrush(IconPath, FVector2D(40.0f, 40.0f)));

		FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
	}

	IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();
	PlacementModeModule.OnPlacementModeCategoryRefreshed().AddRaw(this, &FRTXGIEditor::OnPlacementModeRefresh);

	//Get the property module
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	//Register the custom details panel we have created
	PropertyModule.RegisterCustomClassLayout(ADDGIVolume::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FRTXGIDetails::MakeInstance));
}

void FRTXGIEditor::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}

	if (IPlacementModeModule::IsAvailable())
	{
		IPlacementModeModule::Get().OnPlacementModeCategoryRefreshed().RemoveAll(this);
	}
}

void FRTXGIEditor::OnPlacementModeRefresh(FName CategoryName)
{
	static FName VolumeName = FName(TEXT("Volumes"));
	if (CategoryName == VolumeName)
	{
		FPlaceableItem* DDGIVolumePlacement = new FPlaceableItem(
			*UActorFactory::StaticClass(),
			FAssetData(ADDGIVolume::StaticClass()),
			FName("RTXGIPlacement.ModesIcon"),
			TOptional<FLinearColor>(),
			TOptional<int32>(),
			FText::FromString("RTXGI DDGI Volume")
		);

		IPlacementModeModule::Get().RegisterPlaceableItem(CategoryName, MakeShareable(DDGIVolumePlacement));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRTXGIEditor, RTXGIEditor)
