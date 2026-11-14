#include "AngelscriptBinds.h"

#include "Bind_FGameplayEffectSpec_Functions.h"
#include "GameplayEffect.h"

AS_FORCE_LINK const FAngelscriptBind Bind_FGameplayEffectSpec(
	TEXT("FGameplayEffectSpec"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FGameplayEffectSpec_ = Binds.ExistingClassForTarget("FGameplayEffectSpec");

		FGameplayEffectSpec_.Constructor(
			"void f(const UGameplayEffect InDef, const FGameplayEffectContextHandle& InEffectContext, float32 Level = -1.f)",
			&FAngelscriptFGameplayEffectSpecBinds::ConstructFromEffectDefinition)
			.NativeConstructor("FGameplayEffectSpec", true);

		FGameplayEffectSpec_.Constructor(
			"void f(const FGameplayEffectSpec& Other, const FGameplayEffectContextHandle& InEffectContext)",
			&FAngelscriptFGameplayEffectSpecBinds::ConstructCopyWithContext)
			.NativeConstructor("FGameplayEffectSpec", true);

		FGameplayEffectSpec_.Property("const UGameplayEffect unresolved_object Def", &FGameplayEffectSpec::Def);
		FGameplayEffectSpec_.Property(
			"TMap<FName, float32> SetByCallerNameMagnitudes",
			&FGameplayEffectSpec::SetByCallerNameMagnitudes);
		FGameplayEffectSpec_.Property(
			"TMap<FGameplayTag, float32> SetByCallerTagMagnitudes",
			&FGameplayEffectSpec::SetByCallerTagMagnitudes);

	});
