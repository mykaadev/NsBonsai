#include "NsBonsaiReviewManager.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"
#include "NsBonsaiAssetEvaluator.h"
#include "NsBonsaiNameBuilder.h"
#include "NsBonsaiSettings.h"
#include "NsBonsaiUserSettings.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"

#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

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

	struct FRowModel
	{
		FAssetData AssetData;
		FString CurrentName;

		FName SelectedDomain;
		FName SelectedCategory;
		FString AssetName;

		FString PreviewName;

		bool bConfirmed = false;
		bool bIgnored = false;
	};

	static void RebuildPreview(FRowModel& Row)
	{
		const UNsBonsaiSettings* Settings = GetDefault<UNsBonsaiSettings>();

		FNsBonsaiNameBuildInput BuildInput;
		BuildInput.TypeToken = Settings->ResolveTypeTokenForClassPath(Row.AssetData.AssetClassPath);
		BuildInput.DomainToken = Row.SelectedDomain;
		BuildInput.CategoryToken = Row.SelectedCategory;
		BuildInput.AssetName = Row.AssetName;
		BuildInput.VariantToken = TEXT("A"); // preview placeholder

		Row.PreviewName = FNsBonsaiNameBuilder::BuildFinalAssetName(BuildInput, *Settings);
	}

	static const FSlateBrush* StatusBrush(const FRowModel& Row)
	{
		if (Row.bIgnored)
		{
			return FAppStyle::Get().GetBrush("Icons.X");
		}
		if (Row.bConfirmed)
		{
			return FAppStyle::Get().GetBrush("Icons.Check");
		}
		return nullptr;
	}

	static FText StatusText(const FRowModel& Row)
	{
		if (Row.bIgnored) return FText::FromString(TEXT("Ignored"));
		if (Row.bConfirmed) return FText::FromString(TEXT("Confirmed"));
		return FText::FromString(TEXT("Pending"));
	}
}

class SNsBonsaiReviewWindow final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNsBonsaiReviewWindow) {}
		SLATE_ARGUMENT(TArray<FAssetData>, Assets)
		SLATE_ARGUMENT(TWeakPtr<SWindow>, ParentWindow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		ParentWindow = InArgs._ParentWindow;
		Settings = GetDefault<UNsBonsaiSettings>();
		UserSettings = GetMutableDefault<UNsBonsaiUserSettings>();

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
				if (bL && bR) return Li < Ri;
				if (bL != bR) return bL;
				return L.ToString().Compare(R.ToString(), ESearchCase::IgnoreCase) < 0;
			});

			for (const FName D : Domains)
			{
				DomainOptions.Add(MakeShared<FName>(D));
				DomainOptionByName.Add(D, DomainOptions.Last());
			}
		}

		for (const FAssetData& Asset : InArgs._Assets)
		{
			FNsBonsaiEvaluationResult Eval = FNsBonsaiAssetEvaluator::Evaluate(Asset, *Settings, *UserSettings);
			TSharedPtr<NsBonsaiReview::FRowModel> Row = MakeShared<NsBonsaiReview::FRowModel>();
			Row->AssetData = Asset;
			Row->CurrentName = Asset.AssetName.ToString();
			Row->SelectedDomain = Eval.PreselectedDomain;
			Row->SelectedCategory = Eval.PreselectedCategory;
			Row->AssetName = Eval.ExistingAssetName.IsEmpty() ? Row->CurrentName : Eval.ExistingAssetName;
			NsBonsaiReview::RebuildPreview(*Row);
			Rows.Add(Row);
		}

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(8)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Review newly added assets — choose Domain/Category, edit Asset Name, then Confirm or Ignore.")))
			]
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8)
			[
				SNew(SSplitter)
				+ SSplitter::Slot().Value(0.33f)
				[
					BuildAssetListPanel()
				]
				+ SSplitter::Slot().Value(0.34f)
				[
					BuildDetailsPanel()
				]
				+ SSplitter::Slot().Value(0.33f)
				[
					BuildActionsPanel()
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]
					{
						const int32 Confirmed = CountConfirmed();
						const int32 Ignored = CountIgnored();
						return FText::FromString(FString::Printf(TEXT("Confirmed: %d   Ignored: %d   Total: %d"), Confirmed, Ignored, Rows.Num()));
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Rename Confirmed")))
					.IsEnabled(this, &SNsBonsaiReviewWindow::CanRenameConfirmed)
					.OnClicked(this, &SNsBonsaiReviewWindow::OnRenameConfirmedClicked)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Cancel")))
					.OnClicked(this, &SNsBonsaiReviewWindow::OnCancelClicked)
				]
			]
		];
	}

