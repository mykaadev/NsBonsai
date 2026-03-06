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
  <a href="#-how-it-works">🧠 How it works</a> •
  <a href="#-configuration">🧩 Configuration</a> •
  <a href="#-best-practices">📚 Best practices</a> •
  <a href="#-requirements">⚙️ Requirements</a> •
  <a href="#-installation">🛠️ Installation</a> •
  <a href="#-getting-started">🚀 Getting Started</a> •
  <a href="#-credits">❤️ Credits</a> •
  <a href="#-support">📞 Support</a> •
  <a href="#-license">📃 License</a>
</p>

<a href="https://buymeacoffee.com/mykaadev"><img src="https://www.svgrepo.com/show/476855/coffee-to-go.svg" alt="Coffee" width="50px"></a>
<p><b>Buy me a coffee!</b></p>
</div>
<!-- GH_ONLY_END -->

## 🌳 Summary
**NsBonsai** is a lightweight Unreal Engine editor plugin that standardises asset names *while you work*.
It detects newly created/imported assets, shows a compact review table, and renames assets using a consistent naming pattern.

Default pattern:

`<Type>_<Domain>_<Category>_<AssetName>_<Variant>`

Example:

`SM_Foliage_Tree_Birch_A`

> **Zero-diff by design:** NsBonsai does **not** write metadata into assets.
> It only renames assets when you explicitly confirm.

<div align="center">
  <!-- Replace with your own GIFs/screenshots -->
  <img src="https://github.com/mykaadev/NsBonsai/blob/main/Resources/Showcase_Table.gif" width="800" />
</div>

## 🎯 Why NsBonsai
Naming isn’t “nice to have” - it’s how teams search, filter, batch process, and keep sanity across large projects.

NsBonsai focuses on:
- **Speed:** compact table, bulk actions and a friendly workflow.
- **Safety:** collision-safe variants.
- **Low friction:** minimal setup (TypeRules + Domains/Categories).
- **Team friendliness:** deterministic output and a shared token library.

## ✨ Features
- **Compact “conveyor belt” UI:** one row per asset with inline Domain/Category selection, AssetName edit, live output, ✅ confirm, ❌ ignore.
- **Bulk classification:** multi-select rows and apply the same Domain/Category to many assets at once.
- **Collision-safe variants:** always appends a variant suffix (`A..Z`, `AA..`) to avoid name conflicts.
- **Migration-friendly:** “Skip compliant assets” so new projects don’t get spammed.
- **Non-invasive:** no asset metadata writes, no changes unless you confirm.
- **Dynamic taxonomy (optional):** add Domains/Categories on the fly from dropdowns and use them immediately.

## 🧠 How it works
NsBonsai deliberately avoids “magic” folder rules. Instead it uses a simple token library and a predictable rename pipeline:

### 1) Detect new assets (only after save)
- Tracks newly created/imported assets via editor/AssetRegistry signals.
- Uses package saved events so it only prompts once the asset is persisted.
- Dedupes aggressively to avoid repeated entries and window spam.

### 2) Build the preview name (deterministic)
- Type token is resolved from **TypeRules** (class → prefix).
- Domain/Category come from your token library.
- AssetName is a free-form field (prefilled from original).
- Variant is allocated collision-safe during rename.

### 3) Rename safely
- Uses Unreal’s **AssetTools** rename pipeline (redirector-safe, editor-friendly).
- Guards against registry spam caused by renames so assets don’t re-queue or reopen windows.

### 4) Compliance check (optional)
When enabled, NsBonsai can skip enqueueing assets that already match your rules:
- Valid Type token for the asset class
- Known Domain/Category (and Category allowed under Domain if constrained)
- Valid Variant suffix
- Clean, sanitized AssetName

## 🧩 Configuration
Open: `Edit → Project Settings → NsBonsai`

### Type Rules (class → prefix)
Define how NsBonsai maps asset classes to type prefixes:

- Static Mesh → `SM`
- Texture → `T`
- Material Instance → `MI`
- Blueprint → `BP`
- …and whatever your project needs.

### Domains & Categories (token library)
Domains are your high-level tokens (e.g. `UI`, `Foliage`, `Character`).
Each domain can contain allowed categories (e.g. `UI → Icon/Widget/Font`).

Example config:
```ini
[/Script/NsBonsai.NsBonsaiSettings]
+TypeRules=(ClassPath="/Script/Engine.StaticMesh",TypeToken="SM")
+TypeRules=(ClassPath="/Script/Engine.Texture",TypeToken="T")
+TypeRules=(ClassPath="/Script/Engine.MaterialInstance",TypeToken="MI")
+TypeRules=(ClassPath="/Script/Engine.Blueprint",TypeToken="BP")

+Domains=(DomainToken="UI",Categories=("Icon","Widget","Style","Font"))
+Domains=(DomainToken="Foliage",Categories=("Tree","Bush","Grass","Flower"))
+Domains=(DomainToken="Character",Categories=("Hero","NPC","Enemy","Animation"))

JoinSeparator="_"
bSkipCompliantAssets=True
```

### Normalization
Map legacy tokens into canonical ones:
```ini
+TokenNormalizationRules=(DeprecatedToken="Ui",CanonicalToken="UI")
+TokenNormalizationRules=(DeprecatedToken="Chars",CanonicalToken="Character")
```

## 📚 Best practices
NsBonsai follows the common UE convention of **type prefixes + underscores**.
If you want a broader style guide reference:
- Tom Looman naming guide: https://tomlooman.com/unreal-engine-naming-convention-guide
- Allar UE style guide: https://github.com/Allar/ue5-style-guide

## ⚙️ Requirements
- Unreal Engine **5.2+**

## 🛠️ Installation
1. Clone or download this repository.
2. Copy the `NsBonsai` folder into your project’s `Plugins` directory (create it if it doesn’t exist).
3. Regenerate project files.
4. Open Unreal Editor and enable **NsBonsai** under `Edit → Plugins`, then restart.

## 🚀 Getting Started
1. Create/import an asset.
2. Save the asset/package.
3. NsBonsai opens the review table.
4. Handle it!

### Table workflow
- Select Domain + Category (bulk apply supported).
- Edit AssetName (prefilled from original).
- Verify live output.
- ✅ Confirm renames immediately.
- ❌ Ignore removes the row with no changes.


<!-- GH_ONLY_START -->
## ❤️ Credits
<a href="https://github.com/mykaadev/NsBonsai/graphs/contributors"><img src="https://contrib.rocks/image?repo=mykaadev/NsBonsai"/></a>

## 📞 Support
Reach out via the profile page: https://github.com/mykaadev

## 📃 License
[![License](https://img.shields.io/badge/license-MIT-green)](https://www.tldrlegal.com/license/mit-license)
<!-- GH_ONLY_END -->
