#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"

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

	// Guard against re-queueing assets while we are performing plugin-driven renames.
	void SetApplyingRename(bool bInApplyingRename)
	{
		bApplyingRename = bInApplyingRename;
		if (bApplyingRename)
		{
			// Renames can trigger registry/save callbacks slightly after the rename returns.
			// Keep the guard enabled briefly to avoid re-queuing.
			ApplyingRenameCooldownUntil = FPlatformTime::Seconds() + 1.0;
		}
	}
	bool IsApplyingRename() const { return bApplyingRename; }

	// Called by the review UI when an item is confirmed/ignored so it doesn't reappear.
	void MarkResolved(const FSoftObjectPath& ObjectPath) { QueuedObjectPaths.Remove(ObjectPath); }

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
	bool bApplyingRename = false;
	double ApplyingRenameCooldownUntil = 0.0;
};

#endif // WITH_EDITOR
