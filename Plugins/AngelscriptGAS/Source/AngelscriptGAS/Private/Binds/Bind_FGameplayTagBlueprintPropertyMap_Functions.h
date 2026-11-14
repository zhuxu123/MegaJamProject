#pragma once

#include "CoreMinimal.h"

class UAbilitySystemComponent;
struct FGameplayTagBlueprintPropertyMap;

struct FAngelscriptFGameplayTagBlueprintPropertyMapBinds
{
	static void InitializeChecked(
		FGameplayTagBlueprintPropertyMap* PropertyMap,
		UObject* Owner,
		UAbilitySystemComponent* ASC);
};
