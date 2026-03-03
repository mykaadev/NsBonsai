#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Regex.h"
#include "Settings/NsBonsaiSettings.h"
#include "UObject/TopLevelAssetPath.h"
#include "NsBonsaiCompiledRules.generated.h"

USTRUCT()
struct FNsBonsaiCompiledTreeNode
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	FName Id;

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	FName ParentId;

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	FString Token;

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	ENsBonsaiTreeGroup Group = ENsBonsaiTreeGroup::Domain;

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	int32 Priority = 0;

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	TArray<int32> ChildIndices;

	TArray<FRegexPattern> CompiledMatchers;
};

USTRUCT()
struct FNsBonsaiCompiledDescriptorContextRule
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	TArray<FName> DomainIds;

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	TArray<FName> CategoryIds;

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	TArray<FTopLevelAssetPath> ClassFilters;

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	TArray<FDirectoryPath> PathPrefixes;

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	TArray<FNsBonsaiDescriptorSuggestion> SuggestedDescriptors;

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	bool bAllowFreeText = true;
};

USTRUCT()
struct FNsBonsaiCompiledRules
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	TMap<FTopLevelAssetPath, FString> TypeTokensByClass;

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	TArray<FNsBonsaiCompiledTreeNode> TreeNodes;

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	TMap<FName, int32> TreeIndexById;

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	TArray<FNsBonsaiCompiledDescriptorContextRule> DescriptorContextRules;

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	TMap<FString, FString> NormalizationMap;

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	FNsBonsaiDescriptorOrderingPolicy DescriptorOrdering;

	UPROPERTY(VisibleAnywhere, Category="NsBonsai")
	FString RulesHash;

	void Rebuild(const UNsBonsaiSettings& Settings);
};
