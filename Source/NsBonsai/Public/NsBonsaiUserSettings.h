#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "NsBonsaiUserSettings.generated.h"

UCLASS(Config = EditorPerProjectUserSettings)
class NSBONSAI_API UNsBonsaiUserSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config)
	TArray<FName> RecentDomains;

	UPROPERTY(Config)
	TArray<FName> RecentCategories;

	UPROPERTY(Config)
	int32 MaxRecentTokens = 20;

	void TouchDomain(FName DomainToken);
	void TouchCategory(FName CategoryToken);
	void Save();

private:
	template <typename TokenType>
	void TouchRecent(TArray<TokenType>& Target, const TokenType& Value);
};
