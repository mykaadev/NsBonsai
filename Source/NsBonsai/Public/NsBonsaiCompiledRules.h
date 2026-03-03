#pragma once

#include "CoreMinimal.h"
#include "NsBonsaiSettings.h"

class UClass;
class UNsBonsaiSettings;

struct FNsBonsaiCompiledClassFilter
{
	FSoftClassPath SourceClassPath;
	TObjectPtr<UClass> ResolvedClass = nullptr;

	bool Matches(const UClass* InClass) const;
};

struct FNsBonsaiCompiledTypeRule
{
	FName TypeToken = NAME_None;
	FNsBonsaiCompiledClassFilter ClassFilter;
};

struct FNsBonsaiCompiledTaxonomyRule
{
	FString PathPrefix;
	TArray<FNsBonsaiCompiledClassFilter> ClassFilters;
	TArray<FName> TokenPath;
};

struct FNsBonsaiCompiledDescriptorSuggestion
{
	FName Token = NAME_None;
	int32 Priority = 0;
};

struct FNsBonsaiCompiledDescriptorContextRule
{
	FName Domain = NAME_None;
	FName Category = NAME_None;
	FString PathPrefix;
	TArray<FNsBonsaiCompiledClassFilter> ClassFilters;
	TArray<FNsBonsaiCompiledDescriptorSuggestion> Suggestions;
};

struct FNsBonsaiCompiledOrdering
{
	FString JoinSeparator;
	FString TokenPathSeparator;
	bool bDescriptorPriorityDescending = true;
	bool bDescriptorAlphabeticalAscending = true;
};

struct FCompiledRules
{
	TArray<FNsBonsaiCompiledTypeRule> TypeRules;
	TArray<FNsBonsaiCompiledTaxonomyRule> TaxonomyRules;
	TArray<FNsBonsaiCompiledDescriptorContextRule> DescriptorContextRules;
	TMap<FName, FName> NormalizationMap;
	FNsBonsaiCompiledOrdering Ordering;
	uint32 RulesHash = 0;

	static FCompiledRules Build(const UNsBonsaiSettings& Settings);
};
