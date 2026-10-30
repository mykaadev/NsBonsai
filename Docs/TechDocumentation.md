# NsBonsai Technical Documentation

## Architecture

### Modules

| Module | Type | Loading Phase | Content |
|---|---|---|---|
| `NsBonsai` | Editor | Default | The settings, the name rules, the review manager, the review window, and the menu entries. |

The plugin has no `Content/` folder. `Config/DefaultNsBonsai.ini` holds the default `TypeRules` for the `NsBonsai` config file.

### Folders

| Folder | Files |
|---|---|
| `Source/NsBonsai/Public` | `NsBonsai.h`, `NsBonsaiSettings.h`, `NsBonsaiUserSettings.h`, `NsBonsaiNameRules.h`, `NsBonsaiNameBuilder.h`, `NsBonsaiAssetEvaluator.h`, `NsBonsaiReviewManager.h` |
| `Source/NsBonsai/Private` | One `.cpp` file for each header. `NsBonsaiReviewManager.cpp` also holds the window class `SNsBonsaiReviewWindow`. |

### Main Classes

| Class | Responsibility |
|---|---|
| `FNsBonsaiModule` | Creates and owns the review manager. Adds **Tools > NsBonsai Review Queue...** and the Content Browser entry **Open with Bonsai**. |
| `FNsBonsaiReviewManager` | Listens to asset registry and package save events. Holds the pending assets, the review queue, the popup timing, and the single review window. |
| `SNsBonsaiReviewWindow` | The Slate table, declared in `NsBonsaiReviewManager.cpp`. It builds the rows, the previews, and the rename plans, and calls `IAssetTools::RenameAssets`. |
| `FNsBonsaiNameRules` | Static helpers: sanitize, validate a row, build the preview, allocate a free variant, parse a name against the format, and check compliance. |
| `FNsBonsaiNameBuilder` | Static helpers: parse an old name into tokens, sanitize asset name parts, build a base name and a final name. |
| `FNsBonsaiAssetEvaluator` | Gives the default row values for one asset: type token, sorted domain and category candidates, and the preselected domain and category. |
| `UNsBonsaiSettings` | `UDeveloperSettings`, `Config = NsBonsai`, `DefaultConfig`. Shows in **Project Settings > Plugins > Ns Bonsai**. It also validates and sanitizes its own values. |
| `UNsBonsaiUserSettings` | `Config = EditorPerProjectUserSettings`. Recent domain and category tokens for each user. |

### Data Flow

All the code runs on the game thread.

1. `OnAssetAdded` skips compliant assets (if **Skip Compliant Assets** is on) and adds the rest to `PendingByPackage`.
2. `OnAssetRenamed` moves a rename made outside NsBonsai to the new path. It updates a queued asset or an open row, or tracks the new path as pending.
3. `OnPackageSaved` adds the package name to `SavedPackagesToProcess`.
4. `Tick` runs every 0.2 seconds on the core ticker. It calls `ProcessSavedPackages`, which moves the pending assets of each saved package to `ReviewQueue`. The compliance check runs again here.
5. `RequestPopupDebounced` schedules the popup 0.35 seconds later, but not before the snooze time or the cooldown time. Below the threshold, it shows a notification instead.
6. `Tick` opens the popup when the time comes. If a modal window is open, it waits 0.25 seconds more. If the review window is open, the queue goes into it.
7. `SNsBonsaiReviewWindow::AddAssets` calls `FNsBonsaiAssetEvaluator::Evaluate` and `FNsBonsaiNameRules::SmartPrefillAssetName` for each row.
8. Each edit calls `RebuildAllPreviews`, which runs `AllocateFinalNameWithVariant` for every row with one shared set of reserved paths.
9. The check button calls `BuildRenamePlans`, then starts a Slate active timer. The timer calls `RenameAssets` inside an `FScopedTransaction`, then reads the asset registry to see which renames worked.
10. While the rename runs, and for 3 seconds after it, `IsApplyingRename` makes the manager ignore its own asset events.

`OpenReviewQueueNow` (Tools menu) also calls `FlushPendingToReviewQueue`, which queues pending assets without a save. `OpenReviewForAssets` (**Open with Bonsai**) queues the selected assets without a compliance check.

## Lifetime

1. `StartupModule` creates `FNsBonsaiReviewManager` and calls `Startup`.
2. `Startup` binds `OnAssetAdded`, `OnAssetRemoved`, `OnAssetRenamed`, `UPackage::PackageSavedWithContextEvent`, and a 0.2 second `FTSTicker`.
3. `StartupModule` registers a `UToolMenus` startup callback, which extends `LevelEditor.MainMenu.Tools` and `ContentBrowser.AssetContextMenu`.
4. The review window lives while it is open. When it closes, the manager puts its unhandled rows back in the queue and stops the automatic popup until the queue grows.
5. When **Review Trigger Mode** is **Disabled**, `Tick` calls `DisableRuntimeState`. It closes the window and clears the pending assets, the queue, and the timers.
6. `ShutdownModule` calls `UnRegisterStartupCallback` and `UnregisterOwner`, then `Shutdown`. `Shutdown` removes the delegates and the ticker, and clears all state.

