#include "CQTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptGameplayTagBindingArchitectureTest,
	"Angelscript.GameplayTags.BindingArchitecture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool LoadGameplayTagBindSource(const TCHAR* RelativePath, FString& OutSource)
	{
		const FString SourcePath = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("AngelscriptGameplayTags/Source/AngelscriptGameplayTags/Private/Binds"),
			RelativePath);
		return FFileHelper::LoadFileToString(OutSource, *SourcePath);
	}

	static int32 CountOccurrences(const FString& Source, const TCHAR* Token)
	{
		int32 Count = 0;
		int32 SearchOffset = 0;
		while ((SearchOffset = Source.Find(Token, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchOffset)) != INDEX_NONE)
		{
			++Count;
			SearchOffset += FCString::Strlen(Token);
		}
		return Count;
	}

	static int32 CountGameplayTagTraitCalls(const FString& Line)
	{
		static const TCHAR* TraitCalls[] = {
			TEXT(".EditorOnly("), TEXT(".Deprecated("), TEXT(".PropertyAccessor("),
			TEXT(".GeneratedAccessor("), TEXT(".NoDiscard("), TEXT(".WorldContext("),
			TEXT(".Callable("), TEXT(".ForceConstArgumentExpressions("),
			TEXT(".DeterminesOutputType("), TEXT(".PassScriptFunctionAsFirstParam("),
			TEXT(".PassScriptObjectTypeAsFirstParam("), TEXT(".Documentation("),
			TEXT(".NativeConstructor("), TEXT(".NativeDestructor("), TEXT(".NativeAssignment("),
			TEXT(".NativeUObjectCast("), TEXT(".NativeMethod("), TEXT(".NativeFunction("),
			TEXT(".NativeFunctionHeader("), TEXT(".NativeUFunction("), TEXT(".NativeTArrayIndex("),
			TEXT(".NativeTArrayIteratorCreate("), TEXT(".NativeTArrayIteratorProceed("),
			TEXT(".NativeTemplateInstantiatedCall("), TEXT(".NativeDelegateExecute("),
			TEXT(".NativeMulticastExecute("), TEXT(".NativeEventFunctionExecute("),
			TEXT(".NativePushArgument("), TEXT(".NativePushArgumentRef("),
			TEXT(".CompileOutEntirely("), TEXT(".CompileOutAsMethodChain("),
			TEXT(".CompileOutInTest("), TEXT(".CompileOutIfNoLog("),
			TEXT(".CompileOutAsEnsure("), TEXT(".CompileOutAsCheck("),
			TEXT(".ReplaceWithFirstArgInTest("), TEXT(".PureConstant("),
		};

		int32 Count = 0;
		for (const TCHAR* TraitCall : TraitCalls)
		{
			Count += CountOccurrences(Line, TraitCall);
		}
		return Count;
	}

	static bool StartsWithGameplayTagTraitCall(const FString& Line)
	{
		static const TCHAR* TraitCalls[] = {
			TEXT(".EditorOnly("), TEXT(".Deprecated("), TEXT(".PropertyAccessor("),
			TEXT(".GeneratedAccessor("), TEXT(".NoDiscard("), TEXT(".WorldContext("),
			TEXT(".Callable("), TEXT(".ForceConstArgumentExpressions("),
			TEXT(".DeterminesOutputType("), TEXT(".PassScriptFunctionAsFirstParam("),
			TEXT(".PassScriptObjectTypeAsFirstParam("), TEXT(".Documentation("),
			TEXT(".NativeConstructor("), TEXT(".NativeDestructor("), TEXT(".NativeAssignment("),
			TEXT(".NativeUObjectCast("), TEXT(".NativeMethod("), TEXT(".NativeFunction("),
			TEXT(".NativeFunctionHeader("), TEXT(".NativeUFunction("), TEXT(".NativeTArrayIndex("),
			TEXT(".NativeTArrayIteratorCreate("), TEXT(".NativeTArrayIteratorProceed("),
			TEXT(".NativeTemplateInstantiatedCall("), TEXT(".NativeDelegateExecute("),
			TEXT(".NativeMulticastExecute("), TEXT(".NativeEventFunctionExecute("),
			TEXT(".NativePushArgument("), TEXT(".NativePushArgumentRef("),
			TEXT(".CompileOutEntirely("), TEXT(".CompileOutAsMethodChain("),
			TEXT(".CompileOutInTest("), TEXT(".CompileOutIfNoLog("),
			TEXT(".CompileOutAsEnsure("), TEXT(".CompileOutAsCheck("),
			TEXT(".ReplaceWithFirstArgInTest("), TEXT(".PureConstant("),
		};

		const FString TrimmedLine = Line.TrimStart();
		for (const TCHAR* TraitCall : TraitCalls)
		{
			if (TrimmedLine.StartsWith(TraitCall))
			{
				return true;
			}
		}
		return false;
	}

	static bool ContainsGameplayTagRegistrationCall(const FString& Line)
	{
		static const TCHAR* RegistrationCalls[] = {
			TEXT(".Method("), TEXT(".GenericMethod("), TEXT(".Constructor("),
			TEXT(".ImplicitConstructor("), TEXT(".Destructor("), TEXT(".Factory("),
			TEXT(".Behaviour("), TEXT(".TemplateCallback("), TEXT(".Property("),
			TEXT("BindGlobalFunctionForTarget("), TEXT("BindGlobalGenericFunctionForTarget("),
			TEXT("BindGlobalFunctionDirectForTarget("), TEXT("BindMethodDirectForTarget("),
			TEXT("BindGlobalVariableForTarget("),
		};
		for (const TCHAR* RegistrationCall : RegistrationCalls)
		{
			if (Line.Contains(RegistrationCall))
			{
				return true;
			}
		}
		return false;
	}

	static void FindGameplayTagTraitLayoutViolations(
		const FString& FileName,
		const FString& Source,
		TArray<FString>& OutViolations)
	{
		TArray<FString> Lines;
		Source.ParseIntoArrayLines(Lines, false);
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FString& Line = Lines[LineIndex];
			const int32 TraitCount = CountGameplayTagTraitCalls(Line);
			if (TraitCount == 0)
				continue;

			if (TraitCount > 1)
			{
				OutViolations.Add(FString::Printf(TEXT("%s:%d: multiple traits share one line"), *FileName, LineIndex + 1));
				continue;
			}

			if (!StartsWithGameplayTagTraitCall(Line))
			{
				if (!ContainsGameplayTagRegistrationCall(Line))
					continue;
				if (LineIndex + 1 < Lines.Num() && StartsWithGameplayTagTraitCall(Lines[LineIndex + 1]))
					OutViolations.Add(FString::Printf(TEXT("%s:%d: first trait in a multi-trait chain is inline"), *FileName, LineIndex + 1));
				else if (Line.Len() > 120)
					OutViolations.Add(FString::Printf(TEXT("%s:%d: long single trait is inline"), *FileName, LineIndex + 1));
				continue;
			}

			const bool bMultiTrait =
				(LineIndex > 0 && StartsWithGameplayTagTraitCall(Lines[LineIndex - 1]))
				|| (LineIndex + 1 < Lines.Num() && StartsWithGameplayTagTraitCall(Lines[LineIndex + 1]));
			if (bMultiTrait || LineIndex == 0)
				continue;

			const FString RegistrationLine = Lines[LineIndex - 1].TrimEnd();
			const FString CombinedLine = RegistrationLine + Line.TrimStartAndEnd();
			if (RegistrationLine.TrimStartAndEnd().EndsWith(TEXT(")"))
				&& ContainsGameplayTagRegistrationCall(RegistrationLine)
				&& CombinedLine.Len() <= 120)
			{
				OutViolations.Add(FString::Printf(TEXT("%s:%d: short single trait is split"), *FileName, LineIndex + 1));
			}
		}
	}

