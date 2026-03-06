// Copyright (C) 2025 nulled.softworks. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UNsBonsaiSettings;

/** Stores parsed tokens from an existing asset name. */
struct FNsBonsaiParsedName
{
    /** Type token parsed from the current name. */
    FName TypeToken;

    /** Domain token parsed from the current name. */
    FName DomainToken;

    /** Category token parsed from the current name. */
    FName CategoryToken;

    /** Variant token parsed from the current name, if present. */
    FString VariantToken;

    /** Existing asset-name segment between structural tokens and variant. */
    FString ExistingAssetName;
};

/** Input used when building preview and final asset names. */
struct FNsBonsaiNameBuildInput
{
    /** Type token to include in the final name. */
    FName TypeToken;

    /** Domain token to include in the final name. */
    FName DomainToken;

    /** Category token to include in the final name. */
    FName CategoryToken;

    /** User-entered free-form asset name segment. */
    FString AssetName;

    /** Variant token appended at the end of the name. */
    FString VariantToken;
};

/** Parses and builds names using the active NsBonsai naming settings. */
class NSBONSAI_API FNsBonsaiNameBuilder
{
public:
    /**
     * Parses an existing asset name using current settings.
     * @param ExistingName Asset name to parse.
     * @param Settings Active plugin settings.
     */
    static FNsBonsaiParsedName ParseExistingAssetName(const FString& ExistingName, const UNsBonsaiSettings& Settings);

    /**
     * Sanitizes and splits the free-form asset name into valid parts.
     * @param InAssetName Raw asset name text.
     * @param Settings Active plugin settings.
     */
    static TArray<FString> SanitizeAssetNameParts(const FString& InAssetName, const UNsBonsaiSettings& Settings);

    /**
     * Builds the name without a variant token.
     * @param Input Token and asset-name inputs.
     * @param Settings Active plugin settings.
     */
    static FString BuildBaseAssetName(const FNsBonsaiNameBuildInput& Input, const UNsBonsaiSettings& Settings);

    /**
     * Builds the full name including a variant token.
     * @param Input Token and asset-name inputs.
     * @param Settings Active plugin settings.
     */
    static FString BuildFinalAssetName(const FNsBonsaiNameBuildInput& Input, const UNsBonsaiSettings& Settings);

    /**
     * Checks whether a token is a valid alpha-only variant token.
     * @param Token Candidate token to validate.
     */
    static bool IsVariantToken(const FString& Token);

    /**
     * Sanitizes a variant token to uppercase alpha text.
     * @param VariantToken Raw variant token text.
     */
    static FString SanitizeVariantToken(const FString& VariantToken);

private:
    /**
     * Sanitizes one name part while preserving the original letter case.
     * @param InValue Raw name-part value.
     */
    static FString SanitizeNamePartPreserveCase(const FString& InValue);
};
