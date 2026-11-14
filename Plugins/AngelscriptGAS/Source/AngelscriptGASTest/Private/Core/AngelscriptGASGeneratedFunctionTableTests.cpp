#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptGASGeneratedFunctionTableTests,
	"Angelscript.GAS.Engine.GeneratedFunctionTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL() { ASTEST_CREATE_ENGINE(); }
	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(PreservesHandwrittenEntries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();

		UClass* AbilityAsyncLibraryClass = FindObject<UClass>(
			nullptr,
			TEXT("/Script/AngelscriptGAS.AngelscriptAbilityAsyncLibrary"));
		if (!TestRunner->TestNotNull(
			TEXT("Generated GAS compatibility test should locate UAngelscriptAbilityAsyncLibrary"),
			AbilityAsyncLibraryClass))
		{
			return;
		}

		const TMap<FString, FAngelscriptFunctionBinding>* AsyncLibraryFunctionMap =
			Engine.GetBindState()->ClassFunctionBindings.Find(AbilityAsyncLibraryClass);
		if (!TestRunner->TestNotNull(
			TEXT("Generated GAS compatibility test should expose the async ability helper class in the engine bind state"),
			AsyncLibraryFunctionMap))
		{
			return;
		}

		static const TCHAR* ExpectedDirectFunctions[] = {
			TEXT("WaitForAttributeChanged"),
			TEXT("WaitGameplayEventToActor"),
			TEXT("WaitGameplayTagAddToActor"),
			TEXT("WaitGameplayTagRemoveFromActor"),
		};

		for (const TCHAR* FunctionName : ExpectedDirectFunctions)
		{
			const FAngelscriptFunctionBinding* FunctionEntry =
				AsyncLibraryFunctionMap->Find(FunctionName);
			if (!TestRunner->TestNotNull(
				*FString::Printf(
					TEXT("Generated GAS compatibility test should keep the handwritten %s entry"),
					FunctionName),
				FunctionEntry))
			{
				continue;
			}

			TestRunner->TestTrue(
				*FString::Printf(
					TEXT("Generated GAS compatibility test should preserve the direct-call pointer for %s"),
					FunctionName),
				FunctionEntry->FunctionPointer.IsBound());
			TestRunner->TestFalse(
				*FString::Printf(
					TEXT("Generated GAS compatibility test should not reclassify %s as reflective fallback"),
					FunctionName),
				FunctionEntry->bReflectiveFallbackBound);
		}
	}
};

#endif
