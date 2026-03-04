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
#include "Styling/AppStyle.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
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
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
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
				.Text(FText::FromString(TEXT("Review newly added assets — edit Domain/Category and Asset Name, then confirm (\u2713) or ignore (\u2717).")))
			]
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8)
			[
				BuildCompactTable()
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Text_Lambda([this]
					{
						return FText::FromString(FString::Printf(TEXT("Remaining: %d"), Rows.Num()));
					})
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
	// Compact table-style UI
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
				+ SHeaderRow::Column(TEXT("Asset")).DefaultLabel(FText::FromString(TEXT("Asset"))).FillWidth(0.23f)
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
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (!Row.IsValid() || !Owner)
			{
				return SNew(STextBlock);
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
				});
			}
			if (ColumnName == TEXT("Final"))
			{
				return SNew(STextBlock)
				.Text_Lambda([this]{ return FText::FromString(Row->PreviewName); })
				.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f));
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

	void EnsureCategoryOptions(const TSharedPtr<NsBonsaiReview::FRowModel>& Row)
	{
		if (!Row.IsValid()) return;
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
			Options.Add(MakeShared<FName>(C));
		}
	}

	void ApplyDomain(const TSharedPtr<NsBonsaiReview::FRowModel>& FocusRow, const FName Domain)
	{
		const TArray<TSharedPtr<NsBonsaiReview::FRowModel>> Targets = GetSelectionOrSingle(FocusRow);
		for (const auto& Row : Targets)
		{
			if (!Row.IsValid()) continue;
			Row->SelectedDomain = Domain;
			Row->SelectedCategory = NAME_None;
			EnsureCategoryOptions(Row);
			// Auto-pick first category for convenience.
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
		const TArray<TSharedPtr<NsBonsaiReview::FRowModel>> Targets = GetSelectionOrSingle(FocusRow);
		for (const auto& Row : Targets)
		{
			if (!Row.IsValid()) continue;
			Row->SelectedCategory = Category;
			NsBonsaiReview::RebuildPreview(*Row);
		}
		RequestRefresh();
	}

	void ApplyAssetName(const TSharedPtr<NsBonsaiReview::FRowModel>& Row, const FString& NewName)
	{
		if (!Row.IsValid()) return;
		Row->AssetName = NewName;
		NsBonsaiReview::RebuildPreview(*Row);
		RequestRefresh();
	}

	FReply OnIgnoreClicked(const TSharedPtr<NsBonsaiReview::FRowModel>& FocusRow)
	{
		const TArray<TSharedPtr<NsBonsaiReview::FRowModel>> Targets = GetSelectionOrSingle(FocusRow);
		for (const auto& Row : Targets)
		{
			if (!Row.IsValid()) continue;
			if (Manager)
			{
				Manager->MarkResolved(Row->AssetData.GetSoftObjectPath());
			}
			Rows.Remove(Row);
			CategoryOptionsByRow.Remove(Row);
		}
		if (ListView.IsValid())
		{
			ListView->ClearSelection();
		}
		RequestRefresh();
		if (Rows.Num() == 0)
		{
			return CloseWindow();
		}
		return FReply::Handled();
	}

	FReply OnConfirmClicked(const TSharedPtr<NsBonsaiReview::FRowModel>& FocusRow)
	{
		// Validate required fields
		const TArray<TSharedPtr<NsBonsaiReview::FRowModel>> Targets = GetSelectionOrSingle(FocusRow);
		for (const auto& Row : Targets)
		{
			if (!Row.IsValid() || Row->SelectedDomain.IsNone() || Row->SelectedCategory.IsNone())
			{
				return FReply::Handled();
			}
		}

		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetRenameData> Renames;
		TSet<FString> ReservedObjectPaths;

		for (const auto& Row : Targets)
		{
			if (!Row.IsValid()) continue;
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

			const FSoftObjectPath NewPath(NsBonsaiReview::MakeObjectPathString(PackagePath, FinalName));
			Renames.Emplace(OldPath, NewPath, /*bOnlyFixSoftReferences*/ false, /*bGenerateRedirectors*/ true);

			UserSettings->TouchDomain(Row->SelectedDomain);
			UserSettings->TouchCategory(Row->SelectedCategory);
		}

		UserSettings->Save();
		if (Manager)
		{
			Manager->SetApplyingRename(true);
		}
		AssetToolsModule.Get().RenameAssets(Renames);
		// Do not disable immediately; manager has a short cooldown to absorb late callbacks.

		// Remove rows after rename
		for (const auto& Row : Targets)
		{
			if (!Row.IsValid()) continue;
			if (Manager)
			{
				Manager->MarkResolved(Row->AssetData.GetSoftObjectPath());
			}
			Rows.Remove(Row);
			CategoryOptionsByRow.Remove(Row);
		}
		if (ListView.IsValid())
		{
			ListView->ClearSelection();
		}
		RequestRefresh();
		if (Rows.Num() == 0)
		{
			return CloseWindow();
		}
		return FReply::Handled();
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
	FNsBonsaiReviewManager* Manager = nullptr;
	const UNsBonsaiSettings* Settings = nullptr;
	UNsBonsaiUserSettings* UserSettings = nullptr;

	TArray<TSharedPtr<NsBonsaiReview::FRowModel>> Rows;
	TSharedPtr<SListView<TSharedPtr<NsBonsaiReview::FRowModel>>> ListView;

	TArray<TSharedPtr<FName>> DomainOptions;
	TMap<TSharedPtr<NsBonsaiReview::FRowModel>, TArray<TSharedPtr<FName>>> CategoryOptionsByRow;
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
	if (IsApplyingRename())
	{
		return;
	}
	PendingAssetsByPackage.FindOrAdd(AssetData.PackageName).Add(AssetData.GetSoftObjectPath());
}

void FNsBonsaiReviewManager::OnAssetRemoved(const FAssetData& AssetData)
{
	if (IsApplyingRename())
	{
		return;
	}
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
	if (IsApplyingRename())
	{
		return;
	}
	const FSoftObjectPath OldPath(OldObjectPath);

	for (TPair<FName, TSet<FSoftObjectPath>>& Pair : PendingAssetsByPackage)
	{
		Pair.Value.Remove(OldPath);
	}

	const FSoftObjectPath NewPath = AssetData.GetSoftObjectPath();
	PendingAssetsByPackage.FindOrAdd(AssetData.PackageName).Add(NewPath);
	QueuedObjectPaths.Remove(OldPath);
}

void FNsBonsaiReviewManager::OnPackageSaved(const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext SaveContext)
{
	if (IsApplyingRename())
	{
		return;
	}
	if (!Package)
	{
		return;
	}
	EnqueuePackageAssets(Package->GetFName());
}

bool FNsBonsaiReviewManager::Tick(float)
{
	// Cooldown for rename guard (handles late registry/save callbacks).
	if (bApplyingRename && FPlatformTime::Seconds() >= ApplyingRenameCooldownUntil)
	{
		bApplyingRename = false;
	}

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

	// Track what this window is responsible for so we can release unresolved items on close.
	TSet<FSoftObjectPath> WindowObjectPaths;
	for (const FAssetData& AD : AssetsToReview)
	{
		WindowObjectPaths.Add(AD.GetSoftObjectPath());
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(FText::FromString(TEXT("NsBonsai Asset Review")))
		.ClientSize(FVector2D(1200, 420))
		.SupportsMinimize(true)
		.SupportsMaximize(true);

	Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda([this, WindowObjectPaths](const TSharedRef<SWindow>&)
	{
		// Release anything that wasn't explicitly resolved (confirm/ignore) so it can be queued again later.
		for (const FSoftObjectPath& Path : WindowObjectPaths)
		{
			QueuedObjectPaths.Remove(Path);
		}

		bPopupOpen = false;
		if (ReviewQueue.Num() > 0)
		{
			RequestPopupDebounced();
		}
	}));

	Window->SetContent(
		SNew(SNsBonsaiReviewWindow)
		.Assets(AssetsToReview)
		.Manager(this)
		.ParentWindow(Window)
	);

	FSlateApplication::Get().AddWindow(Window);
}
#endif
