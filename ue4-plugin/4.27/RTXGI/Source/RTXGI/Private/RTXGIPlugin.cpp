/*
* Copyright (c) 2019-2021, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "RTXGIPlugin.h"

#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Tickable.h"

#include "DDGIVolumeUpdate.h"
#include "DDGIVolumeComponent.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "FRTXGIPlugin"

void FRTXGIPlugin::StartupDDGI()
{
	DDGIVolumeUpdate::Startup();
	UDDGIVolumeComponent::Startup();
}

void FRTXGIPlugin::ShutdownDDGI()
{
	DDGIVolumeUpdate::Shutdown();
	UDDGIVolumeComponent::Shutdown();
}

void FRTXGIPlugin::StartupModule()
{
	// Get the base directory of this plugin
	FString BaseDir = IPluginManager::Get().FindPlugin(GetModularFeatureName())->GetBaseDir();

	// Register the shader directory
	FString PluginShaderDir = FPaths::Combine(BaseDir, TEXT("Shaders"));
	FString PluginMapping = TEXT("/Plugin/") + GetModularFeatureName();
	AddShaderSourceDirectoryMapping(PluginMapping, PluginShaderDir);

	StartupDDGI();
}

void FRTXGIPlugin::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	ShutdownDDGI();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRTXGIPlugin, RTXGI)