public:
	TEST_METHOD(ProviderUsesDirectPhasesExplicitTargetAndNamedCallables)
	{
		FString RegistrationSource;
		FString CallableHeaderSource;
		FString CallableImplementationSource;
		ASSERT_THAT(IsTrue(
			LoadGameplayTagBindSource(TEXT("Bind_FGameplayTag.cpp"), RegistrationSource),
			TEXT("GameplayTag registration source should be readable")));
		ASSERT_THAT(IsTrue(
			LoadGameplayTagBindSource(TEXT("Bind_FGameplayTag_Functions.h"), CallableHeaderSource),
			TEXT("GameplayTag callable header should be readable")));
		ASSERT_THAT(IsTrue(
			LoadGameplayTagBindSource(TEXT("Bind_FGameplayTag_Functions.cpp"), CallableImplementationSource),
			TEXT("GameplayTag callable implementation should be readable")));

		ASSERT_THAT(IsFalse(
			RegistrationSource.Contains(TEXT("FAngelscriptBinds::FBind"))
				|| RegistrationSource.Contains(TEXT("FAngelscriptBinds::EOrder"))
				|| RegistrationSource.Contains(TEXT("FAngelscriptBinds::RegisterBinds")),
			TEXT("GameplayTag providers should not retain the legacy registrar")));
		ASSERT_THAT(IsTrue(
			RegistrationSource.Contains(TEXT("EAngelscriptBindPhase::TypeInfrastructure"))
				&& RegistrationSource.Contains(TEXT("EAngelscriptBindPhase::ExplicitBindings")),
			TEXT("GameplayTag providers should declare infrastructure and explicit binding phases")));
		ASSERT_THAT(IsTrue(
			RegistrationSource.Contains(TEXT("ExistingClassForTarget"))
				&& RegistrationSource.Contains(TEXT("Binds.GetTargetEngine()"))
				&& RegistrationSource.Contains(TEXT("BindGlobalVariableForTarget"))
				&& RegistrationSource.Contains(TEXT("BindGlobalFunctionForTarget"))
				&& RegistrationSource.Contains(TEXT("FToStringHelper::Register(Binds")),
			TEXT("GameplayTag registration should mutate only its explicit target engine")));
		ASSERT_THAT(IsFalse(
			RegistrationSource.Contains(TEXT("GetCurrentScriptEngine")),
			TEXT("GameplayTag binding helpers should not select an ambient engine")));
		ASSERT_THAT(IsTrue(
			RegistrationSource.Contains(TEXT("&FAngelscriptFGameplayTagBinds::RemoveTag"))
				&& RegistrationSource.Contains(TEXT("&FAngelscriptFGameplayTagBinds::AppendToString")),
			TEXT("Project-owned GameplayTag callables should use the stable named owner")));
		ASSERT_THAT(IsTrue(
			CallableHeaderSource.Contains(TEXT("struct FAngelscriptFGameplayTagBinds"))
				&& CallableImplementationSource.Contains(TEXT("FAngelscriptFGameplayTagBinds::RemoveTag"))
				&& CallableImplementationSource.Contains(TEXT("FAngelscriptFGameplayTagBinds::AppendToString")),
			TEXT("GameplayTag callable declarations and definitions should live in companion files")));

		const int32 NativeFormCount =
			CountOccurrences(RegistrationSource, TEXT("METHOD_TRIVIAL"))
			+ CountOccurrences(RegistrationSource, TEXT("METHODPR_TRIVIAL"))
			+ CountOccurrences(RegistrationSource, TEXT("FUNC_TRIVIAL"));
		ASSERT_THAT(AreEqual(
			35,
			NativeFormCount,
			TEXT("GameplayTag migration should preserve all 35 native/trivial forms")));
	}

	TEST_METHOD(NamespaceGlobalReplayTracksEachEngineIncrementally)
	{
		FString RegistrationSource;
		ASSERT_THAT(IsTrue(
			LoadGameplayTagBindSource(TEXT("Bind_FGameplayTag.cpp"), RegistrationSource),
			TEXT("GameplayTag registration source should be readable")));

		ASSERT_THAT(IsTrue(
			RegistrationSource.Contains(
				TEXT("TMap<FAngelscriptEngine*, int32> GGameplayTagNamespaceGlobalCounts"))
				&& RegistrationSource.Contains(
					TEXT("int32& BoundTagCount = GGameplayTagNamespaceGlobalCounts.FindOrAdd(&Engine)"))
				&& RegistrationSource.Contains(
					TEXT("for (int32 TagIndex = BoundTagCount; TagIndex < CachedTagCount; ++TagIndex)"))
				&& RegistrationSource.Contains(TEXT("BoundTagCount = CachedTagCount")),
			TEXT("GameplayTag namespace replay should bind each engine's uncopied cache suffix")));
		ASSERT_THAT(IsFalse(
			RegistrationSource.Contains(TEXT("GEnginesWithGameplayTagNamespaceGlobals")),
			TEXT("A boolean per-engine replay marker would skip old tags after another engine grows the process cache")));
		ASSERT_THAT(IsTrue(
			RegistrationSource.Contains(TEXT("GGameplayTagNamespaceGlobalCounts.Remove(&Engine)")),
			TEXT("GameplayTag engine detach should discard its incremental replay cursor")));
	}

	TEST_METHOD(OptionalPluginBindTraitChainsFollowTheExactOneHundredTwentyColumnLayout)
	{
		const FString BindsDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("AngelscriptGameplayTags/Source/AngelscriptGameplayTags/Private/Binds"));
		TArray<FString> BindFiles;
		IFileManager::Get().FindFiles(BindFiles, *FPaths::Combine(BindsDirectory, TEXT("Bind_*.cpp")), true, false);
		BindFiles.Sort();

		TArray<FString> Violations;
		for (const FString& BindFile : BindFiles)
		{
			FString Source;
			ASSERT_THAT(IsTrue(
				FFileHelper::LoadFileToString(Source, *FPaths::Combine(BindsDirectory, BindFile)),
				FString::Printf(TEXT("GameplayTags bind source should be readable: %s"), *BindFile)));
			FindGameplayTagTraitLayoutViolations(BindFile, Source, Violations);
		}

		ASSERT_THAT(IsTrue(
			Violations.IsEmpty(),
			FString::Printf(TEXT("GameplayTags bind trait chain layout violations:\n%s"), *FString::Join(Violations, TEXT("\n")))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
