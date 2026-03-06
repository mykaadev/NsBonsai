// Copyright (C) 2025 nulled.softworks. All rights reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FNsBonsaiReviewManager;

/** Module entry point that owns the NsBonsai editor review manager. */
class FNsBonsaiModule : public IModuleInterface
{
public:
    /** Starts module services and editor integrations. */
    virtual void StartupModule() override;

    /** Shuts down module services and unregisters editor integrations. */
    virtual void ShutdownModule() override;

private:
    /** Registers Tools menu entries for NsBonsai actions. */
    void RegisterMenus();

    /** Opens the review queue from the Tools menu command. */
    void OpenReviewQueueFromMenu();

    /** Owned manager responsible for asset queueing and review window flow. */
    TUniquePtr<FNsBonsaiReviewManager> ReviewManager;
};