private:
	TSharedRef<SWidget> BuildAssetListPanel()
	{
		return SNew(SBorder)
		.Padding(6)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Assets")))
			]
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(2)
			[
				SAssignNew(ListView, SListView<TSharedPtr<NsBonsaiReview::FRowModel>>)
				.ListItemsSource(&Rows)
				.SelectionMode(ESelectionMode::Multi)
				.OnGenerateRow(this, &SNsBonsaiReviewWindow::OnGenerateRow)
				.OnSelectionChanged(this, &SNsBonsaiReviewWindow::OnSelectionChanged)
			]
		];
	}

	TSharedRef<SWidget> BuildDetailsPanel()
	{
		return SNew(SBorder)
		.Padding(10)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Naming")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(6)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Domain")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				 SAssignNew(DomainCombo, SComboBox<TSharedPtr<FName>>)
				.IsEnabled_Lambda([this]{ return SelectedRows.Num() > 0; })
				.OptionsSource(&DomainOptions)
				.OnGenerateWidget_Lambda([](TSharedPtr<FName> Name)
				{
					return SNew(STextBlock).Text(Name.IsValid() ? FText::FromName(*Name) : FText::GetEmpty());
				})
				.OnSelectionChanged(this, &SNsBonsaiReviewWindow::OnDomainPicked)
				[
					SNew(STextBlock)
					.Text_Lambda([this]
					{
						return GetDetailsDomainText();
					})
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(6)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Category")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				SAssignNew(CategoryCombo, SComboBox<TSharedPtr<FName>>)
				.OptionsSource(&CategoryOptions)
				.IsEnabled_Lambda([this]
				{
					return SelectedRows.Num() > 0 && !bDomainMixed && !GetCommonDomain().IsNone();
				})
				.OnGenerateWidget_Lambda([](TSharedPtr<FName> Name)
				{
					return SNew(STextBlock).Text(Name.IsValid() ? FText::FromName(*Name) : FText::GetEmpty());
				})
				.OnSelectionChanged(this, &SNsBonsaiReviewWindow::OnCategoryPicked)
				[
					SNew(STextBlock)
					.Text_Lambda([this]
					{
						return GetDetailsCategoryText();
					})
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(6)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Asset Name")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				SAssignNew(AssetNameBox, SEditableTextBox)
				.IsEnabled_Lambda([this]{ return SelectedRows.Num() == 1; })
				.HintText_Lambda([this]
				{
					return SelectedRows.Num() == 1
						? FText::FromString(TEXT("Name part(s) before the variant"))
						: FText::FromString(TEXT("Select one asset to edit Asset Name"));
				})
				.Text_Lambda([this]
				{
					if (SelectedRows.Num() == 1 && SelectedRows[0].IsValid())
					{
						return FText::FromString(SelectedRows[0]->AssetName);
					}
					return FText::GetEmpty();
				})
				.OnTextChanged(this, &SNsBonsaiReviewWindow::OnAssetNameChanged)
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(8)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Tip: Multi-select assets on the left, then set Domain/Category once.")))
				.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))
			]
		];
	}

	TSharedRef<SWidget> BuildActionsPanel()
	{
		return SNew(SBorder)
		.Padding(10)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Output")))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8)
			[
				SNew(STextBlock)
				.Text_Lambda([this]
				{
					if (SelectedRows.Num() == 0)
					{
						return FText::FromString(TEXT("Select assets to edit."));
					}
					if (SelectedRows.Num() > 1)
					{
						return FText::FromString(TEXT("Multiple assets selected."));
					}
					return FText::FromString(FString::Printf(TEXT("Preview: %s"), *SelectedRows[0]->PreviewName));
				})
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(4)
			[
				SNew(STextBlock)
				.Text_Lambda([this]
				{
					if (SelectedRows.Num() == 1)
					{
						return FText::FromString(FString::Printf(TEXT("Status: %s"), *NsBonsaiReview::StatusText(*SelectedRows[0]).ToString()));
					}
					return FText::GetEmpty();
				})
				.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(10)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Confirm Selected")))
					.IsEnabled_Lambda([this]{ return SelectedRows.Num() > 0; })
					.OnClicked(this, &SNsBonsaiReviewWindow::OnConfirmSelectedClicked)
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Ignore Selected")))
					.IsEnabled_Lambda([this]{ return SelectedRows.Num() > 0; })
					.OnClicked(this, &SNsBonsaiReviewWindow::OnIgnoreSelectedClicked)
				]
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SSpacer)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(2)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Confirm marks assets for renaming. Ignore skips them.")))
				.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))
			]
		];
	}

	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<NsBonsaiReview::FRowModel> Row, const TSharedRef<STableViewBase>& Owner)
	{
		return SNew(STableRow<TSharedPtr<NsBonsaiReview::FRowModel>>, Owner)
		[
			SNew(SBorder)
			.Padding(6)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(2)
				[
					SNew(SBox)
					.WidthOverride(20)
					.HeightOverride(20)
					[
						SNew(SImage)
						.Image_Lambda([Row]
						{
							const FSlateBrush* Brush = NsBonsaiReview::StatusBrush(*Row);
							return Brush;
						})
					]
				]
				+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(1)
					[
						SNew(STextBlock)
						.Text_Lambda([Row]
						{
							return FText::FromString(FString::Printf(TEXT("%s"), *Row->CurrentName));
						})
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(1)
					[
						SNew(STextBlock)
						.Text_Lambda([Row]
						{
							return FText::FromString(Row->PreviewName);
						})
						.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f, 1.0f))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(1)
					[
						SNew(STextBlock)
						.Text_Lambda([Row]
						{
							return NsBonsaiReview::StatusText(*Row);
						})
						.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
					]
				]
			]
		];
	}

	void OnSelectionChanged(TSharedPtr<NsBonsaiReview::FRowModel>, ESelectInfo::Type)
	{
		SelectedRows.Reset();
		if (ListView.IsValid())
		{
			ListView->GetSelectedItems(SelectedRows);
		}

		SyncDetailsFromSelection();
	}

	FName GetCommonDomain() const
	{
		if (SelectedRows.Num() == 0)
		{
			return NAME_None;
		}
		FName First = SelectedRows[0].IsValid() ? SelectedRows[0]->SelectedDomain : NAME_None;
		for (int32 i = 1; i < SelectedRows.Num(); ++i)
		{
			if (!SelectedRows[i].IsValid() || SelectedRows[i]->SelectedDomain != First)
			{
				return NAME_None;
			}
		}
		return First;
	}

	FName GetCommonCategory() const
	{
		if (SelectedRows.Num() == 0)
		{
			return NAME_None;
		}
		FName First = SelectedRows[0].IsValid() ? SelectedRows[0]->SelectedCategory : NAME_None;
		for (int32 i = 1; i < SelectedRows.Num(); ++i)
		{
			if (!SelectedRows[i].IsValid() || SelectedRows[i]->SelectedCategory != First)
			{
				return NAME_None;
			}
		}
		return First;
	}

	void SyncDetailsFromSelection()
	{
		bDomainMixed = false;
		bCategoryMixed = false;

		if (SelectedRows.Num() == 0)
		{
			CategoryOptions.Reset();
			if (CategoryCombo.IsValid()) CategoryCombo->RefreshOptions();
			return;
		}

		// Mixed detection
		FName D0 = SelectedRows[0]->SelectedDomain;
		for (int32 i = 1; i < SelectedRows.Num(); ++i)
		{
			if (SelectedRows[i]->SelectedDomain != D0)
			{
				bDomainMixed = true;
				break;
			}
		}

		FName C0 = SelectedRows[0]->SelectedCategory;
		for (int32 i = 1; i < SelectedRows.Num(); ++i)
		{
			if (SelectedRows[i]->SelectedCategory != C0)
			{
				bCategoryMixed = true;
				break;
			}
		}

		RebuildCategoryOptions();
	}

	void RebuildCategoryOptions()
	{
		CategoryOptions.Reset();

		if (SelectedRows.Num() == 0 || bDomainMixed)
		{
			if (CategoryCombo.IsValid()) CategoryCombo->RefreshOptions();
			return;
		}

		const FName Domain = GetCommonDomain();
		if (Domain.IsNone())
		{
			if (CategoryCombo.IsValid()) CategoryCombo->RefreshOptions();
			return;
		}

		TArray<FName> Cats;
		for (const FNsBonsaiDomainDef& DomainDef : Settings->Domains)
		{
			if (Settings->NormalizeToken(DomainDef.DomainToken) != Domain)
			{
				continue;
			}
			for (const FName C : DomainDef.Categories)
			{
				const FName NC = Settings->NormalizeToken(C);
				if (!NC.IsNone() && !Cats.Contains(NC))
				{
					Cats.Add(NC);
				}
			}
		}

		Cats.Sort([this](const FName& L, const FName& R)
		{
			const int32 Li = UserSettings->RecentCategories.IndexOfByKey(L);
			const int32 Ri = UserSettings->RecentCategories.IndexOfByKey(R);
			const bool bL = Li != INDEX_NONE;
			const bool bR = Ri != INDEX_NONE;
			if (bL && bR) return Li < Ri;
			if (bL != bR) return bL;
			return L.ToString().Compare(R.ToString(), ESearchCase::IgnoreCase) < 0;
		});

		for (const FName C : Cats)
		{
			CategoryOptions.Add(MakeShared<FName>(C));
		}

		if (CategoryCombo.IsValid())
		{
			CategoryCombo->RefreshOptions();
		}
	}

	FText GetDetailsDomainText() const
	{
		if (SelectedRows.Num() == 0)
		{
			return FText::FromString(TEXT("Select asset(s)"));
		}
		if (bDomainMixed)
		{
			return FText::FromString(TEXT("— Mixed —"));
		}
		const FName D = GetCommonDomain();
		return D.IsNone() ? FText::FromString(TEXT("Select Domain")) : FText::FromName(D);
	}

	FText GetDetailsCategoryText() const
	{
		if (SelectedRows.Num() == 0)
		{
			return FText::FromString(TEXT("Select asset(s)"));
		}
		if (bDomainMixed)
		{
			return FText::FromString(TEXT("— Mixed —"));
		}
		if (bCategoryMixed)
		{
			return FText::FromString(TEXT("— Mixed —"));
		}
		const FName C = GetCommonCategory();
		return C.IsNone() ? FText::FromString(TEXT("Select Category")) : FText::FromName(C);
	}

	void OnDomainPicked(TSharedPtr<FName> NewSelection, ESelectInfo::Type)
	{
		if (!NewSelection.IsValid()) return;
		const FName Domain = *NewSelection;

		for (const TSharedPtr<NsBonsaiReview::FRowModel>& Row : SelectedRows)
		{
			if (!Row.IsValid()) continue;
			Row->SelectedDomain = Domain;
			// Reset category if it's not in this domain.
			Row->SelectedCategory = NAME_None;
			Row->bConfirmed = false;
			Row->bIgnored = false;
			NsBonsaiReview::RebuildPreview(*Row);
		}

		SyncDetailsFromSelection();
		RequestRefresh();
	}

	void OnCategoryPicked(TSharedPtr<FName> NewSelection, ESelectInfo::Type)
	{
		if (!NewSelection.IsValid()) return;
		const FName Category = *NewSelection;

		for (const TSharedPtr<NsBonsaiReview::FRowModel>& Row : SelectedRows)
		{
			if (!Row.IsValid()) continue;
			Row->SelectedCategory = Category;
			Row->bConfirmed = false;
			Row->bIgnored = false;
			NsBonsaiReview::RebuildPreview(*Row);
		}

		SyncDetailsFromSelection();
		RequestRefresh();
	}

	void OnAssetNameChanged(const FText& NewText)
	{
		if (SelectedRows.Num() != 1 || !SelectedRows[0].IsValid())
		{
			return;
		}
		SelectedRows[0]->AssetName = NewText.ToString();
		SelectedRows[0]->bConfirmed = false;
		SelectedRows[0]->bIgnored = false;
		NsBonsaiReview::RebuildPreview(*SelectedRows[0]);
		RequestRefresh();
	}

	FReply OnConfirmSelectedClicked()
	{
		for (const TSharedPtr<NsBonsaiReview::FRowModel>& Row : SelectedRows)
		{
			if (!Row.IsValid()) continue;

			if (Row->SelectedDomain.IsNone() || Row->SelectedCategory.IsNone())
			{
				// Leave as pending if incomplete.
				continue;
			}

			Row->bConfirmed = true;
			Row->bIgnored = false;
		}

		RequestRefresh();
		return FReply::Handled();
	}

	FReply OnIgnoreSelectedClicked()
	{
		for (const TSharedPtr<NsBonsaiReview::FRowModel>& Row : SelectedRows)
		{
			if (!Row.IsValid()) continue;
			Row->bIgnored = true;
			Row->bConfirmed = false;
		}
		RequestRefresh();
		return FReply::Handled();
	}

	bool CanRenameConfirmed() const
	{
		for (const TSharedPtr<NsBonsaiReview::FRowModel>& Row : Rows)
		{
			if (Row.IsValid() && Row->bConfirmed && !Row->bIgnored)
			{
				return true;
			}
		}
		return false;
	}

	int32 CountConfirmed() const
	{
		int32 N = 0;
		for (const auto& Row : Rows)
		{
			if (Row.IsValid() && Row->bConfirmed && !Row->bIgnored) ++N;
		}
		return N;
	}

	int32 CountIgnored() const
	{
		int32 N = 0;
		for (const auto& Row : Rows)
		{
			if (Row.IsValid() && Row->bIgnored) ++N;
		}
		return N;
	}

	FReply OnRenameConfirmedClicked()
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

		TArray<FAssetRenameData> Renames;
		TSet<FString> ReservedObjectPaths;

		for (const TSharedPtr<NsBonsaiReview::FRowModel>& Row : Rows)
		{
			if (!Row.IsValid() || !Row->bConfirmed || Row->bIgnored)
			{
				continue;
			}

			// Require valid taxonomy
			if (Row->SelectedDomain.IsNone() || Row->SelectedCategory.IsNone())
			{
				continue;
			}

			const FString PackagePath = Row->AssetData.PackagePath.ToString();
			const FSoftObjectPath OldPath = Row->AssetData.GetSoftObjectPath();

			FNsBonsaiNameBuildInput BuildInput;
			BuildInput.TypeToken = Settings->ResolveTypeTokenForClassPath(Row->AssetData.AssetClassPath);
			BuildInput.DomainToken = Row->SelectedDomain;
			BuildInput.CategoryToken = Row->SelectedCategory;
			BuildInput.AssetName = Row->AssetName;

			const FString Base = FNsBonsaiNameBuilder::BuildBaseAssetName(BuildInput, *Settings);
			if (Base.IsEmpty())
			{
				continue;
			}

			// Allocate collision-safe variant
			FString FinalName;
			for (int32 VariantIndex = 0; VariantIndex < 26 * 26; ++VariantIndex)
			{
				const FString Variant = NsBonsaiReview::VariantFromIndex(VariantIndex);
				BuildInput.VariantToken = Variant;
				FinalName = FNsBonsaiNameBuilder::BuildFinalAssetName(BuildInput, *Settings);

				const FString NewObjectPathString = NsBonsaiReview::MakeObjectPathString(PackagePath, FinalName);
				if (ReservedObjectPaths.Contains(NewObjectPathString))
				{
					continue;
				}

				const FAssetData Existing = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(NewObjectPathString), /*bIncludeOnlyOnDiskAssets*/ false);
				if (!Existing.IsValid())
				{
					ReservedObjectPaths.Add(NewObjectPathString);
					break;
				}
			}

			if (FinalName.IsEmpty())
			{
				continue;
			}

			const FString NewObjectPathString = NsBonsaiReview::MakeObjectPathString(PackagePath, FinalName);
			const FSoftObjectPath NewPath(NewObjectPathString);
			Renames.Emplace(OldPath, NewPath, /*bOnlyFixSoftReferences*/ false, /*bGenerateRedirectors*/ true);

			UserSettings->TouchDomain(Row->SelectedDomain);
			UserSettings->TouchCategory(Row->SelectedCategory);
		}

		UserSettings->Save();
		AssetToolsModule.Get().RenameAssets(Renames);
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

