// Copyright (C) 2025 nulled.softworks. All rights reserved.

#include "NsBonsai.h"
#include "NsBonsaiReviewManager.h"
#include "ContentBrowserMenuContexts.h"
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

    UToolMenu* AssetContextMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu");
    FToolMenuSection& AssetSection = AssetContextMenu->FindOrAddSection("NsBonsai");
    AssetSection.AddDynamicEntry(
        "NsBonsaiOpenWithBonsai",
        FNewToolMenuSectionDelegate::CreateRaw(this, &FNsBonsaiModule::AddAssetContextMenuEntry));
}

void FNsBonsaiModule::OpenReviewQueueFromMenu()
{
    if (ReviewManager)
    {
        ReviewManager->OpenReviewQueueNow();
    }
}

void FNsBonsaiModule::AddAssetContextMenuEntry(FToolMenuSection& Section)
{
    const UContentBrowserAssetContextMenuContext* Context = Section.FindContext<UContentBrowserAssetContextMenuContext>();
    if (!Context || Context->SelectedAssets.Num() == 0)
    {
        return;
    }

    const TArray<FAssetData> SelectedAssets = Context->SelectedAssets;
    Section.AddMenuEntry(
        "NsBonsaiOpenWithBonsaiEntry",
        LOCTEXT("NsBonsaiOpenWithBonsaiLabel", "Open with Bonsai"),
        LOCTEXT("NsBonsaiOpenWithBonsaiTooltip", "Send selected assets to the NsBonsai review window."),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateLambda([this, SelectedAssets]()
        {
            if (ReviewManager)
            {
                ReviewManager->OpenReviewForAssets(SelectedAssets);
            }
        })));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FNsBonsaiModule, NsBonsai)
