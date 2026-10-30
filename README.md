<!-- GH_ONLY_START -->
<h1 align="center">
  <br>
  <a href="https://github.com/mykaadev/NsBonsai"><img src="https://raw.githubusercontent.com/mykaadev/NsBonsai/refs/heads/main/Resources/Banner.png" alt="NsBonsai" width="400"></a>
</h1>

<h4 align="center">Prune asset names into one consistent shape, right in the Unreal Editor.</h4>

<div align="center">
    <a href="https://github.com/mykaadev/NsBonsai/commits/main"><img src="https://img.shields.io/github/last-commit/mykaadev/NsBonsai?style=plastic&logo=github&logoColor=white" alt="GitHub Last Commit"></a>
    <a href="https://github.com/mykaadev/NsBonsai/issues"><img src="https://img.shields.io/github/issues-raw/mykaadev/NsBonsai?style=plastic&logo=github&logoColor=white" alt="GitHub Issues"></a>
    <a href="https://github.com/mykaadev/NsBonsai/pulls"><img src="https://img.shields.io/github/issues-pr-raw/mykaadev/NsBonsai?style=plastic&logo=github&logoColor=white" alt="GitHub Pull Requests"></a>
    <a href="https://github.com/mykaadev/NsBonsai"><img src="https://img.shields.io/github/stars/mykaadev/NsBonsai?style=plastic&logo=github" alt="GitHub Stars"></a>
    <a href="https://twitter.com/mykaadev/"><img src="https://img.shields.io/twitter/follow/mykaadev?style=plastic&logo=x" alt="Twitter Follow"></a>

<p style="display:none;">
  <a href="#-summary">👀 Summary</a> •
  <a href="#-features">📦 Features</a> •
  <a href="#-requirements">⚙️ Requirements</a> •
  <a href="#-installation">🛠️ Installation</a> •
  <a href="#-getting-started">🚀 Getting Started</a> •
  <a href="#-name-parts">🧩 Name Parts</a> •
  <a href="#-api">🔧 API</a> •
  <a href="#-limits">🚧 Limits</a> •
  <a href="#-documentation">📚 Documentation</a> •
  <a href="#-credits">❤️ Credits</a> •
  <a href="#-support">📞 Support</a> •
  <a href="#-license">📃 License</a>
</p>
<a href="https://buymeacoffee.com/mykaadev"><img src="https://www.svgrepo.com/show/476855/coffee-to-go.svg" alt="Coffee" width="50px"></a>
<p><b>Buy me a coffee!</b></p>
</div>
<!-- GH_ONLY_END -->

## 👀 Summary
NsBonsai is an editor plugin that keeps asset names in one pattern. When you add or import an asset and save it, NsBonsai puts it in a review window. In the window, you pick the parts of the name, read the final name, and rename or ignore each asset. The default pattern is `<Type>_<Domain>_<Category>_<AssetName>_<Variant>`, for example `SM_Foliage_Tree_Birch_A`.

NsBonsai renames an asset only when you confirm it. It writes no metadata into assets and does not move assets to other folders. It does not scan the whole project. It is an editor module and is not part of a packaged game.

<div align="center">
 <img src="https://github.com/mykaadev/NsBonsai/blob/main/Resources/Showcase.gif" alt="NsBonsai Showcase" width="700">
</div>

## 📦 Features
- **Review window**: One table for all queued assets, with the name parts and a preview of the final name.
- **Save trigger**: New and renamed assets go to the queue when you save their package.
- **Type tokens**: The type comes from the asset class. The plugin ships 39 class rules, such as `SM`, `M`, `MI`, `T`, and `BP`.
- **Domains and categories**: Token lists in Project Settings. Add a new token from the table.
- **Variants**: A free suffix (`A` to `Z`, then `AA`) stops name collisions.
- **Skip compliant assets**: Assets that already match the pattern stay out of the queue.
- **Open with Bonsai**: Send selected assets from the Content Browser to the review window.
- **Dry Run**: Stage renames, then run them together.
- **Trigger modes**: Automatic popup, manual only, or off. Snooze the popup for 10 minutes.
- **Rename log**: Each result goes to `Saved/Logs/NsBonsai_Rename.log` as CSV.

## ⚙️ Requirements
Unreal Engine 5.7+

NsBonsai needs no other plugin.

## 🛠️ Installation
1. **Clone** or download this repository.
2. Copy the `NsBonsai` folder into the `Plugins` folder of your project.
3. Generate the project files and enable the plugin.
4. Restart the editor.

NsBonsai works without code. Add `"NsBonsai"` to a `Build.cs` only if an editor module calls its C++ helpers.

## 🚀 Getting Started
1. Open **Edit > Project Settings > Plugins > Ns Bonsai**.
2. In **Domains**, add a domain token, for example `Foliage`, with a category, for example `Tree`.
3. Import a static mesh and save it.
4. In the **NsBonsai Asset Review** window, check the **Domain**, **Category**, and **Asset Name** of the row.
5. Read the **Final Name** column, then click the check button to rename the asset.

To review assets that exist already, select them in the Content Browser, right-click, and choose **Open with Bonsai**. To open the queue at any time, use **Tools > NsBonsai Review Queue...**.

## 🧩 Name Parts
| Part | Source |
|---|---|
| Type | The **Type Rules** entry for the asset class or its nearest parent class |
| Domain | A token from **Domains** |
| Category | A token from the categories of the domain, or from **Global Categories** when domains are off |
| Asset Name | Free text. The prefill is the old name without known tokens |
| Variant | The first free letter suffix in the folder: `A`, `B`, and so on |

**Name Format Order** sets the order of the parts. **Join Separator** (default `_`) goes between them.

## 🔧 API
- `UNsBonsaiSettings` – The project settings: name format, token libraries, popup behavior, and table columns.
- `FNsBonsaiNameRules::IsCompliant` – Returns true when an asset name matches the active pattern.
- `FNsBonsaiNameRules::ValidateRowState` – Checks a set of name parts and returns a message.
- `FNsBonsaiNameRules::AllocateFinalNameWithVariant` – Builds the final name and finds a free variant.
- `FNsBonsaiNameRules::SmartPrefillAssetName` – Removes known tokens from an old name.
- `FNsBonsaiNameBuilder::ParseExistingAssetName` – Splits an old name into type, domain, category, asset name, and variant.
- `FNsBonsaiAssetEvaluator::Evaluate` – Gives the default row values for one asset.

## 🚧 Limits
- The domain list is empty by default. Until you add a domain, every row shows "Domain is required."
- The queue lives in memory. An editor restart clears it.
- Variants go from `A` to `YZ` (676 names). After that, the rename fails.
- A rename keeps the asset in its folder.
- The check button renames one row at a time. Dry Run is the only batch rename.

## 📚 Documentation
- [User Guide](https://mykaadev.github.io/plugins/nsbonsai/user-guide/) – Setup, common tasks, settings, and debug tools.
- [Technical Documentation](https://mykaadev.github.io/plugins/nsbonsai/tech-documentation/) – Architecture, lifetime, and extension points.

<!-- GH_ONLY_START -->
## ❤️ Credits
<a href="https://github.com/mykaadev/NsBonsai/graphs/contributors"><img src="https://contrib.rocks/image?repo=mykaadev/NsBonsai"/></a>

## 📞 Support
Reach out via the **[profile page](https://github.com/mykaadev)**.

## 📃 License
[![License](https://img.shields.io/badge/license-MIT-green)](https://www.tldrlegal.com/license/mit-license)
<!-- GH_ONLY_END -->
