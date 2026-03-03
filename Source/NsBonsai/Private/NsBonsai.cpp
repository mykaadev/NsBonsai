// Copyright Epic Games, Inc. All Rights Reserved.

#include "NsBonsai.h"

#include "NsBonsaiReviewManager.h"

#define LOCTEXT_NAMESPACE "FNsBonsaiModule"

void FNsBonsaiModule::StartupModule()
{
#if WITH_EDITOR
	ReviewManager = MakeUnique<FNsBonsaiReviewManager>();
	ReviewManager->Startup();
#endif
}

void FNsBonsaiModule::ShutdownModule()
{
#if WITH_EDITOR
	if (ReviewManager)
	{
		ReviewManager->Shutdown();
		ReviewManager.Reset();
	}
#endif
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FNsBonsaiModule, NsBonsai)