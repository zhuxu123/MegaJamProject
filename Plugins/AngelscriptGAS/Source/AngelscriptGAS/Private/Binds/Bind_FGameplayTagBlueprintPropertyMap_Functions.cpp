#include "Bind_FGameplayTagBlueprintPropertyMap_Functions.h"

#include "AbilitySystemComponent.h"
#include "Core/AngelscriptEngine.h"
#include "GameplayEffectTypes.h"

void FAngelscriptFGameplayTagBlueprintPropertyMapBinds::InitializeChecked(
	FGameplayTagBlueprintPropertyMap* PropertyMap,
	UObject* Owner,
	UAbilitySystemComponent* ASC)
{
	if (Owner == nullptr)
	{
		FAngelscriptEngine::Throw("GameplayTagBlueprintPropertyMap.Initialize received a null Owner.");
		return;
	}

	if (ASC == nullptr)
	{
		FAngelscriptEngine::Throw("GameplayTagBlueprintPropertyMap.Initialize received a null AbilitySystemComponent.");
		return;
	}

	PropertyMap->Initialize(Owner, ASC);
}
