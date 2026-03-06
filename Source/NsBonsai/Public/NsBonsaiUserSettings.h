#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NsBonsaiUserSettings.generated.h"

/** Stores per-user review preferences and recent token history. */
UCLASS(Config = EditorPerProjectUserSettings)
class NSBONSAI_API UNsBonsaiUserSettings : public UObject
{
    GENERATED_BODY()

public:
    /** Most recently used domain tokens ordered by recency. */
    UPROPERTY(Config)
    TArray<FName> RecentDomains;

    /** Most recently used category tokens ordered by recency. */
    UPROPERTY(Config)
    TArray<FName> RecentCategories;

    /** Maximum number of recent tokens kept per list. */
    UPROPERTY(Config)
    int32 MaxRecentTokens = 20;

    /** Minimum queue size required before popup opens automatically. */
    UPROPERTY(Config, EditAnywhere, Category = "Review", meta = (ClampMin = "1", UIMin = "1"))
    int32 MinAssetsToPopup = 1;

    /** Cooldown between automatic popup attempts in seconds. */
    UPROPERTY(Config, EditAnywhere, Category = "Review", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float PopupCooldownSeconds = 120.0f;

    /** Enables compliant-asset skipping for this user profile. */
    UPROPERTY(Config, EditAnywhere, Category = "Review")
    bool bSkipCompliantAssets = true;

    /** Records a domain token as recently used. */
    void TouchDomain(FName DomainToken);

    /** Records a category token as recently used. */
    void TouchCategory(FName CategoryToken);

    /** Persists user settings to config and flushes disk cache. */
    void Save();

private:
    /**
     * Updates a recents list by moving a value to front and trimming size.
     * @param Target Recents array to update.
     * @param Value Value to move to the front.
     */
    template <typename TokenType>
    void TouchRecent(TArray<TokenType>& Target, const TokenType& Value);
};
