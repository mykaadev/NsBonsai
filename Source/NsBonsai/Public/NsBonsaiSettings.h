// Copyright (C) 2025 nulled.softworks. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NsBonsaiSettings.generated.h"

/** Defines supported components that can appear in final asset names. */
UENUM(BlueprintType)
enum class ENsBonsaiNameComponent : uint8
{
    Type UMETA(DisplayName = "Type"),
    Domain UMETA(DisplayName = "Domain"),
    Category UMETA(DisplayName = "Category"),
    AssetName UMETA(DisplayName = "Asset Name"),
    Variant UMETA(DisplayName = "Variant")
};

/** Defines how the review workflow is triggered at runtime. */
UENUM(BlueprintType)
enum class ENsBonsaiReviewTriggerMode : uint8
{
    Automatic UMETA(DisplayName = "Automatic"),
    ManualOnly UMETA(DisplayName = "Manual Only"),
    Disabled UMETA(DisplayName = "Disabled")
};

/** Maps an asset class path to a naming type token. */
USTRUCT(BlueprintType)
struct FNsBonsaiTypeRule
{
    GENERATED_BODY()

    /** Asset class mapped to a type token. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Library|Type Rules", meta = (DisplayName = "Class Path", ToolTip = "Asset class mapped to a type token, e.g. /Script/Engine.StaticMesh -> SM"))
    FSoftClassPath ClassPath;

    /** Token used when this class path resolves. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Library|Type Rules", meta = (DisplayName = "Type Token", ToolTip = "Type token used when this class is detected, e.g. SM, BP, MI."))
    FName TypeToken;

    /** Equality operator */
    inline bool operator==(const FNsBonsaiTypeRule& InRHS) const
    {
        return ClassPath == InRHS.ClassPath
            && TypeToken == InRHS.TypeToken;
    }
};

/** Defines a domain token and its allowed categories. */
USTRUCT(BlueprintType)
struct FNsBonsaiDomainDef
{
    GENERATED_BODY()

    /** High-level grouping token for assets. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Library|Domains", meta = (DisplayName = "Domain Token", ToolTip = "High-level grouping token like UI, Foliage, Character."))
    FName DomainToken;

    /** Optional category tokens allowed for this domain. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Library|Domains", meta = (DisplayName = "Categories", ToolTip = "Optional list of valid categories within this domain."))
    TArray<FName> Categories;

    /** Equality operator */
    inline bool operator==(const FNsBonsaiDomainDef& InRHS) const
    {
        return DomainToken == InRHS.DomainToken
             && Categories == InRHS.Categories;
    }
};

/** Maps deprecated token aliases to canonical token values. */
USTRUCT(BlueprintType)
struct FNsBonsaiTokenNormalizationRule
{
    GENERATED_BODY()

    /** Token alias to normalize from. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Normalization", meta = (DisplayName = "Deprecated Token", ToolTip = "Token alias to normalize from."))
    FName DeprecatedToken;

    /** Token alias to normalize to. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Normalization", meta = (DisplayName = "Canonical Token", ToolTip = "Token alias to normalize to."))
    FName CanonicalToken;
};

/** Stores plugin naming, validation, and review-window behavior settings. */
UCLASS(Config = NsBonsai, DefaultConfig, meta = (DisplayName = "Ns Bonsai"))
class NSBONSAI_API UNsBonsaiSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    /** Constructs settings with safe default values. */
    UNsBonsaiSettings();

