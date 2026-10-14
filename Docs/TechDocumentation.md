# NsBonsai Technical Documentation

## Architecture

One editor module, `NsBonsai` (type `Editor`).

| Type | Role |
|---|---|
| `FNsBonsaiModule` | Starts the review manager and adds the menu entries: Tools, "NsBonsai Review Queue...", and the Content Browser "Open with Bonsai". |
| `FNsBonsaiReviewManager` | Tracks asset events, holds the pending list and the review queue, opens the window, and runs renames. |
| `SNsBonsaiReviewWindow` | The Slate review table, inside `NsBonsaiReviewManager.cpp`. |
| `FNsBonsaiAssetEvaluator` | Builds the default row values for one asset: type token, domain, category, asset name, and compliance. |
| `FNsBonsaiNameBuilder` | Parses existing names, sanitizes name parts, and builds the base name and the final name. |
| `FNsBonsaiNameRules` | Sanitize, compare, variant tokens (`A..Z`, then `AA..`), object paths, and type token lookup. |
| `UNsBonsaiSettings` | Project settings: format, components, library, behavior, normalization, and UI. |
| `UNsBonsaiUserSettings` | Per-user recent domains and categories. |

### Flow

1. `OnAssetAdded` tracks a new asset as pending, with its package name.
2. `OnPackageSaved` moves pending assets of that package to the review queue. Duplicates are removed by soft object path.
3. With `bSkipCompliantAssets`, the evaluator drops assets that already match the rules.
4. `RequestPopupDebounced` opens the window when the queue reaches `PopupThresholdCount` and the cooldown and snooze allow it. If the window is open, new rows are appended.
5. On confirm, the manager plans the new name, allocates a free variant against the asset registry, and renames through AssetTools. A guard flag stops the rename events from queueing the asset again.
6. With Dry Run, confirmed rows are staged, then renamed together on Execute Renames.

## Lifetime

- `Startup` binds the asset registry and package save events. `Shutdown` unbinds them.
- `DisableRuntimeState` clears state when the plugin must stop, for example on shutdown.
- The queue lives in memory only. It is empty after an editor restart.

## Extension Points

| Point | Use |
|---|---|
| `TypeRules`, `Domains`, `GlobalCategories` | The naming library for each project. |
| `TokenNormalizationRules` | Map old tokens to new ones. |
| `FNsBonsaiNameBuilder`, `FNsBonsaiNameRules` | Public static helpers. A validator or a commandlet can reuse them. |

## Dependencies

`Core`, `CoreUObject`, `Engine`, `Slate`, `SlateCore`, `DeveloperSettings`, `AssetRegistry`, `InputCore`, `AssetTools`, `UnrealEd`, `ToolMenus`, `ContentBrowser`, `MainFrame`. No other studio plugin.

## Performance

- The evaluator runs one time for each queued asset.
- Variant allocation queries the asset registry for each candidate name.
- Large imports queue many rows. Use `PopupThresholdCount`, `PopupCooldownSeconds`, or `ManualOnly` to batch them.

## Networking

Not applicable. Editor only.

## Testing

NsBonsai has no automated tests. Manual test path:

1. Add a Type Rule for `StaticMesh` and one Domain with one Category.
2. Import a mesh and save it. Make sure that one row shows.
3. Confirm the row. Make sure that the asset has the new name and the log file has a line.
4. Import a mesh with the same name. Make sure that the variant goes from `A` to `B`.
5. Turn on Dry Run. Confirm and execute. Make sure that all staged rows are renamed.

## Change Rules

- `NsBonsaiReviewManager.cpp` is about 2,900 lines and holds both the manager and the window. Split the window into its own file in the next large change.
- `UNsBonsaiUserSettings` has `MinAssetsToPopup`, `PopupCooldownSeconds`, and `bSkipCompliantAssets`. The code reads the project settings for these instead. Remove the user fields, or use them as per-user overrides.
- Keep the name parse and build logic in one place. Compliance and rename must use the same rules.
- A data validator that reports bad names in a build is a candidate feature. It can reuse `FNsBonsaiNameBuilder`.
