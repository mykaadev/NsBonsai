#include "NsBonsaiNameBuilder.h"

#include "Algo/Sort.h"
#include "Containers/Map.h"
#include "NsBonsaiSettings.h"

namespace NsBonsaiNameBuilder
{
	// Keep user-typed/captured asset name fairly intact:
	// - remove non-alnum chars
	// - preserve existing letter casing
	// - trim whitespace
	FString SanitizeNamePartPreserveCase(const FString& InValue)
	{
		FString Trimmed = InValue;
		Trimmed.TrimStartAndEndInline();

		FString Sanitized;
		Sanitized.Reserve(Trimmed.Len());

		for (TCHAR Character : Trimmed)
		{
			if (FChar::IsAlnum(Character))
			{
				Sanitized.AppendChar(Character);
			}
		}

		return Sanitized;
	}
}

bool FNsBonsaiNameBuilder::IsVariantToken(const FString& Token)
{
	if (Token.IsEmpty())
	{
		return false;
	}

	for (TCHAR Character : Token)
	{
		if (!FChar::IsAlpha(Character))
		{
			return false;
		}
	}

	return true;
}

FString FNsBonsaiNameBuilder::SanitizeVariantToken(const FString& VariantToken)
{
	FString TrimmedVariant = VariantToken;
	TrimmedVariant.TrimStartAndEndInline();

	FString Sanitized;
	Sanitized.Reserve(TrimmedVariant.Len());

	for (TCHAR Character : TrimmedVariant)
	{
		if (FChar::IsAlpha(Character))
		{
			Sanitized.AppendChar(FChar::ToUpper(Character));
		}
	}

	return IsVariantToken(Sanitized) ? Sanitized : TEXT("A");
}

FNsBonsaiParsedName FNsBonsaiNameBuilder::ParseExistingAssetName(const FString& ExistingName, const UNsBonsaiSettings& Settings)
{
	FNsBonsaiParsedName Parsed;

	TArray<FString> Parts;
	const FString Separator = Settings.JoinSeparator.IsEmpty() ? TEXT("_") : Settings.JoinSeparator;
	ExistingName.ParseIntoArray(Parts, *Separator, true);

	if (Parts.Num() == 0)
	{
		return Parsed;
	}

	if (IsVariantToken(Parts.Last()))
	{
		Parsed.VariantToken = SanitizeVariantToken(Parts.Pop());
	}

	TSet<FName> KnownTypeTokens;
	for (const FNsBonsaiTypeRule& TypeRule : Settings.TypeRules)
	{
		if (!TypeRule.TypeToken.IsNone())
		{
			KnownTypeTokens.Add(Settings.NormalizeToken(TypeRule.TypeToken));
		}
	}

	TMap<FName, TSet<FName>> CategoriesByDomain;
	TSet<FName> KnownDomains;
	TSet<FName> KnownCategories;
	for (const FNsBonsaiDomainDef& DomainDef : Settings.Domains)
	{
		if (DomainDef.DomainToken.IsNone())
		{
			continue;
		}

		const FName NormalizedDomain = Settings.NormalizeToken(DomainDef.DomainToken);
		KnownDomains.Add(NormalizedDomain);

		TSet<FName>& DomainCategories = CategoriesByDomain.FindOrAdd(NormalizedDomain);
		for (const FName Category : DomainDef.Categories)
		{
			if (Category.IsNone())
			{
				continue;
			}

			const FName NormalizedCategory = Settings.NormalizeToken(Category);
			DomainCategories.Add(NormalizedCategory);
			KnownCategories.Add(NormalizedCategory);
		}
	}

	TArray<FString> AssetNameParts;

	for (const FString& Part : Parts)
	{
		const FName NormalizedPart = Settings.NormalizeToken(FName(*Part));

		if (Parsed.TypeToken.IsNone() && KnownTypeTokens.Contains(NormalizedPart))
		{
			Parsed.TypeToken = NormalizedPart;
			continue;
		}

		if (Parsed.DomainToken.IsNone() && KnownDomains.Contains(NormalizedPart))
		{
			Parsed.DomainToken = NormalizedPart;
			continue;
		}

		if (Parsed.CategoryToken.IsNone())
		{
			if (!Parsed.DomainToken.IsNone())
			{
				if (const TSet<FName>* DomainCategories = CategoriesByDomain.Find(Parsed.DomainToken))
				{
					if (DomainCategories->Contains(NormalizedPart))
					{
						Parsed.CategoryToken = NormalizedPart;
						continue;
					}
				}
			}
			else if (KnownCategories.Contains(NormalizedPart))
			{
				Parsed.CategoryToken = NormalizedPart;
				continue;
			}
		}

		AssetNameParts.Add(Part);
	}

	Parsed.ExistingAssetName = FString::Join(AssetNameParts, *Separator);
	return Parsed;
}

TArray<FString> FNsBonsaiNameBuilder::SanitizeAssetNameParts(const FString& InAssetName, const UNsBonsaiSettings& Settings)
{
	TArray<FString> Parts;
	const FString Separator = Settings.JoinSeparator.IsEmpty() ? TEXT("_") : Settings.JoinSeparator;
	InAssetName.ParseIntoArray(Parts, *Separator, true);

	TArray<FString> Sanitized;
	Sanitized.Reserve(Parts.Num());

	for (const FString& Part : Parts)
	{
		FString Clean = NsBonsaiNameBuilder::SanitizeNamePartPreserveCase(Part);
		if (Clean.IsEmpty())
		{
			continue;
		}

		// Normalize if it matches any alias rule.
		const FName Normalized = Settings.NormalizeToken(FName(*Clean));
		Clean = Normalized.IsNone() ? Clean : Normalized.ToString();

		Sanitized.Add(Clean);
	}

	if (Settings.bSortDescriptorsAlpha)
	{
		Algo::Sort(Sanitized, [](const FString& L, const FString& R)
		{
			return L.Compare(R, ESearchCase::IgnoreCase) < 0;
		});
	}

	return Sanitized;
}

FString FNsBonsaiNameBuilder::BuildBaseAssetName(const FNsBonsaiNameBuildInput& Input, const UNsBonsaiSettings& Settings)
{
	TArray<FString> NameParts;

	const FName TypeToken = Settings.NormalizeToken(Input.TypeToken);
	const FName DomainToken = Settings.NormalizeToken(Input.DomainToken);
	const FName CategoryToken = Settings.NormalizeToken(Input.CategoryToken);

	if (!TypeToken.IsNone())
	{
		NameParts.Add(TypeToken.ToString());
	}
	if (!DomainToken.IsNone())
	{
		NameParts.Add(DomainToken.ToString());
	}
	if (!CategoryToken.IsNone())
	{
		NameParts.Add(CategoryToken.ToString());
	}

	NameParts.Append(SanitizeAssetNameParts(Input.AssetName, Settings));

	const FString Separator = Settings.JoinSeparator.IsEmpty() ? TEXT("_") : Settings.JoinSeparator;
	return FString::Join(NameParts, *Separator);
}

FString FNsBonsaiNameBuilder::BuildFinalAssetName(const FNsBonsaiNameBuildInput& Input, const UNsBonsaiSettings& Settings)
{
	const FString Separator = Settings.JoinSeparator.IsEmpty() ? TEXT("_") : Settings.JoinSeparator;
	const FString Base = BuildBaseAssetName(Input, Settings);
	return Base.IsEmpty()
		? SanitizeVariantToken(Input.VariantToken)
		: (Base + Separator + SanitizeVariantToken(Input.VariantToken));
}
