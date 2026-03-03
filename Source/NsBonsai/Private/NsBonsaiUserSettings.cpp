#include "NsBonsaiUserSettings.h"

#include "Misc/ConfigCacheIni.h"

void UNsBonsaiUserSettings::TouchDomain(FName DomainToken)
{
	if (!DomainToken.IsNone())
	{
		TouchRecent(RecentDomains, DomainToken);
	}
}

void UNsBonsaiUserSettings::TouchCategory(FName CategoryToken)
{
	if (!CategoryToken.IsNone())
	{
		TouchRecent(RecentCategories, CategoryToken);
	}
}

void UNsBonsaiUserSettings::TouchDescriptor(const FString& DescriptorToken)
{
	if (!DescriptorToken.IsEmpty())
	{
		TouchRecent(RecentDescriptors, DescriptorToken);
	}
}

void UNsBonsaiUserSettings::Save()
{
	SaveConfig();
	if (GConfig)
	{
		GConfig->Flush(false, GetDefaultConfigFilename());
	}
}

template <typename TokenType>
void UNsBonsaiUserSettings::TouchRecent(TArray<TokenType>& Target, const TokenType& Value)
{
	Target.Remove(Value);
	Target.Insert(Value, 0);

	if (MaxRecentTokens <= 0)
	{
		Target.Reset();
		return;
	}

	if (Target.Num() > MaxRecentTokens)
	{
		Target.SetNum(MaxRecentTokens, EAllowShrinking::Yes);
	}
}
