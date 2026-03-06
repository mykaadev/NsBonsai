#include "NsBonsaiReviewManager.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IAssetTools.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "NsBonsaiAssetEvaluator.h"
#include "NsBonsaiNameRules.h"
#include "NsBonsaiSettings.h"
#include "NsBonsaiUserSettings.h"
#include "ObjectTools.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Notifications/SNotificationList.h"

class SNsBonsaiReviewWindow final : public SCompoundWidget
{
private:
    struct FRowModel
    {
        FAssetData AssetData;
        FSoftObjectPath ObjectPath;
        FString CurrentName;
        FString ParsedBaseName;
        FString SmartPrefillName;

        FName SelectedDomain;
        FName SelectedCategory;
        FString AssetName;
        bool bIgnored = false;
        bool bRenameBlockedConflict = false;
        FString RenameBlockedReason;
        FString PreviewName;
    };
public:
    SLATE_BEGIN_ARGS(SNsBonsaiReviewWindow) {}
        SLATE_ARGUMENT(TArray<FAssetData>, Assets)
        SLATE_ARGUMENT(FNsBonsaiReviewManager*, Manager)
        SLATE_ARGUMENT(TWeakPtr<SWindow>, ParentWindow)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        ParentWindow = InArgs._ParentWindow;
        Manager = InArgs._Manager;
        Settings = GetDefault<UNsBonsaiSettings>();
        UserSettings = GetMutableDefault<UNsBonsaiUserSettings>();
        RebuildDomainOptions();

        ChildSlot
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(8)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Review newly added assets. Configure enabled fields, then rename (check) or ignore (x).")))
            ]
            + SVerticalBox::Slot().FillHeight(1.0f).Padding(8)
            [
                BuildCompactTable()
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(8)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]
                    {
                        return FText::FromString(FString::Printf(TEXT("Remaining: %d  |  Confirmed: %d  |  Ignored: %d"), GetRemainingCount(), ConfirmedCount, IgnoredCount));
                    })
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)
                [
                    SNew(SCheckBox)
                    .IsChecked_Lambda([this]()
                    {
                        return bDryRunEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                    })
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
                    {
                        bDryRunEnabled = (NewState == ECheckBoxState::Checked);
                        RequestRefresh();
                    })
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Dry Run")))
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)
                [
                    SNew(SButton)
                    .Visibility_Lambda([this]
                    {
                        return bDryRunEnabled ? EVisibility::Visible : EVisibility::Collapsed;
                    })
                    .IsEnabled_Lambda([this]
                    {
                        return DeferredRenames.Num() > 0;
                    })
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .ContentPadding(FMargin(8, 2))
                    .OnClicked(this, &SNsBonsaiReviewWindow::OnExecuteRenamesClicked)
                    .Text_Lambda([this]
                    {
                        return FText::FromString(FString::Printf(TEXT("Execute Renames (%d)"), DeferredRenames.Num()));
                    })
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0)
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .ContentPadding(FMargin(8, 2))
                    .ToolTipText(FText::FromString(TEXT("Snooze review popups for 10 minutes.")))
                    .Text(FText::FromString(TEXT("Snooze 10m")))
                    .OnClicked(this, &SNsBonsaiReviewWindow::OnSnoozeClicked)
                ]
            ]
        ];

        AddAssets(InArgs._Assets);
    }

    void AddAssets(const TArray<FAssetData>& InAssets)
    {
        bool bAddedAny = false;
        for (const FAssetData& Asset : InAssets)
        {
            const FSoftObjectPath ObjectPath = Asset.GetSoftObjectPath();
            if (!ObjectPath.IsValid() || RowPaths.Contains(ObjectPath))
            {
                continue;
            }

            const FNsBonsaiEvaluationResult Eval = FNsBonsaiAssetEvaluator::Evaluate(Asset, *Settings, *UserSettings);
            TSharedPtr<FRowModel> Row = MakeShared<FRowModel>();
            Row->AssetData = Asset;
            Row->ObjectPath = ObjectPath;
            Row->CurrentName = Asset.AssetName.ToString();
            Row->ParsedBaseName = Eval.ExistingAssetName;
            Row->SmartPrefillName = BuildSmartPrefillAssetName(Row->CurrentName);

            if (IsDomainColumnEnabled())
            {
                Row->SelectedDomain = Eval.PreselectedDomain;
            }
            else
            {
                Row->SelectedDomain = NAME_None;
            }

            if (IsCategoryColumnEnabled())
            {
                if (IsDomainColumnEnabled())
                {
                    Row->SelectedCategory = Eval.PreselectedCategory;
                }
                else
                {
                    TArray<FName> GlobalCategories;
                    Settings->GetCategoryTokens(NAME_None, GlobalCategories);
                    Row->SelectedCategory = GlobalCategories.Num() > 0 ? GlobalCategories[0] : NAME_None;
                }
            }
            else
            {
                Row->SelectedCategory = NAME_None;
            }

            if (IsCategoryColumnEnabled())
            {
                EnsureCategoryOptions(Row);
                if (Row->SelectedCategory.IsNone())
                {
                    if (const TArray<TSharedPtr<FName>>* Options = CategoryOptionsByRow.Find(Row))
                    {
                        for (const TSharedPtr<FName>& Option : *Options)
                        {
                            if (Option.IsValid() && *Option != GetAddCategoryOption())
                            {
                                Row->SelectedCategory = *Option;
                                break;
                            }
                        }
                    }
                }
            }

            const FString InitialAssetName = Row->SmartPrefillName.IsEmpty() ? (Row->ParsedBaseName.IsEmpty() ? Row->CurrentName : Row->ParsedBaseName) : Row->SmartPrefillName;
            SetRowAssetName(Row, InitialAssetName, false);

            Rows.Add(Row);
            RowPaths.Add(ObjectPath);
            bAddedAny = true;
        }

        if (bAddedAny)
        {
            RequestRefresh();
        }
    }

