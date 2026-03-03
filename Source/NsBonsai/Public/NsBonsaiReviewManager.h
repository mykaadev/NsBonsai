#pragma once

#include "AssetRegistry/AssetData.h"
#include "CoreMinimal.h"
	void SchedulePopup();
	void OpenPopupIfReady();
	TSet<FSoftObjectPath> QueuedAssetPaths;
class FNsBonsaiReviewManager
{
public:
	void Startup();
	void Shutdown();

private:
	void OnAssetAdded(const FAssetData& AssetData);
	void OnAssetRemoved(const FAssetData& AssetData);
	void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);
	void OnPackageSaved(const FString& PackageFileName, UPackage* Package, const FObjectPostSaveContext& ObjectSaveContext);
	bool Tick(float DeltaTime);

	void EnqueuePackageAssets(FName PackageName);
	void RequestPopupDebounced();
	void OpenReviewPopup();

	TMap<FName, TSet<FName>> PendingAssetsByPackage;
	TArray<FAssetData> ReviewQueue;
	TSet<FName> QueuedObjectPaths;

	FDelegateHandle AssetAddedHandle;
	FDelegateHandle AssetRemovedHandle;
	FDelegateHandle AssetRenamedHandle;
	FDelegateHandle PackageSavedHandle;
	FDelegateHandle TickHandle;

	double PopupOpenAtTime = 0.0;
	bool bPopupScheduled = false;
	bool bPopupOpen = false;
};
#endif
