#include "NsBonsaiNameRules.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "ObjectTools.h"

FString FNsBonsaiNameRules::GetSeparator(const UNsBonsaiSettings& Settings)
{
    return Settings.JoinSeparator.IsEmpty() ? TEXT("_") : Settings.JoinSeparator;
}

FString FNsBonsaiNameRules::CollapseUnderscores(const FString& InValue)
{
    FString Out;
    Out.Reserve(InValue.Len());

    bool bLastWasUnderscore = false;
    for (const TCHAR Character : InValue)
    {
        if (Character == TCHAR('_'))
        {
            if (!bLastWasUnderscore)
            {
                Out.AppendChar(Character);
                bLastWasUnderscore = true;
            }
            continue;
        }

        Out.AppendChar(Character);
        bLastWasUnderscore = false;
    }

    while (Out.StartsWith(TEXT("_")))
    {
        Out.RightChopInline(1);
    }
    while (Out.EndsWith(TEXT("_")))
    {
        Out.LeftChopInline(1);
    }
    return Out;
}

FString FNsBonsaiNameRules::SanitizeAssetName(const FString& InAssetName)
{
    FString Working = InAssetName;
    Working.TrimStartAndEndInline();
    Working.ReplaceInline(TEXT(" "), TEXT("_"));
    Working = ObjectTools::SanitizeObjectName(Working);
    return CollapseUnderscores(Working);
}

FString FNsBonsaiNameRules::NormalizeTokenForCompare(const FString& InToken)
{
    FString Normalized = InToken;
    Normalized.TrimStartAndEndInline();
    Normalized.ToUpperInline();
    return Normalized;
}

bool FNsBonsaiNameRules::IsVariantToken(const FString& Token)
{
    if (Token.IsEmpty())
    {
        return false;
    }

    for (const TCHAR Character : Token)
    {
        if (!FChar::IsUpper(Character))
        {
            return false;
        }
        if (!FChar::IsAlpha(Character))
        {
            return false;
        }
    }

    return true;
}

FString FNsBonsaiNameRules::VariantFromIndex(int32 Index)
{
    FString Out;
    int32 N = Index;
    do
    {
        const int32 Remainder = N % 26;
        Out.InsertAt(0, TCHAR('A' + Remainder));
        N = (N / 26) - 1;
    } while (N >= 0);
    return Out;
}

FString FNsBonsaiNameRules::BuildObjectPathString(const FString& PackagePath, const FString& AssetName)
{
    return FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
}

FName FNsBonsaiNameRules::ResolveTypeToken(const FAssetData& AssetData, const UNsBonsaiSettings& Settings)
{
    return Settings.NormalizeToken(Settings.ResolveTypeTokenForClassPath(AssetData.AssetClassPath));
}

bool FNsBonsaiNameRules::HasComponent(const UNsBonsaiSettings& Settings, ENsBonsaiNameComponent Component)
{
    return Settings.IsComponentEnabled(Component);
}

FString FNsBonsaiNameRules::ResolveSanitizedAssetName(const FNsBonsaiRowState& RowState)
{
    FString Sanitized = SanitizeAssetName(RowState.AssetName);
    if (Sanitized.IsEmpty())
    {
        Sanitized = SanitizeAssetName(RowState.ParsedAssetName);
    }
    if (Sanitized.IsEmpty())
    {
        Sanitized = SanitizeAssetName(RowState.CurrentName);
    }
    return Sanitized.IsEmpty() ? TEXT("New") : Sanitized;
}

