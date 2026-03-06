#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetData.h"
#include "Containers/Ticker.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/SoftObjectPath.h"

class UPackage;
class SWindow;
class SNsBonsaiReviewWindow;

class NSBONSAI_API FNsBonsaiReviewManager
{
public:
	void Startup();
	void Shutdown();

	// Guard against re-queueing assets while we are performing plugin-driven renames.
	void SetApplyingRename(bool bInApplyingRename)
	{
		bApplyingRename = bInApplyingRename;
		if (!bApplyingRename)
		{
			// Renames can trigger registry/save callbacks slightly after the rename returns.
			// Keep the guard enabled briefly to avoid re-queuing.
			ApplyingRenameCooldownUntil = FPlatformTime::Seconds() + 1.0;
		}
	}
	bool IsApplyingRename() const
	{
		return bApplyingRename || (FPlatformTime::Seconds() < ApplyingRenameCooldownUntil);
	}

	// Called by the review UI when an item is confirmed/ignored.
	// By default we keep the path deduped for the session to avoid noisy re-queues.
	void MarkResolved(const FSoftObjectPath& ObjectPath, bool bAllowRequeue = false);

	void OpenReviewQueueNow();
	void SnoozeForMinutes(double Minutes);

private:
	void OnAssetAdded(const FAssetData& AssetData);
	void OnAssetRemoved(const FAssetData& AssetData);
	void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);
	void OnPackageSaved(const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext SaveContext);
	bool Tick(float DeltaTime);

	void TrackPending(const FSoftObjectPath& Path, FName PackageName);
	void EnqueueSavedPackageAssets(FName PackageName);
	void ProcessSavedPackages();
	void RequeueAssets(const TArray<FAssetData>& Assets);
	void RequestPopupDebounced();
	void OpenReviewPopup();
	void AppendReviewQueueToOpenWindow();
	void ShowQueuedToast();
	int32 GetMinAssetsToPopup() const;
	double GetPopupCooldownSeconds() const;

	TMap<FName, TSet<FSoftObjectPath>> PendingByPackage;
	TSet<FName> SavedPackagesToProcess;
	TArray<FAssetData> ReviewQueue;
	TSet<FSoftObjectPath> QueuedPaths;

	FDelegateHandle AssetAddedHandle;
	FDelegateHandle AssetRemovedHandle;
	FDelegateHandle AssetRenamedHandle;
	FDelegateHandle PackageSavedHandle;
	FTSTicker::FDelegateHandle TickHandle;

	double PopupOpenAtTime = 0.0;
	bool bPopupScheduled = false;
	bool bApplyingRename = false;
	double ApplyingRenameCooldownUntil = 0.0;
	double SnoozedUntilTime = 0.0;
	double NextAutoPopupAllowedTime = 0.0;
	double NextToastAllowedTime = 0.0;

	TWeakPtr<SWindow> ReviewWindow;
	TWeakPtr<SNsBonsaiReviewWindow> ReviewWindowWidget;
};

#endif // WITH_EDITOR
