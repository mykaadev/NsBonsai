#include "NsBonsaiSettings.h"

#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

UNsBonsaiSettings::UNsBonsaiSettings()
	: JoinSeparator(TEXT("_"))
	, bSortDescriptorsAlpha(false)
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

FName UNsBonsaiSettings::ResolveTypeTokenForClassPath(const FTopLevelAssetPath& ClassPath) const
{
	if (!ClassPath.IsValid())
	{
		return NAME_None;
	}

	if (const UClass* AssetClass = FindObject<UClass>(nullptr, *ClassPath.ToString()))
	{
		return ResolveTypeTokenForClass(AssetClass);
	}

	if (UClass* LoadedClass = LoadObject<UClass>(nullptr, *ClassPath.ToString()))
	{
		return ResolveTypeTokenForClass(LoadedClass);
	}

	for (const FNsBonsaiTypeRule& Rule : TypeRules)
	{
		if (Rule.ClassPath.GetAssetPath() == ClassPath)
		{
			return NormalizeToken(Rule.TypeToken);
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
