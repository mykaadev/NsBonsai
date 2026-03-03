#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UObject/SoftObjectPath.h"
#include "NsBonsaiSettings.generated.h"

USTRUCT(Config)
struct FNsBonsaiTypeRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category = "Type")
	FSoftClassPath ClassPath;

	UPROPERTY(EditAnywhere, Config, Category = "Type")
	FName TypeToken = NAME_None;
};

USTRUCT(Config)
struct FNsBonsaiTaxonomyRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category = "Taxonomy")
	FString PathPrefix;

	UPROPERTY(EditAnywhere, Config, Category = "Taxonomy")
	TArray<FSoftClassPath> ClassFilters;

	UPROPERTY(EditAnywhere, Config, Category = "Taxonomy")
	FString TokenPath;
};

USTRUCT(Config)
struct FNsBonsaiDescriptorSuggestion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category = "Descriptors")
	FName Token = NAME_None;

	UPROPERTY(EditAnywhere, Config, Category = "Descriptors")
	int32 Priority = 0;
};

USTRUCT(Config)
struct FNsBonsaiDescriptorContextRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category = "Descriptors")
	FName Domain = NAME_None;

	UPROPERTY(EditAnywhere, Config, Category = "Descriptors")
	FName Category = NAME_None;

	UPROPERTY(EditAnywhere, Config, Category = "Descriptors")
	FString PathPrefix;

	UPROPERTY(EditAnywhere, Config, Category = "Descriptors")
	TArray<FSoftClassPath> ClassFilters;

	UPROPERTY(EditAnywhere, Config, Category = "Descriptors")
	TArray<FNsBonsaiDescriptorSuggestion> Suggestions;
};

USTRUCT(Config)
struct FNsBonsaiTokenNormalizationRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category = "Normalization")
	FName DeprecatedToken = NAME_None;

	UPROPERTY(EditAnywhere, Config, Category = "Normalization")
	FName CanonicalToken = NAME_None;
};

USTRUCT(Config)
struct FNsBonsaiOrderingSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category = "Ordering")
	FString JoinSeparator = TEXT("_");

	UPROPERTY(EditAnywhere, Config, Category = "Ordering")
	FString TokenPathSeparator = TEXT(".");

	UPROPERTY(EditAnywhere, Config, Category = "Ordering")
	bool bDescriptorPriorityDescending = true;

	UPROPERTY(EditAnywhere, Config, Category = "Ordering")
	bool bDescriptorAlphabeticalAscending = true;
};

UCLASS(Config = NsBonsai, DefaultConfig, meta = (DisplayName = "Ns Bonsai"))
class NSBONSAI_API UNsBonsaiSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UNsBonsaiSettings();

	virtual FName GetCategoryName() const override;

	UPROPERTY(EditAnywhere, Config, Category = "Rules|Type")
	TArray<FNsBonsaiTypeRule> TypeRules;

	UPROPERTY(EditAnywhere, Config, Category = "Rules|Taxonomy")
	TArray<FNsBonsaiTaxonomyRule> TaxonomyRules;

	UPROPERTY(EditAnywhere, Config, Category = "Rules|Descriptors")
	TArray<FNsBonsaiDescriptorContextRule> DescriptorContextRules;

	UPROPERTY(EditAnywhere, Config, Category = "Rules|Normalization")
	TArray<FNsBonsaiTokenNormalizationRule> TokenNormalizationRules;

	UPROPERTY(EditAnywhere, Config, Category = "Rules|Ordering")
	FNsBonsaiOrderingSettings Ordering;
};
