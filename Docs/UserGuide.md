# NsBonsai User Guide

## Purpose

NsBonsai keeps asset names in one pattern. When you add, import, or rename an asset and then save it, NsBonsai puts the asset in a review window. In the window, you pick the parts of the name, read the final name, and rename or ignore the asset. The default pattern is `<Type>_<Domain>_<Category>_<AssetName>_<Variant>`, for example `SM_Foliage_Tree_Birch_A`.

NsBonsai renames an asset only when you confirm it. It writes no metadata into assets, does not move assets to other folders, and does not scan the whole project. It is an editor module and is not part of a packaged game.

## Setup

1. Copy the `NsBonsai` folder into the `Plugins` folder of your project.
2. Open the project and enable **NsBonsai** in **Edit > Plugins**.
3. Restart the editor.
4. Open **Edit > Project Settings > Plugins > Ns Bonsai**.
5. In **Domains**, add at least one entry. Set its **Domain Token**, for example `Foliage`.
6. In the same entry, add tokens to **Categories**, for example `Tree` and `Rock`.

The plugin ships 39 **Type Rules** in its `Config/DefaultNsBonsai.ini`. Add a rule only for an asset class that is not in the list.

## Quick Start

1. Import a static mesh into the Content Browser.
2. Save the new asset.
3. The **NsBonsai Asset Review** window opens with one row for the asset.
4. Check the **Domain** and the **Category** of the row.
5. Check the **Asset Name**. Edit it if necessary, then push Enter.
6. Read the **Final Name** column.
7. Click the check button at the end of the row. NsBonsai renames the asset and removes the row.

## Common Tasks

### Review assets that exist already

1. Select the assets in the Content Browser.
2. Right-click and choose **Open with Bonsai**.
3. Rename or ignore each row in the review window.

These assets go to the window even if their names already match the pattern.

### Open the review queue

1. Open the **Tools** menu.
2. Choose **NsBonsai Review Queue...**.

This also adds new assets that you did not save yet. If the queue is empty, a notification shows "NsBonsai review queue is empty".

### Set the domain or the category of many rows

1. Select the rows in the review window. Use Ctrl+click or Shift+click.
2. In one of the selected rows, choose a **Domain** or a **Category**.

The value goes to all selected rows. A new domain clears the category and selects the first category of that domain.

### Add a domain or a category from the window

1. Open the **Domain** or **Category** list of a row.
2. Choose **+ Add Domain...** or **+ Add Category...**.
3. Type the token and click **Add**.

NsBonsai adds the token to the project settings and saves them. A new category goes to the domain of the row, or to **Global Categories** when domains are off.

### Change the asset name format of a row

1. Right-click in the **Asset Name** field of the row.
2. Choose one of these entries:
   - **Reset to original**: Use the current asset name.
   - **Use smart prefill**: Use the current name without known type, domain, category, and variant tokens.
   - **Uppercase acronyms (UI/VFX/etc.)**: Uppercase `AI`, `UI`, `UX`, `HUD`, `VFX`, `SFX`, `FX`, `NPC`, `LOD`, `IK`, and `FK`.
   - **To PascalCase**: Join the words in PascalCase.
   - **To snake_case**: Join the words in lower case with underscores.

### Ignore an asset

1. Click the cross button at the end of the row.

NsBonsai removes the row and does not rename the asset.

### Stage renames and run them together

1. Turn on **Dry Run** at the bottom of the review window.
2. Click the check button on each row to stage it. NsBonsai removes the row but does not rename the asset.
3. Click **Execute Renames (N)** to rename all staged assets.

### Stop the popup for a while

1. Click **Snooze 10m** in the review window.

The window closes. The queued assets come back in a popup after 10 minutes.

### Stop the automatic popup

1. Open **Edit > Project Settings > Plugins > Ns Bonsai**.
2. Set **Review Trigger Mode** to **Manual Only**.

NsBonsai still queues assets. Open the queue from the **Tools** menu. Set **Disabled** to stop the queue too.

## Settings

All settings are in **Edit > Project Settings > Plugins > Ns Bonsai**. NsBonsai saves them in `Config/DefaultNsBonsai.ini` of the project.

### Naming > Format

| Setting | Default | Effect |
|---|---|---|
| **Name Format Order** | Type, Domain, Category, Asset Name, Variant | The order of the parts in the final name. A part that is not in the list is not used. |
| **Join Separator** | `_` | The text between the parts. An empty value resets to `_`. |

### Naming > Components

| Setting | Default | Effect |
|---|---|---|
| **Use Domains** | true | Adds the domain part and the **Domain** column. |
| **Use Categories** | true | Adds the category part and the **Category** column. |
| **Use Variant** | true | Adds a letter suffix that makes the name unique in its folder. When false, a name collision blocks the rename. |
| **Use Asset Name Field** | true | Adds the asset name part and the **Asset Name** column. When false, the final name has no asset name part. |
| **Categories Must Belong To Domain** | true | A category must be in the list of the selected domain. When false, a category from any domain is valid. |
| **Allow Free Category If Empty** | false | When the domain (or **Global Categories**) has no categories, any category is valid. |

### Naming > Library

