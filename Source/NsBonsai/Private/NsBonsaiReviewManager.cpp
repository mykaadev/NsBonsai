#include "NsBonsaiReviewManager.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "IAssetTools.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "NsBonsaiAssetEvaluator.h"
#include "NsBonsaiNameBuilder.h"
#include "NsBonsaiSettings.h"
#include "NsBonsaiUserSettings.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

namespace NsBonsaiReview
{
	struct FRowModel
	{
		FAssetData AssetData;
		FString CurrentName;
		TArray<TSharedPtr<FName>> DomainOptions;
		TArray<TSharedPtr<FName>> CategoryOptions;
		TArray<FString> Descriptors;
		TArray<FString> RecentDescriptorChips;
		TSet<FString> DescriptorKeySet;
		FName SelectedDomain;
		FName SelectedCategory;
		FString Variant;
		FString PreviewName;
		bool bDomainConfirmed = false;
		bool bCategoryConfirmed = false;
		TSharedPtr<SEditableTextBox> DescriptorInput;
		TSharedPtr<SEditableTextBox> FreeCategoryInput;
	};

	void RebuildPreview(FRowModel& Row)
	{
		const UNsBonsaiSettings* Settings = GetDefault<UNsBonsaiSettings>();
		FNsBonsaiNameBuildInput BuildInput;
		BuildInput.TypeToken = Settings->ResolveTypeTokenForClassPath(Row.AssetData.AssetClassPath);
		BuildInput.DomainToken = Row.SelectedDomain;
		BuildInput.CategoryToken = Row.SelectedCategory;
		BuildInput.Descriptors = Row.Descriptors;
		BuildInput.VariantToken = Row.Variant;
		Row.PreviewName = FNsBonsaiNameBuilder::BuildFinalAssetName(BuildInput, *Settings);
	}

