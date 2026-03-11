<!-- GH_ONLY_START -->
<h1 align="center">
  <br>
  <a href="https://github.com/mykaadev/NsBonsai">
    <img src="https://github.com/mykaadev/NsBonsai/blob/main/Resources/Banner.png" alt="NsBonsai" width="400">
  </a>
</h1>

<h4 align="center">Prune asset names into a consistent shape - fast and safe</h4>

<div align="center">
    <a href="https://github.com/mykaadev/NsBonsai/commits/main"><img src="https://img.shields.io/github/last-commit/mykaadev/NsBonsai?style=plastic&logo=github&logoColor=white" alt="GitHub Last Commit"></a>
    <a href="https://github.com/mykaadev/NsBonsai/issues"><img src="https://img.shields.io/github/issues-raw/mykaadev/NsBonsai?style=plastic&logo=github&logoColor=white" alt="GitHub Issues"></a>
    <a href="https://github.com/mykaadev/NsBonsai/pulls"><img src="https://img.shields.io/github/issues-pr-raw/mykaadev/NsBonsai?style=plastic&logo=github&logoColor=white" alt="GitHub Pull Requests"></a>
    <a href="https://github.com/mykaadev/NsBonsai"><img src="https://img.shields.io/github/stars/mykaadev/NsBonsai?style=plastic&logo=github" alt="GitHub Stars"></a>
    <a href="https://twitter.com/mykaadev/"><img src="https://img.shields.io/twitter/follow/mykaadev?style=plastic&logo=x" alt="Twitter Follow"></a>

<p style="display:none;">
  <a href="#-summary">🌳 Summary</a> •
  <a href="#-why-nsbonsai">🎯 Why</a> •
  <a href="#-features">✨ Features</a> •
  <a href="#-getting-started">🚀 Getting Started</a> •
  <a href="#-configuration">🧩 Configuration</a> •
  <a href="#-how-it-works">🧠 How it works</a> •
  <a href="#-best-practices">📚 Best practices</a> •
  <a href="#-requirements">⚙️ Requirements</a> •
  <a href="#-installation">🛠️ Installation</a> •
  <a href="#-credits">❤️ Credits</a> •
  <a href="#-support">📞 Support</a> •
  <a href="#-license">📃 License</a>
</p>

<a href="https://buymeacoffee.com/mykaadev"><img src="https://www.svgrepo.com/show/476855/coffee-to-go.svg" alt="Coffee" width="50px"></a>
<p><b>Buy me a coffee!</b></p>
</div>
<!-- GH_ONLY_END -->

## 🌳 Summary
**NsBonsai** is a lightweight Unreal Engine editor plugin that standardises asset names while you work.
It detects newly created/imported assets, waits for save, and opens one compact review table to classify and rename safely.

Default pattern:

`<Type>_<Domain>_<Category>_<AssetName>_<Variant>`

Example:

`SM_Foliage_Tree_Birch_A`

> **Zero-diff by design:** NsBonsai does **not** write metadata into assets.
> It only renames assets when you explicitly confirm.

<div align="center">
  <img src="https://github.com/mykaadev/NsBonsai/blob/main/Resources/Showcase.gif" width="700" alt="NsBonsai Showcase" />
</div>

## 🎯 Why NsBonsai
Naming is not just pretty formatting. It is searchability, automation, maintainability, and team sanity.

NsBonsai focuses on:
- **Speed:** compact table, inline edit flow, and bulk actions.
- **Safety:** collision-safe variants and transaction-wrapped rename.
- **Low friction:** quick setup and predictable behavior.
- **Team consistency:** central token libraries and deterministic output.

## ✨ Features
- **Single review window policy:** no duplicate popups, no duplicate rows.
- **Compact conveyor table UI:** one row per asset with inline controls.
- **Multi-select bulk apply:** Domain/Category edits apply to selected rows.
- **Immediate actions:** ✅ rename now, ❌ ignore now.
- **Smart AssetName prefill:** strips known structural tokens from original names.
- **Collision-safe variant allocation:** `A..Z`, then `AA..`.
- **Skip compliant assets:** migration-safe queue behavior.
- **Trigger modes:** Automatic, Manual Only, Disabled.
- **Noise controls:** threshold, cooldown, snooze.
- **Manual actions:** Tools menu queue opener + Content Browser `Open with Bonsai`.
- **Dry Run mode:** stage then execute renames in batch.

