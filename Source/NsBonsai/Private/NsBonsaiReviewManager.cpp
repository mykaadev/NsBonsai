#include "NsBonsaiReviewManager.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IAssetTools.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "NsBonsaiAssetEvaluator.h"
#include "NsBonsaiNameBuilder.h"
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
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Notifications/SNotificationList.h"

namespace NsBonsaiReview
{
	static FString VariantFromIndex(int32 Index)
	{
		// A..Z, AA..AZ, BA..BZ, ...
		FString Out;
		int32 N = Index;
		do
		{
			const int32 Rem = N % 26;
			Out.InsertAt(0, TCHAR('A' + Rem));
			N = (N / 26) - 1;
		} while (N >= 0);
		return Out;
	}

	static FString MakeObjectPathString(const FString& PackagePath, const FString& AssetName)
	{
		return FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
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
		FString Normalized = InToken;
		Normalized.TrimStartAndEndInline();
		Normalized.ToUpperInline();
		return Normalized;
	}

	static bool IsLikelyVariantToken(const FString& Token)
	{
		if (!FNsBonsaiNameBuilder::IsVariantToken(Token))
		{
			return false;
		}

		for (const TCHAR Character : Token)
		{
			if (!FChar::IsUpper(Character))
			{
				return false;
			}
		}
		return true;
	}

	static FString CollapseUnderscores(const FString& InValue)
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