	void AddDescriptor(FRowModel& Row, const FString& RawDescriptor)
	{
		const UNsBonsaiSettings* Settings = GetDefault<UNsBonsaiSettings>();
		TArray<FString> Sanitized = FNsBonsaiNameBuilder::SanitizeDescriptors({RawDescriptor}, *Settings);
		if (Sanitized.Num() == 0)
		{
			return;
		}

		const FString& Descriptor = Sanitized[0];
		const FString Key = Descriptor.ToLower();
		if (Row.DescriptorKeySet.Contains(Key))
		{
			return;
		}

		Row.DescriptorKeySet.Add(Key);
		Row.Descriptors.Add(Descriptor);
		if (Settings->bSortDescriptorsAlpha)
		{
			Row.Descriptors.Sort([](const FString& Left, const FString& Right)
			{
				return Left.Compare(Right, ESearchCase::IgnoreCase) < 0;
			});
		}
		RebuildPreview(Row);
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
		const UNsBonsaiSettings* Settings = GetDefault<UNsBonsaiSettings>();
		const UNsBonsaiUserSettings* UserSettings = GetDefault<UNsBonsaiUserSettings>();

		for (const FAssetData& Asset : InArgs._Assets)
		{
			FNsBonsaiEvaluationResult Eval = FNsBonsaiAssetEvaluator::Evaluate(Asset, *Settings, *UserSettings);
			TSharedPtr<NsBonsaiReview::FRowModel> Row = MakeShared<NsBonsaiReview::FRowModel>();
			Row->AssetData = Asset;
			Row->CurrentName = Asset.AssetName.ToString();
			Row->SelectedDomain = Eval.PreselectedDomain;
			Row->SelectedCategory = Eval.PreselectedCategory;
			Row->Variant = TEXT("A");
			for (const FName Domain : Eval.DomainCandidates)
			{
				Row->DomainOptions.Add(MakeShared<FName>(Domain));
			}
			for (const FName Category : Eval.CategoryCandidates)
			{
				Row->CategoryOptions.Add(MakeShared<FName>(Category));
			}

			Row->RecentDescriptorChips = Eval.RecentDescriptorChips;
			for (const FString& Existing : Eval.ExistingDescriptors)
			{
				NsBonsaiReview::AddDescriptor(*Row, Existing);
			}
			NsBonsaiReview::RebuildPreview(*Row);
			Rows.Add(Row);
		}

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(8)
			[
				SNew(STextBlock).Text(FText::FromString(TEXT("Review newly added assets (Domain + Category required)")))
			]
			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8)
			[
				SAssignNew(ListView, SListView<TSharedPtr<NsBonsaiReview::FRowModel>>)
				.ListItemsSource(&Rows)
				.OnGenerateRow(this, &SNsBonsaiReviewWindow::OnGenerateRow)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(8)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(4)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Rename All")))
					.IsEnabled(this, &SNsBonsaiReviewWindow::CanRenameAll)
					.OnClicked(this, &SNsBonsaiReviewWindow::OnRenameAllClicked)
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
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<NsBonsaiReview::FRowModel> Row, const TSharedRef<STableViewBase>& Owner)
	{
		return SNew(STableRow<TSharedPtr<NsBonsaiReview::FRowModel>>, Owner)
		[
			SNew(SBorder)
			.Padding(6)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("Current: %s"), *Row->CurrentName)))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(2)
					[
						SNew(SComboBox<TSharedPtr<FName>>)
						.OptionsSource(&Row->DomainOptions)
						.OnGenerateWidget_Lambda([](TSharedPtr<FName> Name){ return SNew(STextBlock).Text(FText::FromName(Name.IsValid()?*Name:NAME_None)); })
						.OnSelectionChanged_Lambda([this, Row](TSharedPtr<FName> NewSelection, ESelectInfo::Type)
						{
							if (!NewSelection.IsValid()) return;
							Row->SelectedDomain = *NewSelection;
							Row->bDomainConfirmed = true;
							RefreshCategories(*Row);
							NsBonsaiReview::RebuildPreview(*Row);
							if (ListView.IsValid()) { ListView->RequestListRefresh(); }
						})
						[
							SNew(STextBlock).Text_Lambda([Row]{ return FText::FromName(Row->SelectedDomain); })
						]
					]
					+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(2)
					[
						Row->CategoryOptions.Num() > 0
						? StaticCastSharedRef<SWidget>(
							SNew(SComboBox<TSharedPtr<FName>>)
							.OptionsSource(&Row->CategoryOptions)
							.OnGenerateWidget_Lambda([](TSharedPtr<FName> Name){ return SNew(STextBlock).Text(FText::FromName(Name.IsValid()?*Name:NAME_None)); })
							.OnSelectionChanged_Lambda([Row](TSharedPtr<FName> NewSelection, ESelectInfo::Type)
							{
								if (!NewSelection.IsValid()) return;
								Row->SelectedCategory = *NewSelection;
								Row->bCategoryConfirmed = true;
								NsBonsaiReview::RebuildPreview(*Row);
							})
							[
								SNew(STextBlock).Text_Lambda([Row]{ return FText::FromName(Row->SelectedCategory); })
							]
						)
						: StaticCastSharedRef<SWidget>(
							SAssignNew(Row->FreeCategoryInput, SEditableTextBox)
							.HintText(FText::FromString(TEXT("Category")))
							.OnTextChanged_Lambda([Row](const FText& NewText)
							{
								const UNsBonsaiSettings* Settings = GetDefault<UNsBonsaiSettings>();
								TArray<FString> Tokens = FNsBonsaiNameBuilder::SanitizeDescriptors({NewText.ToString()}, *Settings);
								Row->SelectedCategory = Tokens.Num() > 0 ? FName(*Tokens[0]) : NAME_None;
								Row->bCategoryConfirmed = Tokens.Num() > 0;
								NsBonsaiReview::RebuildPreview(*Row);
							})
						)
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(2)
					[
						SAssignNew(Row->DescriptorInput, SEditableTextBox)
						.HintText(FText::FromString(TEXT("Descriptor")))
					]
					+ SHorizontalBox::Slot().AutoWidth().Padding(2)
					[
						SNew(SButton).Text(FText::FromString(TEXT("+"))).OnClicked_Lambda([this, Row]()
						{
							if (Row->DescriptorInput.IsValid())
							{
								NsBonsaiReview::AddDescriptor(*Row, Row->DescriptorInput->GetText().ToString());
								Row->DescriptorInput->SetText(FText::GetEmpty());
								if (ListView.IsValid()) { ListView->RequestListRefresh(); }
							}
							return FReply::Handled();
						})
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					BuildDescriptorChips(Row)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					SNew(STextBlock).Text_Lambda([Row]{ return FText::FromString(FString::Printf(TEXT("Preview: %s"), *Row->PreviewName)); })
				]
			]
		];
	}

	TSharedRef<SWidget> BuildDescriptorChips(TSharedPtr<NsBonsaiReview::FRowModel> Row)
	{
		TSharedRef<SWrapBox> Wrap = SNew(SWrapBox).UseAllottedSize(true);
		for (const FString& Desc : Row->Descriptors)
		{
			Wrap->AddSlot()
			[
				SNew(SButton)
				.Text(FText::FromString(Desc + TEXT(" ✕")))
				.OnClicked_Lambda([this, Row, Desc]()
				{
					Row->Descriptors.Remove(Desc);
					Row->DescriptorKeySet.Remove(Desc.ToLower());
					NsBonsaiReview::RebuildPreview(*Row);
					if (ListView.IsValid()) { ListView->RequestListRefresh(); }
					return FReply::Handled();
				})
			];
		}

		for (const FString& Recent : Row->RecentDescriptorChips)
		{
			Wrap->AddSlot()
			[
				SNew(SButton)
				.Text(FText::FromString(Recent))
				.OnClicked_Lambda([this, Row, Recent]()
				{
					NsBonsaiReview::AddDescriptor(*Row, Recent);
					if (ListView.IsValid()) { ListView->RequestListRefresh(); }
					return FReply::Handled();
				})
			];
		}

		return Wrap;
	}

	void RefreshCategories(NsBonsaiReview::FRowModel& Row)
	{
		const UNsBonsaiSettings* Settings = GetDefault<UNsBonsaiSettings>();
		const UNsBonsaiUserSettings* UserSettings = GetDefault<UNsBonsaiUserSettings>();

		TArray<FName> Categories;
		for (const FNsBonsaiDomainDef& DomainDef : Settings->Domains)
		{
			if (Settings->NormalizeToken(DomainDef.DomainToken) != Row.SelectedDomain)
			{
				continue;
			}

			for (const FName Category : DomainDef.Categories)
			{
				const FName NormalizedCategory = Settings->NormalizeToken(Category);
				if (!NormalizedCategory.IsNone() && !Categories.Contains(NormalizedCategory))
				{
					Categories.Add(NormalizedCategory);
				}
			}
		}

		Categories.Sort([UserSettings](const FName& Left, const FName& Right)
		{
			const int32 LeftIdx = UserSettings->RecentCategories.IndexOfByKey(Left);
			const int32 RightIdx = UserSettings->RecentCategories.IndexOfByKey(Right);
			const bool bLeftRecent = LeftIdx != INDEX_NONE;
			const bool bRightRecent = RightIdx != INDEX_NONE;
			if (bLeftRecent && bRightRecent) return LeftIdx < RightIdx;
			if (bLeftRecent != bRightRecent) return bLeftRecent;
			return Left.ToString().Compare(Right.ToString(), ESearchCase::IgnoreCase) < 0;
		});

		Row.CategoryOptions.Reset();
		for (const FName Category : Categories)
		{
			Row.CategoryOptions.Add(MakeShared<FName>(Category));
		}
		if (!Row.CategoryOptions.ContainsByPredicate([&Row](const TSharedPtr<FName>& C){ return C.IsValid() && *C == Row.SelectedCategory; }))
		{
			Row.SelectedCategory = Row.CategoryOptions.Num() > 0 && Row.CategoryOptions[0].IsValid() ? *Row.CategoryOptions[0] : NAME_None;
			Row.bCategoryConfirmed = false;
		}
	}

	bool CanRenameAll() const
	{
		for (const TSharedPtr<NsBonsaiReview::FRowModel>& Row : Rows)
		{
			if (!Row.IsValid() || !Row->bDomainConfirmed || !Row->bCategoryConfirmed || Row->SelectedDomain.IsNone() || Row->SelectedCategory.IsNone())
			{
				return false;
			}
		}
		return Rows.Num() > 0;
	}

	FReply OnRenameAllClicked()
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		TArray<FAssetRenameData> Renames;
		UNsBonsaiUserSettings* UserSettings = GetMutableDefault<UNsBonsaiUserSettings>();

		for (const TSharedPtr<NsBonsaiReview::FRowModel>& Row : Rows)
		{
			if (!Row.IsValid())
			{
				continue;
			}

			const FString PackagePath = Row->AssetData.PackagePath.ToString();
			const FSoftObjectPath OldPath = Row->AssetData.GetSoftObjectPath();

			// Make sure this is something like "/Game/SomeFolder" (no trailing slash)
			const FString NewPackagePath = PackagePath;
			const FString NewObjectPathString = FString::Printf(TEXT("%s/%s.%s"), *NewPackagePath, *Row->PreviewName, *Row->PreviewName);
			const FSoftObjectPath NewPath(NewObjectPathString);
			Renames.Emplace(OldPath, NewPath, /*bOnlyFixSoftReferences*/ false, /*bGenerateRedirectors*/ true);

			UserSettings->TouchDomain(Row->SelectedDomain);
			UserSettings->TouchCategory(Row->SelectedCategory);
			for (const FString& Descriptor : Row->Descriptors)
			{
				UserSettings->TouchDescriptor(Descriptor);
			}
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

	TWeakPtr<SWindow> ParentWindow;
	TSharedPtr<SListView<TSharedPtr<NsBonsaiReview::FRowModel>>> ListView;
	TArray<TSharedPtr<NsBonsaiReview::FRowModel>> Rows;
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
	TArray<FAssetData> PackageAssets;
	AssetRegistry.GetAssetsByPackageName(PackageName, PackageAssets, false, true);

	for (const FAssetData& AssetData : PackageAssets)
	{
		const FSoftObjectPath Path = AssetData.GetSoftObjectPath();
		if (PendingObjectPaths.Contains(Path) && !QueuedObjectPaths.Contains(Path))
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
		.ClientSize(FVector2D(1000, 700))
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