private:

    static const FName& GetAddDomainOption()
    {
        static const FName AddDomainOption(TEXT("__NSBONSAI_ADD_DOMAIN__"));
        return AddDomainOption;
    }

    static const FName& GetAddCategoryOption()
    {
        static const FName AddCategoryOption(TEXT("__NSBONSAI_ADD_CATEGORY__"));
        return AddCategoryOption;
    }

    static FString EscapeCsv(const FString& Value)
    {
        FString Escaped = Value;
        Escaped.ReplaceInline(TEXT("\""), TEXT("\"\""));
        return FString::Printf(TEXT("\"%s\""), *Escaped);
    }

    static void AppendRenameLog(const FString& OldPath, const FString& NewPath, const FString& Result)
    {
        const FString BonsaiLogPath = FPaths::Combine(FPaths::ProjectLogDir(), TEXT("NsBonsai_Rename.log"));
        FString Line;
        if (!IFileManager::Get().FileExists(*BonsaiLogPath))
        {
            Line += TEXT("OldPath,NewPath,Result") LINE_TERMINATOR;
        }
        Line += EscapeCsv(OldPath) + TEXT(",") + EscapeCsv(NewPath) + TEXT(",") + EscapeCsv(Result) + LINE_TERMINATOR;
        FFileHelper::SaveStringToFile(Line, *BonsaiLogPath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
    }

    static FString NormalizeTokenForCompare(const FString& InToken)
    {
        return FNsBonsaiNameRules::NormalizeTokenForCompare(InToken);
    }

    static FString SanitizeAssetNameCandidate(const FString& InAssetName)
    {
        return FNsBonsaiNameRules::SanitizeAssetName(InAssetName);
    }

    static FString ToPascalCase(const FString& InValue)
    {
        TArray<FString> Parts;
        InValue.ParseIntoArray(Parts, TEXT("_"), true);

        FString Out;
        for (FString Part : Parts)
        {
            Part = SanitizeAssetNameCandidate(Part);
            if (Part.IsEmpty())
            {
                continue;
            }
            Part.ToLowerInline();
            Part[0] = FChar::ToUpper(Part[0]);
            Out += Part;
        }
        return Out;
    }

    static FString ToSnakeCase(const FString& InValue)
    {
        FString Out;
        Out.Reserve(InValue.Len() * 2);

        for (int32 Index = 0; Index < InValue.Len(); ++Index)
        {
            const TCHAR Character = InValue[Index];
            if (FChar::IsWhitespace(Character) || Character == TCHAR('-'))
            {
                Out.AppendChar(TCHAR('_'));
                continue;
            }

            if (FChar::IsUpper(Character) && Out.Len() > 0)
            {
                const TCHAR Previous = InValue[Index - 1];
                if (FChar::IsLower(Previous) || FChar::IsDigit(Previous))
                {
                    Out.AppendChar(TCHAR('_'));
                }
            }

            Out.AppendChar(Character);
        }

        Out = SanitizeAssetNameCandidate(Out);
        Out.ToLowerInline();
        return Out;
    }

    static FString UppercaseCommonAcronyms(const FString& InValue)
    {
        static const TSet<FString> Acronyms =
        {
            TEXT("AI"), TEXT("UI"), TEXT("UX"), TEXT("HUD"), TEXT("VFX"), TEXT("SFX"), TEXT("FX"), TEXT("NPC"), TEXT("LOD"), TEXT("IK"), TEXT("FK")
        };

        TArray<FString> Parts;
        InValue.ParseIntoArray(Parts, TEXT("_"), false);
        for (FString& Part : Parts)
        {
            const FString UpperPart = NormalizeTokenForCompare(Part);
            if (Acronyms.Contains(UpperPart))
            {
                Part = UpperPart;
            }
        }
        return FString::Join(Parts, TEXT("_"));
    }

    FString BuildSmartPrefillAssetName(const FString& OriginalAssetName) const
    {
        if (!Settings)
        {
            const FString Fallback = FNsBonsaiNameRules::SanitizeAssetName(OriginalAssetName);
            return Fallback.IsEmpty() ? TEXT("New") : Fallback;
        }
        return FNsBonsaiNameRules::SmartPrefillAssetName(OriginalAssetName, *Settings);
    }

    static FNsBonsaiRowState ToRowState(const FRowModel& Row)
    {
        FNsBonsaiRowState State;
        State.SelectedDomain = Row.SelectedDomain;
        State.SelectedCategory = Row.SelectedCategory;
        State.AssetName = Row.AssetName;
        State.ParsedAssetName = Row.ParsedBaseName;
        State.CurrentName = Row.CurrentName;
        return State;
    }

    void RebuildPreview(FRowModel& Row) const
    {
        if (!Settings)
        {
            Row.PreviewName.Reset();
            return;
        }

        IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        TSet<FString> ReservedObjectPaths;
        FString FinalName;
        FSoftObjectPath NewPath;
        FString Error;
        if (FNsBonsaiNameRules::AllocateFinalNameWithVariant(Row.AssetData, ToRowState(Row), *Settings, AssetRegistry, ReservedObjectPaths, FinalName, NewPath, Error))
        {
            Row.PreviewName = FinalName;
            return;
        }

        Row.PreviewName = FNsBonsaiNameRules::BuildPreviewName(Row.AssetData, ToRowState(Row), *Settings);
    }

    enum class ERowStatus : uint8
    {
        Ready,
        Warning,
        Conflict,
        Ignored,
    };

    struct FRowStatusInfo
    {
        ERowStatus Status = ERowStatus::Ready;
        FString Reason;
    };

    struct FRenamePlan
    {
        TSharedPtr<FRowModel> Row;
        FSoftObjectPath OldPath;
        FSoftObjectPath NewPath;
        bool bNeedsRename = false;
    };

    struct FDeferredRenameEntry
    {
        FSoftObjectPath OldPath;
        FSoftObjectPath NewPath;
        FName Domain;
        FName Category;
    };

    bool IsTypeColumnEnabled() const
    {
        return Settings && Settings->bShowTypeColumn && Settings->IsComponentEnabled(ENsBonsaiNameComponent::Type);
    }

    bool IsDomainColumnEnabled() const
    {
        return Settings && Settings->IsComponentEnabled(ENsBonsaiNameComponent::Domain);
    }

    bool IsCategoryColumnEnabled() const
    {
        return Settings && Settings->IsComponentEnabled(ENsBonsaiNameComponent::Category);
    }

    bool IsAssetNameColumnEnabled() const
    {
        return Settings && Settings->IsComponentEnabled(ENsBonsaiNameComponent::AssetName);
    }
    bool IsFinalNameColumnEnabled() const
    {
        return Settings && Settings->bShowFinalNameColumn;
    }

    bool IsCurrentNameColumnEnabled() const
    {
        return Settings && Settings->bShowCurrentNameColumn;
    }

    TSharedRef<SWidget> BuildCompactTable()
    {
        TSharedRef<SHeaderRow> HeaderRow = SNew(SHeaderRow)
            + SHeaderRow::Column(TEXT("Status")).DefaultLabel(FText::FromString(TEXT(""))).DefaultTooltip(FText::FromString(TEXT("Row status."))).FixedWidth(40);

        if (IsCurrentNameColumnEnabled())
        {
            HeaderRow->AddColumn(SHeaderRow::Column(TEXT("Asset")).DefaultLabel(FText::FromString(TEXT("Asset"))).DefaultTooltip(FText::FromString(TEXT("Current asset name."))).FillWidth(0.22f));
        }
        if (IsTypeColumnEnabled())
        {
            HeaderRow->AddColumn(SHeaderRow::Column(TEXT("Type")).DefaultLabel(FText::FromString(TEXT("Type"))).DefaultTooltip(FText::FromString(TEXT("Type token resolved from Type Rules."))).FillWidth(0.08f));
        }
        if (IsDomainColumnEnabled())
        {
            HeaderRow->AddColumn(SHeaderRow::Column(TEXT("Domain")).DefaultLabel(FText::FromString(TEXT("Domain"))).DefaultTooltip(FText::FromString(TEXT("Domain token used in naming."))).FillWidth(0.14f));
        }
        if (IsCategoryColumnEnabled())
        {
            HeaderRow->AddColumn(SHeaderRow::Column(TEXT("Category")).DefaultLabel(FText::FromString(TEXT("Category"))).DefaultTooltip(FText::FromString(TEXT("Category token used in naming."))).FillWidth(0.14f));
        }
        if (IsAssetNameColumnEnabled())
        {
            HeaderRow->AddColumn(SHeaderRow::Column(TEXT("AssetName")).DefaultLabel(FText::FromString(TEXT("Asset Name"))).DefaultTooltip(FText::FromString(TEXT("User-editable base name."))).FillWidth(0.20f));
        }
        if (IsFinalNameColumnEnabled())
        {
            HeaderRow->AddColumn(SHeaderRow::Column(TEXT("Final")).DefaultLabel(FText::FromString(TEXT("Final Name"))).DefaultTooltip(FText::FromString(TEXT("Preview generated from active naming format."))).FillWidth(0.18f));
        }

        HeaderRow->AddColumn(SHeaderRow::Column(TEXT("Ok")).DefaultLabel(FText::FromString(TEXT(""))).DefaultTooltip(FText::FromString(TEXT("Rename now and remove row."))).FixedWidth(30));
        HeaderRow->AddColumn(SHeaderRow::Column(TEXT("No")).DefaultLabel(FText::FromString(TEXT(""))).DefaultTooltip(FText::FromString(TEXT("Ignore this asset (won't be renamed)."))).FixedWidth(30));

        return SNew(SBorder)
        .Padding(4)
        [
            SAssignNew(ListView, SListView<TSharedPtr<FRowModel>>)
            .ListItemsSource(&Rows)
            .SelectionMode(ESelectionMode::Multi)
            .OnGenerateRow(this, &SNsBonsaiReviewWindow::OnGenerateCompactRow)
            .HeaderRow(HeaderRow)
        ];
    }

    FText GetDomainOptionText(const TSharedPtr<FName>& Name) const
    {
        if (!Name.IsValid())
        {
            return FText::GetEmpty();
        }
        if (*Name == GetAddDomainOption())
        {
            return FText::FromString(TEXT("+ Add Domain..."));
        }
        return FText::FromName(*Name);
    }

    FText GetCategoryOptionText(const TSharedPtr<FName>& Name) const
    {
        if (!Name.IsValid())
        {
            return FText::GetEmpty();
        }
        if (*Name == GetAddCategoryOption())
        {
            return FText::FromString(TEXT("+ Add Category..."));
        }
        return FText::FromName(*Name);
    }

    class SCompactRow final : public SMultiColumnTableRow<TSharedPtr<FRowModel>>
    {
    public:
        SLATE_BEGIN_ARGS(SCompactRow) {}
            SLATE_ARGUMENT(TSharedPtr<FRowModel>, Row)
            SLATE_ARGUMENT(SNsBonsaiReviewWindow*, Owner)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
        {
            Row = InArgs._Row;
            Owner = InArgs._Owner;
            const FMargin RowPadding = (Owner && Owner->Settings && Owner->Settings->bCompactRows) ? FMargin(2, 1) : FMargin(4, 3);
            SMultiColumnTableRow<TSharedPtr<FRowModel>>::Construct(
                FSuperRowType::FArguments().Padding(RowPadding),
                InOwnerTableView
            );
        }

        virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
        {
            if (!Row.IsValid() || !Owner)
            {
                return SNew(STextBlock);
            }

            if (ColumnName == TEXT("Status"))
            {
                return SNew(STextBlock)
                    .Text_Lambda([this]{ return Owner->GetStatusGlyph(Row); })
                    .ToolTipText_Lambda([this]{ return Owner->GetStatusTooltip(Row); })
                    .ColorAndOpacity_Lambda([this]{ return Owner->GetStatusColor(Row); });
            }
            if (ColumnName == TEXT("Asset"))
            {
                return SNew(STextBlock).Text(FText::FromString(Row->CurrentName));
            }
            if (ColumnName == TEXT("Type"))
            {
                const FName TypeToken = FNsBonsaiNameRules::ResolveTypeToken(Row->AssetData, *Owner->Settings);
                return SNew(STextBlock).Text(FText::FromName(TypeToken));
            }
            if (ColumnName == TEXT("Domain"))
            {
                return SNew(SComboBox<TSharedPtr<FName>>)
                    .OptionsSource(&Owner->DomainOptions)
                    .ToolTipText(FText::FromString(TEXT("Select domain token. If multiple rows are selected, applies to selection.")))
                    .OnGenerateWidget_Lambda([this](TSharedPtr<FName> Name)
                    {
                        return SNew(STextBlock).Text(Owner->GetDomainOptionText(Name));
                    })
                    .OnSelectionChanged_Lambda([this](TSharedPtr<FName> NewSel, ESelectInfo::Type)
                    {
                        if (NewSel.IsValid())
                        {
                            Owner->HandleDomainSelection(Row, *NewSel);
                        }
                    })
                    [
                        SNew(STextBlock)
                        .Text_Lambda([this]
                        {
                            return Row->SelectedDomain.IsNone() ? FText::FromString(TEXT("Domain")) : FText::FromName(Row->SelectedDomain);
                        })
                    ];
            }
            if (ColumnName == TEXT("Category"))
            {
                Owner->EnsureCategoryOptions(Row);
                return SNew(SComboBox<TSharedPtr<FName>>)
                    .OptionsSource(&Owner->CategoryOptionsByRow.FindOrAdd(Row))
                    .IsEnabled_Lambda([this]{ return Owner->CanEditCategory(Row); })
                    .ToolTipText(FText::FromString(TEXT("Select category token. If multiple rows are selected, applies to selection.")))
                    .OnGenerateWidget_Lambda([this](TSharedPtr<FName> Name)
                    {
                        return SNew(STextBlock).Text(Owner->GetCategoryOptionText(Name));
                    })
                    .OnSelectionChanged_Lambda([this](TSharedPtr<FName> NewSel, ESelectInfo::Type)
                    {
                        if (NewSel.IsValid())
                        {
                            Owner->HandleCategorySelection(Row, *NewSel);
                        }
                    })
                    [
                        SNew(STextBlock)
                        .Text_Lambda([this]
                        {
                            return Row->SelectedCategory.IsNone() ? FText::FromString(TEXT("Category")) : FText::FromName(Row->SelectedCategory);
                        })
                    ];
            }
            if (ColumnName == TEXT("AssetName"))
            {
                return SNew(SEditableTextBox)
                    .ToolTipText(FText::FromString(TEXT("Editable Asset Name component.")))
                    .Text_Lambda([this]{ return FText::FromString(Row->AssetName); })
                    .OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type)
                    {
                        Owner->ApplyAssetName(Row, NewText.ToString());
                    })
                    .OnContextMenuOpening(FOnContextMenuOpening::CreateLambda([this]() -> TSharedPtr<SWidget>
                    {
                        return Owner->BuildAssetNameContextMenu(Row);
                    }));
            }
            if (ColumnName == TEXT("Final"))
            {
                return SNew(STextBlock)
                    .Text_Lambda([this]{ return FText::FromString(Row->PreviewName); })
                    .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
                    .ToolTipText_Lambda([this]{ return Owner->BuildFinalNameTooltip(Row); });
            }
            if (ColumnName == TEXT("Ok"))
            {
                return SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .ContentPadding(FMargin(4, 2))
                    .IsEnabled_Lambda([this]{ return Owner->CanConfirmRow(Row); })
                    .ToolTipText(FText::FromString(TEXT("Rename now and remove row.")))
                    .OnClicked_Lambda([this]{ return Owner->OnConfirmClicked(Row); })
                    [
                        SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.Check"))
                    ];
            }
            if (ColumnName == TEXT("No"))
            {
                return SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                    .ContentPadding(FMargin(4, 2))
                    .ToolTipText(FText::FromString(TEXT("Ignore this asset (won't be renamed).")))
                    .OnClicked_Lambda([this]{ return Owner->OnIgnoreClicked(Row); })
                    [
                        SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.X"))
                    ];
            }

            return SNew(STextBlock);
        }

    private:
        TSharedPtr<FRowModel> Row;
        SNsBonsaiReviewWindow* Owner = nullptr;
    };

    TSharedRef<ITableRow> OnGenerateCompactRow(TSharedPtr<FRowModel> Row, const TSharedRef<STableViewBase>& OwnerTable)
    {
        return SNew(SCompactRow, OwnerTable)
            .Row(Row)
            .Owner(this);
    }

    TArray<TSharedPtr<FRowModel>> GetSelectionOrSingle(const TSharedPtr<FRowModel>& FocusRow)
    {
        TArray<TSharedPtr<FRowModel>> Selection;
        if (ListView.IsValid())
        {
            ListView->GetSelectedItems(Selection);
        }
        if (Selection.Num() > 0 && Selection.Contains(FocusRow))
        {
            return Selection;
        }
        Selection.Reset();
        Selection.Add(FocusRow);
        return Selection;
    }

    TArray<TSharedPtr<FRowModel>> GetDomainOrCategoryTargets(const TSharedPtr<FRowModel>& FocusRow)
    {
        TArray<TSharedPtr<FRowModel>> Selection;
        if (ListView.IsValid())
        {
            ListView->GetSelectedItems(Selection);
        }
        if (Selection.Num() > 1 && Selection.Contains(FocusRow))
        {
            return Selection;
        }
        Selection.Reset();
        Selection.Add(FocusRow);
        return Selection;
    }

    void ClearRowConflictFlags(const TSharedPtr<FRowModel>& Row)
    {
        if (!Row.IsValid())
        {
            return;
        }
        Row->bRenameBlockedConflict = false;
        Row->RenameBlockedReason.Reset();
    }

    FRowStatusInfo EvaluateRowStatus(const TSharedPtr<FRowModel>& Row) const
    {
        FRowStatusInfo Info;
        if (!Row.IsValid())
        {
            Info.Status = ERowStatus::Warning;
            Info.Reason = TEXT("Invalid row data.");
            return Info;
        }
        if (Row->bIgnored)
        {
            Info.Status = ERowStatus::Ignored;
            Info.Reason = TEXT("Ignored.");
            return Info;
        }
        if (Row->bRenameBlockedConflict)
        {
            Info.Status = ERowStatus::Conflict;
            Info.Reason = Row->RenameBlockedReason;
            return Info;
        }

        const FNsBonsaiValidationResult Validation = FNsBonsaiNameRules::ValidateRowState(Row->AssetData, ToRowState(*Row), *Settings);
        if (!Validation.bIsValid)
        {
            Info.Status = ERowStatus::Warning;
            Info.Reason = Validation.Message;
            return Info;
        }

        if (bEnableConflictPrecheck)
        {
            IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
            TSet<FString> ReservedNames;
            FString FinalName;
            FSoftObjectPath NewPath;
            FString Error;
            if (!FNsBonsaiNameRules::AllocateFinalNameWithVariant(Row->AssetData, ToRowState(*Row), *Settings, AssetRegistry, ReservedNames, FinalName, NewPath, Error))
            {
                Info.Status = ERowStatus::Conflict;
                Info.Reason = Error;
                return Info;
            }
        }

        Info.Status = ERowStatus::Ready;
        Info.Reason = TEXT("Ready to rename.");
        return Info;
    }

    bool CanConfirmRow(const TSharedPtr<FRowModel>& Row) const
    {
        return EvaluateRowStatus(Row).Status == ERowStatus::Ready;
    }

    FText GetStatusGlyph(const TSharedPtr<FRowModel>& Row) const
    {
        switch (EvaluateRowStatus(Row).Status)
        {
        case ERowStatus::Ready: return FText::FromString(TEXT("OK"));
        case ERowStatus::Warning: return FText::FromString(TEXT("!"));
        case ERowStatus::Conflict: return FText::FromString(TEXT("X"));
        case ERowStatus::Ignored: return FText::FromString(TEXT("-"));
        default: return FText::GetEmpty();
        }
    }

    FSlateColor GetStatusColor(const TSharedPtr<FRowModel>& Row) const
    {
        switch (EvaluateRowStatus(Row).Status)
        {
        case ERowStatus::Ready: return FSlateColor(FLinearColor(0.2f, 0.7f, 0.2f, 1.0f));
        case ERowStatus::Warning: return FSlateColor(FLinearColor(0.85f, 0.65f, 0.1f, 1.0f));
        case ERowStatus::Conflict: return FSlateColor(FLinearColor(0.85f, 0.2f, 0.2f, 1.0f));
        case ERowStatus::Ignored: return FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
        default: return FSlateColor(FLinearColor::White);
        }
    }

    FText GetStatusTooltip(const TSharedPtr<FRowModel>& Row) const
    {
        return FText::FromString(EvaluateRowStatus(Row).Reason);
    }

    FText BuildFinalNameTooltip(const TSharedPtr<FRowModel>& Row) const
    {
        if (!Row.IsValid())
        {
            return FText::GetEmpty();
        }

        const FNsBonsaiRowState RowState = ToRowState(*Row);
        const FString TypeText = FNsBonsaiNameRules::ResolveTypeToken(Row->AssetData, *Settings).ToString();
        const FString DomainText = Settings->NormalizeToken(RowState.SelectedDomain).ToString();
        const FString CategoryText = Settings->NormalizeToken(RowState.SelectedCategory).ToString();
        const FString AssetNameText = FNsBonsaiNameRules::SanitizeAssetName(RowState.AssetName);

        TArray<FString> Breakdown;
        if (Settings->IsComponentEnabled(ENsBonsaiNameComponent::Type))
        {
            Breakdown.Add(FString::Printf(TEXT("Type: %s"), *TypeText));
        }
        if (Settings->IsComponentEnabled(ENsBonsaiNameComponent::Domain))
        {
            Breakdown.Add(FString::Printf(TEXT("Domain: %s"), *DomainText));
        }
        if (Settings->IsComponentEnabled(ENsBonsaiNameComponent::Category))
        {
            Breakdown.Add(FString::Printf(TEXT("Category: %s"), *CategoryText));
        }
        if (Settings->IsComponentEnabled(ENsBonsaiNameComponent::AssetName))
        {
            Breakdown.Add(FString::Printf(TEXT("Asset: %s"), *AssetNameText));
        }

        return FText::FromString(FString::Printf(TEXT("Current: %s\nNew: %s\n%s"),
            *Row->CurrentName,
            *Row->PreviewName,
            *FString::Join(Breakdown, TEXT(" | "))));
    }

    FSlateColor GetRowBackgroundColor(const TSharedPtr<FRowModel>& Row) const
    {
        switch (EvaluateRowStatus(Row).Status)
        {
        case ERowStatus::Ignored: return FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f, 0.12f));
        case ERowStatus::Warning: return FSlateColor(FLinearColor(0.95f, 0.86f, 0.25f, 0.16f));
        default: return FSlateColor(FLinearColor::White);
        }
    }

    int32 GetRemainingCount() const
    {
        int32 Count = 0;
        for (const TSharedPtr<FRowModel>& Row : Rows)
        {
            if (Row.IsValid() && !Row->bIgnored)
            {
                ++Count;
            }
        }
        return Count;
    }
    void SortByRecentThenAlpha(TArray<FName>& Tokens, const TArray<FName>& Recents) const
    {
        Tokens.Sort([&Recents](const FName& L, const FName& R)
        {
            const int32 Li = Recents.IndexOfByKey(L);
            const int32 Ri = Recents.IndexOfByKey(R);
            const bool bL = Li != INDEX_NONE;
            const bool bR = Ri != INDEX_NONE;
            if (bL && bR)
            {
                return Li < Ri;
            }
            if (bL != bR)
            {
                return bL;
            }
            return L.ToString().Compare(R.ToString(), ESearchCase::IgnoreCase) < 0;
        });
    }

    void RebuildDomainOptions()
    {
        DomainOptions.Reset();
        if (!IsDomainColumnEnabled())
        {
            return;
        }

        TArray<FName> Domains;
        FNsBonsaiNameRules::BuildDomainOptions(*Settings, Domains);
        SortByRecentThenAlpha(Domains, UserSettings->RecentDomains);
        for (const FName Domain : Domains)
        {
            DomainOptions.Add(MakeShared<FName>(Domain));
        }
        DomainOptions.Add(MakeShared<FName>(GetAddDomainOption()));
    }

    bool CanEditCategory(const TSharedPtr<FRowModel>& Row) const
    {
        if (!Row.IsValid() || !IsCategoryColumnEnabled())
        {
            return false;
        }
        if (!IsDomainColumnEnabled())
        {
            return true;
        }
        return !Row->SelectedDomain.IsNone();
    }

    bool PromptForTokenInput(const FString& Title, const FString& Label, FString& OutToken)
    {
        OutToken.Reset();
        if (!FSlateApplication::IsInitialized())
        {
            return false;
        }

        bool bAccepted = false;
        TSharedPtr<SEditableTextBox> InputBox;
        TSharedRef<SWindow> Dialog = SNew(SWindow)
            .Title(FText::FromString(Title))
            .ClientSize(FVector2D(420, 120))
            .SupportsMinimize(false)
            .SupportsMaximize(false)
            .IsTopmostWindow(true)
            .SizingRule(ESizingRule::FixedSize);

        Dialog->SetContent(
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(8)
            [
                SNew(STextBlock).Text(FText::FromString(Label))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(8)
            [
                SAssignNew(InputBox, SEditableTextBox)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(8)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.f)
                [
                    SNew(SSpacer)
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Cancel")))
                    .OnClicked_Lambda([Dialog]()
                    {
                        Dialog->RequestDestroyWindow();
                        return FReply::Handled();
                    })
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Add")))
                    .OnClicked_Lambda([&bAccepted, Dialog]()
                    {
                        bAccepted = true;
                        Dialog->RequestDestroyWindow();
                        return FReply::Handled();
                    })
                ]
            ]);

        FSlateApplication::Get().AddModalWindow(Dialog, ParentWindow.Pin(), false);
        if (!bAccepted || !InputBox.IsValid())
        {
            return false;
        }

        OutToken = InputBox->GetText().ToString();
        OutToken.TrimStartAndEndInline();
        return !OutToken.IsEmpty();
    }

    FName SanitizeTokenForLibrary(const FString& InToken) const
    {
        const FString Sanitized = FNsBonsaiNameRules::SanitizeAssetName(InToken);
        if (Sanitized.IsEmpty())
        {
            return NAME_None;
        }
        return Settings->NormalizeToken(FName(*Sanitized));
    }

    void AddDomainTokenToSettings(const FName DomainToken)
    {
        if (DomainToken.IsNone())
        {
            return;
        }

        UNsBonsaiSettings* MutableSettings = GetMutableDefault<UNsBonsaiSettings>();
        if (!MutableSettings)
        {
            return;
        }

        for (const FNsBonsaiDomainDef& Existing : MutableSettings->Domains)
        {
            if (MutableSettings->NormalizeToken(Existing.DomainToken) == DomainToken)
            {
                Settings = MutableSettings;
                RebuildDomainOptions();
                return;
            }
        }

        MutableSettings->Modify();
        FNsBonsaiDomainDef& NewDomain = MutableSettings->Domains.AddDefaulted_GetRef();
        NewDomain.DomainToken = DomainToken;
        MutableSettings->SaveConfig();
        if (GConfig)
        {
            GConfig->Flush(false, MutableSettings->GetDefaultConfigFilename());
        }
        Settings = MutableSettings;
        RebuildDomainOptions();
    }

    void AddCategoryTokenToSettings(const FName DomainToken, const FName CategoryToken)
    {
        if (CategoryToken.IsNone())
        {
            return;
        }

        UNsBonsaiSettings* MutableSettings = GetMutableDefault<UNsBonsaiSettings>();
        if (!MutableSettings)
        {
            return;
        }

        MutableSettings->Modify();
        bool bChanged = false;
        if (!IsDomainColumnEnabled())
        {
            if (!MutableSettings->GlobalCategories.Contains(CategoryToken))
            {
                MutableSettings->GlobalCategories.Add(CategoryToken);
                bChanged = true;
            }
        }
        else
        {
            for (FNsBonsaiDomainDef& DomainDef : MutableSettings->Domains)
            {
                if (MutableSettings->NormalizeToken(DomainDef.DomainToken) != DomainToken)
                {
                    continue;
                }
                if (!DomainDef.Categories.Contains(CategoryToken))
                {
                    DomainDef.Categories.Add(CategoryToken);
                    bChanged = true;
                }
                break;
            }
        }

        if (bChanged)
        {
            MutableSettings->SaveConfig();
            if (GConfig)
            {
                GConfig->Flush(false, MutableSettings->GetDefaultConfigFilename());
            }
        }
        Settings = MutableSettings;
    }

    void EnsureCategoryOptions(const TSharedPtr<FRowModel>& Row)
    {
        if (!Row.IsValid())
        {
            return;
        }

        TArray<TSharedPtr<FName>>& Options = CategoryOptionsByRow.FindOrAdd(Row);
        Options.Reset();
        if (!IsCategoryColumnEnabled())
        {
            return;
        }
        if (IsDomainColumnEnabled() && Row->SelectedDomain.IsNone())
        {
            return;
        }

        TArray<FName> Categories;
        FNsBonsaiNameRules::BuildCategoryOptions(*Settings, Row->SelectedDomain, Categories);
        SortByRecentThenAlpha(Categories, UserSettings->RecentCategories);
        for (const FName Category : Categories)
        {
            Options.Add(MakeShared<FName>(Category));
        }
        Options.Add(MakeShared<FName>(GetAddCategoryOption()));
    }

    void ApplyDomain(const TSharedPtr<FRowModel>& FocusRow, const FName Domain)
    {
        const TArray<TSharedPtr<FRowModel>> Targets = GetDomainOrCategoryTargets(FocusRow);
        for (const TSharedPtr<FRowModel>& Row : Targets)
        {
            if (!Row.IsValid())
            {
                continue;
            }

            ClearRowConflictFlags(Row);
            Row->SelectedDomain = Domain;
            if (IsCategoryColumnEnabled())
            {
                Row->SelectedCategory = NAME_None;
                EnsureCategoryOptions(Row);
                if (const TArray<TSharedPtr<FName>>* Opts = CategoryOptionsByRow.Find(Row))
                {
                    for (const TSharedPtr<FName>& Option : *Opts)
                    {
                        if (Option.IsValid() && *Option != GetAddCategoryOption())
                        {
                            Row->SelectedCategory = *Option;
                            break;
                        }
                    }
                }
            }
            RebuildPreview(*Row);
        }
        RequestRefresh();
    }

    void ApplyCategory(const TSharedPtr<FRowModel>& FocusRow, const FName Category)
    {
        const TArray<TSharedPtr<FRowModel>> Targets = GetDomainOrCategoryTargets(FocusRow);
        for (const TSharedPtr<FRowModel>& Row : Targets)
        {
            if (!Row.IsValid())
            {
                continue;
            }
            ClearRowConflictFlags(Row);
            Row->SelectedCategory = Category;
            RebuildPreview(*Row);
        }
        RequestRefresh();
    }

    void HandleDomainSelection(const TSharedPtr<FRowModel>& FocusRow, const FName DomainOrCommand)
    {
        if (DomainOrCommand == GetAddDomainOption())
        {
            FString NewDomainText;
            if (!PromptForTokenInput(TEXT("Add Domain"), TEXT("Enter a new domain token:"), NewDomainText))
            {
                return;
            }
            const FName DomainToken = SanitizeTokenForLibrary(NewDomainText);
            AddDomainTokenToSettings(DomainToken);
            if (!DomainToken.IsNone())
            {
                ApplyDomain(FocusRow, DomainToken);
            }
            return;
        }
        ApplyDomain(FocusRow, DomainOrCommand);
    }

    void HandleCategorySelection(const TSharedPtr<FRowModel>& FocusRow, const FName CategoryOrCommand)
    {
        if (CategoryOrCommand == GetAddCategoryOption())
        {
            FString NewCategoryText;
            if (!PromptForTokenInput(TEXT("Add Category"), TEXT("Enter a new category token:"), NewCategoryText))
            {
                return;
            }
            const FName CategoryToken = SanitizeTokenForLibrary(NewCategoryText);
            const FName DomainToken = FocusRow.IsValid() ? FocusRow->SelectedDomain : NAME_None;
            AddCategoryTokenToSettings(DomainToken, CategoryToken);
            if (!CategoryToken.IsNone())
            {
                CategoryOptionsByRow.Reset();
                ApplyCategory(FocusRow, CategoryToken);
            }
            return;
        }
        ApplyCategory(FocusRow, CategoryOrCommand);
    }

    void SetRowAssetName(const TSharedPtr<FRowModel>& Row, const FString& ProposedName, bool bRequestRefresh)
    {
        if (!Row.IsValid())
        {
            return;
        }

        FString Sanitized = SanitizeAssetNameCandidate(ProposedName);
        if (Sanitized.IsEmpty())
        {
            Sanitized = SanitizeAssetNameCandidate(Row->SmartPrefillName);
        }
        if (Sanitized.IsEmpty())
        {
            Sanitized = SanitizeAssetNameCandidate(Row->ParsedBaseName);
        }
        if (Sanitized.IsEmpty())
        {
            Sanitized = SanitizeAssetNameCandidate(Row->CurrentName);
        }
        if (Sanitized.IsEmpty())
        {
            Sanitized = TEXT("New");
        }

        ClearRowConflictFlags(Row);
        Row->AssetName = Sanitized;
        RebuildPreview(*Row);
        if (bRequestRefresh)
        {
            RequestRefresh();
        }
    }

    void ApplyAssetName(const TSharedPtr<FRowModel>& Row, const FString& NewName)
    {
        SetRowAssetName(Row, NewName, true);
    }

    TSharedPtr<SWidget> BuildAssetNameContextMenu(const TSharedPtr<FRowModel>& Row)
    {
        if (!Row.IsValid())
        {
            return nullptr;
        }

        FMenuBuilder MenuBuilder(true, nullptr);
        MenuBuilder.BeginSection(NAME_None, FText::FromString(TEXT("Asset Name")));
        MenuBuilder.AddMenuEntry(
            FText::FromString(TEXT("Reset to original")),
            FText::FromString(TEXT("Use the original imported/created asset name.")),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([this, Row]()
            {
                SetRowAssetName(Row, Row->CurrentName, true);
            })));

        MenuBuilder.AddMenuEntry(
            FText::FromString(TEXT("Use smart prefill")),
            FText::FromString(TEXT("Strip type/domain/category/variant from the original name.")),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([this, Row]()
            {
                SetRowAssetName(Row, Row->SmartPrefillName, true);
            })));

        MenuBuilder.AddMenuEntry(
            FText::FromString(TEXT("Uppercase acronyms (UI/VFX/etc.)")),
            FText::FromString(TEXT("Uppercase common acronyms in the current name.")),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([this, Row]()
            {
                SetRowAssetName(Row, UppercaseCommonAcronyms(Row->AssetName), true);
            })));

        MenuBuilder.AddMenuEntry(
            FText::FromString(TEXT("To PascalCase")),
            FText::FromString(TEXT("Convert current name to PascalCase.")),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([this, Row]()
            {
                SetRowAssetName(Row, ToPascalCase(Row->AssetName), true);
            })));

        MenuBuilder.AddMenuEntry(
            FText::FromString(TEXT("To snake_case")),
            FText::FromString(TEXT("Convert current name to snake_case.")),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([this, Row]()
            {
                SetRowAssetName(Row, ToSnakeCase(Row->AssetName), true);
            })));
        MenuBuilder.EndSection();

        return MenuBuilder.MakeWidget();
    }

    void RemoveRows(const TArray<TSharedPtr<FRowModel>>& Targets)
    {
        for (const TSharedPtr<FRowModel>& Row : Targets)
        {
            if (!Row.IsValid())
            {
                continue;
            }

            if (Manager)
            {
                Manager->MarkResolved(Row->ObjectPath);
            }
            Rows.Remove(Row);
            CategoryOptionsByRow.Remove(Row);
            RowPaths.Remove(Row->ObjectPath);
        }

        if (ListView.IsValid())
        {
            ListView->ClearSelection();
        }
        RequestRefresh();
    }

    FReply OnIgnoreClicked(const TSharedPtr<FRowModel>& FocusRow)
    {
        const TArray<TSharedPtr<FRowModel>> Targets = GetSelectionOrSingle(FocusRow);
        TArray<TSharedPtr<FRowModel>> Removals;
        for (const TSharedPtr<FRowModel>& Row : Targets)
        {
            if (!Row.IsValid())
            {
                continue;
            }
            ++IgnoredCount;
            AppendRenameLog(Row->ObjectPath.ToString(), FString(), TEXT("Ignored"));
            Removals.Add(Row);
        }
        if (Removals.Num() > 0)
        {
            RemoveRows(Removals);
        }
        if (GetRemainingCount() == 0 && Settings->bAutoCloseWindowWhenEmpty)
        {
            return CloseWindow();
        }
        return FReply::Handled();
    }

    TArray<FRenamePlan> BuildRenamePlans(const TArray<TSharedPtr<FRowModel>>& SourceRows)
    {
        IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        TSet<FString> ReservedObjectPaths;
        for (const FDeferredRenameEntry& Deferred : DeferredRenames)
        {
            if (Deferred.NewPath.IsValid())
            {
                ReservedObjectPaths.Add(Deferred.NewPath.ToString());
            }
        }

        TArray<FRenamePlan> Plans;
        for (const TSharedPtr<FRowModel>& Row : SourceRows)
        {
            if (!Row.IsValid())
            {
                continue;
            }

            ClearRowConflictFlags(Row);
            const FNsBonsaiValidationResult Validation = FNsBonsaiNameRules::ValidateRowState(Row->AssetData, ToRowState(*Row), *Settings);
            if (!Validation.bIsValid)
            {
                Row->bRenameBlockedConflict = true;
                Row->RenameBlockedReason = Validation.Message;
                AppendRenameLog(Row->ObjectPath.ToString(), FString(), TEXT("BlockedMissingFields"));
                continue;
            }

            FString FinalName;
            FSoftObjectPath NewPath;
            FString AllocationError;
            if (!FNsBonsaiNameRules::AllocateFinalNameWithVariant(Row->AssetData, ToRowState(*Row), *Settings, AssetRegistry, ReservedObjectPaths, FinalName, NewPath, AllocationError))
            {
                Row->bRenameBlockedConflict = true;
                Row->RenameBlockedReason = AllocationError;
                AppendRenameLog(Row->ObjectPath.ToString(), FString(), TEXT("ConflictNoVariant"));
                continue;
            }

            FRenamePlan& Plan = Plans.Emplace_GetRef();
            Plan.Row = Row;
            Plan.OldPath = Row->ObjectPath;
            Plan.NewPath = NewPath;
            Plan.bNeedsRename = (Plan.OldPath != Plan.NewPath);
        }

        return Plans;
    }

    bool RunRenameAssetsWithTransaction(const TArray<FAssetRenameData>& Renames)
    {
        if (Renames.Num() == 0)
        {
            return true;
        }

        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
        FScopedTransaction RenameTransaction(NSLOCTEXT("NsBonsaiReview", "RenameTransaction", "NsBonsai Rename Assets"));
        if (Manager)
        {
            Manager->SetApplyingRename(true);
        }
        const bool bRenameCallSucceeded = AssetToolsModule.Get().RenameAssets(Renames);
        if (Manager)
        {
            Manager->SetApplyingRename(false);
        }
        return bRenameCallSucceeded;
    }

    FReply OnExecuteRenamesClicked()
    {
        if (DeferredRenames.Num() == 0)
        {
            return FReply::Handled();
        }

        IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        TArray<FAssetRenameData> Renames;
        for (const FDeferredRenameEntry& Deferred : DeferredRenames)
        {
            if (Deferred.OldPath.IsValid() && Deferred.NewPath.IsValid() && Deferred.OldPath != Deferred.NewPath)
            {
                Renames.Emplace(Deferred.OldPath, Deferred.NewPath, false, true);
            }
        }

        RunRenameAssetsWithTransaction(Renames);

        bool bTouchedUserSettings = false;
        TArray<int32> SuccessfulIndices;
        for (int32 Index = 0; Index < DeferredRenames.Num(); ++Index)
        {
            const FDeferredRenameEntry& Deferred = DeferredRenames[Index];
            const bool bSucceeded = (Deferred.OldPath == Deferred.NewPath) || AssetRegistry.GetAssetByObjectPath(Deferred.NewPath, false).IsValid();
            AppendRenameLog(Deferred.OldPath.ToString(), Deferred.NewPath.ToString(), bSucceeded ? TEXT("Renamed") : TEXT("RenameFailed"));
            if (bSucceeded)
            {
                UserSettings->TouchDomain(Deferred.Domain);
                UserSettings->TouchCategory(Deferred.Category);
                bTouchedUserSettings = true;
                SuccessfulIndices.Add(Index);
            }
        }

        for (int32 RemoveIdx = SuccessfulIndices.Num() - 1; RemoveIdx >= 0; --RemoveIdx)
        {
            DeferredRenames.RemoveAt(SuccessfulIndices[RemoveIdx]);
        }

        if (bTouchedUserSettings)
        {
            UserSettings->Save();
        }
        RequestRefresh();
        return FReply::Handled();
    }

    FReply OnConfirmClicked(const TSharedPtr<FRowModel>& FocusRow)
    {
        const TArray<TSharedPtr<FRowModel>> Targets = GetSelectionOrSingle(FocusRow);
        TArray<TSharedPtr<FRowModel>> ActionableTargets;
        for (const TSharedPtr<FRowModel>& Row : Targets)
        {
            if (!Row.IsValid())
            {
                continue;
            }

            const FNsBonsaiValidationResult Validation = FNsBonsaiNameRules::ValidateRowState(Row->AssetData, ToRowState(*Row), *Settings);
            if (!Validation.bIsValid)
            {
                AppendRenameLog(Row->ObjectPath.ToString(), FString(), TEXT("BlockedMissingFields"));
                continue;
            }
            ActionableTargets.Add(Row);
        }

        if (ActionableTargets.Num() == 0)
        {
            RequestRefresh();
            return FReply::Handled();
        }

        TArray<FRenamePlan> Plans = BuildRenamePlans(ActionableTargets);
        if (Plans.Num() == 0)
        {
            RequestRefresh();
            return FReply::Handled();
        }

        if (bDryRunEnabled)
        {
            for (const FRenamePlan& Plan : Plans)
            {
                if (!Plan.Row.IsValid())
                {
                    continue;
                }

                if (Plan.bNeedsRename)
                {
                    const int32 ExistingIndex = DeferredRenames.IndexOfByPredicate([&Plan](const FDeferredRenameEntry& Entry)
                    {
                        return Entry.OldPath == Plan.OldPath;
                    });

                    FDeferredRenameEntry Deferred;
                    Deferred.OldPath = Plan.OldPath;
                    Deferred.NewPath = Plan.NewPath;
                    Deferred.Domain = Plan.Row->SelectedDomain;
                    Deferred.Category = Plan.Row->SelectedCategory;
                    if (ExistingIndex != INDEX_NONE)
                    {
                        DeferredRenames[ExistingIndex] = Deferred;
                    }
                    else
                    {
                        DeferredRenames.Add(Deferred);
                    }
                    AppendRenameLog(Plan.OldPath.ToString(), Plan.NewPath.ToString(), TEXT("DryRunQueued"));
                }
                else
                {
                    AppendRenameLog(Plan.OldPath.ToString(), Plan.NewPath.ToString(), TEXT("DryRunNoOp"));
                }

                UserSettings->TouchDomain(Plan.Row->SelectedDomain);
                UserSettings->TouchCategory(Plan.Row->SelectedCategory);
            }
            UserSettings->Save();

            TArray<TSharedPtr<FRowModel>> PlannedRows;
            for (const FRenamePlan& Plan : Plans)
            {
                if (Plan.Row.IsValid())
                {
                    PlannedRows.Add(Plan.Row);
                }
            }
            if (PlannedRows.Num() > 0)
            {
                ConfirmedCount += PlannedRows.Num();
                RemoveRows(PlannedRows);
            }
            if (GetRemainingCount() == 0 && Settings->bAutoCloseWindowWhenEmpty)
            {
                return CloseWindow();
            }
            return FReply::Handled();
        }

        TArray<FAssetRenameData> Renames;
        for (const FRenamePlan& Plan : Plans)
        {
            if (Plan.bNeedsRename)
            {
                Renames.Emplace(Plan.OldPath, Plan.NewPath, false, true);
            }
        }
        RunRenameAssetsWithTransaction(Renames);

        IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        TArray<TSharedPtr<FRowModel>> SuccessfulRows;
        bool bTouchedUserSettings = false;
        for (const FRenamePlan& Plan : Plans)
        {
            if (!Plan.Row.IsValid())
            {
                continue;
            }

            const bool bSucceeded = !Plan.bNeedsRename || AssetRegistry.GetAssetByObjectPath(Plan.NewPath, false).IsValid();
            if (bSucceeded)
            {
                if (Manager)
                {
                    Manager->MarkResolved(Plan.NewPath);
                }
                UserSettings->TouchDomain(Plan.Row->SelectedDomain);
                UserSettings->TouchCategory(Plan.Row->SelectedCategory);
                bTouchedUserSettings = true;
                SuccessfulRows.Add(Plan.Row);
                AppendRenameLog(Plan.OldPath.ToString(), Plan.NewPath.ToString(), Plan.bNeedsRename ? TEXT("Renamed") : TEXT("NoOp"));
            }
            else
            {
                Plan.Row->bRenameBlockedConflict = true;
                Plan.Row->RenameBlockedReason = TEXT("Rename failed or destination remained in conflict.");
                AppendRenameLog(Plan.OldPath.ToString(), Plan.NewPath.ToString(), TEXT("RenameFailed"));
            }
        }

        if (bTouchedUserSettings)
        {
            UserSettings->Save();
        }
        if (SuccessfulRows.Num() > 0)
        {
            ConfirmedCount += SuccessfulRows.Num();
            RemoveRows(SuccessfulRows);
        }
        if (GetRemainingCount() == 0 && Settings->bAutoCloseWindowWhenEmpty)
        {
            return CloseWindow();
        }
        RequestRefresh();
        return FReply::Handled();
    }
    FReply OnSnoozeClicked()
    {
        if (Manager)
        {
            Manager->SnoozeForMinutes(10.0);
        }
        bClosedBySnooze = true;
        return CloseWindow();
    }
    FReply OnCancelClicked()
    {
        return CloseWindow();
    }

    FReply CloseWindow()
    {
        if (ParentWindow.IsValid())
        {
            ParentWindow.Pin()->RequestDestroyWindow();
        }
        return FReply::Handled();
    }

    void RequestRefresh() const
    {
        if (ListView.IsValid())
        {
            ListView->RequestListRefresh();
        }
    }
