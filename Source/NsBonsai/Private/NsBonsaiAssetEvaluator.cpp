#include "NsBonsaiAssetEvaluator.h"

#include "Algo/Sort.h"
#include "Containers/Set.h"
#include "NsBonsaiSettings.h"
#include "NsBonsaiUserSettings.h"

namespace NsBonsaiAssetEvaluator
{
	TMap<FName, TArray<FName>> BuildCategoriesByDomain(const UNsBonsaiSettings& Settings)
	{
		TMap<FName, TArray<FName>> Result;

		for (const FNsBonsaiDomainDef& DomainDef : Settings.Domains)
		{
			const FName Domain = Settings.NormalizeToken(DomainDef.DomainToken);
			if (Domain.IsNone())
			{
				continue;
			}

			TArray<FName>& Categories = Result.FindOrAdd(Domain);
			TSet<FName> Seen;
			for (const FName Category : DomainDef.Categories)
			{
				const FName NormalizedCategory = Settings.NormalizeToken(Category);
				if (!NormalizedCategory.IsNone() && !Seen.Contains(NormalizedCategory))
				{
					Seen.Add(NormalizedCategory);
					Categories.Add(NormalizedCategory);
				}
			}
		}

		return Result;
	}

	void SortNamesRecentsThenAlpha(TArray<FName>& Tokens, const TArray<FName>& Recents)
	{
		Tokens.Sort([&Recents](const FName& Left, const FName& Right)
		{
			const int32 LeftIdx = Recents.IndexOfByKey(Left);
			const int32 RightIdx = Recents.IndexOfByKey(Right);
			const bool bLeftRecent = LeftIdx != INDEX_NONE;
			const bool bRightRecent = RightIdx != INDEX_NONE;

			if (bLeftRecent && bRightRecent)
			{
				return LeftIdx < RightIdx;
			}

			if (bLeftRecent != bRightRecent)
			{
				return bLeftRecent;
			}

			return Left.ToString().Compare(Right.ToString(), ESearchCase::IgnoreCase) < 0;
		});
	}

	void SortStringsRecentsThenAlpha(TArray<FString>& Tokens, const TArray<FString>& Recents)
	{
		Tokens.Sort([&Recents](const FString& Left, const FString& Right)
		{
			const int32 LeftIdx = Recents.IndexOfByPredicate([&Left](const FString& Value)
			{
				return Value.Equals(Left, ESearchCase::IgnoreCase);
			});

			const int32 RightIdx = Recents.IndexOfByPredicate([&Right](const FString& Value)
			{
				return Value.Equals(Right, ESearchCase::IgnoreCase);
			});

			const bool bLeftRecent = LeftIdx != INDEX_NONE;
			const bool bRightRecent = RightIdx != INDEX_NONE;

			if (bLeftRecent && bRightRecent)
			{
				return LeftIdx < RightIdx;
			}

			if (bLeftRecent != bRightRecent)
			{
				return bLeftRecent;
			}

			return Left.Compare(Right, ESearchCase::IgnoreCase) < 0;
		});
	}
}

FNsBonsaiEvaluationResult FNsBonsaiAssetEvaluator::Evaluate(const FAssetData& AssetData, const UNsBonsaiSettings& Settings, const UNsBonsaiUserSettings& UserSettings)
{
	FNsBonsaiEvaluationResult Result;

	Result.TypeToken = Settings.ResolveTypeTokenForClassPath(AssetData.AssetClassPath);

	const FNsBonsaiParsedName ParsedName = FNsBonsaiNameBuilder::ParseExistingAssetName(AssetData.AssetName.ToString(), Settings);
	Result.ExistingDescriptors = FNsBonsaiNameBuilder::SanitizeDescriptors(ParsedName.ExistingDescriptors, Settings);

	TArray<FName> AllDomains;
	TSet<FName> SeenDomains;
	for (const FNsBonsaiDomainDef& DomainDef : Settings.Domains)
	{
		const FName Domain = Settings.NormalizeToken(DomainDef.DomainToken);
		if (!Domain.IsNone() && !SeenDomains.Contains(Domain))
		{
			SeenDomains.Add(Domain);
			AllDomains.Add(Domain);
		}
	}

	Result.DomainCandidates = AllDomains;
	NsBonsaiAssetEvaluator::SortNamesRecentsThenAlpha(Result.DomainCandidates, UserSettings.RecentDomains);

	if (!ParsedName.DomainToken.IsNone() && Result.DomainCandidates.Contains(ParsedName.DomainToken))
	{
		Result.PreselectedDomain = ParsedName.DomainToken;
	}
	else if (Result.DomainCandidates.Num() > 0)
	{
		Result.PreselectedDomain = Result.DomainCandidates[0];
	}

	const TMap<FName, TArray<FName>> CategoriesByDomain = NsBonsaiAssetEvaluator::BuildCategoriesByDomain(Settings);
	if (!Result.PreselectedDomain.IsNone())
	{
		if (const TArray<FName>* Categories = CategoriesByDomain.Find(Result.PreselectedDomain))
		{
			Result.CategoryCandidates = *Categories;
			NsBonsaiAssetEvaluator::SortNamesRecentsThenAlpha(Result.CategoryCandidates, UserSettings.RecentCategories);
		}
	}

	if (!ParsedName.CategoryToken.IsNone() && Result.CategoryCandidates.Contains(ParsedName.CategoryToken))
	{
		Result.PreselectedCategory = ParsedName.CategoryToken;
	}
	else if (Result.CategoryCandidates.Num() > 0)
	{
		Result.PreselectedCategory = Result.CategoryCandidates[0];
	}

	TArray<FString> Recents = FNsBonsaiNameBuilder::SanitizeDescriptors(UserSettings.RecentDescriptors, Settings);
	TSet<FString> ExistingKeys;
	for (const FString& Existing : Result.ExistingDescriptors)
	{
		ExistingKeys.Add(Existing.ToLower());
	}

	for (const FString& Recent : Recents)
	{
		if (!ExistingKeys.Contains(Recent.ToLower()))
		{
			Result.RecentDescriptorChips.Add(Recent);
		}
	}

	NsBonsaiAssetEvaluator::SortStringsRecentsThenAlpha(Result.RecentDescriptorChips, Recents);
	Result.bRequireDomainConfirmation = true;
	Result.bRequireCategoryConfirmation = true;

	return Result;
}
