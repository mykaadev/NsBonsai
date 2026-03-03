#include "NsBonsaiSettings.h"

#include "UObject/Class.h"

UNsBonsaiSettings::UNsBonsaiSettings()
	: JoinSeparator(TEXT("_"))
	, bSortDescriptorsAlpha(true)
{
}

FName UNsBonsaiSettings::ResolveTypeTokenForClass(const UClass* InClass) const
{
	if (!InClass)
	{
		return NAME_None;
	}

	TMap<FTopLevelAssetPath, FName> ClassToToken;
	ClassToToken.Reserve(TypeRules.Num());

	for (const FNsBonsaiTypeRule& Rule : TypeRules)
	{
		if (Rule.ClassPath.IsNull() || Rule.TypeToken.IsNone())
		{
			continue;
		}

		ClassToToken.FindOrAdd(Rule.ClassPath.GetAssetPath()) = NormalizeToken(Rule.TypeToken);
	}

	for (const UClass* CurrentClass = InClass; CurrentClass; CurrentClass = CurrentClass->GetSuperClass())
	{
		if (const FName* FoundTypeToken = ClassToToken.Find(CurrentClass->GetClassPathName()))
		{
			return *FoundTypeToken;
		}
	}

	return NAME_None;
}

FName UNsBonsaiSettings::NormalizeToken(FName InToken) const
{
	if (InToken.IsNone())
	{
		return NAME_None;
	}

	for (const FNsBonsaiTokenNormalizationRule& Rule : TokenNormalizationRules)
	{
		if (Rule.DeprecatedToken.IsNone() || Rule.CanonicalToken.IsNone())
		{
			continue;
		}

		if (InToken.IsEqual(Rule.DeprecatedToken, ENameCase::IgnoreCase))
		{
			return Rule.CanonicalToken;
		}
	}

	return InToken;
}

FName UNsBonsaiSettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}
