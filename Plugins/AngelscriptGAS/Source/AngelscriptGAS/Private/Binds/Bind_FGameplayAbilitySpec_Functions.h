#pragma once

#include "CoreMinimal.h"

class UGameplayAbility;
struct FActiveGameplayEffectHandle;
struct FGameplayAbilitySpec;
struct FGameplayAbilitySpecDef;

struct FAngelscriptFGameplayAbilitySpecBinds
{
	static void ConstructFromAbilityClass(
		FGameplayAbilitySpec* Address,
		TSubclassOf<UGameplayAbility> InAbilityClass,
		int32 InLevel,
		int32 InInputID,
		UObject* InSourceObject);

	static void ConstructFromAbilityInstance(
		FGameplayAbilitySpec* Address,
		UGameplayAbility* InAbility,
		int32 InLevel,
		int32 InInputID,
		UObject* InSourceObject);

	static void ConstructFromDefinition(
		FGameplayAbilitySpec* Address,
		FGameplayAbilitySpecDef& InDef,
		int32 InGameplayEffectLevel,
		FActiveGameplayEffectHandle InGameplayEffectHandle);

	static bool GetInputPressed(const FGameplayAbilitySpec& Spec);
	static void SetInputPressed(FGameplayAbilitySpec& Spec, bool bValue);
	static bool GetRemoveAfterActivation(const FGameplayAbilitySpec& Spec);
	static void SetRemoveAfterActivation(FGameplayAbilitySpec& Spec, bool bValue);
	static bool GetPendingRemove(const FGameplayAbilitySpec& Spec);
	static void SetPendingRemove(FGameplayAbilitySpec& Spec, bool bValue);
	static bool GetActivateOnce(const FGameplayAbilitySpec& Spec);
	static void SetActivateOnce(FGameplayAbilitySpec& Spec, bool bValue);
};
