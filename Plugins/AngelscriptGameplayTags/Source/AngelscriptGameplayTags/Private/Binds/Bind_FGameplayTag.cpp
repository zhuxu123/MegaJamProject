#include "AngelscriptGameplayTagsBinds.h"
#include "AngelscriptGameplayTagsInternal.h"
#include "Bind_FGameplayTag_Functions.h"

#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "Debugging/AngelscriptDebugServer.h"

#include "Binds/Helper_StructType.h"
#include "Binds/Helper_ToString.h"

#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "GameplayTagsSettings.h"

namespace AngelscriptGameplayTags_Private
{
// These are global variables because they are meant
// to be static, persistent memory, and they are used
// in multiple functions in this file (for organization reasons).
// 25 is an arbitrary number chosen for how many tags
// fit in one chunk before a new chunk is allocated.
TChunkedArray<FGameplayTag, sizeof(FGameplayTag) * 25> GAngelscriptGameplayTags;

// This is a duplication of data for checking tag containment.
// A TSet gives us a O(1) lookup rather than O(n) TArray gives.
// Reallocation of this variable as tags are added is acceptable
// since it doesn't break bound pointers used in GAngelscriptGameplayTags.
TSet<FName> GAngelscriptGameplayTagsLookup;
TMap<FAngelscriptEngine*, int32> GGameplayTagNamespaceGlobalCounts;

AS_FORCE_LINK const FAngelscriptBind Bind_FGameplayTagQuery_ManualBindings(
	TEXT("FGameplayTagQuery.ExplicitBindings"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FGameplayTagQuery_ = Binds.ExistingClassForTarget("FGameplayTagQuery");

		{
			FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FGameplayTagQuery");
			Binds.BindGlobalVariableForTarget("FGameplayTagQuery EmptyQuery", &FGameplayTagQuery::EmptyQuery);

			Binds.BindGlobalFunctionForTarget("FGameplayTagQuery MakeQuery_MatchAnyTags(const FGameplayTagContainer& InTags) no_discard", &FGameplayTagQuery::MakeQuery_MatchAnyTags);
			Binds.BindGlobalFunctionForTarget("FGameplayTagQuery MakeQuery_MatchAllTags(const FGameplayTagContainer& InTags) no_discard", &FGameplayTagQuery::MakeQuery_MatchAllTags);
			Binds.BindGlobalFunctionForTarget("FGameplayTagQuery MakeQuery_MatchNoTags(const FGameplayTagContainer& InTags) no_discard", &FGameplayTagQuery::MakeQuery_MatchNoTags);
			Binds.BindGlobalFunctionForTarget("FGameplayTagQuery MakeQuery_ExactMatchAnyTags(const FGameplayTagContainer& InTags) no_discard", &FGameplayTagQuery::MakeQuery_ExactMatchAnyTags);
			Binds.BindGlobalFunctionForTarget("FGameplayTagQuery MakeQuery_ExactMatchAllTags(const FGameplayTagContainer& InTags) no_discard", &FGameplayTagQuery::MakeQuery_ExactMatchAllTags);
			Binds.BindGlobalFunctionForTarget("FGameplayTagQuery MakeQuery_MatchTag(const FGameplayTag& InTag) no_discard", &FGameplayTagQuery::MakeQuery_MatchTag);
		}

		FGameplayTagQuery_.Method("bool IsEmpty() const", METHOD_TRIVIAL(FGameplayTagQuery, IsEmpty));
		FGameplayTagQuery_.Method("bool opEquals(const FGameplayTagQuery& Other) const", METHODPR_TRIVIAL(bool, FGameplayTagQuery, operator==, (const FGameplayTagQuery& Other) const));
	});

void Bind_AddNewGameplayTag(FAngelscriptBinds& Binds, int32 GameplayTagIndex)
{
	FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "GameplayTags");

	const FGameplayTag& GameplayTag = GAngelscriptGameplayTags[GameplayTagIndex];

	FString TagAsString = GameplayTag.ToString();

	// Exchange dots and other invalid characters in the tag for underscores
	for (int32 Index = TagAsString.Len() - 1; Index >= 0; --Index)
	{
		if (!FAngelscriptEngine::IsValidIdentifierCharacter(TagAsString[Index]))
		{
			TagAsString[Index] = '_';
		}
	}

