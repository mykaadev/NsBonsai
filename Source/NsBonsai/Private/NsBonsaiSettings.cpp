// Copyright (C) 2025 nulled.softworks. All rights reserved.

#include "NsBonsaiSettings.h"

#include "Misc/ConfigCacheIni.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "UObject/UnrealType.h"
#include "Widgets/Notifications/SNotificationList.h"

DEFINE_LOG_CATEGORY_STATIC(LogNsBonsaiSettings, Log, All);

UNsBonsaiSettings::UNsBonsaiSettings()
    : JoinSeparator(TEXT("_"))
    , bUseDomains(true)
    , bUseCategories(true)
    , bUseVariant(true)
    , bUseAssetNameField(true)
    , bCategoriesMustBelongToDomain(true)
    , bAllowFreeCategoryTextIfNoCategories(false)
    , bSkipCompliantAssets(true)
    , ReviewTriggerMode(ENsBonsaiReviewTriggerMode::Automatic)
    , PopupThresholdCount(1)
    , PopupCooldownSeconds(2.0f)
    , bAutoCloseWindowWhenEmpty(true)
    , bNormalizeAssetNameExactMatch(false)
    , bShowTypeColumn(true)
    , bShowDomainColumn(true)
    , bShowCategoryColumn(true)
    , bShowAssetNameColumn(true)
    , bShowFinalNameColumn(true)
    , bShowCurrentNameColumn(true)
    , bCompactRows(true)
{
    NameFormatOrder =
    {
        ENsBonsaiNameComponent::Type,
        ENsBonsaiNameComponent::Domain,
        ENsBonsaiNameComponent::Category,
        ENsBonsaiNameComponent::AssetName,
        ENsBonsaiNameComponent::Variant
    };
}

