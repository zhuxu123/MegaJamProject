#include "CQTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptGASBindingArchitectureTests,
	"Angelscript.GAS.BindingArchitecture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FString MakeBindSourcePath(const TCHAR* RelativePath)
	{
		return FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("AngelscriptGAS/Source/AngelscriptGAS/Private/Binds"),
			RelativePath);
	}

	static bool LoadBindSource(const TCHAR* RelativePath, FString& OutSource)
	{
		return FFileHelper::LoadFileToString(OutSource, *MakeBindSourcePath(RelativePath));
	}

	static int32 CountOccurrences(const FString& Source, const TCHAR* Token)
	{
		int32 Count = 0;
		int32 SearchOffset = 0;
		while ((SearchOffset = Source.Find(
			Token,
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			SearchOffset)) != INDEX_NONE)
		{
			++Count;
			SearchOffset += FCString::Strlen(Token);
		}
		return Count;
	}

	static int32 CountGASTraitCalls(const FString& Line)
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

	static bool StartsWithGASTraitCall(const FString& Line)
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

	static bool ContainsGASRegistrationCall(const FString& Line)
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

	static void FindGASTraitLayoutViolations(
		const FString& FileName,
		const FString& Source,
		TArray<FString>& OutViolations)
	{
		TArray<FString> Lines;
		Source.ParseIntoArrayLines(Lines, false);
		for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
		{
			const FString& Line = Lines[LineIndex];
			const int32 TraitCount = CountGASTraitCalls(Line);
			if (TraitCount == 0)
				continue;

			if (TraitCount > 1)
			{
				OutViolations.Add(FString::Printf(TEXT("%s:%d: multiple traits share one line"), *FileName, LineIndex + 1));
				continue;
			}

			if (!StartsWithGASTraitCall(Line))
			{
				if (!ContainsGASRegistrationCall(Line))
					continue;
				if (LineIndex + 1 < Lines.Num() && StartsWithGASTraitCall(Lines[LineIndex + 1]))
					OutViolations.Add(FString::Printf(TEXT("%s:%d: first trait in a multi-trait chain is inline"), *FileName, LineIndex + 1));
				else if (Line.Len() > 120)
					OutViolations.Add(FString::Printf(TEXT("%s:%d: long single trait is inline"), *FileName, LineIndex + 1));
				continue;
			}

			const bool bMultiTrait =
				(LineIndex > 0 && StartsWithGASTraitCall(Lines[LineIndex - 1]))
				|| (LineIndex + 1 < Lines.Num() && StartsWithGASTraitCall(Lines[LineIndex + 1]));
			if (bMultiTrait || LineIndex == 0)
				continue;

			const FString RegistrationLine = Lines[LineIndex - 1].TrimEnd();
			const FString CombinedLine = RegistrationLine + Line.TrimStartAndEnd();
			if (RegistrationLine.TrimStartAndEnd().EndsWith(TEXT(")"))
				&& ContainsGASRegistrationCall(RegistrationLine)
				&& CombinedLine.Len() <= 120)
			{
				OutViolations.Add(FString::Printf(TEXT("%s:%d: short single trait is split"), *FileName, LineIndex + 1));
			}
		}
	}

