#include "NsBonsaiAssetEvaluator.h"

#include "NsBonsaiSettings.h"
#include "NsBonsaiUserSettings.h"

void FNsBonsaiAssetEvaluator::SortNamesRecentsThenAlpha(TArray<FName>& Tokens, const TArray<FName>& Recents)
{
    Tokens.Sort([&Recents](const FName& Left, const FName& Right)
    {
        const int32 LeftIndex = Recents.IndexOfByKey(Left);
        const int32 RightIndex = Recents.IndexOfByKey(Right);
        const bool bLeftRecent = LeftIndex != INDEX_NONE;
        const bool bRightRecent = RightIndex != INDEX_NONE;

        if (bLeftRecent && bRightRecent)
        {
            return LeftIndex < RightIndex;
        }

        if (bLeftRecent != bRightRecent)
        {
            return bLeftRecent;
        }

        return Left.ToString().Compare(Right.ToString(), ESearchCase::IgnoreCase) < 0;
    });
}

FNsBonsaiEvaluationResult FNsBonsaiAssetEvaluator::Evaluate(const FAssetData& AssetData, const UNsBonsaiSettings& Settings, const UNsBonsaiUserSettings& UserSettings)
{
    FNsBonsaiEvaluationResult Result;
    Result.TypeToken = Settings.ResolveTypeTokenForClassPath(AssetData.AssetClassPath);

    const FNsBonsaiParsedName ParsedName = FNsBonsaiNameBuilder::ParseExistingAssetName(AssetData.AssetName.ToString(), Settings);
    Result.ExistingAssetName = ParsedName.ExistingAssetName;

    const bool bUseDomainComponent = Settings.IsComponentEnabled(ENsBonsaiNameComponent::Domain);
    const bool bUseCategoryComponent = Settings.IsComponentEnabled(ENsBonsaiNameComponent::Category);

    if (bUseDomainComponent)
    {
        Settings.GetDomainTokens(Result.DomainCandidates);
        SortNamesRecentsThenAlpha(Result.DomainCandidates, UserSettings.RecentDomains);

        if (!ParsedName.DomainToken.IsNone() && Result.DomainCandidates.Contains(ParsedName.DomainToken))
        {
            Result.PreselectedDomain = ParsedName.DomainToken;
        }
        else if (Result.DomainCandidates.Num() > 0)
        {
            Result.PreselectedDomain = Result.DomainCandidates[0];
        }
    }

    if (bUseCategoryComponent)
    {
        Settings.GetCategoryTokens(Result.PreselectedDomain, Result.CategoryCandidates);
        SortNamesRecentsThenAlpha(Result.CategoryCandidates, UserSettings.RecentCategories);

        if (!ParsedName.CategoryToken.IsNone() && Result.CategoryCandidates.Contains(ParsedName.CategoryToken))
        {
            Result.PreselectedCategory = ParsedName.CategoryToken;
        }
        else if (Result.CategoryCandidates.Num() > 0)
        {
            Result.PreselectedCategory = Result.CategoryCandidates[0];
        }
    }

    Result.bRequireDomainConfirmation = bUseDomainComponent;
    Result.bRequireCategoryConfirmation = bUseCategoryComponent;
    return Result;
}
