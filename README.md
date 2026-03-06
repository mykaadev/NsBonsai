# NsBonsai

NsBonsai is an Unreal Editor plugin that keeps asset names consistent without slowing your team down.

It watches newly added assets, waits until they are saved, then opens one compact review table where you can classify and rename in seconds.

Default pattern:

`<Type>_<Domain>_<Category>_<AssetName>_<Variant>`

Example:

`SM_Foliage_Tree_Birch_A`

## Why teams use it

Most naming tools are either too strict or too passive.

NsBonsai sits in the middle:

- Fast: one dense table, inline edits, multi-select bulk apply.
- Safe: collision-safe variant allocation and guarded rename callbacks.
- Practical: no SQLite, no asset metadata writes, no hidden data model.
- Low-noise: single-window queue flow with threshold, cooldown, and snooze.

## Core guarantees

- Editor-only module.
- No metadata written into assets.
- No external database required.
- Uses `AssetTools` for rename operations.
- Uses UE save events (`UPackage::PackageSavedWithContextEvent`) to process added assets after they are persisted.

## How it works (technical)

### 1) Detection and queueing

NsBonsai tracks assets through:

- `AssetRegistry.OnAssetAdded`
- `AssetRegistry.OnAssetRenamed`
- `UPackage::PackageSavedWithContextEvent`

Internal flow:

- Pending assets are tracked by package using soft paths.
- On package save, pending paths are resolved through the Asset Registry.
- Assets are queued once per session using a dedupe set.
- A short ticker debounce coalesces bursts.

### 2) Single-window manager

The review manager keeps one weak window reference and never opens duplicates.

If the window is already open, new queue items are appended to the same list and refreshed in place.

### 3) Review table UX

The review popup is a compact multi-column list with per-row controls:

- Status
- Current Asset
- Type
- Domain
- Category
- Asset Name
- Final Name preview
- Confirm / Ignore actions

Bulk behavior:

- Multi-select rows
- Changing Domain or Category on one selected row applies to selected rows
- Asset Name remains row-local by default

### 4) Rename execution

On confirm:

- Row state is validated against active settings.
- Final candidate is allocated with collision-safe variants.
- `IAssetTools::RenameAssets` is executed inside a transaction.
- Rename guard blocks re-queue noise from plugin-driven events.
- Success/failure is logged to `Saved/Logs/NsBonsai_Rename.log`.

## Naming model and settings

Settings live under Project Settings (`Ns Bonsai`).

### Pattern builder

- Component order is configurable via `NameFormatOrder`.
- Separator is configurable (`JoinSeparator`, default `_`).

Available components:

- `Type`
- `Domain`
- `Category`
- `AssetName`
- `Variant`

### Feature toggles

- Enable/disable Domains
- Enable/disable Categories
- Enable/disable Variant suffixes
- Enable/disable editable Asset Name field

### Token libraries

- Type Rules: class path -> type token
- Domain definitions with categories
- Global categories (for domain-disabled workflows)
- Optional normalization rules (`DeprecatedToken` -> `CanonicalToken`)

### Behavior controls

- Skip compliant assets
- Popup threshold
- Popup cooldown
- Auto-close window when empty

### Validation built in

- Dedupes TypeRules by class path
- Dedupes domains by domain token
- Dedupes categories per domain
- Sanitizes tokens and supports compliance checks

## Installation

1. Copy `NsBonsai` into your project's `Plugins/` directory.
2. Regenerate project files.
3. Build the project.
4. Enable the plugin in Unreal Editor and restart.

## Quick start

1. Create or import assets.
2. Save packages.
3. Review queued rows in the NsBonsai window.
4. Set Domain/Category, edit Asset Name if needed.
5. Confirm to rename or Ignore to dismiss.

Manual open:

- `Tools -> NsBonsai Review Queue...`

## Configuration example

```ini
[/Script/NsBonsai.NsBonsaiSettings]
JoinSeparator="_"
NameFormatOrder=(Type,Domain,Category,AssetName,Variant)

bUseDomains=True
bUseCategories=True
bUseVariant=True
bUseAssetNameField=True
bSkipCompliantAssets=True

+TypeRules=(ClassPath="/Script/Engine.StaticMesh",TypeToken="SM")
+TypeRules=(ClassPath="/Script/Engine.Texture",TypeToken="T")
+TypeRules=(ClassPath="/Script/Engine.Blueprint",TypeToken="BP")

+Domains=(DomainToken="Foliage",Categories=("Tree","Bush","Grass"))
+Domains=(DomainToken="UI",Categories=("Widget","Icon","Style"))
```

## Troubleshooting

### Ctrl+S does not trigger queueing

NsBonsai only queues assets when save callbacks fire and pending paths resolve to valid Asset Registry entries.

Check:

- plugin is enabled and editor restarted
- save event is binding correctly in your editor version
- assets are actually being saved to disk

### Duplicate rows or repeated popup behavior

NsBonsai uses a session dedupe set and a single-window policy. If behavior looks noisy, verify you are on the latest plugin revision and check rename guard/cooldown settings.

### Asset appears but cannot confirm

Row validation blocks rename when required components are missing or invalid for current settings. Hover status/final columns to inspect why.

## Developer notes

Main code areas:

- `FNsBonsaiReviewManager`: queue lifecycle, event hooks, popup policy
- `SNsBonsaiReviewWindow`: table UI, row actions, bulk apply
- `UNsBonsaiSettings`: schema, validation, token normalization
- `FNsBonsaiNameRules`: parse/build/compliance/collision-safe allocation
- `FNsBonsaiAssetEvaluator`: row prefill and candidate selection

## License

MIT (see `LICENSE`).