The `UNsBonsaiSettings` class default object validates its values in `PostInitProperties` and in `PostEditChangeProperty`. If a value changes, it calls `SaveConfig`. It logs each validation message and shows a notification. `UNsBonsaiUserSettings::Save` runs after each rename or dry-run stage.

The queue and the pending assets live in memory only.

## Extension Points

The plugin has no virtual functions for extension, no delegates, and no interfaces. These are the points that a project can use:

| Point | Kind | Signature or use |
|---|---|---|
| `UNsBonsaiSettings` | Settings class | Read with `GetDefault<UNsBonsaiSettings>()`. Edit `TypeRules`, `Domains`, `GlobalCategories`, and `TokenNormalizationRules` in the project config. |
| `FNsBonsaiNameRules::IsCompliant` | Static, exported | `static bool IsCompliant(const FAssetData& AssetData, const UNsBonsaiSettings& Settings)` |
| `FNsBonsaiNameRules::ValidateRowState` | Static, exported | `static FNsBonsaiValidationResult ValidateRowState(const FAssetData& AssetData, const FNsBonsaiRowState& RowState, const UNsBonsaiSettings& Settings)` |
| `FNsBonsaiNameRules::AllocateFinalNameWithVariant` | Static, exported | `static bool AllocateFinalNameWithVariant(const FAssetData&, const FNsBonsaiRowState&, const UNsBonsaiSettings&, IAssetRegistry&, TSet<FString>& InOutReservedObjectPaths, FString& OutFinalName, FSoftObjectPath& OutNewPath, FString& OutError)` |
| `FNsBonsaiNameRules::SmartPrefillAssetName` | Static, exported | `static FString SmartPrefillAssetName(const FString& OriginalAssetName, const UNsBonsaiSettings& Settings)` |
| `FNsBonsaiNameBuilder::ParseExistingAssetName` | Static, exported | `static FNsBonsaiParsedName ParseExistingAssetName(const FString& ExistingName, const UNsBonsaiSettings& Settings)` |
| `FNsBonsaiNameBuilder::BuildFinalAssetName` | Static, exported | `static FString BuildFinalAssetName(const FNsBonsaiNameBuildInput& Input, const UNsBonsaiSettings& Settings)` |
| `FNsBonsaiAssetEvaluator::Evaluate` | Static, exported | `static FNsBonsaiEvaluationResult Evaluate(const FAssetData& AssetData, const UNsBonsaiSettings& Settings, const UNsBonsaiUserSettings& UserSettings)` |

`FNsBonsaiReviewManager` is exported, but the module keeps its instance in a private member with no getter.

## Dependencies

| Dependency | Kind | Why |
|---|---|---|
| `Core` | Public module | Core types and `FTSTicker`. |
| `CoreUObject` | Private module | `UObject`, config, soft object paths. |
| `Engine` | Private module | Listed in `Build.cs`. |
| `Slate`, `SlateCore` | Private modules | The review window and notifications. |
| `DeveloperSettings` | Private module | `UDeveloperSettings` base of `UNsBonsaiSettings`. |
| `AssetRegistry` | Private module | Asset events and asset lookups. |
| `AssetTools` | Private module | `IAssetTools::RenameAssets`. |
| `UnrealEd` | Private module | `ObjectTools::SanitizeObjectName` and `FScopedTransaction`. |
| `ToolMenus` | Private module | The Tools menu and Content Browser menu entries. |
| `ContentBrowser` | Private module | `UContentBrowserAssetContextMenuContext`. |
| `MainFrame` | Private module | The parent window of the review window. |
| `InputCore` | Private module | Listed in `Build.cs`. The source does not use it directly. |

The public headers include `Engine/DeveloperSettings.h` and `AssetRegistry/AssetData.h`. A module that includes them must add `DeveloperSettings` and `CoreUObject` itself, because these are private dependencies here. The `.uplugin` lists no plugin dependencies.

## Performance

- The ticker runs every 0.2 seconds while the editor is open.
- Each `OnAssetAdded` event runs a compliance check when **Skip Compliant Assets** is on. The handler has no guard for the first asset registry scan.
- The status glyph, color, tooltip, and check button of each row call `EvaluateRowStatus`. It validates the row and allocates a variant against the asset registry each time Slate asks for the value.
- Each edit in the window runs `RebuildAllPreviews` for all rows. Variant allocation tries up to 676 names, with one asset registry lookup for each name.
- `ResolveTypeTokenForClassPath` can call `LoadObject<UClass>` when the class is not in memory.
- Each rename result appends one line to `Saved/Logs/NsBonsai_Rename.log` with a synchronous file write.