FString FNsBonsaiNameRules::SmartPrefillAssetName(const FString& OriginalAssetName, const UNsBonsaiSettings& Settings)
{
    const FString Separator = GetSeparator(Settings);

    TSet<FString> KnownTypes;
    for (const FNsBonsaiTypeRule& Rule : Settings.TypeRules)
    {
        const FName TypeToken = Settings.NormalizeToken(Rule.TypeToken);
        if (!TypeToken.IsNone())
        {
            KnownTypes.Add(NormalizeTokenForCompare(TypeToken.ToString()));
        }
    }

    TSet<FString> KnownDomains;
    TArray<FName> Domains;
    Settings.GetDomainTokens(Domains);
    for (const FName Domain : Domains)
    {
        KnownDomains.Add(NormalizeTokenForCompare(Domain.ToString()));
    }

    TSet<FString> KnownCategories;
    for (const FNsBonsaiDomainDef& DomainDef : Settings.Domains)
    {
        for (const FName Category : DomainDef.Categories)
        {
            const FName NormalizedCategory = Settings.NormalizeToken(Category);
            if (!NormalizedCategory.IsNone())
            {
                KnownCategories.Add(NormalizeTokenForCompare(NormalizedCategory.ToString()));
            }
        }
    }
    for (const FName GlobalCategory : Settings.GlobalCategories)
    {
        const FName NormalizedCategory = Settings.NormalizeToken(GlobalCategory);
        if (!NormalizedCategory.IsNone())
        {
            KnownCategories.Add(NormalizeTokenForCompare(NormalizedCategory.ToString()));
        }
    }

    TArray<FString> Tokens;
    OriginalAssetName.ParseIntoArray(Tokens, *Separator, true);
    if (Tokens.Num() == 0)
    {
        const FString Fallback = SanitizeAssetName(OriginalAssetName);
        return Fallback.IsEmpty() ? TEXT("New") : Fallback;
    }

    int32 StartIndex = 0;
    if (KnownTypes.Contains(NormalizeTokenForCompare(Tokens[0])))
    {
        StartIndex = 1;
    }

    TArray<FString> Remaining;
    for (int32 Index = StartIndex; Index < Tokens.Num(); ++Index)
    {
        const FString Normalized = NormalizeTokenForCompare(Tokens[Index]);
        if (KnownDomains.Contains(Normalized) || KnownCategories.Contains(Normalized))
        {
            continue;
        }
        Remaining.Add(Tokens[Index]);
    }

    if (Settings.IsComponentEnabled(ENsBonsaiNameComponent::Variant) && Remaining.Num() > 0 && IsVariantToken(Remaining.Last()))
    {
        Remaining.Pop();
    }

    const FString Candidate = SanitizeAssetName(FString::Join(Remaining, *Separator));
    if (!Candidate.IsEmpty())
    {
        return Candidate;
    }

    const FString OriginalFallback = SanitizeAssetName(OriginalAssetName);
    return OriginalFallback.IsEmpty() ? TEXT("New") : OriginalFallback;
}

bool FNsBonsaiNameRules::ParseNameWithSettings(const FString& Name, const UNsBonsaiSettings& Settings, FParsedNameParts& OutParsed, FString& OutError)
{
    OutParsed = FParsedNameParts();
    OutError.Reset();

    TArray<ENsBonsaiNameComponent> ActiveOrder;
    Settings.GetActiveNameFormatOrder(ActiveOrder);
    if (ActiveOrder.Num() == 0)
    {
        OutError = TEXT("Name format has no active components.");
        return false;
    }

    const FString Separator = GetSeparator(Settings);
    TArray<FString> Parts;
    Name.ParseIntoArray(Parts, *Separator, true);
    if (Parts.Num() < ActiveOrder.Num())
    {
        OutError = TEXT("Name has fewer parts than required by active format.");
        return false;
    }

    int32 PartIndex = 0;
    for (int32 ComponentIndex = 0; ComponentIndex < ActiveOrder.Num(); ++ComponentIndex)
    {
        const ENsBonsaiNameComponent Component = ActiveOrder[ComponentIndex];
        const int32 RemainingComponentsAfter = ActiveOrder.Num() - ComponentIndex - 1;
        const int32 RemainingParts = Parts.Num() - PartIndex;

        if (Component == ENsBonsaiNameComponent::AssetName)
        {
            const int32 AssetNamePartCount = RemainingParts - RemainingComponentsAfter;
            if (AssetNamePartCount < 1)
            {
                OutError = TEXT("Asset Name token is missing.");
                return false;
            }

            TArray<FString> AssetNameParts;
            for (int32 i = 0; i < AssetNamePartCount; ++i)
            {
                AssetNameParts.Add(Parts[PartIndex + i]);
            }
            OutParsed.AssetName = FString::Join(AssetNameParts, *Separator);
            PartIndex += AssetNamePartCount;
            continue;
        }

        if (RemainingParts <= RemainingComponentsAfter)
        {
            OutError = TEXT("Name parts are missing for active format.");
            return false;
        }

        const FString& Value = Parts[PartIndex++];
        switch (Component)
        {
        case ENsBonsaiNameComponent::Type:
            OutParsed.Type = Value;
            break;
        case ENsBonsaiNameComponent::Domain:
            OutParsed.Domain = Value;
            break;
        case ENsBonsaiNameComponent::Category:
            OutParsed.Category = Value;
            break;
        case ENsBonsaiNameComponent::Variant:
            OutParsed.Variant = Value;
            break;
        default:
            break;
        }
    }

    if (PartIndex != Parts.Num())
    {
        OutError = TEXT("Name has extra parts that do not fit active format.");
        return false;
    }

    return true;
}

