#include "NsBonsaiNameBuilder.h"

#include "Algo/Sort.h"
#include "Containers/Map.h"
#include "NsBonsaiSettings.h"

namespace NsBonsaiNameBuilder
{
	FString SanitizeFreeFormToken(const FString& InValue)
	{
		FString Trimmed = InValue;
		Trimmed.TrimStartAndEndInline();

		FString Sanitized;
		Sanitized.Reserve(Trimmed.Len());

		bool bUppercaseNext = true;
		for (TCHAR Character : Trimmed)
		{
			if (FChar::IsAlnum(Character))
			{
				if (FChar::IsDigit(Character))
				{
					Sanitized.AppendChar(Character);
					bUppercaseNext = true;
				}
				else
				{
					Sanitized.AppendChar(bUppercaseNext ? FChar::ToUpper(Character) : FChar::ToLower(Character));
					bUppercaseNext = false;
				}
			}
			else
			{
				bUppercaseNext = true;
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

		Parsed.ExistingDescriptors.Add(Part);
	}

	return Parsed;
}

TArray<FString> FNsBonsaiNameBuilder::SanitizeDescriptors(const TArray<FString>& InDescriptors, const UNsBonsaiSettings& Settings)
{
	TArray<FString> SanitizedDescriptors;
	TSet<FString> SeenDescriptorsLower;

	for (const FString& Descriptor : InDescriptors)
	{
		const FString SanitizedDescriptor = NsBonsaiNameBuilder::SanitizeFreeFormToken(Descriptor);
		if (SanitizedDescriptor.IsEmpty())
		{
			continue;
		}

		FString NormalizedDescriptor = Settings.NormalizeToken(FName(*SanitizedDescriptor)).ToString();
		if (NormalizedDescriptor.IsEmpty())
		{
			continue;
		}

		const FString DescriptorKey = NormalizedDescriptor.ToLower();
		if (!SeenDescriptorsLower.Contains(DescriptorKey))
		{
			SeenDescriptorsLower.Add(DescriptorKey);
			SanitizedDescriptors.Add(NormalizedDescriptor);
		}
	}

	if (Settings.bSortDescriptorsAlpha)
	{
		Algo::Sort(SanitizedDescriptors, [](const FString& Left, const FString& Right)
		{
			return Left.Compare(Right, ESearchCase::IgnoreCase) < 0;
		});
	}

	return SanitizedDescriptors;
}

FString FNsBonsaiNameBuilder::BuildFinalAssetName(const FNsBonsaiNameBuildInput& Input, const UNsBonsaiSettings& Settings)
{
	TArray<FString> NameParts;
	NameParts.Reserve(4 + Input.Descriptors.Num());

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

	NameParts.Append(SanitizeDescriptors(Input.Descriptors, Settings));
	NameParts.Add(SanitizeVariantToken(Input.VariantToken));

	const FString Separator = Settings.JoinSeparator.IsEmpty() ? TEXT("_") : Settings.JoinSeparator;
	return FString::Join(NameParts, *Separator);
}
