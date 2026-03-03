#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NsBonsaiSettings.generated.h"

UENUM()
enum class ENsBonsaiTreeGroup : uint8
{
	Domain,
	Category
};

USTRUCT()
struct FNsBonsaiTypeMapEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category="TypeMap")
	FSoftClassPath ClassPath;

	UPROPERTY(EditAnywhere, Config, Category="TypeMap")
	FString TypeToken;
};

USTRUCT()
struct FNsBonsaiTreeNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category="Tree")
	FName Id;

	UPROPERTY(EditAnywhere, Config, Category="Tree")
	FName ParentId;

	UPROPERTY(EditAnywhere, Config, Category="Tree")
	FString Token;

	UPROPERTY(EditAnywhere, Config, Category="Tree")
	ENsBonsaiTreeGroup Group = ENsBonsaiTreeGroup::Domain;

	UPROPERTY(EditAnywhere, Config, Category="Tree")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, Config, Category="Tree")
	TArray<FString> Matchers;
};

USTRUCT()
struct FNsBonsaiDescriptorSuggestion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category="Descriptors")
	FString Token;

	UPROPERTY(EditAnywhere, Config, Category="Descriptors")
	int32 Priority = 0;
};

USTRUCT()
struct FNsBonsaiDescriptorContextRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category="Descriptors|Filters")
	TArray<FName> DomainIds;

	UPROPERTY(EditAnywhere, Config, Category="Descriptors|Filters")
	TArray<FName> CategoryIds;

	UPROPERTY(EditAnywhere, Config, Category="Descriptors|Filters")
	TArray<FSoftClassPath> ClassFilters;

	UPROPERTY(EditAnywhere, Config, Category="Descriptors|Filters")
	TArray<FDirectoryPath> PathPrefixes;

	UPROPERTY(EditAnywhere, Config, Category="Descriptors")
	TArray<FNsBonsaiDescriptorSuggestion> SuggestedDescriptors;

	UPROPERTY(EditAnywhere, Config, Category="Descriptors")
	bool bAllowFreeText = true;
};

USTRUCT()
struct FNsBonsaiNormalizationEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category="Normalization")
	FString DeprecatedToken;

	UPROPERTY(EditAnywhere, Config, Category="Normalization")
	FString CanonicalToken;
};

USTRUCT()
struct FNsBonsaiDescriptorOrderingPolicy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category="Ordering")
	bool bSortDescriptors = true;

	UPROPERTY(EditAnywhere, Config, Category="Ordering")
	FString JoinSeparator = TEXT("_");
};

UCLASS(Config=Editor, DefaultConfig, meta=(DisplayName="NsBonsai"))
class NSBONSAIEDITOR_API UNsBonsaiSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override;

	UPROPERTY(EditAnywhere, Config, Category="TypeMap")
	TArray<FNsBonsaiTypeMapEntry> TypeMap;

	UPROPERTY(EditAnywhere, Config, Category="Tree")
	TArray<FNsBonsaiTreeNode> TreeNodes;

	UPROPERTY(EditAnywhere, Config, Category="Descriptors")
	TArray<FNsBonsaiDescriptorContextRule> DescriptorContextRules;

	UPROPERTY(EditAnywhere, Config, Category="Normalization")
	TArray<FNsBonsaiNormalizationEntry> NormalizationMap;

	UPROPERTY(EditAnywhere, Config, Category="Ordering")
	FNsBonsaiDescriptorOrderingPolicy DescriptorOrdering;
};