bool FNsBonsaiNameRules::BuildNameFromRowState(
    const FAssetData& AssetData,
    const FNsBonsaiRowState& RowState,
    const UNsBonsaiSettings& Settings,
    bool bIncludeVariant,
    const FString& VariantToken,
    FString& OutName,
    FString& OutError)
{
    OutName.Reset();
    OutError.Reset();

    TArray<ENsBonsaiNameComponent> ActiveOrder;
    Settings.GetActiveNameFormatOrder(ActiveOrder);
    if (ActiveOrder.Num() == 0)
    {
        OutError = TEXT("No active name components are configured.");
        return false;
    }

    const FName TypeToken = ResolveTypeToken(AssetData, Settings);
    const FName DomainToken = Settings.NormalizeToken(RowState.SelectedDomain);
    const FName CategoryToken = Settings.NormalizeToken(RowState.SelectedCategory);
    const FString SanitizedAssetName = ResolveSanitizedAssetName(RowState);
    const FString SanitizedVariant = VariantToken.IsEmpty() ? TEXT("A") : VariantToken;

    TArray<FString> Parts;
    Parts.Reserve(ActiveOrder.Num());

    for (const ENsBonsaiNameComponent Component : ActiveOrder)
    {
        switch (Component)
        {
        case ENsBonsaiNameComponent::Type:
            if (TypeToken.IsNone())
            {
                OutError = TEXT("Missing Type token mapping for asset class.");
                return false;
            }
            Parts.Add(TypeToken.ToString());
            break;
        case ENsBonsaiNameComponent::Domain:
            if (DomainToken.IsNone())
            {
                OutError = TEXT("Domain is required by active format.");
                return false;
            }
            Parts.Add(DomainToken.ToString());
            break;
        case ENsBonsaiNameComponent::Category:
            if (CategoryToken.IsNone())
            {
                OutError = TEXT("Category is required by active format.");
                return false;
            }
            Parts.Add(CategoryToken.ToString());
            break;
        case ENsBonsaiNameComponent::AssetName:
            if (SanitizedAssetName.IsEmpty())
            {
                OutError = TEXT("Asset Name is empty after sanitize.");
                return false;
            }
            Parts.Add(SanitizedAssetName);
            break;
        case ENsBonsaiNameComponent::Variant:
            if (bIncludeVariant)
            {
                Parts.Add(SanitizedVariant);
            }
            break;
        default:
            break;
        }
    }

    const FString Separator = GetSeparator(Settings);
    OutName = FString::Join(Parts, *Separator);
    return !OutName.IsEmpty();
}

