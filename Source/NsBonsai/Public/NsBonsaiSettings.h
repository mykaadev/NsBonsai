#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NsBonsaiSettings.generated.h"

USTRUCT(BlueprintType)
struct FNsBonsaiTypeRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category = "Type Rules")
	FSoftClassPath ClassPath;

	UPROPERTY(EditAnywhere, Config, Category = "Type Rules")
	FName TypeToken;
};

USTRUCT(BlueprintType)
struct FNsBonsaiDomainDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category = "Token Library")
	FName DomainToken;

	UPROPERTY(EditAnywhere, Config, Category = "Token Library")
	TArray<FName> Categories;
};

USTRUCT(BlueprintType)
struct FNsBonsaiTokenNormalizationRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Config, Category = "Normalization")
	FName DeprecatedToken;

	UPROPERTY(EditAnywhere, Config, Category = "Normalization")
	FName CanonicalToken;
};

UCLASS(Config = NsBonsai, DefaultConfig, meta = (DisplayName = "Ns Bonsai"))
class NSBONSAI_API UNsBonsaiSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UNsBonsaiSettings();

	UPROPERTY(EditAnywhere, Config, Category = "Type Rules")
	TArray<FNsBonsaiTypeRule> TypeRules;

	UPROPERTY(EditAnywhere, Config, Category = "Token Library")
	TArray<FNsBonsaiDomainDef> Domains;

	UPROPERTY(EditAnywhere, Config, Category = "Normalization")
	TArray<FNsBonsaiTokenNormalizationRule> TokenNormalizationRules;

	UPROPERTY(EditAnywhere, Config, Category = "Format", meta = (ClampMin = 1))
	FString JoinSeparator;

	UPROPERTY(EditAnywhere, Config, Category = "Format")
	bool bSortDescriptorsAlpha;

	FName ResolveTypeTokenForClass(const UClass* InClass) const;
	FName NormalizeToken(FName InToken) const;

	virtual FName GetCategoryName() const override;
};
