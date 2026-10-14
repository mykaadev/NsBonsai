# NsBonsai User Guide

## Purpose

NsBonsai is an editor tool that keeps asset names consistent. It finds new and imported assets, waits for the save, and opens one review table. In the table, you pick the parts of the name and confirm the rename.

The default name pattern is `<Type>_<Domain>_<Category>_<AssetName>_<Variant>`, for example `SM_Foliage_Tree_Birch_A`.

NsBonsai writes no metadata into assets. It renames an asset only when you confirm. It is an editor-only plugin and is not part of a packaged game.

## Setup

1. Add `NsBonsai` to the `Plugins` list of the `.uproject` file.
2. Open Project Settings, "Ns Bonsai".
3. Add at least one Type Rule, for example `StaticMesh` to `SM`.
4. Add Domains and their Categories, or Global Categories if you turn off Domains.

## Quick Start

1. Make or import an asset.
2. Save it.
3. In the review table, pick the Domain and the Category.
4. Edit the Asset Name if needed. Read the Final Name column.
5. Click the check button to rename, or the cross button to ignore the row.

## Common Tasks

### Rename many assets

1. Select several rows.
2. Change the Domain or the Category in one selected row. The change applies to all selected rows.
3. Confirm each row, or use Dry Run (below).

### Rename assets that exist already

Select the assets in the Content Browser. Right-click and choose "Open with Bonsai".

### Open the queue

Use Tools, "NsBonsai Review Queue...".

### Stage renames first

1. Turn on Dry Run in the review window.
2. Confirm rows. They are staged, not renamed.
3. Click Execute Renames to apply all staged rows.

### Stop popups for a while

Click "Snooze 10m" in the review window. Or set `ReviewTriggerMode` to `ManualOnly` or `Disabled`.

## Rules

- Keep the Type Rules, Domains, and Categories short and owned by the team.
- Keep `NameFormatOrder` and `JoinSeparator` the same for the whole project. Compliance checks use them.
- Keep `bUseVariant` on. It stops name collisions.
- Do not rename assets by hand when NsBonsai tracks them. Use the table.

## Settings

Project Settings, "Ns Bonsai". The README has the full table with the effect of each setting.

| Group | Settings | Default |
|---|---|---|
| Format | `NameFormatOrder`, `JoinSeparator` | Type, Domain, Category, AssetName, Variant. `_` |
| Components | `bUseDomains`, `bUseCategories`, `bUseVariant`, `bUseAssetNameField` | All on |
| Components | `bCategoriesMustBelongToDomain` | On |
| Components | `bAllowFreeCategoryTextIfNoCategories` | Off |
| Library | `TypeRules`, `Domains`, `GlobalCategories` | Empty |
| Behavior | `bSkipCompliantAssets` | On |
| Behavior | `ReviewTriggerMode` | `Automatic` |
| Behavior | `PopupThresholdCount` | 1 |
| Behavior | `PopupCooldownSeconds` | 2 |
| Behavior | `bAutoCloseWindowWhenEmpty` | On |
| Normalization | `TokenNormalizationRules`, `bNormalizeAssetNameExactMatch` | Empty, off |
| UI | `bShowTypeColumn`, `bShowDomainColumn`, `bShowCategoryColumn`, `bShowAssetNameColumn`, `bShowFinalNameColumn`, `bShowCurrentNameColumn`, `bCompactRows` | All on |

`ReviewTriggerMode`:

| Value | Behavior |
|---|---|
| `Automatic` | Tracks assets and opens the popup by threshold and cooldown. |
| `ManualOnly` | Tracks assets. You open the queue from the menu. |
| `Disabled` | No tracking and no popup. |

## Debug

- Each rename and each dry-run result is added to `Saved/Logs/NsBonsai_Rename.log`, a CSV file with `OldPath,NewPath,Result`.
- Settings warnings use the log category `LogNsBonsaiSettings`.

## Limits

- Editor only. It does not run in a packaged game or in commandlets with no UI.
- The Type token comes from the class path rules. An asset class with no rule gets a validation warning.
- Recent Domains and Categories are kept for each user in `EditorPerProjectUserSettings`.
