#pragma once

#include "CoreMinimal.h"

class UGameplayEffect;
struct FGameplayEffectContextHandle;
struct FGameplayEffectSpec;

struct FAngelscriptFGameplayEffectSpecBinds
{
	static void ConstructFromEffectDefinition(
		FGameplayEffectSpec* Address,
		const UGameplayEffect* InDef,
		const FGameplayEffectContextHandle& InEffectContext,
		float Level);

	static void ConstructCopyWithContext(
		FGameplayEffectSpec* Address,
		const FGameplayEffectSpec& Other,
		const FGameplayEffectContextHandle& InEffectContext);
};
