#include "AngelscriptAbilityAsyncLibrary.h"
#include "AngelscriptBinds.h"
#include "Core/FunctionCallers.h"

AS_FORCE_LINK const FAngelscriptBind Bind_AngelscriptGASLibrary(
	TEXT("AngelscriptGASLibrary.GeneratedOverrides"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		// Install exact addresses before GeneratedBindings can fall back to reflection.
		Binds.RegisterFunctionBindingForTarget(
			UAngelscriptAbilityAsyncLibrary::StaticClass(),
			"WaitForAttributeChanged",
			{ERASE_FUNCTION_PTR(
				UAngelscriptAbilityAsyncLibrary::WaitForAttributeChanged,
				(AActor*, const FGameplayAttribute&, const bool),
				ERASE_ARGUMENT_PACK(UAbilityAsync_WaitAttributeChanged*))});
		Binds.RegisterFunctionBindingForTarget(
			UAngelscriptAbilityAsyncLibrary::StaticClass(),
			"WaitGameplayEventToActor",
			{ERASE_FUNCTION_PTR(
				UAngelscriptAbilityAsyncLibrary::WaitGameplayEventToActor,
				(AActor*, const FGameplayTag, const bool, const bool),
				ERASE_ARGUMENT_PACK(UAbilityAsync_WaitGameplayEvent*))});
		Binds.RegisterFunctionBindingForTarget(
			UAngelscriptAbilityAsyncLibrary::StaticClass(),
			"WaitGameplayTagAddToActor",
			{ERASE_FUNCTION_PTR(
				UAngelscriptAbilityAsyncLibrary::WaitGameplayTagAddToActor,
				(AActor*, const FGameplayTag, const bool),
				ERASE_ARGUMENT_PACK(UAbilityAsync_WaitGameplayTagAdded*))});
		Binds.RegisterFunctionBindingForTarget(
			UAngelscriptAbilityAsyncLibrary::StaticClass(),
			"WaitGameplayTagRemoveFromActor",
			{ERASE_FUNCTION_PTR(
				UAngelscriptAbilityAsyncLibrary::WaitGameplayTagRemoveFromActor,
				(AActor*, const FGameplayTag, const bool),
				ERASE_ARGUMENT_PACK(UAbilityAsync_WaitGameplayTagRemoved*))});

	});
