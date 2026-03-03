#pragma once

#include "CoreMinimal.h"

class UNsBonsaiSettings;

struct FNsBonsaiParsedName
{
	FName TypeToken;
	FName DomainToken;
	FName CategoryToken;
	FString VariantToken;
	TArray<FString> ExistingDescriptors;
};

struct FNsBonsaiNameBuildInput
{
	FName TypeToken;
	FName DomainToken;
	FName CategoryToken;
	TArray<FString> Descriptors;
	FString VariantToken;
};

class NSBONSAI_API FNsBonsaiNameBuilder
{
public:
	static FNsBonsaiParsedName ParseExistingAssetName(const FString& ExistingName, const UNsBonsaiSettings& Settings);
	static TArray<FString> SanitizeDescriptors(const TArray<FString>& InDescriptors, const UNsBonsaiSettings& Settings);
	static FString BuildFinalAssetName(const FNsBonsaiNameBuildInput& Input, const UNsBonsaiSettings& Settings);

	static bool IsVariantToken(const FString& Token);
	static FString SanitizeVariantToken(const FString& VariantToken);
};