private:
	TWeakPtr<SWindow> ParentWindow;
	const UNsBonsaiSettings* Settings = nullptr;
	UNsBonsaiUserSettings* UserSettings = nullptr;

	TArray<TSharedPtr<NsBonsaiReview::FRowModel>> Rows;
	TArray<TSharedPtr<NsBonsaiReview::FRowModel>> SelectedRows;

	TSharedPtr<SListView<TSharedPtr<NsBonsaiReview::FRowModel>>> ListView;

	TArray<TSharedPtr<FName>> DomainOptions;
	TMap<FName, TSharedPtr<FName>> DomainOptionByName;

	TArray<TSharedPtr<FName>> CategoryOptions;

	TSharedPtr<SComboBox<TSharedPtr<FName>>> DomainCombo;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> CategoryCombo;
	TSharedPtr<SEditableTextBox> AssetNameBox;

	bool bDomainMixed = false;
	bool bCategoryMixed = false;
};

void FNsBonsaiReviewManager::Startup()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetAddedHandle = AssetRegistry.OnAssetAdded().AddRaw(this, &FNsBonsaiReviewManager::OnAssetAdded);
	AssetRemovedHandle = AssetRegistry.OnAssetRemoved().AddRaw(this, &FNsBonsaiReviewManager::OnAssetRemoved);
	AssetRenamedHandle = AssetRegistry.OnAssetRenamed().AddRaw(this, &FNsBonsaiReviewManager::OnAssetRenamed);
	PackageSavedHandle = UPackage::PackageSavedWithContextEvent.AddRaw(this, &FNsBonsaiReviewManager::OnPackageSaved);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FNsBonsaiReviewManager::Tick), 0.1f);
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
}

