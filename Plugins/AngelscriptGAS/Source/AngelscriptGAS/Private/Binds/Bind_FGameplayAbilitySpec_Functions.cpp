#include "Bind_FGameplayAbilitySpec_Functions.h"

#include "GameplayAbilitySpec.h"

void FAngelscriptFGameplayAbilitySpecBinds::ConstructFromAbilityClass(
	FGameplayAbilitySpec* Address,
	TSubclassOf<UGameplayAbility> InAbilityClass,
	int32 InLevel,
	int32 InInputID,
	UObject* InSourceObject)
{
	new (Address) FGameplayAbilitySpec(InAbilityClass, InLevel, InInputID, InSourceObject);
}

void FAngelscriptFGameplayAbilitySpecBinds::ConstructFromAbilityInstance(
	FGameplayAbilitySpec* Address,
	UGameplayAbility* InAbility,
	int32 InLevel,
	int32 InInputID,
	UObject* InSourceObject)
{
	new (Address) FGameplayAbilitySpec(InAbility, InLevel, InInputID, InSourceObject);
}

void FAngelscriptFGameplayAbilitySpecBinds::ConstructFromDefinition(
	FGameplayAbilitySpec* Address,
	FGameplayAbilitySpecDef& InDef,
	int32 InGameplayEffectLevel,
	FActiveGameplayEffectHandle InGameplayEffectHandle)
{
	new (Address) FGameplayAbilitySpec(InDef, InGameplayEffectLevel, InGameplayEffectHandle);
}

bool FAngelscriptFGameplayAbilitySpecBinds::GetInputPressed(const FGameplayAbilitySpec& Spec)
{
	return Spec.InputPressed != 0;
}

void FAngelscriptFGameplayAbilitySpecBinds::SetInputPressed(FGameplayAbilitySpec& Spec, bool bValue)
{
	Spec.InputPressed = bValue;
}

bool FAngelscriptFGameplayAbilitySpecBinds::GetRemoveAfterActivation(const FGameplayAbilitySpec& Spec)
{
	return Spec.RemoveAfterActivation != 0;
}

void FAngelscriptFGameplayAbilitySpecBinds::SetRemoveAfterActivation(
	FGameplayAbilitySpec& Spec,
	bool bValue)
{
	Spec.RemoveAfterActivation = bValue;
}

bool FAngelscriptFGameplayAbilitySpecBinds::GetPendingRemove(const FGameplayAbilitySpec& Spec)
{
	return Spec.PendingRemove != 0;
}

void FAngelscriptFGameplayAbilitySpecBinds::SetPendingRemove(FGameplayAbilitySpec& Spec, bool bValue)
{
	Spec.PendingRemove = bValue;
}

bool FAngelscriptFGameplayAbilitySpecBinds::GetActivateOnce(const FGameplayAbilitySpec& Spec)
{
	return Spec.bActivateOnce != 0;
}

void FAngelscriptFGameplayAbilitySpecBinds::SetActivateOnce(FGameplayAbilitySpec& Spec, bool bValue)
{
	Spec.bActivateOnce = bValue;
}