	static FString SanitizeAssetNameCandidate(const FString& InAssetName)
	{
		FString Working = InAssetName;
		Working.TrimStartAndEndInline();
		Working.ReplaceInline(TEXT(" "), TEXT("_"));
		Working = ObjectTools::SanitizeObjectName(Working);
		return CollapseUnderscores(Working);
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

	static void BuildKnownTokenSets(const UNsBonsaiSettings& Settings, TSet<FString>& OutTypes, TSet<FString>& OutDomains, TSet<FString>& OutCategories)
	{
		OutTypes.Reset();
		OutDomains.Reset();
		OutCategories.Reset();

		for (const FNsBonsaiTypeRule& Rule : Settings.TypeRules)
		{
			const FName TypeToken = Settings.NormalizeToken(Rule.TypeToken);
			if (!TypeToken.IsNone())
			{
				OutTypes.Add(NormalizeTokenForCompare(TypeToken.ToString()));
			}
		}

		for (const FNsBonsaiDomainDef& DomainDef : Settings.Domains)
		{
			const FName Domain = Settings.NormalizeToken(DomainDef.DomainToken);
			if (!Domain.IsNone())
			{
				OutDomains.Add(NormalizeTokenForCompare(Domain.ToString()));
			}
			for (const FName Category : DomainDef.Categories)
			{
				const FName NormalizedCategory = Settings.NormalizeToken(Category);
				if (!NormalizedCategory.IsNone())
				{
					OutCategories.Add(NormalizeTokenForCompare(NormalizedCategory.ToString()));
				}
			}
		}
	}
	static TMap<FName, TSet<FName>> BuildCategoriesByDomain(const UNsBonsaiSettings& Settings)
	{
		TMap<FName, TSet<FName>> CategoriesByDomain;
		for (const FNsBonsaiDomainDef& DomainDef : Settings.Domains)
		{
			const FName DomainToken = Settings.NormalizeToken(DomainDef.DomainToken);
			if (DomainToken.IsNone())
			{
				continue;
			}

			TSet<FName>& Categories = CategoriesByDomain.FindOrAdd(DomainToken);
			for (const FName Category : DomainDef.Categories)
			{
				const FName CategoryToken = Settings.NormalizeToken(Category);
				if (!CategoryToken.IsNone())
				{
					Categories.Add(CategoryToken);
				}
			}
		}
		return CategoriesByDomain;
	}

	static bool IsCompliant(const FAssetData& AssetData, const UNsBonsaiSettings& Settings)
	{
		const FString Separator = Settings.JoinSeparator.IsEmpty() ? TEXT("_") : Settings.JoinSeparator;
		TArray<FString> Parts;
		AssetData.AssetName.ToString().ParseIntoArray(Parts, *Separator, true);
		if (Parts.Num() < 5)
		{
			return false;
		}

		const FName ExpectedType = Settings.NormalizeToken(Settings.ResolveTypeTokenForClassPath(AssetData.AssetClassPath));
		if (ExpectedType.IsNone())
		{
			return false;
		}

		const FName TypeToken = Settings.NormalizeToken(FName(*Parts[0]));
		if (TypeToken != ExpectedType)
		{
			return false;
		}

		const FName DomainToken = Settings.NormalizeToken(FName(*Parts[1]));
		if (DomainToken.IsNone())
		{
			return false;
		}

		const TMap<FName, TSet<FName>> CategoriesByDomain = BuildCategoriesByDomain(Settings);
		const TSet<FName>* DomainCategories = CategoriesByDomain.Find(DomainToken);
		if (!DomainCategories)
		{
			return false;
		}

		const FName CategoryToken = Settings.NormalizeToken(FName(*Parts[2]));
		if (CategoryToken.IsNone() || !DomainCategories->Contains(CategoryToken))
		{
			return false;
		}

		if (!IsLikelyVariantToken(Parts.Last()))
		{
			return false;
		}

		TArray<FString> AssetNameParts;
		for (int32 Index = 3; Index < Parts.Num() - 1; ++Index)
		{
			AssetNameParts.Add(Parts[Index]);
		}

		const FString AssetNameToken = FString::Join(AssetNameParts, *Separator);
		return !SanitizeAssetNameCandidate(AssetNameToken).IsEmpty();
	}

	static bool ShouldSkipCompliantAssets()
	{
		const UNsBonsaiUserSettings* UserSettings = GetDefault<UNsBonsaiUserSettings>();
		return UserSettings ? UserSettings->bSkipCompliantAssets : true;
	}

	static FString BuildSmartPrefillAssetName(const FString& OriginalAssetName, const TSet<FString>& KnownTypes, const TSet<FString>& KnownDomains, const TSet<FString>& KnownCategories)
	{
		TArray<FString> Tokens;
		OriginalAssetName.ParseIntoArray(Tokens, TEXT("_"), true);
		if (Tokens.Num() == 0)
		{
			const FString Fallback = SanitizeAssetNameCandidate(OriginalAssetName);
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

		if (Remaining.Num() > 0 && IsLikelyVariantToken(Remaining.Last()))
		{
			Remaining.Pop();
		}

		const FString Candidate = SanitizeAssetNameCandidate(FString::Join(Remaining, TEXT("_")));
		if (!Candidate.IsEmpty())
		{
			return Candidate;
		}

		const FString OriginalFallback = SanitizeAssetNameCandidate(OriginalAssetName);
		return OriginalFallback.IsEmpty() ? TEXT("New") : OriginalFallback;
	}

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

	static FString ResolveAssetNameForBuild(const FRowModel& Row)
	{
		FString SanitizedAssetName = SanitizeAssetNameCandidate(Row.AssetName);
		if (SanitizedAssetName.IsEmpty())
		{
			SanitizedAssetName = SanitizeAssetNameCandidate(Row.SmartPrefillName);
		}
		if (SanitizedAssetName.IsEmpty())
		{
			SanitizedAssetName = SanitizeAssetNameCandidate(Row.ParsedBaseName);
		}
		if (SanitizedAssetName.IsEmpty())
		{
			SanitizedAssetName = SanitizeAssetNameCandidate(Row.CurrentName);
		}
		return SanitizedAssetName.IsEmpty() ? TEXT("New") : SanitizedAssetName;
	}

	static FString BuildReviewBaseName(const UNsBonsaiSettings& Settings, const FName TypeToken, const FName DomainToken, const FName CategoryToken, const FString& AssetName)
	{
		const FName NormalizedType = Settings.NormalizeToken(TypeToken);
		const FName NormalizedDomain = Settings.NormalizeToken(DomainToken);
		const FName NormalizedCategory = Settings.NormalizeToken(CategoryToken);
		if (NormalizedType.IsNone() || NormalizedDomain.IsNone() || NormalizedCategory.IsNone() || AssetName.IsEmpty())
		{
			return FString();
		}

		return FString::Printf(TEXT("%s_%s_%s_%s"),
			*NormalizedType.ToString(),
			*NormalizedDomain.ToString(),
			*NormalizedCategory.ToString(),
			*AssetName);
	}

	static FString BuildReviewFinalNameFromBase(const FString& BaseWithoutVariant, const FString& VariantToken)
	{
		if (BaseWithoutVariant.IsEmpty())
		{
			return FString();
		}

		return BaseWithoutVariant + TEXT("_") + FNsBonsaiNameBuilder::SanitizeVariantToken(VariantToken);
	}

	static void RebuildPreview(FRowModel& Row)
	{
		const UNsBonsaiSettings* Settings = GetDefault<UNsBonsaiSettings>();
		const FName TypeToken = Settings->ResolveTypeTokenForClassPath(Row.AssetData.AssetClassPath);
		const FString SanitizedAssetName = ResolveAssetNameForBuild(Row);
		const FString BaseWithoutVariant = BuildReviewBaseName(*Settings, TypeToken, Row.SelectedDomain, Row.SelectedCategory, SanitizedAssetName);
		Row.PreviewName = BuildReviewFinalNameFromBase(BaseWithoutVariant, TEXT("A"));
	}
}

class SNsBonsaiReviewWindow final : public SCompoundWidget
{
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
		NsBonsaiReview::BuildKnownTokenSets(*Settings, KnownTypeTokensUpper, KnownDomainTokensUpper, KnownCategoryTokensUpper);

		// Build global domain options once.
		{
			TArray<FName> Domains;
			TSet<FName> Seen;
			for (const FNsBonsaiDomainDef& DomainDef : Settings->Domains)
			{
				const FName Domain = Settings->NormalizeToken(DomainDef.DomainToken);
				if (!Domain.IsNone() && !Seen.Contains(Domain))
				{
					Seen.Add(Domain);
					Domains.Add(Domain);
				}
			}

			Domains.Sort([this](const FName& L, const FName& R)
			{
				const int32 Li = UserSettings->RecentDomains.IndexOfByKey(L);
				const int32 Ri = UserSettings->RecentDomains.IndexOfByKey(R);
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

			for (const FName Domain : Domains)
			{
				DomainOptions.Add(MakeShared<FName>(Domain));
			}
		}

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(8)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Review newly added assets - edit Domain/Category and Asset Name, then confirm (check) or ignore (x).")))
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
			TSharedPtr<NsBonsaiReview::FRowModel> Row = MakeShared<NsBonsaiReview::FRowModel>();
			Row->AssetData = Asset;
			Row->ObjectPath = ObjectPath;
			Row->CurrentName = Asset.AssetName.ToString();
			Row->ParsedBaseName = Eval.ExistingAssetName;
			Row->SmartPrefillName = NsBonsaiReview::BuildSmartPrefillAssetName(Row->CurrentName, KnownTypeTokensUpper, KnownDomainTokensUpper, KnownCategoryTokensUpper);
			Row->SelectedDomain = Eval.PreselectedDomain;
			Row->SelectedCategory = Eval.PreselectedCategory;
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
		TSharedPtr<NsBonsaiReview::FRowModel> Row;
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

	TSharedRef<SWidget> BuildCompactTable()
	{
		return SNew(SBorder)
		.Padding(4)
		[
			SAssignNew(ListView, SListView<TSharedPtr<NsBonsaiReview::FRowModel>>)
			.ListItemsSource(&Rows)
			.SelectionMode(ESelectionMode::Multi)
			.OnGenerateRow(this, &SNsBonsaiReviewWindow::OnGenerateCompactRow)
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column(TEXT("Status")).DefaultLabel(FText::FromString(TEXT(""))).FixedWidth(28)
				+ SHeaderRow::Column(TEXT("Asset")).DefaultLabel(FText::FromString(TEXT("Asset"))).FillWidth(0.22f)
				+ SHeaderRow::Column(TEXT("Type")).DefaultLabel(FText::FromString(TEXT("Type"))).FillWidth(0.07f)
				+ SHeaderRow::Column(TEXT("Domain")).DefaultLabel(FText::FromString(TEXT("Domain"))).FillWidth(0.14f)
				+ SHeaderRow::Column(TEXT("Category")).DefaultLabel(FText::FromString(TEXT("Category"))).FillWidth(0.14f)
				+ SHeaderRow::Column(TEXT("AssetName")).DefaultLabel(FText::FromString(TEXT("Asset Name"))).FillWidth(0.20f)
				+ SHeaderRow::Column(TEXT("Final")).DefaultLabel(FText::FromString(TEXT("Final Name"))).FillWidth(0.18f)
				+ SHeaderRow::Column(TEXT("Ok")).DefaultLabel(FText::FromString(TEXT(""))).FixedWidth(28)
				+ SHeaderRow::Column(TEXT("No")).DefaultLabel(FText::FromString(TEXT(""))).FixedWidth(28)
			)
		];
	}

	class SCompactRow final : public SMultiColumnTableRow<TSharedPtr<NsBonsaiReview::FRowModel>>
	{
	public:
		SLATE_BEGIN_ARGS(SCompactRow) {}
			SLATE_ARGUMENT(TSharedPtr<NsBonsaiReview::FRowModel>, Row)
			SLATE_ARGUMENT(SNsBonsaiReviewWindow*, Owner)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
		{
			Row = InArgs._Row;
			Owner = InArgs._Owner;
			SMultiColumnTableRow<TSharedPtr<NsBonsaiReview::FRowModel>>::Construct(
				FSuperRowType::FArguments().Padding(FMargin(2, 1)),
				InOwnerTableView
			);
			SetBorderBackgroundColor(TAttribute<FSlateColor>::CreateLambda([this]
			{
				if (this->IsSelected())
				{
					// Keep default UE row selection visuals for selected rows.
					return FSlateColor(FLinearColor::White);
				}
				return Owner ? Owner->GetRowBackgroundColor(Row) : FSlateColor(FLinearColor::White);
			}));
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
				const FName TypeToken = Owner->Settings->ResolveTypeTokenForClassPath(Row->AssetData.AssetClassPath);
				return SNew(STextBlock).Text(FText::FromName(TypeToken));
			}
			if (ColumnName == TEXT("Domain"))
			{
				return SNew(SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&Owner->DomainOptions)
					.OnGenerateWidget_Lambda([](TSharedPtr<FName> Name)
					{
						return SNew(STextBlock).Text(Name.IsValid() ? FText::FromName(*Name) : FText::GetEmpty());
					})
					.OnSelectionChanged_Lambda([this](TSharedPtr<FName> NewSel, ESelectInfo::Type)
					{
						if (NewSel.IsValid())
						{
							Owner->ApplyDomain(Row, *NewSel);
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
					.IsEnabled_Lambda([this]{ return !Row->SelectedDomain.IsNone(); })
					.OnGenerateWidget_Lambda([](TSharedPtr<FName> Name)
					{
						return SNew(STextBlock).Text(Name.IsValid() ? FText::FromName(*Name) : FText::GetEmpty());
					})
					.OnSelectionChanged_Lambda([this](TSharedPtr<FName> NewSel, ESelectInfo::Type)
					{
						if (NewSel.IsValid())
						{
							Owner->ApplyCategory(Row, *NewSel);
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
					.ToolTipText(FText::FromString(TEXT("Confirm + Rename")))
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
					.ToolTipText(FText::FromString(TEXT("Ignore")))
					.OnClicked_Lambda([this]{ return Owner->OnIgnoreClicked(Row); })
					[
						SNew(SImage).Image(FAppStyle::Get().GetBrush("Icons.X"))
					];
			}

			return SNew(STextBlock);
		}

	private:
		TSharedPtr<NsBonsaiReview::FRowModel> Row;
		SNsBonsaiReviewWindow* Owner = nullptr;
	};

	TSharedRef<ITableRow> OnGenerateCompactRow(TSharedPtr<NsBonsaiReview::FRowModel> Row, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return SNew(SCompactRow, OwnerTable)
			.Row(Row)
			.Owner(this);
	}

	TArray<TSharedPtr<NsBonsaiReview::FRowModel>> GetSelectionOrSingle(const TSharedPtr<NsBonsaiReview::FRowModel>& FocusRow)
	{
		TArray<TSharedPtr<NsBonsaiReview::FRowModel>> Selection;
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

	TArray<TSharedPtr<NsBonsaiReview::FRowModel>> GetDomainOrCategoryTargets(const TSharedPtr<NsBonsaiReview::FRowModel>& FocusRow)
	{
		TArray<TSharedPtr<NsBonsaiReview::FRowModel>> Selection;
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

	void ClearRowConflictFlags(const TSharedPtr<NsBonsaiReview::FRowModel>& Row)
	{
		if (!Row.IsValid())
		{
			return;
		}
		Row->bRenameBlockedConflict = false;
		Row->RenameBlockedReason.Reset();
	}

	FRowStatusInfo EvaluateRowStatus(const TSharedPtr<NsBonsaiReview::FRowModel>& Row) const
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

		TArray<FString> Missing;
		if (Row->SelectedDomain.IsNone())
		{
			Missing.Add(TEXT("Domain is required."));
		}
		if (Row->SelectedCategory.IsNone())
		{
			Missing.Add(TEXT("Category is required."));
		}
		if (NsBonsaiReview::SanitizeAssetNameCandidate(Row->AssetName).IsEmpty())
		{
			Missing.Add(TEXT("Asset Name is empty after sanitize."));
		}
		if (Missing.Num() > 0)
		{
			Info.Status = ERowStatus::Warning;
			Info.Reason = FString::Join(Missing, TEXT(" "));
			return Info;
		}

		if (bEnableConflictPrecheck)
		{
			const FName TypeToken = Settings->ResolveTypeTokenForClassPath(Row->AssetData.AssetClassPath);
			const FString BaseWithoutVariant = NsBonsaiReview::BuildReviewBaseName(*Settings, TypeToken, Row->SelectedDomain, Row->SelectedCategory, NsBonsaiReview::ResolveAssetNameForBuild(*Row));
			if (!BaseWithoutVariant.IsEmpty())
			{
				const FString CandidateName = NsBonsaiReview::BuildReviewFinalNameFromBase(BaseWithoutVariant, TEXT("A"));
				const FSoftObjectPath CandidatePath(NsBonsaiReview::MakeObjectPathString(Row->AssetData.PackagePath.ToString(), CandidateName));
				if (CandidatePath != Row->ObjectPath)
				{
					IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
					const FAssetData Existing = AssetRegistry.GetAssetByObjectPath(CandidatePath, false);
					if (Existing.IsValid())
					{
						Info.Status = ERowStatus::Conflict;
						Info.Reason = FString::Printf(TEXT("Conflict: '%s' already exists."), *CandidateName);
						return Info;
					}
				}
			}
		}

		Info.Status = ERowStatus::Ready;
		Info.Reason = TEXT("Ready to rename.");
		return Info;
	}

	FText GetStatusGlyph(const TSharedPtr<NsBonsaiReview::FRowModel>& Row) const
	{
		switch (EvaluateRowStatus(Row).Status)
		{
		case ERowStatus::Ready: return FText::FromString(TEXT("\u2705"));
		case ERowStatus::Warning: return FText::FromString(TEXT("\u26A0"));
		case ERowStatus::Conflict: return FText::FromString(TEXT("\u26D4"));
		case ERowStatus::Ignored: return FText::FromString(TEXT("�"));
		default: return FText::GetEmpty();
		}
	}

	FSlateColor GetStatusColor(const TSharedPtr<NsBonsaiReview::FRowModel>& Row) const
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

	FText GetStatusTooltip(const TSharedPtr<NsBonsaiReview::FRowModel>& Row) const
	{
		return FText::FromString(EvaluateRowStatus(Row).Reason);
	}

	FText BuildFinalNameTooltip(const TSharedPtr<NsBonsaiReview::FRowModel>& Row) const
	{
		if (!Row.IsValid())
		{
			return FText::GetEmpty();
		}

		const FName TypeToken = Settings->ResolveTypeTokenForClassPath(Row->AssetData.AssetClassPath);
		const FString SanitizedAssetName = NsBonsaiReview::ResolveAssetNameForBuild(*Row);
		const FString TypeText = Settings->NormalizeToken(TypeToken).ToString();
		const FString DomainText = Settings->NormalizeToken(Row->SelectedDomain).ToString();
		const FString CategoryText = Settings->NormalizeToken(Row->SelectedCategory).ToString();
		return FText::FromString(FString::Printf(TEXT("Current: %s\nNew: %s\nType: %s | Domain: %s | Category: %s | Asset: %s"),
			*Row->CurrentName,
			*Row->PreviewName,
			*TypeText,
			*DomainText,
			*CategoryText,
			*SanitizedAssetName));
	}

	FSlateColor GetRowBackgroundColor(const TSharedPtr<NsBonsaiReview::FRowModel>& Row) const
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
		for (const TSharedPtr<NsBonsaiReview::FRowModel>& Row : Rows)
		{
			if (Row.IsValid() && !Row->bIgnored)
			{
				++Count;
			}
		}
		return Count;
	}

	void EnsureCategoryOptions(const TSharedPtr<NsBonsaiReview::FRowModel>& Row)
	{
		if (!Row.IsValid())
		{
			return;
		}

		TArray<TSharedPtr<FName>>& Options = CategoryOptionsByRow.FindOrAdd(Row);
		Options.Reset();
		if (Row->SelectedDomain.IsNone())
		{
			return;
		}

		TArray<FName> Cats;
		for (const FNsBonsaiDomainDef& DomainDef : Settings->Domains)
		{
			if (Settings->NormalizeToken(DomainDef.DomainToken) != Row->SelectedDomain)
			{
				continue;
			}
			for (const FName Category : DomainDef.Categories)
			{
				const FName NormalizedCategory = Settings->NormalizeToken(Category);
				if (!NormalizedCategory.IsNone() && !Cats.Contains(NormalizedCategory))
				{
					Cats.Add(NormalizedCategory);
				}
			}
		}

		Cats.Sort([this](const FName& L, const FName& R)
		{
			const int32 Li = UserSettings->RecentCategories.IndexOfByKey(L);
			const int32 Ri = UserSettings->RecentCategories.IndexOfByKey(R);
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

		for (const FName Category : Cats)
		{
			Options.Add(MakeShared<FName>(Category));
		}
	}

	void ApplyDomain(const TSharedPtr<NsBonsaiReview::FRowModel>& FocusRow, const FName Domain)
	{
		const TArray<TSharedPtr<NsBonsaiReview::FRowModel>> Targets = GetDomainOrCategoryTargets(FocusRow);
		for (const TSharedPtr<NsBonsaiReview::FRowModel>& Row : Targets)
		{
			if (!Row.IsValid())
			{
				continue;
			}

			ClearRowConflictFlags(Row);
			Row->SelectedDomain = Domain;
			Row->SelectedCategory = NAME_None;
			EnsureCategoryOptions(Row);
			if (const TArray<TSharedPtr<FName>>* Opts = CategoryOptionsByRow.Find(Row))
			{
				if (Opts->Num() > 0)
				{
					Row->SelectedCategory = *(*Opts)[0];
				}
			}
			NsBonsaiReview::RebuildPreview(*Row);
		}
		RequestRefresh();
	}

	void ApplyCategory(const TSharedPtr<NsBonsaiReview::FRowModel>& FocusRow, const FName Category)
	{
		const TArray<TSharedPtr<NsBonsaiReview::FRowModel>> Targets = GetDomainOrCategoryTargets(FocusRow);
		for (const TSharedPtr<NsBonsaiReview::FRowModel>& Row : Targets)
		{
			if (!Row.IsValid())
			{
				continue;
			}
			ClearRowConflictFlags(Row);
			Row->SelectedCategory = Category;
			NsBonsaiReview::RebuildPreview(*Row);
		}
		RequestRefresh();
	}

	void SetRowAssetName(const TSharedPtr<NsBonsaiReview::FRowModel>& Row, const FString& ProposedName, bool bRequestRefresh)
	{
		if (!Row.IsValid())
		{
			return;
		}

		FString Sanitized = NsBonsaiReview::SanitizeAssetNameCandidate(ProposedName);
		if (Sanitized.IsEmpty())
		{
			Sanitized = NsBonsaiReview::SanitizeAssetNameCandidate(Row->SmartPrefillName);
		}
		if (Sanitized.IsEmpty())
		{
			Sanitized = NsBonsaiReview::SanitizeAssetNameCandidate(Row->ParsedBaseName);
		}
		if (Sanitized.IsEmpty())
		{
			Sanitized = NsBonsaiReview::SanitizeAssetNameCandidate(Row->CurrentName);
		}
		if (Sanitized.IsEmpty())
		{
			Sanitized = TEXT("New");
		}

		ClearRowConflictFlags(Row);
		Row->AssetName = Sanitized;
		NsBonsaiReview::RebuildPreview(*Row);
		if (bRequestRefresh)
		{
			RequestRefresh();
		}
	}

	void ApplyAssetName(const TSharedPtr<NsBonsaiReview::FRowModel>& Row, const FString& NewName)
	{
		SetRowAssetName(Row, NewName, true);
	}

	TSharedPtr<SWidget> BuildAssetNameContextMenu(const TSharedPtr<NsBonsaiReview::FRowModel>& Row)
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
				SetRowAssetName(Row, NsBonsaiReview::UppercaseCommonAcronyms(Row->AssetName), true);
			})));

		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("To PascalCase")),
			FText::FromString(TEXT("Convert current name to PascalCase.")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, Row]()
			{
				SetRowAssetName(Row, NsBonsaiReview::ToPascalCase(Row->AssetName), true);
			})));

		MenuBuilder.AddMenuEntry(
			FText::FromString(TEXT("To snake_case")),
			FText::FromString(TEXT("Convert current name to snake_case.")),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, Row]()
			{
				SetRowAssetName(Row, NsBonsaiReview::ToSnakeCase(Row->AssetName), true);
			})));
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	void RemoveRows(const TArray<TSharedPtr<NsBonsaiReview::FRowModel>>& Targets)
	{
		for (const TSharedPtr<NsBonsaiReview::FRowModel>& Row : Targets)
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

	FReply OnIgnoreClicked(const TSharedPtr<NsBonsaiReview::FRowModel>& FocusRow)
	{
		const TArray<TSharedPtr<NsBonsaiReview::FRowModel>> Targets = GetSelectionOrSingle(FocusRow);
		for (const TSharedPtr<NsBonsaiReview::FRowModel>& Row : Targets)
		{
			if (!Row.IsValid() || Row->bIgnored)
			{
				continue;
			}
			Row->bIgnored = true;
			++IgnoredCount;
			if (Manager)
			{
				Manager->MarkResolved(Row->ObjectPath);
			}
		}
		if (ListView.IsValid())
		{
			ListView->ClearSelection();
		}
		RequestRefresh();
		return FReply::Handled();
	}

	TArray<FRenamePlan> BuildRenamePlans(const TArray<TSharedPtr<NsBonsaiReview::FRowModel>>& SourceRows)
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
		for (const TSharedPtr<NsBonsaiReview::FRowModel>& Row : SourceRows)
		{
			if (!Row.IsValid())
			{
				continue;
			}

			ClearRowConflictFlags(Row);
			const FString PackagePath = Row->AssetData.PackagePath.ToString();
			const FSoftObjectPath OldPath = Row->ObjectPath;
			const FName TypeToken = Settings->ResolveTypeTokenForClassPath(Row->AssetData.AssetClassPath);
			const FString SanitizedAssetName = NsBonsaiReview::ResolveAssetNameForBuild(*Row);
			const FString BaseWithoutVariant = NsBonsaiReview::BuildReviewBaseName(*Settings, TypeToken, Row->SelectedDomain, Row->SelectedCategory, SanitizedAssetName);
			if (BaseWithoutVariant.IsEmpty())
			{
				Row->bRenameBlockedConflict = true;
				Row->RenameBlockedReason = TEXT("Unable to build rename base from current tokens.");
				NsBonsaiReview::AppendRenameLog(OldPath.ToString(), FString(), TEXT("ConflictInvalidBase"));
				continue;
			}

			FString FinalName;
			FString NewObjectPathString;
			bool bFoundAvailableVariant = false;
			for (int32 VariantIndex = 0; VariantIndex < 26 * 26; ++VariantIndex)
			{
				const FString Variant = NsBonsaiReview::VariantFromIndex(VariantIndex);
				FinalName = NsBonsaiReview::BuildReviewFinalNameFromBase(BaseWithoutVariant, Variant);
				NewObjectPathString = NsBonsaiReview::MakeObjectPathString(PackagePath, FinalName);
				if (ReservedObjectPaths.Contains(NewObjectPathString))
				{
					continue;
				}

				const FSoftObjectPath CandidatePath(NewObjectPathString);
				if (CandidatePath == OldPath)
				{
					ReservedObjectPaths.Add(NewObjectPathString);
					bFoundAvailableVariant = true;
					break;
				}

				const FAssetData Existing = AssetRegistry.GetAssetByObjectPath(CandidatePath, false);
				if (!Existing.IsValid())
				{
					ReservedObjectPaths.Add(NewObjectPathString);
					bFoundAvailableVariant = true;
					break;
				}
			}

			if (!bFoundAvailableVariant || FinalName.IsEmpty())
			{
				Row->bRenameBlockedConflict = true;
				Row->RenameBlockedReason = FString::Printf(TEXT("Conflict: no free variant for '%s'."), *BaseWithoutVariant);
				NsBonsaiReview::AppendRenameLog(OldPath.ToString(), FString(), TEXT("ConflictNoVariant"));
				continue;
			}

			const FSoftObjectPath NewPath(NewObjectPathString);
			FRenamePlan& Plan = Plans.Emplace_GetRef();
			Plan.Row = Row;
			Plan.OldPath = OldPath;
			Plan.NewPath = NewPath;
			Plan.bNeedsRename = (OldPath != NewPath);
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
			NsBonsaiReview::AppendRenameLog(Deferred.OldPath.ToString(), Deferred.NewPath.ToString(), bSucceeded ? TEXT("Renamed") : TEXT("RenameFailed"));
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

	FReply OnConfirmClicked(const TSharedPtr<NsBonsaiReview::FRowModel>& FocusRow)
	{
		const TArray<TSharedPtr<NsBonsaiReview::FRowModel>> Targets = GetSelectionOrSingle(FocusRow);
		TArray<TSharedPtr<NsBonsaiReview::FRowModel>> ActionableTargets;
		for (const TSharedPtr<NsBonsaiReview::FRowModel>& Row : Targets)
		{
			if (!Row.IsValid() || Row->bIgnored)
			{
				continue;
			}

			if (Row->SelectedDomain.IsNone() || Row->SelectedCategory.IsNone() || NsBonsaiReview::SanitizeAssetNameCandidate(Row->AssetName).IsEmpty())
			{
				NsBonsaiReview::AppendRenameLog(Row->ObjectPath.ToString(), FString(), TEXT("BlockedMissingFields"));
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
					NsBonsaiReview::AppendRenameLog(Plan.OldPath.ToString(), Plan.NewPath.ToString(), TEXT("DryRunQueued"));
				}
				else
				{
					NsBonsaiReview::AppendRenameLog(Plan.OldPath.ToString(), Plan.NewPath.ToString(), TEXT("DryRunNoOp"));
				}

				UserSettings->TouchDomain(Plan.Row->SelectedDomain);
				UserSettings->TouchCategory(Plan.Row->SelectedCategory);
			}
			UserSettings->Save();

			TArray<TSharedPtr<NsBonsaiReview::FRowModel>> PlannedRows;
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
			if (GetRemainingCount() == 0)
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
		TArray<TSharedPtr<NsBonsaiReview::FRowModel>> SuccessfulRows;
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
				NsBonsaiReview::AppendRenameLog(Plan.OldPath.ToString(), Plan.NewPath.ToString(), Plan.bNeedsRename ? TEXT("Renamed") : TEXT("NoOp"));
			}
			else
			{
				Plan.Row->bRenameBlockedConflict = true;
				Plan.Row->RenameBlockedReason = TEXT("Rename failed or destination remained in conflict.");
				NsBonsaiReview::AppendRenameLog(Plan.OldPath.ToString(), Plan.NewPath.ToString(), TEXT("RenameFailed"));
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
		if (GetRemainingCount() == 0)
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
		for (const TSharedPtr<NsBonsaiReview::FRowModel>& Row : Rows)
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

	TSet<FString> KnownTypeTokensUpper;
	TSet<FString> KnownDomainTokensUpper;
	TSet<FString> KnownCategoryTokensUpper;
	bool bEnableConflictPrecheck = true;
	bool bDryRunEnabled = false;
	bool bClosedBySnooze = false;
	int32 ConfirmedCount = 0;
	int32 IgnoredCount = 0;
	TArray<FDeferredRenameEntry> DeferredRenames;

	TArray<TSharedPtr<NsBonsaiReview::FRowModel>> Rows;
	TSet<FSoftObjectPath> RowPaths;
	TSharedPtr<SListView<TSharedPtr<NsBonsaiReview::FRowModel>>> ListView;

	TArray<TSharedPtr<FName>> DomainOptions;
	TMap<TSharedPtr<NsBonsaiReview::FRowModel>, TArray<TSharedPtr<FName>>> CategoryOptionsByRow;
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
	if (NsBonsaiReview::ShouldSkipCompliantAssets())
	{
		const UNsBonsaiSettings* Settings = GetDefault<UNsBonsaiSettings>();
		if (Settings && NsBonsaiReview::IsCompliant(AssetData, *Settings))
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

	if (NsBonsaiReview::ShouldSkipCompliantAssets())
	{
		const UNsBonsaiSettings* Settings = GetDefault<UNsBonsaiSettings>();
		if (Settings && NsBonsaiReview::IsCompliant(AssetData, *Settings))
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
	const bool bSkipCompliantAssets = NsBonsaiReview::ShouldSkipCompliantAssets();
	const UNsBonsaiSettings* Settings = bSkipCompliantAssets ? GetDefault<UNsBonsaiSettings>() : nullptr;

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

		if (bSkipCompliantAssets && Settings && NsBonsaiReview::IsCompliant(AssetData, *Settings))
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
	const UNsBonsaiUserSettings* UserSettings = GetDefault<UNsBonsaiUserSettings>();
	return FMath::Max(1, UserSettings ? UserSettings->MinAssetsToPopup : 1);
}

double FNsBonsaiReviewManager::GetPopupCooldownSeconds() const
{
	const UNsBonsaiUserSettings* UserSettings = GetDefault<UNsBonsaiUserSettings>();
	return FMath::Max(0.0, static_cast<double>(UserSettings ? UserSettings->PopupCooldownSeconds : 2.0f));
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
		if (const TSharedPtr<SNsBonsaiReviewWindow> ClosedWidget = ClosedWidgetRef.Pin())
		{
			bClosedBySnooze = ClosedWidget->WasClosedBySnooze();
			TArray<FAssetData> UnresolvedAssets;
			ClosedWidget->CollectUnresolvedAssets(UnresolvedAssets);
			RequeueAssets(UnresolvedAssets);
		}

		ReviewWindow.Reset();
		ReviewWindowWidget.Reset();
		bPopupScheduled = false;
		PopupOpenAtTime = 0.0;
		NextAutoPopupAllowedTime = FPlatformTime::Seconds() + GetPopupCooldownSeconds();
		if (bClosedBySnooze && ReviewQueue.Num() > 0)
		{
			RequestPopupDebounced();
		}
	}));

	Window->SetContent(ReviewWidget);
	ReviewWindow = Window;
	ReviewWindowWidget = ReviewWidget;
	FSlateApplication::Get().AddWindow(Window);
}
#endif
