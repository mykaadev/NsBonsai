// Copyright (C) 2025 nulled.softworks. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/SoftObjectPath.h"

class UPackage;
class SWindow;
class SNsBonsaiReviewWindow;

/** Manages asset tracking, queueing, and the single NsBonsai review window lifecycle. */
class NSBONSAI_API FNsBonsaiReviewManager
{
public:
    /** Binds editor callbacks and starts queue-processing ticker. */
    void Startup();

    /** Unbinds callbacks and clears runtime queue/window state. */
    void Shutdown();

    /**
     * Sets rename guard state while plugin-driven renames execute.
     * @param bInApplyingRename True while rename callbacks should be ignored.
     */
    void SetApplyingRename(bool bInApplyingRename)
    {
        bApplyingRename = bInApplyingRename;
        if (!bApplyingRename)
        {
            ApplyingRenameCooldownUntil = FPlatformTime::Seconds() + 1.0;
        }
    }

    /** Returns true while rename callback suppression is active. */
    bool IsApplyingRename() const
    {
        return bApplyingRename || (FPlatformTime::Seconds() < ApplyingRenameCooldownUntil);
    }

    /**
     * Marks a queued asset path as resolved.
     * @param ObjectPath Asset object path that was handled.
     * @param bAllowRequeue True to remove from dedupe set and allow queueing again.
     */
    void MarkResolved(const FSoftObjectPath& ObjectPath, bool bAllowRequeue = false);

    /** Opens review queue immediately or focuses existing review window. */
    void OpenReviewQueueNow();

    /** Snoozes automatic popup opening for the specified number of minutes. */
    void SnoozeForMinutes(double Minutes);

private:
    /** Handles asset-added notifications and tracks pending save candidates. */
    void OnAssetAdded(const FAssetData& AssetData);

    /** Handles asset-removed notifications and prunes pending/queued state. */
    void OnAssetRemoved(const FAssetData& AssetData);

    /** Handles asset-renamed notifications and updates pending state. */
    void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);

    /** Handles package-saved notifications and schedules package processing. */
    void OnPackageSaved(const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext SaveContext);

    /** Ticker callback used to process saves and debounce popup behavior. */
    bool Tick(float DeltaTime);

    /** Adds an object path to the pending set keyed by package name. */
    void TrackPending(const FSoftObjectPath& Path, FName PackageName);

    /** Moves saved package pending assets into the review queue. */
    void EnqueueSavedPackageAssets(FName PackageName);

    /** Processes all packages captured by save callbacks. */
    void ProcessSavedPackages();

    /** Requeues unresolved assets returned by a closed review window. */
    void RequeueAssets(const TArray<FAssetData>& Assets);

    /** Requests popup opening after debounce/cooldown policy checks. */
    void RequestPopupDebounced();

    /** Creates the review popup window or appends to an existing one. */
    void OpenReviewPopup();

    /** Appends current queue items into the existing review window widget. */
    void AppendReviewQueueToOpenWindow();

    /** Shows a non-intrusive toast when assets are queued below popup threshold. */
    void ShowQueuedToast();

    /** Returns configured minimum queued asset count to auto-open popup. */
    int32 GetMinAssetsToPopup() const;

    /** Returns configured auto-popup cooldown duration in seconds. */
    double GetPopupCooldownSeconds() const;

    /** Returns true when compliant assets should be skipped during tracking. */
    bool ShouldSkipCompliantAssets() const;

    /** Returns true when an asset already matches active naming rules. */
    bool IsAssetCompliant(const FAssetData& AssetData) const;

    /** Pending object paths grouped by package until package save completes. */
    TMap<FName, TSet<FSoftObjectPath>> PendingByPackage;

    /** Package names waiting to be processed by the ticker after save events. */
    TSet<FName> SavedPackagesToProcess;

    /** Asset data queue waiting to be shown in the review window. */
    TArray<FAssetData> ReviewQueue;

    /** Session-level dedupe set preventing duplicate queue entries. */
    TSet<FSoftObjectPath> QueuedPaths;

    /** Delegate handle for the AssetRegistry OnAssetAdded callback. */
    FDelegateHandle AssetAddedHandle;

    /** Delegate handle for the AssetRegistry OnAssetRemoved callback. */
    FDelegateHandle AssetRemovedHandle;

    /** Delegate handle for the AssetRegistry OnAssetRenamed callback. */
    FDelegateHandle AssetRenamedHandle;

    /** Delegate handle for the package-saved callback. */
    FDelegateHandle PackageSavedHandle;

    /** Delegate handle for the periodic queue-processing ticker. */
    FTSTicker::FDelegateHandle TickHandle;

    /** Scheduled time for next popup attempt. */
    double PopupOpenAtTime = 0.0;

    /** True when a popup open request is currently scheduled. */
    bool bPopupScheduled = false;

    /** True while plugin-triggered rename operations are in progress. */
    bool bApplyingRename = false;

    /** End timestamp for the short post-rename callback guard cooldown. */
    double ApplyingRenameCooldownUntil = 0.0;

    /** Timestamp until which auto-popup opening remains snoozed. */
    double SnoozedUntilTime = 0.0;

    /** Earliest timestamp for the next automatic popup attempt. */
    double NextAutoPopupAllowedTime = 0.0;

    /** Earliest timestamp for the next queued-assets toast. */
    double NextToastAllowedTime = 0.0;

    /** True when auto-popup is temporarily suppressed after manual close. */
    bool bSuppressAutoPopup = false;

    /** Queue size baseline used to detect newly added assets during suppression. */
    int32 SuppressedQueueCount = 0;

    /** Weak pointer to the single active review popup window. */
    TWeakPtr<SWindow> ReviewWindow;

    /** Weak pointer to the active review window widget instance. */
    TWeakPtr<SNsBonsaiReviewWindow> ReviewWindowWidget;
};
