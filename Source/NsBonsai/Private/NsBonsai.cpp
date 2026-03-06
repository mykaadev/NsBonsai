// Copyright Epic Games, Inc. All Rights Reserved.

#include "NsBonsai.h"

#include "NsBonsaiReviewManager.h"

#if WITH_EDITOR
#include "ToolMenus.h"
#endif

#define LOCTEXT_NAMESPACE "FNsBonsaiModule"

void FNsBonsaiModule::StartupModule()
{
#if WITH_EDITOR
	ReviewManager = MakeUnique<FNsBonsaiReviewManager>();
	ReviewManager->Startup();

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FNsBonsaiModule::RegisterMenus));
#endif
}

void FNsBonsaiModule::ShutdownModule()
{
#if WITH_EDITOR
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	if (ReviewManager)
	{
		ReviewManager->Shutdown();
		ReviewManager.Reset();
	}
#endif
}

#if WITH_EDITOR
void FNsBonsaiModule::RegisterMenus()
{
	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& Section = ToolsMenu->FindOrAddSection("NsBonsai");
	Section.AddMenuEntry(
		"NsBonsaiReviewQueue",
		LOCTEXT("NsBonsaiReviewQueueLabel", "NsBonsai Review Queue..."),
		LOCTEXT("NsBonsaiReviewQueueTooltip", "Open the NsBonsai rename review queue."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateRaw(this, &FNsBonsaiModule::OpenReviewQueueFromMenu)));
}

void FNsBonsaiModule::OpenReviewQueueFromMenu()
{
	if (ReviewManager)
	{
		ReviewManager->OpenReviewQueueNow();
	}
}
#endif

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FNsBonsaiModule, NsBonsai)