public:
    bool WasClosedBySnooze() const
    {
        return bClosedBySnooze;
    }

    void CollectUnresolvedAssets(TArray<FAssetData>& OutAssets) const
    {
        for (const TSharedPtr<FRowModel>& Row : Rows)
        {
            if (Row.IsValid() && !Row->bIgnored)
            {
                OutAssets.Add(Row->AssetData);
            }
        }
    }

private:
    friend class SCompactRow;

    TWeakPtr<SWindow> ParentWindow;
    FNsBonsaiReviewManager* Manager = nullptr;
    const UNsBonsaiSettings* Settings = nullptr;
    UNsBonsaiUserSettings* UserSettings = nullptr;

    bool bEnableConflictPrecheck = true;
    bool bDryRunEnabled = false;
    bool bClosedBySnooze = false;
    int32 ConfirmedCount = 0;
    int32 IgnoredCount = 0;
    TArray<FDeferredRenameEntry> DeferredRenames;

    TArray<TSharedPtr<FRowModel>> Rows;
    TSet<FSoftObjectPath> RowPaths;
    TSharedPtr<SListView<TSharedPtr<FRowModel>>> ListView;

    TArray<TSharedPtr<FName>> DomainOptions;
    TMap<TSharedPtr<FRowModel>, TArray<TSharedPtr<FName>>> CategoryOptionsByRow;
};
void FNsBonsaiReviewManager::MarkResolved(const FSoftObjectPath& ObjectPath, bool bAllowRequeue)
{
    if (!ObjectPath.IsValid())
    {
        return;
    }

    if (bAllowRequeue)
    {
        QueuedPaths.Remove(ObjectPath);
    }
    else
    {
        QueuedPaths.Add(ObjectPath);
    }
}