void FNsBonsaiReviewManager::OnAssetAdded(const FAssetData& AssetData)
{
	PendingAssetsByPackage.FindOrAdd(AssetData.PackageName).Add(AssetData.GetSoftObjectPath());
}

void FNsBonsaiReviewManager::OnAssetRemoved(const FAssetData& AssetData)
{
	const FSoftObjectPath Path = AssetData.GetSoftObjectPath();

	if (TSet<FSoftObjectPath>* PendingSet = PendingAssetsByPackage.Find(AssetData.PackageName))
	{
		PendingSet->Remove(Path);
		if (PendingSet->IsEmpty())
		{
			PendingAssetsByPackage.Remove(AssetData.PackageName);
		}
	}

	QueuedObjectPaths.Remove(Path);
}

void FNsBonsaiReviewManager::OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
{
	const FSoftObjectPath OldPath(OldObjectPath);

	for (TPair<FName, TSet<FSoftObjectPath>>& Pair : PendingAssetsByPackage)
	{
		Pair.Value.Remove(OldPath);
	}

	const FSoftObjectPath NewPath = AssetData.GetSoftObjectPath();
	PendingAssetsByPackage.FindOrAdd(AssetData.PackageName).Add(NewPath);
	QueuedObjectPaths.Remove(OldPath);
}

