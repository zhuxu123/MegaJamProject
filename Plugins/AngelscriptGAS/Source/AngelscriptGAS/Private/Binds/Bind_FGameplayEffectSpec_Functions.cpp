#include "Bind_FGameplayEffectSpec_Functions.h"

#include "AngelscriptEngine.h"
#include "GameplayEffect.h"

void FAngelscriptFGameplayEffectSpecBinds::ConstructFromEffectDefinition(
	FGameplayEffectSpec* Address,
	const UGameplayEffect* InDef,
	const FGameplayEffectContextHandle& InEffectContext,
	float Level)
{
	if (InDef == nullptr)
	{
		new (Address) FGameplayEffectSpec();
		FAngelscriptEngine::Throw("GameplayEffect was null.");
		return;
	}

	new (Address) FGameplayEffectSpec(InDef, InEffectContext, Level);
}

void FAngelscriptFGameplayEffectSpecBinds::ConstructCopyWithContext(
	FGameplayEffectSpec* Address,
	const FGameplayEffectSpec& Other,
	const FGameplayEffectContextHandle& InEffectContext)
{
	new (Address) FGameplayEffectSpec(Other, InEffectContext);
}
