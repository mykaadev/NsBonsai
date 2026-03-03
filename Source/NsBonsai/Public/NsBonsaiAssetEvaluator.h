#pragma once

#include "AssetRegistry/AssetData.h"
#include "CoreMinimal.h"
#include "NsBonsaiNameBuilder.h"
#include "NsBonsaiAssetEvaluator.generated.h"

class UNsBonsaiSettings;
class UNsBonsaiUserSettings;

USTRUCT()
struct FNsBonsaiEvaluationResult
{
	GENERATED_BODY()

	UPROPERTY()
	FName TypeToken;

	UPROPERTY()
	TArray<FName> DomainCandidates;

	UPROPERTY()
	FName PreselectedDomain;

	UPROPERTY()
	TArray<FName> CategoryCandidates;

	UPROPERTY()
	FName PreselectedCategory;

	UPROPERTY()
	TArray<FString> ExistingDescriptors;

	UPROPERTY()
	TArray<FString> RecentDescriptorChips;

	UPROPERTY()
	bool bRequireDomainConfirmation = true;

	UPROPERTY()
	bool bRequireCategoryConfirmation = true;
};

class NSBONSAI_API FNsBonsaiAssetEvaluator
{
public:
	static FNsBonsaiEvaluationResult Evaluate(const FAssetData& AssetData, const UNsBonsaiSettings& Settings, const UNsBonsaiUserSettings& UserSettings);
};
