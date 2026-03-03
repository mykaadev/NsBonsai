#include "Rules/NsBonsaiCompiledRules.h"

#include "Misc/SecureHash.h"
#include "UObject/SoftObjectPath.h"

namespace NsBonsai
{
	static FString StableHashFromString(const FString& Source)
	{
		FTCHARToUTF8 Converter(*Source);
		uint8 Hash[FSHA1::DigestSize];
		FSHA1::HashBuffer(Converter.Get(), Converter.Length(), Hash);
		return BytesToHex(Hash, FSHA1::DigestSize);
	}

	static FString BuildSettingsSignature(const UNsBonsaiSettings& Settings)
	{
		TArray<FString> Rows;

		for (const FNsBonsaiTypeMapEntry& Entry : Settings.TypeMap)
		{
			Rows.Add(FString::Printf(TEXT("Type|%s|%s"), *Entry.ClassPath.ToString(), *Entry.TypeToken));
		}

		for (const FNsBonsaiTreeNode& Node : Settings.TreeNodes)
		{
			FString Matchers = FString::Join(Node.Matchers, TEXT(","));
			Rows.Add(FString::Printf(TEXT("Tree|%s|%s|%s|%d|%d|%s"), *Node.Id.ToString(), *Node.ParentId.ToString(), *Node.Token, static_cast<int32>(Node.Group), Node.Priority, *Matchers));
		}

		for (const FNsBonsaiDescriptorContextRule& Rule : Settings.DescriptorContextRules)
		{
			TArray<FString> DomainNames;
			for (const FName DomainId : Rule.DomainIds)
			{
				DomainNames.Add(DomainId.ToString());
			}

			TArray<FString> CategoryNames;
			for (const FName CategoryId : Rule.CategoryIds)
			{
				CategoryNames.Add(CategoryId.ToString());
			}

			TArray<FString> ClassNames;
			for (const FSoftClassPath& ClassFilter : Rule.ClassFilters)
			{
				ClassNames.Add(ClassFilter.ToString());
			}

			TArray<FString> Paths;
			for (const FDirectoryPath& PathPrefix : Rule.PathPrefixes)
			{
				Paths.Add(PathPrefix.Path);
			}

			Rows.Add(FString::Printf(
				TEXT("DescRule|%s|%s|%s|%s|%d"),
				*FString::Join(DomainNames, TEXT(",")),
				*FString::Join(CategoryNames, TEXT(",")),
				*FString::Join(ClassNames, TEXT(",")),
				*FString::Join(Paths, TEXT(",")),
				Rule.bAllowFreeText ? 1 : 0));

			for (const FNsBonsaiDescriptorSuggestion& Suggestion : Rule.SuggestedDescriptors)
			{
				Rows.Add(FString::Printf(TEXT("DescSuggestion|%s|%d"), *Suggestion.Token, Suggestion.Priority));
			}
		}

		for (const FNsBonsaiNormalizationEntry& Entry : Settings.NormalizationMap)
		{
			Rows.Add(FString::Printf(TEXT("Norm|%s|%s"), *Entry.DeprecatedToken, *Entry.CanonicalToken));
		}

		Rows.Add(FString::Printf(TEXT("Ordering|%d|%s"), Settings.DescriptorOrdering.bSortDescriptors ? 1 : 0, *Settings.DescriptorOrdering.JoinSeparator));

		Rows.Sort();
		return StableHashFromString(FString::Join(Rows, TEXT("\n")));
	}

	static FTopLevelAssetPath ResolveClassPath(const FSoftClassPath& ClassPath)
	{
		if (ClassPath.IsNull())
		{
			return FTopLevelAssetPath();
		}

		const FTopLevelAssetPath AssetPath = ClassPath.GetAssetPath();
		if (!AssetPath.IsNull())
		{
			return AssetPath;
		}

		if (const UClass* ResolvedClass = ClassPath.ResolveClass())
		{
			return ResolvedClass->GetClassPathName();
		}

		return FTopLevelAssetPath();
	}
}

void FNsBonsaiCompiledRules::Rebuild(const UNsBonsaiSettings& Settings)
{
	TypeTokensByClass.Reset();
	TreeNodes.Reset();
	TreeIndexById.Reset();
	DescriptorContextRules.Reset();
	NormalizationMap.Reset();

	for (const FNsBonsaiTypeMapEntry& Entry : Settings.TypeMap)
	{
		const FTopLevelAssetPath ResolvedPath = NsBonsai::ResolveClassPath(Entry.ClassPath);
		if (!ResolvedPath.IsNull() && !Entry.TypeToken.IsEmpty())
		{
			TypeTokensByClass.Add(ResolvedPath, Entry.TypeToken);
		}
	}

	TreeNodes.Reserve(Settings.TreeNodes.Num());
	for (const FNsBonsaiTreeNode& Node : Settings.TreeNodes)
	{
		if (Node.Id.IsNone())
		{
			continue;
		}

		FNsBonsaiCompiledTreeNode& CompiledNode = TreeNodes.AddDefaulted_GetRef();
		CompiledNode.Id = Node.Id;
		CompiledNode.ParentId = Node.ParentId;
		CompiledNode.Token = Node.Token;
		CompiledNode.Group = Node.Group;
		CompiledNode.Priority = Node.Priority;

		for (const FString& Matcher : Node.Matchers)
		{
			if (!Matcher.IsEmpty())
			{
				CompiledNode.CompiledMatchers.Emplace(Matcher);
			}
		}

		TreeIndexById.Add(CompiledNode.Id, TreeNodes.Num() - 1);
	}

	for (int32 NodeIndex = 0; NodeIndex < TreeNodes.Num(); ++NodeIndex)
	{
		FNsBonsaiCompiledTreeNode& Node = TreeNodes[NodeIndex];
		if (Node.ParentId.IsNone())
		{
			continue;
		}

		if (const int32* ParentIndex = TreeIndexById.Find(Node.ParentId))
		{
			TreeNodes[*ParentIndex].ChildIndices.Add(NodeIndex);
		}
	}

	for (const FNsBonsaiDescriptorContextRule& Rule : Settings.DescriptorContextRules)
	{
		FNsBonsaiCompiledDescriptorContextRule& CompiledRule = DescriptorContextRules.AddDefaulted_GetRef();
		CompiledRule.DomainIds = Rule.DomainIds;
		CompiledRule.CategoryIds = Rule.CategoryIds;
		CompiledRule.PathPrefixes = Rule.PathPrefixes;
		CompiledRule.SuggestedDescriptors = Rule.SuggestedDescriptors;
		CompiledRule.bAllowFreeText = Rule.bAllowFreeText;

		for (const FSoftClassPath& ClassFilter : Rule.ClassFilters)
		{
			const FTopLevelAssetPath ResolvedPath = NsBonsai::ResolveClassPath(ClassFilter);
			if (!ResolvedPath.IsNull())
			{
				CompiledRule.ClassFilters.Add(ResolvedPath);
			}
		}
	}

	for (const FNsBonsaiNormalizationEntry& Entry : Settings.NormalizationMap)
	{
		if (!Entry.DeprecatedToken.IsEmpty() && !Entry.CanonicalToken.IsEmpty())
		{
			NormalizationMap.Add(Entry.DeprecatedToken, Entry.CanonicalToken);
		}
	}

	DescriptorOrdering = Settings.DescriptorOrdering;
	RulesHash = NsBonsai::BuildSettingsSignature(Settings);
}
