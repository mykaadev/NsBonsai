#include "NsBonsaiCompiledRules.h"

#include "Misc/Crc.h"
#include "UObject/Class.h"
#include "UObject/Object.h"

namespace NsBonsai::RuleCompiler
{
	static TArray<FName> SplitTokenPath(const FString& TokenPath, const FString& Separator)
	{
		TArray<FName> Tokens;
		TArray<FString> RawTokens;
		TokenPath.ParseIntoArray(RawTokens, *Separator, true);

		for (const FString& RawToken : RawTokens)
		{
			const FString Trimmed = RawToken.TrimStartAndEnd();
			if (!Trimmed.IsEmpty())
			{
				Tokens.Add(FName(*Trimmed));
			}
		}

		return Tokens;
	}

	static uint32 CombineHash(uint32 CurrentHash, const FString& Value)
	{
		return FCrc::StrCrc32(*Value, CurrentHash);
	}

	static uint32 CombineHash(uint32 CurrentHash, const FName Name)
	{
		return FCrc::StrCrc32(*Name.ToString(), CurrentHash);
	}

	static FNsBonsaiCompiledClassFilter BuildClassFilter(const FSoftClassPath& ClassPath)
	{
		FNsBonsaiCompiledClassFilter Filter;
		Filter.SourceClassPath = ClassPath;
		Filter.ResolvedClass = ClassPath.IsValid() ? ClassPath.TryLoadClass<UObject>() : nullptr;
		return Filter;
	}
}

bool FNsBonsaiCompiledClassFilter::Matches(const UClass* InClass) const
{
	if (!ResolvedClass || !InClass)
	{
		return false;
	}

	return InClass->IsChildOf(ResolvedClass.Get());
}

FCompiledRules FCompiledRules::Build(const UNsBonsaiSettings& Settings)
{
	using namespace NsBonsai::RuleCompiler;

	FCompiledRules Compiled;

	Compiled.Ordering.JoinSeparator = Settings.Ordering.JoinSeparator;
	Compiled.Ordering.TokenPathSeparator = Settings.Ordering.TokenPathSeparator;
	Compiled.Ordering.bDescriptorPriorityDescending = Settings.Ordering.bDescriptorPriorityDescending;
	Compiled.Ordering.bDescriptorAlphabeticalAscending = Settings.Ordering.bDescriptorAlphabeticalAscending;

	uint32 Hash = 0;
	Hash = CombineHash(Hash, Compiled.Ordering.JoinSeparator);
	Hash = CombineHash(Hash, Compiled.Ordering.TokenPathSeparator);
	Hash = FCrc::TypeCrc32(Compiled.Ordering.bDescriptorPriorityDescending, Hash);
	Hash = FCrc::TypeCrc32(Compiled.Ordering.bDescriptorAlphabeticalAscending, Hash);

	Compiled.TypeRules.Reserve(Settings.TypeRules.Num());
	for (const FNsBonsaiTypeRule& Rule : Settings.TypeRules)
	{
		if (Rule.TypeToken.IsNone())
		{
			continue;
		}

		FNsBonsaiCompiledTypeRule CompiledRule;
		CompiledRule.TypeToken = Rule.TypeToken;
		CompiledRule.ClassFilter = BuildClassFilter(Rule.ClassPath);
		Compiled.TypeRules.Add(MoveTemp(CompiledRule));

		Hash = CombineHash(Hash, Rule.ClassPath.ToString());
		Hash = CombineHash(Hash, Rule.TypeToken);
	}

	Compiled.TaxonomyRules.Reserve(Settings.TaxonomyRules.Num());
	for (const FNsBonsaiTaxonomyRule& Rule : Settings.TaxonomyRules)
	{
		FNsBonsaiCompiledTaxonomyRule CompiledRule;
		CompiledRule.PathPrefix = Rule.PathPrefix.TrimStartAndEnd();
		CompiledRule.TokenPath = SplitTokenPath(Rule.TokenPath, Compiled.Ordering.TokenPathSeparator);

		CompiledRule.ClassFilters.Reserve(Rule.ClassFilters.Num());
		for (const FSoftClassPath& ClassPath : Rule.ClassFilters)
		{
			CompiledRule.ClassFilters.Add(BuildClassFilter(ClassPath));
			Hash = CombineHash(Hash, ClassPath.ToString());
		}

		Compiled.TaxonomyRules.Add(MoveTemp(CompiledRule));

		Hash = CombineHash(Hash, Rule.PathPrefix);
		Hash = CombineHash(Hash, Rule.TokenPath);
	}

	Compiled.DescriptorContextRules.Reserve(Settings.DescriptorContextRules.Num());
	for (const FNsBonsaiDescriptorContextRule& Rule : Settings.DescriptorContextRules)
	{
		FNsBonsaiCompiledDescriptorContextRule CompiledRule;
		CompiledRule.Domain = Rule.Domain;
		CompiledRule.Category = Rule.Category;
		CompiledRule.PathPrefix = Rule.PathPrefix.TrimStartAndEnd();

		CompiledRule.ClassFilters.Reserve(Rule.ClassFilters.Num());
		for (const FSoftClassPath& ClassPath : Rule.ClassFilters)
		{
			CompiledRule.ClassFilters.Add(BuildClassFilter(ClassPath));
			Hash = CombineHash(Hash, ClassPath.ToString());
		}

		CompiledRule.Suggestions.Reserve(Rule.Suggestions.Num());
		for (const FNsBonsaiDescriptorSuggestion& Suggestion : Rule.Suggestions)
		{
			if (Suggestion.Token.IsNone())
			{
				continue;
			}

			FNsBonsaiCompiledDescriptorSuggestion CompiledSuggestion;
			CompiledSuggestion.Token = Suggestion.Token;
			CompiledSuggestion.Priority = Suggestion.Priority;
			CompiledRule.Suggestions.Add(CompiledSuggestion);

			Hash = CombineHash(Hash, Suggestion.Token);
			Hash = FCrc::TypeCrc32(Suggestion.Priority, Hash);
		}

		Compiled.DescriptorContextRules.Add(MoveTemp(CompiledRule));

		Hash = CombineHash(Hash, Rule.Domain);
		Hash = CombineHash(Hash, Rule.Category);
		Hash = CombineHash(Hash, Rule.PathPrefix);
	}

	for (const FNsBonsaiTokenNormalizationRule& Rule : Settings.TokenNormalizationRules)
	{
		if (Rule.DeprecatedToken.IsNone() || Rule.CanonicalToken.IsNone())
		{
			continue;
		}

		Compiled.NormalizationMap.Add(Rule.DeprecatedToken, Rule.CanonicalToken);
		Hash = CombineHash(Hash, Rule.DeprecatedToken);
		Hash = CombineHash(Hash, Rule.CanonicalToken);
	}

	Compiled.RulesHash = Hash;
	return Compiled;
}