void FNsBonsaiReviewManager::OpenReviewQueueNow()
{
    if (const TSharedPtr<SWindow> ReviewWindowPinned = ReviewWindow.Pin())
    {
        ReviewWindowPinned->BringToFront(true);
        return;
    }

    if (ReviewQueue.Num() == 0)
    {
        if (FSlateApplication::IsInitialized())
        {
            FNotificationInfo Info(NSLOCTEXT("NsBonsaiReview", "QueueEmptyToast", "NsBonsai review queue is empty"));
            Info.bFireAndForget = true;
            Info.bUseLargeFont = false;
            Info.bUseSuccessFailIcons = false;
            Info.ExpireDuration = 2.5f;
            FSlateNotificationManager::Get().AddNotification(Info);
        }
        return;
    }

    bSuppressAutoPopup = false;
    SuppressedQueueCount = 0;
    bPopupScheduled = false;
    PopupOpenAtTime = 0.0;
    OpenReviewPopup();
}

void FNsBonsaiReviewManager::SnoozeForMinutes(double Minutes)
{
    const double SnoozeSeconds = FMath::Max(0.0, Minutes) * 60.0;
    SnoozedUntilTime = FMath::Max(SnoozedUntilTime, FPlatformTime::Seconds() + SnoozeSeconds);

    if (ReviewQueue.Num() > 0)
    {
        bPopupScheduled = true;
        PopupOpenAtTime = SnoozedUntilTime;
    }
}