	const FString Declaration = TEXT("const FGameplayTag ") + TagAsString;
	Binds.BindGlobalVariableForTarget(TCHAR_TO_ANSI(*Declaration), &GameplayTag);
}

bool Bind_ProcessGameplayTag(const FName TagName)
{
	bool Result = false;
	const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(TagName, false);

	if (Tag.IsValid())
	{
		// Both the chunked array and its dedup lookup are process-level
		// containers (see AngelscriptEngine.cpp::FAngelscriptEngine::Shutdown
		// for the rationale on why these are not cleared on shutdown).
		// Tagging the inserts so the LLM report attributes the residual size
		// to "Angelscript/GlobalState/GameplayTags" rather than the default bucket.

		if (!GAngelscriptGameplayTagsLookup.Contains(TagName))
		{
			Result = true;
			GAngelscriptGameplayTags.AddElement(Tag);
			GAngelscriptGameplayTagsLookup.Add(TagName);
		}
		for (auto& ParentTag : Tag.GetGameplayTagParents())
		{
			FName ParentTagName = ParentTag.GetTagName();
			if (!GAngelscriptGameplayTagsLookup.Contains(ParentTagName))
			{
				Result = true;
				GAngelscriptGameplayTags.AddElement(ParentTag);
				GAngelscriptGameplayTagsLookup.Add(ParentTagName);
			}
		}
	}

	return Result;
}

bool Bind_AddNewGameplayTags()
{
	bool Result = false;
	TArray<const FGameplayTagSource*> Sources;
	UGameplayTagsManager::Get().FindTagSourcesWithType(EGameplayTagSourceType::Native, Sources);
	UGameplayTagsManager::Get().FindTagSourcesWithType(EGameplayTagSourceType::DefaultTagList, Sources);
	UGameplayTagsManager::Get().FindTagSourcesWithType(EGameplayTagSourceType::TagList, Sources);
	UGameplayTagsManager::Get().FindTagSourcesWithType(EGameplayTagSourceType::RestrictedTagList, Sources);
	UGameplayTagsManager::Get().FindTagSourcesWithType(EGameplayTagSourceType::DataTable, Sources);

	for (const FGameplayTagSource* Source : Sources)
	{
		if (Source->SourceTagList != nullptr)
		{
			for (const auto& RowTag : Source->SourceTagList->GameplayTagList)
			{
				Result |= Bind_ProcessGameplayTag(RowTag.Tag);
			}
		}

		if (Source->SourceRestrictedTagList != nullptr)
		{
			for (const auto& RowTag : Source->SourceRestrictedTagList->RestrictedGameplayTagList)
			{
				Result |= Bind_ProcessGameplayTag(RowTag.Tag);
			}
		}
	}

	return Result;
}

void ReplayCachedGameplayTagsToEngine(FAngelscriptEngine& Engine)
{
	int32& BoundTagCount = GGameplayTagNamespaceGlobalCounts.FindOrAdd(&Engine);
	const int32 CachedTagCount = GAngelscriptGameplayTags.Num();
	if (BoundTagCount >= CachedTagCount)
	{
		return;
	}

	FAngelscriptBinds Binds(Engine);
	for (int32 TagIndex = BoundTagCount; TagIndex < CachedTagCount; ++TagIndex)
	{
		Bind_AddNewGameplayTag(Binds, TagIndex);
	}

	BoundTagCount = CachedTagCount;
}

AS_FORCE_LINK const FAngelscriptBind Bind_FGameplayTag_ToStringContribution(
	TEXT("FGameplayTag.ToStringContribution"),
	EAngelscriptBindPhase::TypeInfrastructure,
	[](FAngelscriptBinds& Binds)
	{
		FToStringHelper::Register(Binds, TEXT("FGameplayTag"), &FAngelscriptFGameplayTagBinds::AppendToString);
	});

