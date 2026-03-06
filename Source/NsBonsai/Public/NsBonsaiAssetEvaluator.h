// Copyright (C) 2025 nulled.softworks. All rights reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "CoreMinimal.h"
#include "NsBonsaiNameBuilder.h"
#include "NsBonsaiAssetEvaluator.generated.h"

class UNsBonsaiSettings;
class UNsBonsaiUserSettings;

/** Holds resolved and candidate naming data for a single asset review row. */
USTRUCT()
struct FNsBonsaiEvaluationResult
{
    GENERATED_BODY()

    /** Type token resolved from class-to-token rules. */
    UPROPERTY()
    FName TypeToken;

    /** Available domain tokens sorted for the UI. */
    UPROPERTY()
    TArray<FName> DomainCandidates;

    /** Domain selected by default for this asset. */
    UPROPERTY()
    FName PreselectedDomain;

    /** Available category tokens sorted for the UI. */
    UPROPERTY()
    TArray<FName> CategoryCandidates;

    /** Category selected by default for this asset. */
    UPROPERTY()
    FName PreselectedCategory;

    /** Prefill value for the user-editable Asset Name field. */
    UPROPERTY()
    FString ExistingAssetName;

    /** True when domain selection is required by active settings. */
    UPROPERTY()
    bool bRequireDomainConfirmation = true;

    /** True when category selection is required by active settings. */
    UPROPERTY()
    bool bRequireCategoryConfirmation = true;
};

/** Resolves initial per-asset review values from settings and user recents. */
class NSBONSAI_API FNsBonsaiAssetEvaluator
{
public:
    /**
     * Builds review defaults for one asset.
     * @param AssetData Asset to evaluate.
     * @param Settings Global plugin settings used for token libraries and feature toggles.
     * @param UserSettings Per-user settings used for recent token ordering.
     */
    static FNsBonsaiEvaluationResult Evaluate(const FAssetData& AssetData, const UNsBonsaiSettings& Settings, const UNsBonsaiUserSettings& UserSettings);

private:
    /**
     * Sorts tokens by recency first, then alphabetically.
     * @param Tokens In-place token array to sort.
     * @param Recents Ordered recents where lower index means higher priority.
     */
    static void SortNamesRecentsThenAlpha(TArray<FName>& Tokens, const TArray<FName>& Recents);
};