void FNsBonsaiReviewManager::Startup()
{
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    AssetAddedHandle = AssetRegistry.OnAssetAdded().AddRaw(this, &FNsBonsaiReviewManager::OnAssetAdded);
    AssetRemovedHandle = AssetRegistry.OnAssetRemoved().AddRaw(this, &FNsBonsaiReviewManager::OnAssetRemoved);
    AssetRenamedHandle = AssetRegistry.OnAssetRenamed().AddRaw(this, &FNsBonsaiReviewManager::OnAssetRenamed);
    PackageSavedHandle = UPackage::PackageSavedWithContextEvent.AddRaw(this, &FNsBonsaiReviewManager::OnPackageSaved);
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FNsBonsaiReviewManager::Tick), 0.2f);
}

void FNsBonsaiReviewManager::Shutdown()
{
    if (FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
    {
        IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
        AssetRegistry.OnAssetAdded().Remove(AssetAddedHandle);
        AssetRegistry.OnAssetRemoved().Remove(AssetRemovedHandle);
        AssetRegistry.OnAssetRenamed().Remove(AssetRenamedHandle);
    }
    UPackage::PackageSavedWithContextEvent.Remove(PackageSavedHandle);
    if (TickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    }

    ReviewWindow.Reset();
    ReviewWindowWidget.Reset();
    PendingByPackage.Reset();
    SavedPackagesToProcess.Reset();
    ReviewQueue.Reset();
    QueuedPaths.Reset();
    bPopupScheduled = false;
    PopupOpenAtTime = 0.0;
    SnoozedUntilTime = 0.0;
    NextAutoPopupAllowedTime = 0.0;
    NextToastAllowedTime = 0.0;
    bSuppressAutoPopup = false;
    SuppressedQueueCount = 0;
}

void FNsBonsaiReviewManager::TrackPending(const FSoftObjectPath& Path, FName PackageName)
{
    if (!Path.IsValid() || PackageName.IsNone())
    {
        return;
    }
    PendingByPackage.FindOrAdd(PackageName).Add(Path);
}

void FNsBonsaiReviewManager::OnAssetAdded(const FAssetData& AssetData)
{
    if (IsApplyingRename())
    {
        return;
    }
    if (ShouldSkipCompliantAssets())
    {
        if (IsAssetCompliant(AssetData))
        {
            return;
        }
    }

    TrackPending(AssetData.GetSoftObjectPath(), AssetData.PackageName);
}

void FNsBonsaiReviewManager::OnAssetRemoved(const FAssetData& AssetData)
{
    if (IsApplyingRename())
    {
        return;
    }

    const FSoftObjectPath Path = AssetData.GetSoftObjectPath();
    if (TSet<FSoftObjectPath>* PendingSet = PendingByPackage.Find(AssetData.PackageName))
    {
        PendingSet->Remove(Path);
        if (PendingSet->IsEmpty())
        {
            PendingByPackage.Remove(AssetData.PackageName);
        }
    }

    ReviewQueue.RemoveAll([&Path](const FAssetData& InAsset)
    {
        return InAsset.GetSoftObjectPath() == Path;
    });
}

void FNsBonsaiReviewManager::OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
{
    if (IsApplyingRename())
    {
        return;
    }

    const FSoftObjectPath OldPath(OldObjectPath);
    for (TPair<FName, TSet<FSoftObjectPath>>& Pair : PendingByPackage)
    {
        Pair.Value.Remove(OldPath);
    }

    if (ShouldSkipCompliantAssets())
    {
        if (IsAssetCompliant(AssetData))
        {
            return;
        }
    }

    TrackPending(AssetData.GetSoftObjectPath(), AssetData.PackageName);
}

void FNsBonsaiReviewManager::OnPackageSaved(const FString&, UPackage* Package, FObjectPostSaveContext)
{
    if (IsApplyingRename() || !Package)
    {
        return;
    }

    SavedPackagesToProcess.Add(Package->GetFName());
}

bool FNsBonsaiReviewManager::Tick(float)
{
    ProcessSavedPackages();

    if (!bPopupScheduled)
    {
        return true;
    }

    const double Now = FPlatformTime::Seconds();
    if (Now < PopupOpenAtTime)
    {
        return true;
    }

    if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetActiveModalWindow().IsValid())
    {
        PopupOpenAtTime = Now + 0.25;
        return true;
    }

    if (ReviewWindow.IsValid())
    {
        bPopupScheduled = false;
        AppendReviewQueueToOpenWindow();
        return true;
    }

    if (Now < SnoozedUntilTime)
    {
        PopupOpenAtTime = SnoozedUntilTime;
        return true;
    }

    if (ReviewQueue.Num() < GetMinAssetsToPopup())
    {
        bPopupScheduled = false;
        ShowQueuedToast();
        return true;
    }

    bPopupScheduled = false;
    OpenReviewPopup();
    return true;
}