    /** Active output component order used by the pattern builder. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Format", meta = (DisplayName = "Name Format Order", ToolTip = "Controls the order of parts in the final asset name."))
    TArray<ENsBonsaiNameComponent> NameFormatOrder;

    /** Separator inserted between enabled naming components. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Format", meta = (DisplayName = "Join Separator", ToolTip = "Separator used between name components."))
    FString JoinSeparator;

    /** Enables domain component usage in naming and UI. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Components", meta = (DisplayName = "Use Domains", ToolTip = "Enable or disable Domains in naming and review UI."))
    bool bUseDomains;

    /** Enables category component usage in naming and UI. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Components", meta = (DisplayName = "Use Categories", ToolTip = "Enable or disable Categories in naming and review UI. Categories can be constrained by domain if enabled."))
    bool bUseCategories;

    /** Enables variant suffix allocation for collision safety. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Components", meta = (DisplayName = "Use Variant", ToolTip = "Always append a variant suffix to avoid collisions (A..Z, AA..)."))
    bool bUseVariant;

    /** Enables editable Asset Name component in naming and UI. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Components", meta = (DisplayName = "Use Asset Name Field", ToolTip = "Include a user-editable Asset Name field. If disabled, Asset Name is derived from the original asset name."))
    bool bUseAssetNameField;

    /** Requires categories to belong to selected domain when domains are enabled. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Components", meta = (DisplayName = "Categories Must Belong To Domain", ToolTip = "If enabled, category must belong to the selected domain when domains are enabled."))
    bool bCategoriesMustBelongToDomain;

    /** Allows free category text when no configured categories are available. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Components", meta = (DisplayName = "Allow Free Category If Empty", ToolTip = "Allow free category text when no configured categories are available for the current context."))
    bool bAllowFreeCategoryTextIfNoCategories;

    /** Class-to-type-token library used by type resolution. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Library|Type Rules", meta = (DisplayName = "Type Rules", ToolTip = "ClassPath -> TypeToken rules used to resolve type tokens."))
    TArray<FNsBonsaiTypeRule> TypeRules;

    /** Domain and per-domain category definitions. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Library|Domains", meta = (DisplayName = "Domains", ToolTip = "Domain and category token library."))
    TArray<FNsBonsaiDomainDef> Domains;

    /** Global categories used when domains are disabled. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Library|Categories", meta = (DisplayName = "Global Categories", ToolTip = "Global categories used when domains are disabled."))
    TArray<FName> GlobalCategories;

    /** Skips queueing assets that already match active naming rules. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Behavior", meta = (DisplayName = "Skip Compliant Assets", ToolTip = "Skip enqueueing assets that already comply with the current naming format."))
    bool bSkipCompliantAssets;

    /** Controls automatic tracking/popup behavior for the review workflow. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Behavior", meta = (DisplayName = "Review Trigger Mode", ToolTip = "Automatic: track and auto-popup. Manual Only: track and queue, but open only from Tools menu. Disabled: stop tracking and popups entirely."))
    ENsBonsaiReviewTriggerMode ReviewTriggerMode;

    /** Minimum queued assets required for automatic popup opening. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Behavior", meta = (DisplayName = "Popup Threshold Count", ToolTip = "Minimum queued assets required before showing popup automatically.", ClampMin = "1", UIMin = "1", ClampMax = "1000", UIMax = "1000"))
    int32 PopupThresholdCount;

    /** Cooldown in seconds between automatic popup attempts. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Behavior", meta = (DisplayName = "Popup Cooldown Seconds", ToolTip = "Minimum seconds between automatic popups.", ClampMin = "0.0", UIMin = "0.0", ClampMax = "60.0", UIMax = "60.0"))
    float PopupCooldownSeconds;

    /** Closes the review window automatically when no rows remain. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Behavior", meta = (DisplayName = "Auto Close When Empty", ToolTip = "Automatically close review window when all rows are handled."))
    bool bAutoCloseWindowWhenEmpty;

    /** Alias normalization rules applied to token values. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Normalization", meta = (DisplayName = "Token Normalization Rules", ToolTip = "DeprecatedToken -> CanonicalToken replacement rules."))
    TArray<FNsBonsaiTokenNormalizationRule> TokenNormalizationRules;

    /** Enables exact-token normalization on asset-name parts. */
    UPROPERTY(EditAnywhere, Config, Category = "Naming|Normalization", meta = (DisplayName = "Normalize AssetName Exact Tokens", ToolTip = "Apply token normalization rules to AssetName parts only when exact token matches occur."))
    bool bNormalizeAssetNameExactMatch;