FNsBonsaiValidationResult FNsBonsaiNameRules::ValidateRowState(const FAssetData& AssetData, const FNsBonsaiRowState& RowState, const UNsBonsaiSettings& Settings)
{
    FNsBonsaiValidationResult Result;
    TArray<FString> Issues;

    const FName TypeToken = ResolveTypeToken(AssetData, Settings);
    if (HasComponent(Settings, ENsBonsaiNameComponent::Type) && TypeToken.IsNone())
    {
        Issues.Add(TEXT("Type token mapping missing for this asset class."));
    }

    if (HasComponent(Settings, ENsBonsaiNameComponent::Domain))
    {
        const FName DomainToken = Settings.NormalizeToken(RowState.SelectedDomain);
        if (DomainToken.IsNone())
        {
            Issues.Add(TEXT("Domain is required."));
        }
        else if (!Settings.IsDomainTokenValid(DomainToken))
        {
            Issues.Add(TEXT("Domain token is not configured."));
        }
    }

    if (HasComponent(Settings, ENsBonsaiNameComponent::Category))
    {
        const FName DomainToken = Settings.NormalizeToken(RowState.SelectedDomain);
        const FName CategoryToken = Settings.NormalizeToken(RowState.SelectedCategory);
        if (CategoryToken.IsNone())
        {
            Issues.Add(TEXT("Category is required."));
        }
        else if (!Settings.IsCategoryTokenValid(DomainToken, CategoryToken))
        {
            Issues.Add(TEXT("Category is not valid for current configuration."));
        }
    }

    if (HasComponent(Settings, ENsBonsaiNameComponent::AssetName))
    {
        if (ResolveSanitizedAssetName(RowState).IsEmpty())
        {
            Issues.Add(TEXT("Asset Name is empty after sanitize."));
        }
    }

    if (Issues.Num() > 0)
    {
        Result.bIsValid = false;
        Result.Message = FString::Join(Issues, TEXT(" "));
        return Result;
    }

    FString Name;
    FString Error;
    if (!BuildNameFromRowState(AssetData, RowState, Settings, HasComponent(Settings, ENsBonsaiNameComponent::Variant), TEXT("A"), Name, Error))
    {
        Result.bIsValid = false;
        Result.Message = Error;
        return Result;
    }

    Result.bIsValid = true;
    Result.Message = TEXT("Ready to rename.");
    return Result;
}

FString FNsBonsaiNameRules::BuildPreviewName(const FAssetData& AssetData, const FNsBonsaiRowState& RowState, const UNsBonsaiSettings& Settings)
{
    FString PreviewName;
    FString Error;
    if (!BuildNameFromRowState(AssetData, RowState, Settings, HasComponent(Settings, ENsBonsaiNameComponent::Variant), TEXT("A"), PreviewName, Error))
    {
        return FString();
    }
    return PreviewName;
}

