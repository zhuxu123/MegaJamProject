#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

struct FAngelscriptFGameplayTagBinds
{
	static bool RemoveTag(FGameplayTagContainer& Container, const FGameplayTag& TagToRemove);
	static void AppendToString(void* Value, FString& String);
};