    /** Shows the Type column in the review table. */
    UPROPERTY(EditAnywhere, Config, Category = "UI", meta = (DisplayName = "Show Type Column", ToolTip = "Show Type column in review table."))
    bool bShowTypeColumn;

    /** Shows the Domain column when domain component is enabled. */
    UPROPERTY(EditAnywhere, Config, Category = "UI", meta = (DisplayName = "Show Domain Column", ToolTip = "Show Domain column when domains are enabled."))
    bool bShowDomainColumn;

    /** Shows the Category column when category component is enabled. */
    UPROPERTY(EditAnywhere, Config, Category = "UI", meta = (DisplayName = "Show Category Column", ToolTip = "Show Category column when categories are enabled."))
    bool bShowCategoryColumn;

    /** Shows the editable Asset Name column when enabled. */
    UPROPERTY(EditAnywhere, Config, Category = "UI", meta = (DisplayName = "Show Asset Name Column", ToolTip = "Show Asset Name editable column when asset-name field is enabled."))
    bool bShowAssetNameColumn;

    /** Shows the generated Final Name preview column. */
    UPROPERTY(EditAnywhere, Config, Category = "UI", meta = (DisplayName = "Show Final Name Column", ToolTip = "Show Final Name preview column."))
    bool bShowFinalNameColumn;

    /** Shows the current asset name column. */
    UPROPERTY(EditAnywhere, Config, Category = "UI", meta = (DisplayName = "Show Current Name Column", ToolTip = "Show current asset name column."))
    bool bShowCurrentNameColumn;

    /** Enables compact spacing for table rows. */
    UPROPERTY(EditAnywhere, Config, Category = "UI", meta = (DisplayName = "Compact Rows", ToolTip = "Use compact row spacing in review table."))
    bool bCompactRows;

    /** Resolves type token by walking class hierarchy against configured type rules. */
    FName ResolveTypeTokenForClass(const UClass* InClass) const;

    /** Resolves type token using class path with optional class load fallback. */
    FName ResolveTypeTokenForClassPath(const FTopLevelAssetPath& ClassPath) const;

    /** Applies token normalization rules to a token value. */
    FName NormalizeToken(FName InToken) const;

    /** Returns true when a component is both in the format and enabled by toggles. */
    bool IsComponentEnabled(ENsBonsaiNameComponent Component) const;

    /** Returns active format order filtered by component toggles. */
    void GetActiveNameFormatOrder(TArray<ENsBonsaiNameComponent>& OutOrder) const;

    /** Returns unique normalized domain tokens from settings. */
    void GetDomainTokens(TArray<FName>& OutDomains) const;

    /** Returns valid category tokens for the supplied domain context. */
    void GetCategoryTokens(FName DomainToken, TArray<FName>& OutCategories) const;

    /** Validates whether a domain token exists in current settings. */
    bool IsDomainTokenValid(FName DomainToken) const;

    /** Validates whether a category token is valid in current settings context. */
    bool IsCategoryTokenValid(FName DomainToken, FName CategoryToken) const;

    /** Runs initial config validation after object initialization. */
    virtual void PostInitProperties() override;
    /** Revalidates and persists settings after editor property changes. */
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

    /** Returns the Project Settings category for this settings object. */
    virtual FName GetCategoryName() const override;

private:
    /** Normalizes config values, removes duplicates, and records validation messages. */
    bool NormalizeAndValidateSettings(TArray<FString>& OutValidationMessages);

    /** Collapses repeated underscores and trims edges for token sanitization. */
    FString CollapseUnderscores(const FString& InValue) const;

    /** Sanitizes a token into alnum/underscore form and trims invalid separators. */
    FName SanitizeToken(FName InToken) const;

    /** Emits log and notification entries for setting-validation adjustments. */
    void NotifyValidationMessages(const TArray<FString>& Messages) const;
};