FString UNsBonsaiSettings::CollapseUnderscores(const FString& InValue) const
{
    FString Out;
    Out.Reserve(InValue.Len());

    bool bLastUnderscore = false;
    for (const TCHAR Character : InValue)
    {
        if (Character == TCHAR('_'))
        {
            if (!bLastUnderscore)
            {
                Out.AppendChar(Character);
                bLastUnderscore = true;
            }
            continue;
        }

        Out.AppendChar(Character);
        bLastUnderscore = false;
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

FName UNsBonsaiSettings::SanitizeToken(FName InToken) const
{
    FString Working = InToken.ToString();
    Working.TrimStartAndEndInline();
    Working.ReplaceInline(TEXT(" "), TEXT("_"));
    Working.ReplaceInline(TEXT("-"), TEXT("_"));

    FString Out;
    Out.Reserve(Working.Len());
    for (const TCHAR Character : Working)
    {
        if (FChar::IsAlnum(Character) || Character == TCHAR('_'))
        {
            Out.AppendChar(Character);
        }
    }

    Out = CollapseUnderscores(Out);
    return Out.IsEmpty() ? NAME_None : FName(*Out);
}

void UNsBonsaiSettings::NotifyValidationMessages(const TArray<FString>& Messages) const
{
    if (Messages.Num() == 0)
    {
        return;
    }

    for (const FString& Message : Messages)
    {
        UE_LOG(LogNsBonsaiSettings, Warning, TEXT("%s"), *Message);
    }

    if (!FSlateApplication::IsInitialized())
    {
        return;
    }

    const FText NotificationText = (Messages.Num() == 1)
        ? FText::FromString(Messages[0])
        : FText::Format(
            NSLOCTEXT("NsBonsaiSettings", "ValidationMany", "{0} (+{1} more settings adjustments)"),
            FText::FromString(Messages[0]),
            FText::AsNumber(Messages.Num() - 1));

    FNotificationInfo Info(NotificationText);
    Info.bFireAndForget = true;
    Info.bUseLargeFont = false;
    Info.bUseSuccessFailIcons = false;
    Info.ExpireDuration = 5.0f;
    FSlateNotificationManager::Get().AddNotification(Info);
}

bool UNsBonsaiSettings::NormalizeAndValidateSettings(TArray<FString>& OutValidationMessages)
{
    bool bChanged = false;
    OutValidationMessages.Reset();

    if (JoinSeparator.IsEmpty())
    {
        JoinSeparator = TEXT("_");
        OutValidationMessages.Add(TEXT("NsBonsai: Join Separator was empty and has been reset to '_'"));
        bChanged = true;
    }

    {
        TSet<ENsBonsaiNameComponent> SeenComponents;
        TArray<ENsBonsaiNameComponent> DedupedOrder;
        for (const ENsBonsaiNameComponent Component : NameFormatOrder)
        {
            if (SeenComponents.Contains(Component))
            {
                bChanged = true;
                continue;
            }
            SeenComponents.Add(Component);
            DedupedOrder.Add(Component);
        }
        if (DedupedOrder.Num() == 0)
        {
            DedupedOrder =
            {
                ENsBonsaiNameComponent::Type,
                ENsBonsaiNameComponent::AssetName,
                ENsBonsaiNameComponent::Variant
            };
            OutValidationMessages.Add(TEXT("NsBonsai: Name Format Order was empty and has been reset to Type, Asset Name, Variant."));
            bChanged = true;
        }
        if (DedupedOrder != NameFormatOrder)
        {
            NameFormatOrder = DedupedOrder;
            OutValidationMessages.Add(TEXT("NsBonsai: Removed duplicate components from Name Format Order."));
            bChanged = true;
        }
    }

    PopupThresholdCount = FMath::Clamp(PopupThresholdCount, 1, 1000);
    PopupCooldownSeconds = FMath::Max(0.0f, PopupCooldownSeconds);

    if (ReviewTriggerMode != ENsBonsaiReviewTriggerMode::Automatic
        && ReviewTriggerMode != ENsBonsaiReviewTriggerMode::ManualOnly
        && ReviewTriggerMode != ENsBonsaiReviewTriggerMode::Disabled)
    {
        ReviewTriggerMode = ENsBonsaiReviewTriggerMode::ManualOnly;
        OutValidationMessages.Add(TEXT("NsBonsai: Invalid Review Trigger Mode detected and reset to Manual Only."));
        bChanged = true;
    }

    {
        TSet<FSoftClassPath> SeenClassPaths;
        TSet<FSoftClassPath> DuplicateClassPaths;
        TArray<FNsBonsaiTypeRule> DedupedRules;
        DedupedRules.Reserve(TypeRules.Num());
        for (FNsBonsaiTypeRule Rule : TypeRules)
        {
            Rule.TypeToken = SanitizeToken(Rule.TypeToken);
            if (!Rule.ClassPath.IsNull() && SeenClassPaths.Contains(Rule.ClassPath))
            {
                DuplicateClassPaths.Add(Rule.ClassPath);
                bChanged = true;
                continue;
            }
            if (!Rule.ClassPath.IsNull())
            {
                SeenClassPaths.Add(Rule.ClassPath);
            }
            DedupedRules.Add(Rule);
        }
        if (DedupedRules != TypeRules)
        {
            TypeRules = DedupedRules;
        }
        for (const FSoftClassPath& DuplicatePath : DuplicateClassPaths)
        {
            OutValidationMessages.Add(FString::Printf(TEXT("NsBonsai: Removed duplicate TypeRules for %s"), *DuplicatePath.ToString()));
        }
    }

    {
        TSet<FName> SeenDomains;
        TArray<FNsBonsaiDomainDef> DedupedDomains;
        for (FNsBonsaiDomainDef DomainDef : Domains)
        {
            const FName SanitizedDomain = NormalizeToken(SanitizeToken(DomainDef.DomainToken));
            if (SanitizedDomain.IsNone())
            {
                bChanged = true;
                continue;
            }
            if (SeenDomains.Contains(SanitizedDomain))
            {
                OutValidationMessages.Add(FString::Printf(TEXT("NsBonsai: Removed duplicate Domain '%s'"), *SanitizedDomain.ToString()));
                bChanged = true;
                continue;
            }
            SeenDomains.Add(SanitizedDomain);
            DomainDef.DomainToken = SanitizedDomain;

            TSet<FName> SeenCategories;
            TArray<FName> DedupedCategories;
            for (const FName Category : DomainDef.Categories)
            {
                const FName SanitizedCategory = NormalizeToken(SanitizeToken(Category));
                if (SanitizedCategory.IsNone() || SeenCategories.Contains(SanitizedCategory))
                {
                    bChanged = true;
                    continue;
                }
                SeenCategories.Add(SanitizedCategory);
                DedupedCategories.Add(SanitizedCategory);
            }
            if (DedupedCategories.Num() != DomainDef.Categories.Num())
            {
                OutValidationMessages.Add(FString::Printf(TEXT("NsBonsai: Deduped categories in Domain '%s'"), *SanitizedDomain.ToString()));
                bChanged = true;
            }
            DomainDef.Categories = DedupedCategories;
            DedupedDomains.Add(DomainDef);
        }

        if (DedupedDomains != Domains)
        {
            Domains = DedupedDomains;
        }
    }

    {
        TSet<FName> SeenGlobalCategories;
        TArray<FName> DedupedGlobalCategories;
        for (const FName Category : GlobalCategories)
        {
            const FName SanitizedCategory = NormalizeToken(SanitizeToken(Category));
            if (SanitizedCategory.IsNone() || SeenGlobalCategories.Contains(SanitizedCategory))
            {
                bChanged = true;
                continue;
            }
            SeenGlobalCategories.Add(SanitizedCategory);
            DedupedGlobalCategories.Add(SanitizedCategory);
        }
        if (DedupedGlobalCategories != GlobalCategories)
        {
            GlobalCategories = DedupedGlobalCategories;
            OutValidationMessages.Add(TEXT("NsBonsai: Deduped/sanitized Global Categories."));
            bChanged = true;
        }
    }

    if (!bUseDomains && bUseCategories && GlobalCategories.Num() == 0 && !bAllowFreeCategoryTextIfNoCategories)
    {
        OutValidationMessages.Add(TEXT("NsBonsai: Categories enabled without Domains and Global Categories are empty. Category validation will block until categories are added or free-category mode is enabled."));
    }

    return bChanged;
}

void UNsBonsaiSettings::PostInitProperties()
{
    Super::PostInitProperties();

    if (!HasAnyFlags(RF_ClassDefaultObject))
    {
        return;
    }

    TArray<FString> ValidationMessages;
    const bool bChanged = NormalizeAndValidateSettings(ValidationMessages);

    if (bChanged)
    {
        SaveConfig();
        if (GConfig)
        {
            GConfig->Flush(false, GetDefaultConfigFilename());
        }
    }
    NotifyValidationMessages(ValidationMessages);
}

void UNsBonsaiSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    TArray<FString> ValidationMessages;
    const bool bChanged = NormalizeAndValidateSettings(ValidationMessages);
    if (bChanged)
    {
        SaveConfig();
        if (GConfig)
        {
            GConfig->Flush(false, GetDefaultConfigFilename());
        }
    }
    NotifyValidationMessages(ValidationMessages);

    Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool UNsBonsaiSettings::IsComponentEnabled(ENsBonsaiNameComponent Component) const
{
    if (!NameFormatOrder.Contains(Component))
    {
        return false;
    }

    switch (Component)
    {
    case ENsBonsaiNameComponent::Domain:
        return bUseDomains;
    case ENsBonsaiNameComponent::Category:
        return bUseCategories;
    case ENsBonsaiNameComponent::Variant:
        return bUseVariant;
    case ENsBonsaiNameComponent::AssetName:
        return bUseAssetNameField;
    default:
        return true;
    }
}

void UNsBonsaiSettings::GetActiveNameFormatOrder(TArray<ENsBonsaiNameComponent>& OutOrder) const
{
    OutOrder.Reset();
    for (const ENsBonsaiNameComponent Component : NameFormatOrder)
    {
        if (IsComponentEnabled(Component))
        {
            OutOrder.Add(Component);
        }
    }
}

void UNsBonsaiSettings::GetDomainTokens(TArray<FName>& OutDomains) const
{
    OutDomains.Reset();
    TSet<FName> Seen;
    for (const FNsBonsaiDomainDef& DomainDef : Domains)
    {
        const FName Domain = NormalizeToken(DomainDef.DomainToken);
        if (!Domain.IsNone() && !Seen.Contains(Domain))
        {
            Seen.Add(Domain);
            OutDomains.Add(Domain);
        }
    }
}

void UNsBonsaiSettings::GetCategoryTokens(FName DomainToken, TArray<FName>& OutCategories) const
{
    OutCategories.Reset();
    if (!bUseCategories)
    {
        return;
    }

    TSet<FName> Seen;
    if (!bUseDomains)
    {
        for (const FName GlobalCategory : GlobalCategories)
        {
            const FName Category = NormalizeToken(GlobalCategory);
            if (!Category.IsNone() && !Seen.Contains(Category))
            {
                Seen.Add(Category);
                OutCategories.Add(Category);
            }
        }
        return;
    }

    const FName NormalizedDomain = NormalizeToken(DomainToken);
    for (const FNsBonsaiDomainDef& DomainDef : Domains)
    {
        if (NormalizeToken(DomainDef.DomainToken) != NormalizedDomain)
        {
            continue;
        }

        for (const FName CategoryValue : DomainDef.Categories)
        {
            const FName Category = NormalizeToken(CategoryValue);
            if (!Category.IsNone() && !Seen.Contains(Category))
            {
                Seen.Add(Category);
                OutCategories.Add(Category);
            }
        }
        break;
    }
}

bool UNsBonsaiSettings::IsDomainTokenValid(FName DomainToken) const
{
    if (!bUseDomains)
    {
        return true;
    }

    const FName NormalizedDomain = NormalizeToken(DomainToken);
    if (NormalizedDomain.IsNone())
    {
        return false;
    }

    for (const FNsBonsaiDomainDef& DomainDef : Domains)
    {
        if (NormalizeToken(DomainDef.DomainToken) == NormalizedDomain)
        {
            return true;
        }
    }
    return false;
}

bool UNsBonsaiSettings::IsCategoryTokenValid(FName DomainToken, FName CategoryToken) const
{
    if (!bUseCategories)
    {
        return true;
    }

    const FName NormalizedCategory = NormalizeToken(CategoryToken);
    if (NormalizedCategory.IsNone())
    {
        return false;
    }

    if (!bUseDomains)
    {
        if (GlobalCategories.Num() == 0)
        {
            return bAllowFreeCategoryTextIfNoCategories;
        }

        for (const FName GlobalCategory : GlobalCategories)
        {
            if (NormalizeToken(GlobalCategory) == NormalizedCategory)
            {
                return true;
            }
        }
        return false;
    }

    TArray<FName> Categories;
    GetCategoryTokens(DomainToken, Categories);
    if (Categories.Num() == 0)
    {
        return bAllowFreeCategoryTextIfNoCategories;
    }

    if (!bCategoriesMustBelongToDomain)
    {
        for (const FNsBonsaiDomainDef& DomainDef : Domains)
        {
            for (const FName CategoryValue : DomainDef.Categories)
            {
                if (NormalizeToken(CategoryValue) == NormalizedCategory)
                {
                    return true;
                }
            }
        }
        return false;
    }

    for (const FName Category : Categories)
    {
        if (Category == NormalizedCategory)
        {
            return true;
        }
    }
    return false;
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