AS_FORCE_LINK const FAngelscriptBind Bind_FGameplayTag_ManualBindings(
	TEXT("FGameplayTag.ExplicitBindings"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FGameplayTag_ = Binds.ExistingClassForTarget("FGameplayTag");

		{
			FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FGameplayTag");
			Binds.BindGlobalVariableForTarget("FGameplayTag EmptyTag", &FGameplayTag::EmptyTag);
			Binds.BindGlobalFunctionForTarget("FGameplayTag RequestGameplayTag(const FName& TagName, bool ErrorIfNotFound = true) no_discard", FUNC_TRIVIAL(FGameplayTag::RequestGameplayTag));
		}

		FGameplayTag_.Method("bool opEquals(const FGameplayTag& Other) const", METHODPR_TRIVIAL(bool, FGameplayTag, operator==, (FGameplayTag const& Other) const));
		FGameplayTag_.Method("bool IsValid() const", METHOD_TRIVIAL(FGameplayTag, IsValid));
		FGameplayTag_.Method("FName GetTagName() const", METHOD_TRIVIAL(FGameplayTag, GetTagName));
		FGameplayTag_.Method("bool MatchesTag(const FGameplayTag& TagToCheck) const", METHOD_TRIVIAL(FGameplayTag, MatchesTag));
		FGameplayTag_.Method("bool MatchesTagExact(const FGameplayTag& TagToCheck) const", METHOD_TRIVIAL(FGameplayTag, MatchesTagExact));
		FGameplayTag_.Method("int MatchesTagDepth(const FGameplayTag& TagToCheck) const", METHOD_TRIVIAL(FGameplayTag, MatchesTagDepth));
		FGameplayTag_.Method("bool MatchesAny(const FGameplayTagContainer& ContainerToCheck) const", METHOD_TRIVIAL(FGameplayTag, MatchesAny));
		FGameplayTag_.Method("bool MatchesAnyExact(const FGameplayTagContainer& ContainerToCheck) const", METHOD_TRIVIAL(FGameplayTag, MatchesAnyExact));
		FGameplayTag_.Method("FGameplayTagContainer GetSingleTagContainer() const", METHOD_TRIVIAL(FGameplayTag, GetSingleTagContainer));
		FGameplayTag_.Method("FGameplayTag RequestDirectParent() const", METHOD_TRIVIAL(FGameplayTag, RequestDirectParent));
		FGameplayTag_.Method("FGameplayTagContainer GetGameplayTagParents() const", METHOD_TRIVIAL(FGameplayTag, GetGameplayTagParents));

		auto FGameplayTagContainer_ = Binds.ExistingClassForTarget("FGameplayTagContainer");
		{
			FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FGameplayTagContainer");
			Binds.BindGlobalVariableForTarget("FGameplayTagContainer EmptyContainer", &FGameplayTagContainer::EmptyContainer);
		}
		FGameplayTagContainer_.Method("bool opEquals(const FGameplayTagContainer& Other) const", METHODPR_TRIVIAL(bool, FGameplayTagContainer, operator==, (FGameplayTagContainer const& Other) const));
		FGameplayTagContainer_.Method("bool IsEmpty() const", METHOD_TRIVIAL(FGameplayTagContainer, IsEmpty));
		FGameplayTagContainer_.Method("bool IsValid() const", METHOD_TRIVIAL(FGameplayTagContainer, IsValid));
		FGameplayTagContainer_.Method("int Num() const", METHOD_TRIVIAL(FGameplayTagContainer, Num));
		FGameplayTagContainer_.Method("FGameplayTag First() const", METHOD_TRIVIAL(FGameplayTagContainer, First));
		FGameplayTagContainer_.Method("void AddTag(const FGameplayTag& TagToAdd)", METHOD_TRIVIAL(FGameplayTagContainer, AddTag));
		FGameplayTagContainer_.Method("void AddTagFast(const FGameplayTag& TagToAdd)", METHOD_TRIVIAL(FGameplayTagContainer, AddTagFast));
		FGameplayTagContainer_.Method("bool AddLeafTag(const FGameplayTag& TagToAdd)", METHOD_TRIVIAL(FGameplayTagContainer, AddLeafTag));
		FGameplayTagContainer_.Method("bool RemoveTag(const FGameplayTag& TagToRemove)", &FAngelscriptFGameplayTagBinds::RemoveTag);
		FGameplayTagContainer_.Method("void RemoveTags(const FGameplayTagContainer& TagsToRemove)", METHOD_TRIVIAL(FGameplayTagContainer, RemoveTags));
		FGameplayTagContainer_.Method("void AppendTags(const FGameplayTagContainer& Other)", METHOD_TRIVIAL(FGameplayTagContainer, AppendTags));
		FGameplayTagContainer_.Method("void Reset(int Slack = 0)", METHOD_TRIVIAL(FGameplayTagContainer, Reset));
		FGameplayTagContainer_.Method("bool HasTag(const FGameplayTag& TagToCheck) const", METHOD_TRIVIAL(FGameplayTagContainer, HasTag));
		FGameplayTagContainer_.Method("bool HasTagExact(const FGameplayTag& TagToCheck) const", METHOD_TRIVIAL(FGameplayTagContainer, HasTagExact));
		FGameplayTagContainer_.Method("bool HasAny(const FGameplayTagContainer& ContainerToCheck) const", METHOD_TRIVIAL(FGameplayTagContainer, HasAny));
		FGameplayTagContainer_.Method("bool HasAnyExact(const FGameplayTagContainer& ContainerToCheck) const", METHOD_TRIVIAL(FGameplayTagContainer, HasAnyExact));
		FGameplayTagContainer_.Method("bool HasAll(const FGameplayTagContainer& ContainerToCheck) const", METHOD_TRIVIAL(FGameplayTagContainer, HasAll));
		FGameplayTagContainer_.Method("bool HasAllExact(const FGameplayTagContainer& ContainerToCheck) const", METHOD_TRIVIAL(FGameplayTagContainer, HasAllExact));
		FGameplayTagContainer_.Method("FGameplayTagContainer GetGameplayTagParents() const", METHOD_TRIVIAL(FGameplayTagContainer, GetGameplayTagParents));
		FGameplayTagContainer_.Method("FGameplayTagContainer Filter(const FGameplayTagContainer& OtherContainer) const", METHOD_TRIVIAL(FGameplayTagContainer, Filter));
		FGameplayTagContainer_.Method("FGameplayTagContainer FilterExact(const FGameplayTagContainer& OtherContainer) const", METHOD_TRIVIAL(FGameplayTagContainer, FilterExact));
		FGameplayTagContainer_.Method("bool MatchesQuery(const FGameplayTagQuery& Query) const", METHOD_TRIVIAL(FGameplayTagContainer, MatchesQuery));

		Bind_AddNewGameplayTags();
		ReplayCachedGameplayTagsToEngine(Binds.GetTargetEngine());
	});

