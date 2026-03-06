// Copyright (C) 2025 nulled.softworks. All rights reserved.

#pragma once

#include "AssetRegistry/AssetData.h"
#include "CoreMinimal.h"
#include "NsBonsaiSettings.h"
#include "UObject/SoftObjectPath.h"

class IAssetRegistry;
class UNsBonsaiSettings;

/** Captures editable row inputs used by name rules and validation. */
struct FNsBonsaiRowState
{
    /** Selected domain token for this row. */
    FName SelectedDomain;

    /** Selected category token for this row. */
    FName SelectedCategory;

    /** User-entered asset-name text. */
    FString AssetName;

    /** Parsed asset-name fallback from existing name. */
    FString ParsedAssetName;

    /** Current source asset name fallback. */
    FString CurrentName;
};

/** Describes the validation state of a row. */
struct FNsBonsaiValidationResult
{
    /** True when the row is valid for rename. */
    bool bIsValid = true;

    /** Human-readable reason for valid or invalid state. */
    FString Message;
};

/** Implements parse/build/compliance logic driven by NsBonsai settings. */
class NSBONSAI_API FNsBonsaiNameRules
{
public:
    /** Sanitizes asset-name text into a valid UObject name segment. */
    static FString SanitizeAssetName(const FString& InAssetName);

    /** Normalizes token text for case-insensitive comparisons. */
    static FString NormalizeTokenForCompare(const FString& InToken);

    /** Checks whether a token is a valid uppercase alpha variant token. */
    static bool IsVariantToken(const FString& Token);

    /** Converts a zero-based index to alphabetical variant text (A..Z, AA..). */
    static FString VariantFromIndex(int32 Index);

    /** Builds a fully qualified object path string from package path and name. */
    static FString BuildObjectPathString(const FString& PackagePath, const FString& AssetName);

    /** Resolves the configured type token for the asset class. */
    static FName ResolveTypeToken(const FAssetData& AssetData, const UNsBonsaiSettings& Settings);

    /** Returns true when a name component is enabled in active settings. */
    static bool HasComponent(const UNsBonsaiSettings& Settings, ENsBonsaiNameComponent Component);

    /** Builds a smart asset-name prefill by stripping known structural tokens. */
    static FString SmartPrefillAssetName(const FString& OriginalAssetName, const UNsBonsaiSettings& Settings);

    /** Validates one editable row against active settings and token libraries. */
    static FNsBonsaiValidationResult ValidateRowState(const FAssetData& AssetData, const FNsBonsaiRowState& RowState, const UNsBonsaiSettings& Settings);

    /** Builds preview name text for the current row state. */
    static FString BuildPreviewName(const FAssetData& AssetData, const FNsBonsaiRowState& RowState, const UNsBonsaiSettings& Settings);

    /**
     * Allocates a unique final name (and path) including variant logic when enabled.
     * @param AssetData Source asset data.
     * @param RowState Editable row state.
     * @param Settings Active settings.
     * @param AssetRegistry Asset registry used for collision checks.
     * @param InOutReservedObjectPaths Intra-batch reserved object paths.
     * @param OutFinalName Allocated final asset name.
     * @param OutNewPath Allocated destination soft object path.
     * @param OutError Error details when allocation fails.
     */
    static bool AllocateFinalNameWithVariant(
        const FAssetData& AssetData,
        const FNsBonsaiRowState& RowState,
        const UNsBonsaiSettings& Settings,
        IAssetRegistry& AssetRegistry,
        TSet<FString>& InOutReservedObjectPaths,
        FString& OutFinalName,
        FSoftObjectPath& OutNewPath,
        FString& OutError);

    /** Returns true when an asset already complies with current naming settings. */
    static bool IsCompliant(const FAssetData& AssetData, const UNsBonsaiSettings& Settings);

    /** Fills domain options using active settings. */
    static void BuildDomainOptions(const UNsBonsaiSettings& Settings, TArray<FName>& OutDomains);

    /** Fills category options for the given domain using active settings. */
    static void BuildCategoryOptions(const UNsBonsaiSettings& Settings, FName DomainToken, TArray<FName>& OutCategories);

private:
    /** Internal parse result used by compliance checks. */
    struct FParsedNameParts
    {
        /** Parsed type token string. */
        FString Type;

        /** Parsed domain token string. */
        FString Domain;

        /** Parsed category token string. */
        FString Category;

        /** Parsed asset-name string. */
        FString AssetName;

        /** Parsed variant token string. */
        FString Variant;
    };

    /** Returns the configured join separator or the default underscore separator. */
    static FString GetSeparator(const UNsBonsaiSettings& Settings);

    /** Collapses repeated underscores and trims leading/trailing separators. */
    static FString CollapseUnderscores(const FString& InValue);

    /** Resolves sanitized asset-name text with row fallbacks. */
    static FString ResolveSanitizedAssetName(const FNsBonsaiRowState& RowState);

    /** Parses a full name according to active settings components and order. */
    static bool ParseNameWithSettings(const FString& Name, const UNsBonsaiSettings& Settings, FParsedNameParts& OutParsed, FString& OutError);

    /** Builds a candidate name from row state with optional explicit variant. */
    static bool BuildNameFromRowState(
        const FAssetData& AssetData,
        const FNsBonsaiRowState& RowState,
        const UNsBonsaiSettings& Settings,
        bool bIncludeVariant,
        const FString& VariantToken,
        FString& OutName,
        FString& OutError);
};
