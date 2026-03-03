#include "NsBonsaiReviewManager.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Framework/Application/SlateApplication.h"
#include "IAssetTools.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableRow.h"
		FString PreviewName;
		FString Variant = TEXT("A");


		TArray<FString> Descriptors;
		TSet<FString> DescriptorKeys;
		TArray<FString> RecentDescriptorChips;
	static void RebuildPreview(FRowModel& Row)
	static void AddDescriptor(FRowModel& Row, const FString& RawDescriptor)
		const TArray<FString> Sanitized = FNsBonsaiNameBuilder::SanitizeDescriptors({RawDescriptor}, *Settings);
		const FString Descriptor = Sanitized[0];
		if (Row.DescriptorKeys.Contains(Key))
		Row.DescriptorKeys.Add(Key);
			Row.Descriptors.Sort([](const FString& A, const FString& B)
				return A.Compare(B, ESearchCase::IgnoreCase) < 0;

class SNsBonsaiReviewWindow : public SCompoundWidget

		for (const FAssetData& AssetData : InArgs._Assets)
			const FNsBonsaiEvaluationResult Eval = FNsBonsaiAssetEvaluator::Evaluate(AssetData, *Settings, *UserSettings);
			Row->AssetData = AssetData;
			Row->CurrentName = AssetData.AssetName.ToString();
			Row->RecentDescriptorChips = Eval.RecentDescriptorChips;


			for (const FString& Descriptor : Eval.ExistingDescriptors)
				NsBonsaiReview::AddDescriptor(*Row, Descriptor);

				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Review newly added assets (Domain + Category required)")))
				.OnGenerateRow(this, &SNsBonsaiReviewWindow::GenerateRow)
					.OnClicked(this, &SNsBonsaiReviewWindow::OnRenameAll)
					.OnClicked(this, &SNsBonsaiReviewWindow::OnCancel)
	TSharedRef<ITableRow> GenerateRow(TSharedPtr<NsBonsaiReview::FRowModel> Row, const TSharedRef<STableViewBase>& OwnerTable)
		return SNew(STableRow<TSharedPtr<NsBonsaiReview::FRowModel>>, OwnerTable)
						BuildDomainSelector(Row)
						BuildCategorySelector(Row)
						SNew(SButton)
						.Text(FText::FromString(TEXT("+")))
						.OnClicked_Lambda([this, Row]()
								if (ListView.IsValid())
								{
									ListView->RequestListRefresh();
								}
					SNew(STextBlock)
					.Text_Lambda([Row]
					{
						return FText::FromString(FString::Printf(TEXT("Preview: %s"), *Row->PreviewName));
					})
	TSharedRef<SWidget> BuildDomainSelector(TSharedPtr<NsBonsaiReview::FRowModel> Row)
	{
		return SNew(SComboBox<TSharedPtr<FName>>)
			.OptionsSource(&Row->DomainOptions)
			.OnGenerateWidget_Lambda([](TSharedPtr<FName> Item)
			{
				return SNew(STextBlock).Text(FText::FromName(Item.IsValid() ? *Item : NAME_None));
			})
			.OnSelectionChanged_Lambda([this, Row](TSharedPtr<FName> Item, ESelectInfo::Type)
			{
				if (!Item.IsValid())
				{
					return;
				}

				Row->SelectedDomain = *Item;
				Row->bDomainConfirmed = true;
				RefreshCategories(*Row);
				NsBonsaiReview::RebuildPreview(*Row);
				if (ListView.IsValid())
				{
					ListView->RequestListRefresh();
				}
			})
			[
				SNew(STextBlock)
				.Text_Lambda([Row]
				{
					return FText::FromName(Row->SelectedDomain);
				})
			];
	}

				.OnGenerateWidget_Lambda([](TSharedPtr<FName> Item)
					return SNew(STextBlock).Text(FText::FromName(Item.IsValid() ? *Item : NAME_None));
				.OnSelectionChanged_Lambda([Row](TSharedPtr<FName> Item, ESelectInfo::Type)
					if (!Item.IsValid())
					Row->SelectedCategory = *Item;
					SNew(STextBlock)
					.Text_Lambda([Row]

		return SNew(SEditableTextBox)

		for (const FString& Descriptor : Row->Descriptors)
				.Text(FText::FromString(Descriptor + TEXT(" ✕")))
				.OnClicked_Lambda([this, Row, Descriptor]()
					Row->Descriptors.Remove(Descriptor);
					Row->DescriptorKeys.Remove(Descriptor.ToLower());
					if (ListView.IsValid())
					{
						ListView->RequestListRefresh();
					}
					if (ListView.IsValid())
					{
						ListView->RequestListRefresh();
					}
				const FName Normalized = Settings->NormalizeToken(Category);
				if (!Normalized.IsNone() && !Categories.Contains(Normalized))
					Categories.Add(Normalized);
		Categories.Sort([UserSettings](const FName& A, const FName& B)
			const int32 ARecent = UserSettings->RecentCategories.IndexOfByKey(A);
			const int32 BRecent = UserSettings->RecentCategories.IndexOfByKey(B);
			const bool bARecent = ARecent != INDEX_NONE;
			const bool bBRecent = BRecent != INDEX_NONE;

			if (bARecent && bBRecent)
			{
				return ARecent < BRecent;
			}
			if (bARecent != bBRecent)
			{
				return bARecent;
			}
			return A.ToString().Compare(B.ToString(), ESearchCase::IgnoreCase) < 0;


		if (!Row.CategoryOptions.ContainsByPredicate([&Row](const TSharedPtr<FName>& Item)
		{
			return Item.IsValid() && *Item == Row.SelectedCategory;
		}))
			Row.SelectedCategory = Row.CategoryOptions.Num() > 0 && Row.CategoryOptions[0].IsValid()
				? *Row.CategoryOptions[0]
				: NAME_None;
		if (Rows.Num() == 0)
		{
			return false;
		}


		return true;
	FReply OnRenameAll()
			Renames.Emplace(Row->AssetData.GetSoftObjectPath(), Row->AssetData.PackagePath.ToString(), Row->PreviewName);

	FReply OnCancel()
	{


	QueuedAssetPaths.Reset();
	if (TSet<FSoftObjectPath>* Pending = PendingAssetsByPackage.Find(AssetData.PackageName))
		Pending->Remove(AssetData.GetSoftObjectPath());
		if (Pending->Num() == 0)
		{

	QueuedAssetPaths.Remove(AssetData.GetSoftObjectPath());
	const FSoftObjectPath OldPath(OldObjectPath);
		Pair.Value.Remove(OldPath);

	QueuedAssetPaths.Remove(OldPath);

		OpenPopupIfReady();
	TSet<FSoftObjectPath> PendingSet;
	if (!PendingAssetsByPackage.RemoveAndCopyValue(PackageName, PendingSet) || PendingSet.Num() == 0)
	TArray<FAssetData> AssetsInPackage;
	AssetRegistry.GetAssetsByPackageName(PackageName, AssetsInPackage, true);
	for (const FAssetData& AssetData : AssetsInPackage)
		if (PendingSet.Contains(AssetPath) && !QueuedAssetPaths.Contains(AssetPath))
			QueuedAssetPaths.Add(AssetPath);
		SchedulePopup();
void FNsBonsaiReviewManager::SchedulePopup()

void FNsBonsaiReviewManager::OpenPopupIfReady()
	bPopupScheduled = false;

	QueuedAssetPaths.Reset();
			SchedulePopup();
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
		TSharedRef<SWidget> CategoryWidget = BuildCategorySelector(Row);

						CategoryWidget
	TSharedRef<SWidget> BuildCategorySelector(TSharedPtr<NsBonsaiReview::FRowModel> Row)
	{
		if (Row->CategoryOptions.Num() > 0)
		{
			return SNew(SComboBox<TSharedPtr<FName>>)
				.OptionsSource(&Row->CategoryOptions)
				.OnGenerateWidget_Lambda([](TSharedPtr<FName> Name)
				{
					return SNew(STextBlock).Text(FText::FromName(Name.IsValid() ? *Name : NAME_None));
				})
				.OnSelectionChanged_Lambda([Row](TSharedPtr<FName> NewSelection, ESelectInfo::Type)
				{
					if (!NewSelection.IsValid())
					{
						return;
					}

					Row->SelectedCategory = *NewSelection;
					Row->bCategoryConfirmed = true;
					NsBonsaiReview::RebuildPreview(*Row);
				})
				[
					SNew(STextBlock).Text_Lambda([Row]
					{
						return FText::FromName(Row->SelectedCategory);
					})
				];
		}

		return SAssignNew(Row->FreeCategoryInput, SEditableTextBox)
			.HintText(FText::FromString(TEXT("Category")))
			.OnTextChanged_Lambda([Row](const FText& NewText)
			{
				const UNsBonsaiSettings* Settings = GetDefault<UNsBonsaiSettings>();
				const TArray<FString> Tokens = FNsBonsaiNameBuilder::SanitizeDescriptors({NewText.ToString()}, *Settings);
				Row->SelectedCategory = Tokens.Num() > 0 ? FName(*Tokens[0]) : NAME_None;
				Row->bCategoryConfirmed = Tokens.Num() > 0;
				NsBonsaiReview::RebuildPreview(*Row);
			});
	}

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
		TSharedRef<SWrapBox> Wrap = SNew(SWrapBox).UseAllottedWidth(true);
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
			Renames.Emplace(Row->AssetData.GetSoftObjectPath(), PackagePath, Row->PreviewName);

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

	// Use PackageSavedEvent for broad engine-version compatibility (older versions do not expose OnPackageSavedWithContext).
		TickHandle.Reset();

	PendingAssetsByPackage.Reset();
	ReviewQueue.Reset();
	QueuedObjectPaths.Reset();
	bPopupScheduled = false;
	bPopupOpen = false;
	FReply OnCancelClicked()
	{
		return CloseWindow();
	}

	FReply CloseWindow()
	{
		if (ParentWindow.IsValid())
		{
			ParentWindow.Pin()->RequestDestroyWindow();
	PendingAssetsByPackage.FindOrAdd(AssetData.PackageName).Add(AssetData.GetSoftObjectPath());
	if (TSet<FSoftObjectPath>* PendingSet = PendingAssetsByPackage.Find(AssetData.PackageName))
		PendingSet->Remove(AssetData.GetSoftObjectPath());
	QueuedObjectPaths.Remove(AssetData.GetSoftObjectPath());
	const FSoftObjectPath OldObjectSoftPath(OldObjectPath);
	for (TPair<FName, TSet<FSoftObjectPath>>& Pair : PendingAssetsByPackage)
		Pair.Value.Remove(OldObjectSoftPath);
	PendingAssetsByPackage.FindOrAdd(AssetData.PackageName).Add(AssetData.GetSoftObjectPath());
	QueuedObjectPaths.Remove(OldObjectSoftPath);
{
	TSet<FSoftObjectPath> PendingObjectPaths;
		const FSoftObjectPath AssetPath = AssetData.GetSoftObjectPath();
		if (PendingObjectPaths.Contains(AssetPath) && !QueuedObjectPaths.Contains(AssetPath))
			QueuedObjectPaths.Add(AssetPath);
	AssetRemovedHandle = AssetRegistry.OnAssetRemoved().AddRaw(this, &FNsBonsaiReviewManager::OnAssetRemoved);
	AssetRenamedHandle = AssetRegistry.OnAssetRenamed().AddRaw(this, &FNsBonsaiReviewManager::OnAssetRenamed);
	PackageSavedHandle = FCoreUObjectDelegates::OnPackageSavedWithContext.AddRaw(this, &FNsBonsaiReviewManager::OnPackageSaved);
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
	FCoreUObjectDelegates::OnPackageSavedWithContext.Remove(PackageSavedHandle);
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	}
}

void FNsBonsaiReviewManager::OnAssetAdded(const FAssetData& AssetData)
{
	PendingAssetsByPackage.FindOrAdd(AssetData.PackageName).Add(AssetData.ObjectPath);
}

void FNsBonsaiReviewManager::OnAssetRemoved(const FAssetData& AssetData)
{
	if (TSet<FName>* PendingSet = PendingAssetsByPackage.Find(AssetData.PackageName))
	{
		PendingSet->Remove(AssetData.ObjectPath);
		if (PendingSet->Num() == 0)
		{
			PendingAssetsByPackage.Remove(AssetData.PackageName);
		}
	}
	QueuedObjectPaths.Remove(AssetData.ObjectPath);
}

void FNsBonsaiReviewManager::OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
{
	const FName OldObjectPathName(*OldObjectPath);
	for (TPair<FName, TSet<FName>>& Pair : PendingAssetsByPackage)
	{
		Pair.Value.Remove(OldObjectPathName);
	}
	PendingAssetsByPackage.FindOrAdd(AssetData.PackageName).Add(AssetData.ObjectPath);
	QueuedObjectPaths.Remove(OldObjectPathName);
}

void FNsBonsaiReviewManager::OnPackageSaved(const FString&, UPackage* Package, const FObjectPostSaveContext&)
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
	TSet<FName> PendingObjectPaths;
	if (!PendingAssetsByPackage.RemoveAndCopyValue(PackageName, PendingObjectPaths) || PendingObjectPaths.Num() == 0)
	{
		return;
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> PackageAssets;
	AssetRegistry.GetAssetsByPackageName(PackageName, PackageAssets, true);

	for (const FAssetData& AssetData : PackageAssets)
	{
		if (PendingObjectPaths.Contains(AssetData.ObjectPath) && !QueuedObjectPaths.Contains(AssetData.ObjectPath))
		{
			QueuedObjectPaths.Add(AssetData.ObjectPath);
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
