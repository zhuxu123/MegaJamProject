#include "AngelscriptBinds.h"

#include "AbilitySystemComponent.h"
#include "Bind_FGameplayTagBlueprintPropertyMap_Functions.h"
#include "GameplayEffectTypes.h"

AS_FORCE_LINK const FAngelscriptBind Bind_FGameplayTagBlueprintPropertyMap(
	TEXT("FGameplayTagBlueprintPropertyMap"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FGameplayTagBlueprintPropertyMap_ =
			Binds.ExistingClassForTarget("FGameplayTagBlueprintPropertyMap");
		FGameplayTagBlueprintPropertyMap_.Method(
			"void Initialize(UObject Owner, UAbilitySystemComponent ASC)",
			&FAngelscriptFGameplayTagBlueprintPropertyMapBinds::InitializeChecked);
		FGameplayTagBlueprintPropertyMap_.Method(
			"void ApplyCurrentTags()",
			METHOD_TRIVIAL(FGameplayTagBlueprintPropertyMap, ApplyCurrentTags));

	});