void ReplayEngine(FAngelscriptEngine& Engine)
{
	ReplayCachedGameplayTagsToEngine(Engine);
}

void DetachEngine(FAngelscriptEngine& Engine)
{
	GGameplayTagNamespaceGlobalCounts.Remove(&Engine);
}
}

void AngelscriptReloadGameplayTags(FAngelscriptEngine& Engine)
{
	if (AngelscriptGameplayTags_Private::Bind_AddNewGameplayTags())
	{
		AngelscriptGameplayTags_Private::ReplayCachedGameplayTagsToEngine(Engine);
#if WITH_AS_DEBUGSERVER
		FAngelscriptDebugServer* DebugServer = Engine.DebugServer;
		if (DebugServer != nullptr)
		{
			DebugServer->BroadcastDebugDatabase();
		}
#endif
	}
}

void AngelscriptReloadGameplayTags()
{
	if (FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine())
	{
		AngelscriptReloadGameplayTags(*CurrentEngine);
	}
}

void AngelscriptRebindGameplayTagsToCurrentEngine()
{
	if (FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine())
	{
		AngelscriptGameplayTags_Private::ReplayCachedGameplayTagsToEngine(*CurrentEngine);
	}
}

void AngelscriptGameplayTagsReplayEngine(FAngelscriptEngine& Engine)
{
	AngelscriptGameplayTags_Private::ReplayEngine(Engine);
}

void AngelscriptGameplayTagsDetachEngine(FAngelscriptEngine& Engine)
{
	AngelscriptGameplayTags_Private::DetachEngine(Engine);
}
