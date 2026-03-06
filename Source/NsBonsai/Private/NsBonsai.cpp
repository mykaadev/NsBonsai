// Copyright (C) 2025 nulled.softworks. All rights reserved.

#include "NsBonsai.h"
#include "NsBonsaiReviewManager.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FNsBonsaiModule"

void FNsBonsaiModule::StartupModule()
{
    ReviewManager = MakeUnique<FNsBonsaiReviewManager>();
    ReviewManager->Startup();

    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FNsBonsaiModule::RegisterMenus));
}

void FNsBonsaiModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);

    if (ReviewManager)
    {
        ReviewManager->Shutdown();
        ReviewManager.Reset();
    }
}

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

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FNsBonsaiModule, NsBonsai)