public:
	TEST_METHOD(ProvidersUseDirectExplicitPhaseAndExplicitTargets)
	{
		static const TCHAR* ProviderFiles[] = {
			TEXT("Bind_AngelscriptGASLibrary.cpp"),
			TEXT("Bind_FGameplayAbilitySpec.cpp"),
			TEXT("Bind_FGameplayAttribute.cpp"),
			TEXT("Bind_FGameplayEffectSpec.cpp"),
			TEXT("Bind_FGameplayTagBlueprintPropertyMap.cpp"),
		};

		for (const TCHAR* ProviderFile : ProviderFiles)
		{
			FString Source;
			ASSERT_THAT(IsTrue(
				LoadBindSource(ProviderFile, Source),
				*FString::Printf(TEXT("%s should be readable"), ProviderFile)));
			ASSERT_THAT(IsFalse(
				Source.Contains(TEXT("FAngelscriptBinds::FBind"))
					|| Source.Contains(TEXT("FAngelscriptBinds::EOrder"))
					|| Source.Contains(TEXT("FAngelscriptBinds::RegisterBinds")),
				*FString::Printf(TEXT("%s should not retain the legacy registrar"), ProviderFile)));
			ASSERT_THAT(IsTrue(
				Source.Contains(TEXT("EAngelscriptBindPhase::ExplicitBindings"))
					&& Source.Contains(TEXT("FAngelscriptBinds& Binds")),
				*FString::Printf(TEXT("%s should use an ExplicitBindings callback"), ProviderFile)));
		}

		FString LibrarySource;
		ASSERT_THAT(IsTrue(
			LoadBindSource(TEXT("Bind_AngelscriptGASLibrary.cpp"), LibrarySource),
			TEXT("GAS library provider should be readable")));
		ASSERT_THAT(AreEqual(
			4,
			CountOccurrences(LibrarySource, TEXT("Binds.RegisterFunctionBindingForTarget")),
			TEXT("GAS library provider should install all four exact function-address overrides")));
		ASSERT_THAT(IsFalse(
			LibrarySource.Contains(TEXT("FAngelscriptBinds::RegisterFunctionBinding(")),
			TEXT("GAS library provider should not mutate an ambient function table")));

		static const TCHAR* ExistingClassProviderFiles[] = {
			TEXT("Bind_FGameplayAbilitySpec.cpp"),
			TEXT("Bind_FGameplayAttribute.cpp"),
			TEXT("Bind_FGameplayEffectSpec.cpp"),
			TEXT("Bind_FGameplayTagBlueprintPropertyMap.cpp"),
		};
		for (const TCHAR* ProviderFile : ExistingClassProviderFiles)
		{
			FString Source;
			ASSERT_THAT(IsTrue(
				LoadBindSource(ProviderFile, Source),
				*FString::Printf(TEXT("%s should be readable"), ProviderFile)));
			ASSERT_THAT(IsTrue(
				Source.Contains(TEXT("Binds.ExistingClassForTarget")),
				*FString::Printf(TEXT("%s should resolve its type through the explicit target"), ProviderFile)));
		}
	}

	TEST_METHOD(ProjectOwnedCallablesUseNamedCompanionOwners)
	{
		struct FCallableProviderExpectation
		{
			const TCHAR* FileStem;
			const TCHAR* OwnerName;
			int32 CallableCount;
		};

		static const FCallableProviderExpectation Providers[] = {
			{TEXT("FGameplayAbilitySpec"), TEXT("FAngelscriptFGameplayAbilitySpecBinds"), 11},
			{TEXT("FGameplayAttribute"), TEXT("FAngelscriptFGameplayAttributeBinds"), 1},
			{TEXT("FGameplayEffectSpec"), TEXT("FAngelscriptFGameplayEffectSpecBinds"), 2},
			{TEXT("FGameplayTagBlueprintPropertyMap"), TEXT("FAngelscriptFGameplayTagBlueprintPropertyMapBinds"), 1},
		};

		for (const FCallableProviderExpectation& Provider : Providers)
		{
			const FString RegistrationFile = FString::Printf(TEXT("Bind_%s.cpp"), Provider.FileStem);
			const FString HeaderFile = FString::Printf(TEXT("Bind_%s_Functions.h"), Provider.FileStem);
			const FString ImplementationFile = FString::Printf(TEXT("Bind_%s_Functions.cpp"), Provider.FileStem);

			FString RegistrationSource;
			FString HeaderSource;
			FString ImplementationSource;
			ASSERT_THAT(IsTrue(
				LoadBindSource(*RegistrationFile, RegistrationSource),
				*FString::Printf(TEXT("%s should be readable"), *RegistrationFile)));
			ASSERT_THAT(IsTrue(
				LoadBindSource(*HeaderFile, HeaderSource),
				*FString::Printf(TEXT("%s should declare the named callable owner"), *HeaderFile)));
			ASSERT_THAT(IsTrue(
				LoadBindSource(*ImplementationFile, ImplementationSource),
				*FString::Printf(TEXT("%s should define the named callables"), *ImplementationFile)));

			ASSERT_THAT(AreEqual(
				Provider.CallableCount,
				CountOccurrences(
					RegistrationSource,
					*FString::Printf(TEXT("&%s::"), Provider.OwnerName)),
				*FString::Printf(TEXT("%s should route every project-owned callable through its owner"), Provider.FileStem)));
			ASSERT_THAT(IsTrue(
				HeaderSource.Contains(FString::Printf(TEXT("struct %s"), Provider.OwnerName))
					&& ImplementationSource.Contains(FString::Printf(TEXT("%s::"), Provider.OwnerName)),
				*FString::Printf(TEXT("%s companion files should own declarations and definitions"), Provider.FileStem)));
		}

		ASSERT_THAT(IsFalse(
			IFileManager::Get().FileExists(*MakeBindSourcePath(TEXT("Bind_AngelscriptGASLibrary_Functions.h")))
				|| IFileManager::Get().FileExists(*MakeBindSourcePath(TEXT("Bind_AngelscriptGASLibrary_Functions.cpp"))),
			TEXT("Pointer-only GAS library overrides should not gain empty companion files")));
	}

	TEST_METHOD(NativeAndTrivialFormsRemainAttachedToExactResults)
	{
		FString AbilitySpecSource;
		FString AttributeSource;
		FString EffectSpecSource;
		FString PropertyMapSource;
		ASSERT_THAT(IsTrue(
			LoadBindSource(TEXT("Bind_FGameplayAbilitySpec.cpp"), AbilitySpecSource),
			TEXT("AbilitySpec provider should be readable")));
		ASSERT_THAT(IsTrue(
			LoadBindSource(TEXT("Bind_FGameplayAttribute.cpp"), AttributeSource),
			TEXT("GameplayAttribute provider should be readable")));
		ASSERT_THAT(IsTrue(
			LoadBindSource(TEXT("Bind_FGameplayEffectSpec.cpp"), EffectSpecSource),
			TEXT("EffectSpec provider should be readable")));
		ASSERT_THAT(IsTrue(
			LoadBindSource(TEXT("Bind_FGameplayTagBlueprintPropertyMap.cpp"), PropertyMapSource),
			TEXT("GameplayTag property-map provider should be readable")));

		const int32 AbilitySpecTrivialMethodCount =
			CountOccurrences(AbilitySpecSource, TEXT("METHOD_TRIVIAL"))
			+ CountOccurrences(AbilitySpecSource, TEXT("METHODPR_TRIVIAL"));
		ASSERT_THAT(AreEqual(
			7,
			AbilitySpecTrivialMethodCount,
			TEXT("AbilitySpec should preserve all seven trivial method forms")));
		ASSERT_THAT(AreEqual(
			1,
			CountOccurrences(AttributeSource, TEXT("METHOD_TRIVIAL")),
			TEXT("GameplayAttribute should preserve its trivial IsValid form")));
		ASSERT_THAT(AreEqual(
			1,
			CountOccurrences(PropertyMapSource, TEXT("METHOD_TRIVIAL")),
			TEXT("GameplayTag property map should preserve its trivial ApplyCurrentTags form")));

		ASSERT_THAT(AreEqual(
			3,
			CountOccurrences(AbilitySpecSource, TEXT(".NativeConstructor(\"FGameplayAbilitySpec\", true)")),
			TEXT("AbilitySpec should attach all three native constructor forms through exact results")));
		ASSERT_THAT(AreEqual(
			2,
			CountOccurrences(EffectSpecSource, TEXT(".NativeConstructor(\"FGameplayEffectSpec\", true)")),
			TEXT("EffectSpec should attach both native constructor forms through exact results")));
		ASSERT_THAT(IsFalse(
			AbilitySpecSource.Contains(TEXT("SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR"))
				|| EffectSpecSource.Contains(TEXT("SCRIPT_TRIVIAL_NATIVE_CONSTRUCTOR")),
			TEXT("GAS constructors should not retain implicit PreviousBind native metadata")));
		ASSERT_THAT(IsFalse(
			AbilitySpecSource.Contains(TEXT(").NativeConstructor"))
				|| EffectSpecSource.Contains(TEXT(").NativeConstructor")),
			TEXT("Long GAS constructor registrations should place fluent traits on continuation lines")));
		ASSERT_THAT(IsFalse(
			AbilitySpecSource.Contains(TEXT("FUNC_TRIVIAL(FAngelscriptFGameplayAbilitySpecBinds"))
				|| AttributeSource.Contains(TEXT("FUNC_TRIVIAL(FAngelscriptFGameplayAttributeBinds"))
				|| EffectSpecSource.Contains(TEXT("FUNC_TRIVIAL(FAngelscriptFGameplayEffectSpecBinds"))
				|| PropertyMapSource.Contains(TEXT("FUNC_TRIVIAL(FAngelscriptFGameplayTagBlueprintPropertyMapBinds")),
			TEXT("Former GAS lambdas should remain ordinary non-native callables")));
	}

	TEST_METHOD(OptionalPluginBindTraitChainsFollowTheExactOneHundredTwentyColumnLayout)
	{
		const FString BindsDirectory = MakeBindSourcePath(TEXT(""));
		TArray<FString> BindFiles;
		IFileManager::Get().FindFiles(BindFiles, *FPaths::Combine(BindsDirectory, TEXT("Bind_*.cpp")), true, false);
		BindFiles.Sort();

		TArray<FString> Violations;
		for (const FString& BindFile : BindFiles)
		{
			FString Source;
			ASSERT_THAT(IsTrue(
				FFileHelper::LoadFileToString(Source, *FPaths::Combine(BindsDirectory, BindFile)),
				FString::Printf(TEXT("GAS bind source should be readable: %s"), *BindFile)));
			FindGASTraitLayoutViolations(BindFile, Source, Violations);
		}

		ASSERT_THAT(IsTrue(
			Violations.IsEmpty(),
			FString::Printf(TEXT("GAS bind trait chain layout violations:\n%s"), *FString::Join(Violations, TEXT("\n")))));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
