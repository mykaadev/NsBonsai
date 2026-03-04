#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetData.h"
#include "Containers/Ticker.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/SoftObjectPath.h"

class UPackage;

class NSBONSAI_API FNsBonsaiReviewManager
{
public:
	void Startup();
	void Shutdown();

private:
	void OnAssetAdded(const FAssetData& AssetData);
	void OnAssetRemoved(const FAssetData& AssetData);
	void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);
	void OnPackageSaved(const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext SaveContext);
	bool Tick(float DeltaTime);

	void EnqueuePackageAssets(FName PackageName);
	void RequestPopupDebounced();
	void OpenReviewPopup();

	TMap<FName, TSet<FSoftObjectPath>> PendingAssetsByPackage;
	TArray<FAssetData> ReviewQueue;
	TSet<FSoftObjectPath> QueuedObjectPaths;

	FDelegateHandle AssetAddedHandle;
	FDelegateHandle AssetRemovedHandle;
	FDelegateHandle AssetRenamedHandle;
	FDelegateHandle PackageSavedHandle;
	FTSTicker::FDelegateHandle TickHandle;

	double PopupOpenAtTime = 0.0;
	bool bPopupScheduled = false;
	bool bPopupOpen = false;
};

#endif // WITH_EDITOR