void FNsBonsaiReviewManager::EnqueueSavedPackageAssets(FName PackageName)
{
    TSet<FSoftObjectPath>* PendingObjectPaths = PendingByPackage.Find(PackageName);
    if (!PendingObjectPaths || PendingObjectPaths->Num() == 0)
    {
        PendingByPackage.Remove(PackageName);
        SavedPackagesToProcess.Remove(PackageName);
        return;
    }

    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    const bool bSkipCompliantAssets = ShouldSkipCompliantAssets();

    TSet<FSoftObjectPath> ResolvedPaths;
    bool bQueuedAny = false;
    for (const FSoftObjectPath& Path : *PendingObjectPaths)
    {
        if (QueuedPaths.Contains(Path))
        {
            ResolvedPaths.Add(Path);
            continue;
        }

        const FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(Path, false, true);
        if (!AssetData.IsValid())
        {
            continue;
        }

        if (bSkipCompliantAssets && IsAssetCompliant(AssetData))
        {
            ResolvedPaths.Add(Path);
            continue;
        }

        QueuedPaths.Add(Path);
        ReviewQueue.Add(AssetData);
        ResolvedPaths.Add(Path);
        bQueuedAny = true;
    }

    for (const FSoftObjectPath& ResolvedPath : ResolvedPaths)
    {
        PendingObjectPaths->Remove(ResolvedPath);
    }

    if (PendingObjectPaths->Num() == 0)
    {
        PendingByPackage.Remove(PackageName);
        SavedPackagesToProcess.Remove(PackageName);
    }

    if (bQueuedAny)
    {
        RequestPopupDebounced();
    }
}