## 🚀 Getting Started
1. Open `Edit -> Project Settings -> Ns Bonsai`.
2. Add at least one **Type Rule** (`ClassPath -> TypeToken`).
3. Add your Domains/Categories (or Global Categories if you disable Domains).
4. Create or import an asset.
5. Save package.
6. In the table, set Domain/Category/AssetName and confirm.

### Table workflow (daily use)
- Select **Domain** and **Category**.
- Edit **Asset Name** (prefilled smartly).
- Check **Final Name** preview.
- Click ✅ to rename now.
- Click ❌ to ignore row.

### Bulk workflow
- Multi-select rows.
- Change Domain in one selected row -> applies to selected set.
- Change Category in one selected row -> applies to selected set.
- AssetName remains per-row unless manually edited.

## 🧩 Configuration
Open: `Edit -> Project Settings -> Ns Bonsai`

**This page controls three things:**
- how names are built,
- which fields are required in the review table,
- how noisy (or quiet) queue/popup behavior is.

If you change one setting and the UI/validation changes, that is expected: most settings are intentionally connected.

### 1) Naming | Format
| Setting | What it does | What changing it does |
|---|---|---|
| `NameFormatOrder` | Defines final token order in generated names. | Reordering changes preview and final output shape immediately. If a component is removed from this list, it is effectively disabled even if its toggle is ON. |
| `JoinSeparator` | Sets separator between components (default `_`). | Every generated name uses this separator. Compliance parsing also expects this separator. |

**Why this group exists:**
- NsBonsai treats order as part of your naming standard, not just display preference.
- Build and parse logic must use the same delimiter to stay deterministic.

**Recommendation:**
- Keep: `Type, Domain, Category, AssetName, Variant` for classic UE-style pipelines.
- Keep `_` unless your team has a strict alternative standard.

### 2) Naming | Components
These toggles control both naming and UI validation.

| Setting | What it does | If ON | If OFF |
|---|---|---|---|
| `bUseDomains` | Enables Domain component in naming and Domain column in UI. | Domain becomes a required input when Domain component is in `NameFormatOrder`. | Domain is not used in names. Category can still work through Global Categories mode. |
| `bUseCategories` | Enables Category component in naming and Category column in UI. | Category becomes required when Category component is active. | Category is removed from naming and validation. |
| `bUseVariant` | Enables trailing variant token allocation. | Final rename allocates unique variants (`A..Z`, `AA..`) to avoid collisions. | No variant component in output; collisions are more likely if base names repeat. |
| `bUseAssetNameField` | Enables editable Asset Name input in UI and naming. | Asset Name is required after sanitize. | Name is derived from parsed/original fallback; field is hidden. |
| `bCategoriesMustBelongToDomain` | Enforces domain-specific category membership when domains are enabled. | Category must exist under selected domain. | Any configured category is allowed regardless of selected domain. |
| `bAllowFreeCategoryTextIfNoCategories` | Allows category use when library is empty for the current context. | Empty category library does not hard-block category usage. | Missing category options will block validation until categories are configured. |

**Recommendation:**
- Keep `bUseVariant=True` for production projects.

### 3) Naming | Library
| Setting | What it does | What changing it does | Why |
|---|---|---|---|
| `TypeRules` | Maps class path to type token (example: StaticMesh -> `SM`). | Affects Type column and Type token in preview/final names. Missing mapping for an active Type component produces validation warning. | Type token is resolved from actual asset class path, not folder/name guesses. |
| `Domains` | Defines valid domain tokens and per-domain category lists. | Updates dropdown choices and validation logic immediately. | Stable token libraries prevent drift and reduce naming debates in production. |
| `GlobalCategories` | Provides category options when Domains are disabled. | Category validation uses this global list instead of per-domain lists. | Keeps categories usable in domain-free naming setups. |

**Recommendation:**
- Keep domain list short, stable, and team-owned.

### 4) Naming | Behavior
| Setting | What it does | What changing it does |
|---|---|---|
| `bSkipCompliantAssets` | Skips enqueueing assets that already match active naming rules. | ON: less queue noise in migrated/clean projects. OFF: all tracked assets can still show up for review. |
| `PopupThresholdCount` | Minimum queue size before auto popup opens. | Lower value = more immediate popups. Higher value = more batching, less interruption. |
| `PopupCooldownSeconds` | Minimum time between automatic popups. | Higher value reduces popup frequency during large bursts/imports. |
| `bAutoCloseWindowWhenEmpty` | Closes review window automatically after all rows are handled. | ON: less cleanup clicks. OFF: keep window open for continuous/manual sessions. |

