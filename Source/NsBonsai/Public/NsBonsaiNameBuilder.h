#pragma once

#include "CoreMinimal.h"

class UNsBonsaiSettings;

struct FNsBonsaiParsedName
{
	FName TypeToken;
	FName DomainToken;
	FName CategoryToken;
	FString VariantToken;

	// Everything between Category and Variant (if present), joined with JoinSeparator.
	FString ExistingAssetName;
};

struct FNsBonsaiNameBuildInput
{
	FName TypeToken;
	FName DomainToken;
	FName CategoryToken;

	// User-provided free-form name for the asset. This is NOT persisted anywhere.
	// It is appended after Type/Domain/Category and before the Variant token.
	FString AssetName;

	FString VariantToken;
};

class NSBONSAI_API FNsBonsaiNameBuilder
{
public:
	static FNsBonsaiParsedName ParseExistingAssetName(const FString& ExistingName, const UNsBonsaiSettings& Settings);

	// Sanitizes user-provided AssetName into one or more name parts (split by JoinSeparator).
	// Keeps order unless Settings.bSortDescriptorsAlpha is enabled.
	static TArray<FString> SanitizeAssetNameParts(const FString& InAssetName, const UNsBonsaiSettings& Settings);

	// Builds name without variant token.
	static FString BuildBaseAssetName(const FNsBonsaiNameBuildInput& Input, const UNsBonsaiSettings& Settings);

	// Builds full name including variant token.
	static FString BuildFinalAssetName(const FNsBonsaiNameBuildInput& Input, const UNsBonsaiSettings& Settings);

	static bool IsVariantToken(const FString& Token);
	static FString SanitizeVariantToken(const FString& VariantToken);
};
