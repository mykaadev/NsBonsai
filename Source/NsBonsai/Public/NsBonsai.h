// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FNsBonsaiReviewManager;

class FNsBonsaiModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
#if WITH_EDITOR
	void RegisterMenus();
	void OpenReviewQueueFromMenu();
#endif

	TUniquePtr<FNsBonsaiReviewManager> ReviewManager;
};