**`ReviewTriggerMode` values:**
| Value | Behavior |
|---|---|
| `Automatic` | track + auto popup by threshold/cooldown rules. |
| `ManualOnly` | track + queue only, open manually from menu. |
| `Disabled` | stops tracking/popup behavior. |

**What changing it does:**
- This is your global "how active should Bonsai be right now?" control.

### 5) Naming | Normalization
| Setting | What it does | What changing it does | Why |
|---|---|---|---|
| `TokenNormalizationRules` | Maps deprecated aliases to canonical tokens (`Ui` -> `UI`). | Domain/category token comparisons normalize through these rules. | Prevents token drift from mixed casing/legacy aliases. |
| `bNormalizeAssetNameExactMatch` | Applies normalization rules to AssetName tokens when exact token matches occur. | Helps unify known aliases in AssetName without aggressive rewriting. | Keeps naming consistency without over-transforming free text. |

### 6) UI
| Setting | What it does |
|---|---|
| `bShowTypeColumn` | Shows/hides Type column. |
| `bShowDomainColumn` | Shows/hides Domain column when domains are in use. |
| `bShowCategoryColumn` | Shows/hides Category column when categories are in use. |
| `bShowAssetNameColumn` | Shows/hides editable Asset Name column when asset-name field is enabled. |
| `bShowFinalNameColumn` | Shows/hides live preview column. |
| `bShowCurrentNameColumn` | Shows/hides source/current asset name column. |
| `bCompactRows` | Reduces row padding for denser review throughput. |

**Recommendation:**
- Keep `Final Name` visible.
- Keep compact rows ON if your workflow is high-volume.

### Practical presets
| Preset | Settings | Best for |
|---|---|---|
| Strict team preset | `bUseDomains=True`<br>`bUseCategories=True`<br>`bCategoriesMustBelongToDomain=True`<br>`bUseVariant=True`<br>`bSkipCompliantAssets=True` | large teams with clear taxonomy. |
| Lightweight preset | `bUseDomains=False`<br>`bUseCategories=False` (or use global categories only)<br>`bUseVariant=True`<br>`NameFormatOrder=Type,AssetName,Variant` | small teams and fast prototyping. |
| Quiet automation preset | `ReviewTriggerMode=Disabled` | scripted imports/build steps where UI interruptions are unwanted. |

## 🧠 How it works
### Detection and queueing
- Tracks asset-created/imported events.
- Defers enqueue until package save event.
- Dedupes by soft object path to avoid duplicates.

### Single-window behavior
- Keeps one live review window.
- New queued items append to existing window if already open.

### Compliance filtering
When enabled, compliant assets are skipped early.

Compliant means:
- name parses under active format,
- type token matches class mapping,
- domain/category are valid under current rules,
- variant is valid (when enabled),
- asset name is valid after sanitization.

### Rename execution
- Uses AssetTools rename pipeline.
- Runs with callback guards to avoid self-requeue storms.
- Allocates safe variant suffixes against registry collisions.
- Removes rows immediately after confirm/ignore.

### Conflict handling
If target name exists, variant increments automatically.

Example:
- Existing: `BP_Thing_A`
- Candidate: `BP_Thing_A`
- Result: `BP_Thing_B`

### Dry Run
- Confirm in Dry Run does not rename immediately.
- Rows are staged for batch execution.
- `Execute Renames` applies staged set.

## 📚 Best practices
- Start with a small, opinionated TypeRules set.
- Keep Domains/Categories strict and easy to scan.
- Use Skip Compliant for migration-heavy projects.
- Treat NameFormatOrder as team convention, not personal preference.
- Keep variant enabled to prevent silent collisions.

Good style references:
- Tom Looman naming guide: https://tomlooman.com/unreal-engine-naming-convention-guide
- Allar UE style guide: https://github.com/Allar/ue5-style-guide

## ⚙️ Requirements
- Unreal Engine **5.2+**

## 🛠️ Installation
1. Clone or download this repository.
2. Copy the `NsBonsai` folder into your project `Plugins` directory.
3. Regenerate project files.
4. Enable **NsBonsai** in `Edit -> Plugins`.
5. Restart editor.

<!-- GH_ONLY_START -->
## ❤️ Credits
<a href="https://github.com/mykaadev/NsBonsai/graphs/contributors"><img src="https://contrib.rocks/image?repo=mykaadev/NsBonsai" alt="Contributors"/></a>

## 📞 Support
Reach out via profile page: https://github.com/mykaadev

## 📃 License
[![License](https://img.shields.io/badge/license-MIT-green)](https://www.tldrlegal.com/license/mit-license)
<!-- GH_ONLY_END -->