void FNsBonsaiReviewManager::OnPackageSaved(
	const FString& PackageFileName,
	UPackage* Package,
	FObjectPostSaveContext SaveContext)
{
	if (!Package)
	{
		return;
	}
	EnqueuePackageAssets(Package->GetFName());
}

bool FNsBonsaiReviewManager::Tick(float)
{
	if (bPopupScheduled && !bPopupOpen && FPlatformTime::Seconds() >= PopupOpenAtTime)
	{
		bPopupScheduled = false;
		OpenReviewPopup();
	}
	return true;
}

void FNsBonsaiReviewManager::EnqueuePackageAssets(FName PackageName)
{
	TSet<FSoftObjectPath> PendingObjectPaths;
	if (!PendingAssetsByPackage.RemoveAndCopyValue(PackageName, PendingObjectPaths) || PendingObjectPaths.Num() == 0)
	{
		return;
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	for (const FSoftObjectPath& Path : PendingObjectPaths)
	{
		if (QueuedObjectPaths.Contains(Path))
		{
			continue;
		}

		const FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(Path, /*bIncludeOnlyOnDiskAssets*/ false, /*bSkipARFilteredAssets*/ true);
		if (AssetData.IsValid())
		{
			QueuedObjectPaths.Add(Path);
			ReviewQueue.Add(AssetData);
		}
	}

	if (ReviewQueue.Num() > 0)
	{
		RequestPopupDebounced();
	}
}

void FNsBonsaiReviewManager::RequestPopupDebounced()
{
	bPopupScheduled = true;
	PopupOpenAtTime = FPlatformTime::Seconds() + 0.35;
}

void FNsBonsaiReviewManager::OpenReviewPopup()
{
	if (ReviewQueue.Num() == 0 || bPopupOpen || !FSlateApplication::IsInitialized())
	{
		return;
	}

	bPopupOpen = true;
	TArray<FAssetData> AssetsToReview = MoveTemp(ReviewQueue);
	ReviewQueue.Reset();
	QueuedObjectPaths.Reset();

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(TEXT("NsBonsai Asset Review")))
		.ClientSize(FVector2D(1200, 720))
		.SupportsMinimize(true)
		.SupportsMaximize(true);

	Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda([this](const TSharedRef<SWindow>&)
	{
		bPopupOpen = false;
		if (ReviewQueue.Num() > 0)
		{
			RequestPopupDebounced();
		}
	}));

	Window->SetContent(
		SNew(SNsBonsaiReviewWindow)
		.Assets(AssetsToReview)
		.ParentWindow(Window)
	);

	FSlateApplication::Get().AddWindow(Window);
}

#endif