void FNsBonsaiReviewManager::ProcessSavedPackages()
{
    if (SavedPackagesToProcess.Num() == 0)
    {
        return;
    }

    const TArray<FName> PackageNames = SavedPackagesToProcess.Array();
    for (const FName PackageName : PackageNames)
    {
        EnqueueSavedPackageAssets(PackageName);
    }
}

void FNsBonsaiReviewManager::RequeueAssets(const TArray<FAssetData>& Assets)
{
    if (Assets.Num() == 0)
    {
        return;
    }

    TSet<FSoftObjectPath> ExistingPaths;
    for (const FAssetData& Existing : ReviewQueue)
    {
        const FSoftObjectPath ExistingPath = Existing.GetSoftObjectPath();
        if (ExistingPath.IsValid())
        {
            ExistingPaths.Add(ExistingPath);
        }
    }

    for (const FAssetData& Asset : Assets)
    {
        const FSoftObjectPath ObjectPath = Asset.GetSoftObjectPath();
        if (!ObjectPath.IsValid() || ExistingPaths.Contains(ObjectPath))
        {
            continue;
        }

        QueuedPaths.Add(ObjectPath);
        ReviewQueue.Add(Asset);
        ExistingPaths.Add(ObjectPath);
    }
}

void FNsBonsaiReviewManager::RequestPopupDebounced()
{
    if (ReviewQueue.Num() == 0)
    {
        return;
    }

    if (bSuppressAutoPopup)
    {
        if (ReviewQueue.Num() <= SuppressedQueueCount)
        {
            return;
        }
        bSuppressAutoPopup = false;
        SuppressedQueueCount = 0;
    }

    const double Now = FPlatformTime::Seconds();
    if (ReviewWindow.IsValid())
    {
        bPopupScheduled = true;
        PopupOpenAtTime = Now + 0.2;
        return;
    }

    if (ReviewQueue.Num() < GetMinAssetsToPopup())
    {
        ShowQueuedToast();
        return;
    }

    bPopupScheduled = true;
    PopupOpenAtTime = Now + 0.35;
    PopupOpenAtTime = FMath::Max(PopupOpenAtTime, SnoozedUntilTime);
    PopupOpenAtTime = FMath::Max(PopupOpenAtTime, NextAutoPopupAllowedTime);
}