| Setting | Default | Effect |
|---|---|---|
| **Type Rules** | 39 rules from the plugin `Config/DefaultNsBonsai.ini` | Each rule maps a **Class Path** to a **Type Token**, for example `/Script/Engine.StaticMesh` to `SM`. A child class uses the rule of its nearest parent. |
| **Domains** | Empty | Each entry has a **Domain Token** and a list of **Categories**. |
| **Global Categories** | Empty | The category list when **Use Domains** is false. |

### Naming > Behavior

| Setting | Default | Effect |
|---|---|---|
| **Skip Compliant Assets** | true | Assets that already match the pattern do not go to the queue. |
| **Review Trigger Mode** | Automatic | **Automatic**: queue assets and open the popup. **Manual Only**: queue assets, open from the **Tools** menu. **Disabled**: no queue and no popup. |
| **Popup Threshold Count** | 1 | The popup opens only when the queue has this many assets. Below it, a notification shows with an **Open** button. Range 1 to 1000. |
| **Popup Cooldown Seconds** | 2.0 | The minimum time from a closed window to the next automatic popup. Range 0 to 60. |
| **Auto Close When Empty** | true | Closes the window when you rename or ignore the last row. |

### Naming > Normalization

| Setting | Default | Effect |
|---|---|---|
| **Token Normalization Rules** | Empty | Each rule replaces a **Deprecated Token** with a **Canonical Token**. The match ignores case. NsBonsai applies the rules to type, domain, and category tokens. |
| **Normalize AssetName Exact Tokens** | false | This setting has no effect. |

### UI

| Setting | Default | Effect |
|---|---|---|
| **Show Type Column** | true | Shows the **Type** column when the type part is in use. |
| **Show Domain Column** | true | This setting has no effect. The **Domain** column shows when the domain part is in use. |
| **Show Category Column** | true | This setting has no effect. The **Category** column shows when the category part is in use. |
| **Show Asset Name Column** | true | This setting has no effect. The **Asset Name** column shows when the asset name part is in use. |
| **Show Final Name Column** | true | Shows the **Final Name** preview column. |
| **Show Current Name Column** | true | Shows the **Asset** column with the current name. |
| **Compact Rows** | true | Uses less padding in each row. |

### Per-user data

NsBonsai keeps your recent domains and categories in `EditorPerProjectUserSettings`, 20 of each. The lists put recent tokens first, then the others in alphabetical order.

## Debug

- **Status column**: Each row shows `OK` (ready), `!` (a field is missing or not valid), `X` (a name conflict), or `-` (ignored). Hover over it to read the reason.
- **Final Name tooltip**: Hover over the final name to read the current name, the new name, and each part.
- **Rename log**: NsBonsai adds one line for each action to `Saved/Logs/NsBonsai_Rename.log`, in CSV form `OldPath,NewPath,Result`. The results are `Renamed`, `NoOp`, `RenameFailed`, `Ignored`, `BlockedMissingFields`, `ConflictNoVariant`, `DryRunQueued`, and `DryRunNoOp`.
- **Log category**: `LogNsBonsaiSettings` shows a warning when NsBonsai fixes a setting value.

NsBonsai has no console commands or CVars.

## Troubleshooting

| Problem | Cause | Fix |
|---|---|---|
| A row shows "Domain is required." | The domain list is empty, or the row has no domain. | Add a domain in **Domains**, or choose **+ Add Domain...** in the row. |
| A row shows "Domain token is not configured." | The domain is not in **Domains**. | Add the domain to **Domains**. |
| A row shows "Category is not valid for current configuration." | The category is not in the list of the domain. | Add the category to the domain, or set **Categories Must Belong To Domain** to false. |
| A row shows "Type token mapping missing for this asset class." | No **Type Rules** entry matches the class. | Add a rule for the class in **Type Rules**. |
| A row shows "Conflict: '...' already exists." | **Use Variant** is false and an asset with that name is in the folder. | Change the asset name, or set **Use Variant** to true. |
| A row shows "No free variant could be allocated." | All 676 variants of the name are taken in the folder. | Change the asset name. |
| The popup does not open | The queue is below **Popup Threshold Count**, the window is snoozed, or the mode is **Manual Only**. | Click **Open** on the notification, or use **Tools > NsBonsai Review Queue...**. |
| The popup does not open after you close the window | NsBonsai waits for new assets after you close the window. | Use **Tools > NsBonsai Review Queue...**. |
| A notification shows "NsBonsai is disabled in settings." | **Review Trigger Mode** is **Disabled**. | Set it to **Automatic** or **Manual Only**. |
| A warning about empty Global Categories | **Use Domains** is false, **Use Categories** is true, and **Global Categories** is empty. | Add tokens to **Global Categories**, or set **Allow Free Category If Empty** to true. |

## Limits

- The queue and the list of new unsaved assets live in memory. An editor restart clears them.
- The check button renames one row at a time. Dry Run is the only batch rename.
- Variants go from `A` to `Z`, then `AA` to `YZ`, 676 names in total.
- A rename keeps the asset in its folder.
- When **Use Variant** is true, the smart prefill removes a last word in all capital letters, for example `HUD`, as a variant.
- **Show Domain Column**, **Show Category Column**, **Show Asset Name Column**, and **Normalize AssetName Exact Tokens** have no effect.