bool FNsBonsaiNameRules::AllocateFinalNameWithVariant(
    const FAssetData& AssetData,
    const FNsBonsaiRowState& RowState,
    const UNsBonsaiSettings& Settings,
    IAssetRegistry& AssetRegistry,
    TSet<FString>& InOutReservedObjectPaths,
    FString& OutFinalName,
    FSoftObjectPath& OutNewPath,
    FString& OutError)
{
    OutFinalName.Reset();
    OutNewPath.Reset();
    OutError.Reset();

    const bool bUseVariant = HasComponent(Settings, ENsBonsaiNameComponent::Variant);
    const FSoftObjectPath OldPath = AssetData.GetSoftObjectPath();
    const FString PackagePath = AssetData.PackagePath.ToString();

    if (!bUseVariant)
    {
        FString CandidateName;
        if (!BuildNameFromRowState(AssetData, RowState, Settings, false, FString(), CandidateName, OutError))
        {
            return false;
        }

        const FString CandidateObjectPathString = BuildObjectPathString(PackagePath, CandidateName);
        if (InOutReservedObjectPaths.Contains(CandidateObjectPathString))
        {
            OutError = FString::Printf(TEXT("Conflict: '%s' is already reserved in this batch."), *CandidateName);
            return false;
        }

        const FSoftObjectPath CandidatePath(CandidateObjectPathString);
        if (CandidatePath != OldPath)
        {
            const FAssetData Existing = AssetRegistry.GetAssetByObjectPath(CandidatePath, false);
            if (Existing.IsValid())
            {
                OutError = FString::Printf(TEXT("Conflict: '%s' already exists."), *CandidateName);
                return false;
            }
        }

        InOutReservedObjectPaths.Add(CandidateObjectPathString);
        OutFinalName = CandidateName;
        OutNewPath = CandidatePath;
        return true;
    }

    for (int32 VariantIndex = 0; VariantIndex < 26 * 26; ++VariantIndex)
    {
        const FString VariantToken = VariantFromIndex(VariantIndex);
        FString CandidateName;
        FString BuildError;
        if (!BuildNameFromRowState(AssetData, RowState, Settings, true, VariantToken, CandidateName, BuildError))
        {
            OutError = BuildError;
            return false;
        }

        const FString CandidateObjectPathString = BuildObjectPathString(PackagePath, CandidateName);
        if (InOutReservedObjectPaths.Contains(CandidateObjectPathString))
        {
            continue;
        }

        const FSoftObjectPath CandidatePath(CandidateObjectPathString);
        if (CandidatePath == OldPath)
        {
            InOutReservedObjectPaths.Add(CandidateObjectPathString);
            OutFinalName = CandidateName;
            OutNewPath = CandidatePath;
            return true;
        }

        const FAssetData Existing = AssetRegistry.GetAssetByObjectPath(CandidatePath, false);
        if (!Existing.IsValid())
        {
            InOutReservedObjectPaths.Add(CandidateObjectPathString);
            OutFinalName = CandidateName;
            OutNewPath = CandidatePath;
            return true;
        }
    }

    OutError = TEXT("No free variant could be allocated.");
    return false;
}

bool FNsBonsaiNameRules::IsCompliant(const FAssetData& AssetData, const UNsBonsaiSettings& Settings)
{
    FParsedNameParts Parsed;
    FString ParseError;
    if (!ParseNameWithSettings(AssetData.AssetName.ToString(), Settings, Parsed, ParseError))
    {
        return false;
    }

    if (HasComponent(Settings, ENsBonsaiNameComponent::Type))
    {
        const FName ExpectedType = ResolveTypeToken(AssetData, Settings);
        if (ExpectedType.IsNone())
        {
            return false;
        }
        if (Settings.NormalizeToken(FName(*Parsed.Type)) != ExpectedType)
        {
            return false;
        }
    }

    FName DomainToken = NAME_None;
    if (HasComponent(Settings, ENsBonsaiNameComponent::Domain))
    {
        DomainToken = Settings.NormalizeToken(FName(*Parsed.Domain));
        if (!Settings.IsDomainTokenValid(DomainToken))
        {
            return false;
        }
    }

    if (HasComponent(Settings, ENsBonsaiNameComponent::Category))
    {
        const FName CategoryToken = Settings.NormalizeToken(FName(*Parsed.Category));
        if (!Settings.IsCategoryTokenValid(DomainToken, CategoryToken))
        {
            return false;
        }
    }

    if (HasComponent(Settings, ENsBonsaiNameComponent::AssetName))
    {
        if (SanitizeAssetName(Parsed.AssetName).IsEmpty())
        {
            return false;
        }
    }

    if (HasComponent(Settings, ENsBonsaiNameComponent::Variant))
    {
        if (!IsVariantToken(Parsed.Variant))
        {
            return false;
        }
    }

    return true;
}

void FNsBonsaiNameRules::BuildDomainOptions(const UNsBonsaiSettings& Settings, TArray<FName>& OutDomains)
{
    Settings.GetDomainTokens(OutDomains);
}

void FNsBonsaiNameRules::BuildCategoryOptions(const UNsBonsaiSettings& Settings, FName DomainToken, TArray<FName>& OutCategories)
{
    Settings.GetCategoryTokens(DomainToken, OutCategories);
}