void FNsBonsaiReviewManager::ShowQueuedToast()
{
    if (!FSlateApplication::IsInitialized())
    {
        return;
    }

    const double Now = FPlatformTime::Seconds();
    if (Now < NextToastAllowedTime)
    {
        return;
    }
    NextToastAllowedTime = Now + FMath::Max(1.0, GetPopupCooldownSeconds());

    const int32 AssetCount = ReviewQueue.Num();
    FNotificationInfo Info(FText::Format(
        NSLOCTEXT("NsBonsaiReview", "QueuedToast", "NsBonsai queued {0} assets"),
        FText::AsNumber(AssetCount)));
    Info.bFireAndForget = true;
    Info.bUseLargeFont = false;
    Info.bUseSuccessFailIcons = false;
    Info.ExpireDuration = 4.0f;
    Info.ButtonDetails.Add(FNotificationButtonInfo(
        NSLOCTEXT("NsBonsaiReview", "QueuedToastOpen", "Open"),
        NSLOCTEXT("NsBonsaiReview", "QueuedToastOpenTooltip", "Open the NsBonsai review queue."),
        FSimpleDelegate::CreateRaw(this, &FNsBonsaiReviewManager::OpenReviewQueueNow),
        SNotificationItem::CS_None));

    FSlateNotificationManager::Get().AddNotification(Info);
}

int32 FNsBonsaiReviewManager::GetMinAssetsToPopup() const
{
    const UNsBonsaiSettings* Settings = GetDefault<UNsBonsaiSettings>();
    return FMath::Max(1, Settings ? Settings->PopupThresholdCount : 1);
}

double FNsBonsaiReviewManager::GetPopupCooldownSeconds() const
{
    const UNsBonsaiSettings* Settings = GetDefault<UNsBonsaiSettings>();
    return FMath::Max(0.0, static_cast<double>(Settings ? Settings->PopupCooldownSeconds : 2.0f));
}
void FNsBonsaiReviewManager::AppendReviewQueueToOpenWindow()
{
    if (ReviewQueue.Num() == 0)
    {
        return;
    }

    const TSharedPtr<SNsBonsaiReviewWindow> ReviewWidgetPinned = ReviewWindowWidget.Pin();
    if (!ReviewWidgetPinned.IsValid())
    {
        return;
    }

    const TArray<FAssetData> AssetsToAppend = MoveTemp(ReviewQueue);
    ReviewQueue.Reset();
    ReviewWidgetPinned->AddAssets(AssetsToAppend);

    if (const TSharedPtr<SWindow> ReviewWindowPinned = ReviewWindow.Pin())
    {
        ReviewWindowPinned->BringToFront(true);
    }
}

void FNsBonsaiReviewManager::OpenReviewPopup()
{
    if (ReviewQueue.Num() == 0 || !FSlateApplication::IsInitialized())
    {
        return;
    }

    if (ReviewWindow.IsValid())
    {
        AppendReviewQueueToOpenWindow();
        return;
    }

    bSuppressAutoPopup = false;
    SuppressedQueueCount = 0;

    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(FText::FromString(TEXT("NsBonsai Asset Review")))
        .ClientSize(FVector2D(1200, 420))
        .SupportsMinimize(true)
        .SupportsMaximize(true);

    TSharedRef<SNsBonsaiReviewWindow> ReviewWidget =
        SNew(SNsBonsaiReviewWindow)
        .Assets(ReviewQueue)
        .Manager(this)
        .ParentWindow(Window);

    ReviewQueue.Reset();
    TWeakPtr<SNsBonsaiReviewWindow> ClosedWidgetRef = ReviewWidget;

    Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda([this, ClosedWidgetRef](const TSharedRef<SWindow>&)
    {
        bool bClosedBySnooze = false;
        TArray<FAssetData> UnresolvedAssets;
        if (const TSharedPtr<SNsBonsaiReviewWindow> ClosedWidget = ClosedWidgetRef.Pin())
        {
            bClosedBySnooze = ClosedWidget->WasClosedBySnooze();
            ClosedWidget->CollectUnresolvedAssets(UnresolvedAssets);
        }

        RequeueAssets(UnresolvedAssets);

        ReviewWindow.Reset();
        ReviewWindowWidget.Reset();
        bPopupScheduled = false;
        PopupOpenAtTime = 0.0;
        NextAutoPopupAllowedTime = FPlatformTime::Seconds() + GetPopupCooldownSeconds();

        if (bClosedBySnooze && ReviewQueue.Num() > 0)
        {
            bSuppressAutoPopup = false;
            SuppressedQueueCount = 0;
            RequestPopupDebounced();
        }
        else
        {
            bSuppressAutoPopup = ReviewQueue.Num() > 0;
            SuppressedQueueCount = ReviewQueue.Num();
        }
    }));

    Window->SetContent(ReviewWidget);
    ReviewWindow = Window;
    ReviewWindowWidget = ReviewWidget;
    FSlateApplication::Get().AddWindow(Window);
}
bool FNsBonsaiReviewManager::ShouldSkipCompliantAssets() const
{
    const UNsBonsaiSettings* Settings = GetDefault<UNsBonsaiSettings>();
    return Settings ? Settings->bSkipCompliantAssets : true;
}

bool FNsBonsaiReviewManager::IsAssetCompliant(const FAssetData& AssetData) const
{
    const UNsBonsaiSettings* Settings = GetDefault<UNsBonsaiSettings>();
    return Settings ? FNsBonsaiNameRules::IsCompliant(AssetData, *Settings) : false;
}
