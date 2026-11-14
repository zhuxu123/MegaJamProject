#include "AngelscriptBinds.h"

#include "Bind_FGameplayAbilitySpec_Functions.h"
#include "GameplayAbilitySpec.h"

AS_FORCE_LINK const FAngelscriptBind Bind_FGameplayAbilitySpec(
	TEXT("FGameplayAbilitySpec"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FGameplayAbilitySpec_ = Binds.ExistingClassForTarget("FGameplayAbilitySpec");

		FGameplayAbilitySpec_.Constructor(
			"void f(TSubclassOf<UGameplayAbility> InAbilityClass, int32 InLevel = 1,"
			" int32 InInputID = -1, UObject InSourceObject = nullptr)",
			&FAngelscriptFGameplayAbilitySpecBinds::ConstructFromAbilityClass)
			.NativeConstructor("FGameplayAbilitySpec", true);

		FGameplayAbilitySpec_.Constructor(
			"void f(UGameplayAbility InAbility, int32 InLevel = 1, int32 InInputID = -1, UObject InSourceObject = nullptr)",
			&FAngelscriptFGameplayAbilitySpecBinds::ConstructFromAbilityInstance)
			.NativeConstructor("FGameplayAbilitySpec", true);

		FGameplayAbilitySpec_.Constructor(
			"void f(FGameplayAbilitySpecDef& InDef, int32 InGameplayEffectLevel,"
			" FActiveGameplayEffectHandle InGameplayEffectHandle = FActiveGameplayEffectHandle())",
			&FAngelscriptFGameplayAbilitySpecBinds::ConstructFromDefinition)
			.NativeConstructor("FGameplayAbilitySpec", true);

		FGameplayAbilitySpec_.Property("FGameplayAbilitySpecHandle Handle", &FGameplayAbilitySpec::Handle);
		FGameplayAbilitySpec_.Property(
			"UGameplayAbility unresolved_object Ability",
			&FGameplayAbilitySpec::Ability);
		FGameplayAbilitySpec_.Property("int32 Level", &FGameplayAbilitySpec::Level);
		FGameplayAbilitySpec_.Property("int32 InputID", &FGameplayAbilitySpec::InputID);
		FGameplayAbilitySpec_.Property(
			"TWeakObjectPtr<UObject> SourceObject",
			&FGameplayAbilitySpec::SourceObject);
		FGameplayAbilitySpec_.Property("uint8 ActiveCount", &FGameplayAbilitySpec::ActiveCount);
		FGameplayAbilitySpec_.Property(
			"FGameplayAbilityActivationInfo ActivationInfo",
			&FGameplayAbilitySpec::ActivationInfo);
		FGameplayAbilitySpec_.Property(
			"FGameplayTagContainer DynamicAbilityTags",
			&FGameplayAbilitySpec::DynamicAbilityTags);
		FGameplayAbilitySpec_.Property(
			"TArray<TObjectPtr<UGameplayAbility>> NonReplicatedInstances",
			&FGameplayAbilitySpec::NonReplicatedInstances);
		FGameplayAbilitySpec_.Property(
			"TArray<TObjectPtr<UGameplayAbility>> ReplicatedInstances",
			&FGameplayAbilitySpec::ReplicatedInstances);
		FGameplayAbilitySpec_.Property(
			"FActiveGameplayEffectHandle GameplayEffectHandle",
			&FGameplayAbilitySpec::GameplayEffectHandle);
		FGameplayAbilitySpec_.Property(
			"TMap<FGameplayTag, float32> SetByCallerTagMagnitudes",
			&FGameplayAbilitySpec::SetByCallerTagMagnitudes);

		// Bit-fields cannot be bound directly as properties.
		FGameplayAbilitySpec_.Method(
			"bool GetbInputPressed() const",
			&FAngelscriptFGameplayAbilitySpecBinds::GetInputPressed);
		FGameplayAbilitySpec_.Method(
			"void SetbInputPressed(bool bValue)",
			&FAngelscriptFGameplayAbilitySpecBinds::SetInputPressed);
		FGameplayAbilitySpec_.Method(
			"bool GetbRemoveAfterActivation() const",
			&FAngelscriptFGameplayAbilitySpecBinds::GetRemoveAfterActivation);
		FGameplayAbilitySpec_.Method(
			"void SetbRemoveAfterActivation(bool bValue)",
			&FAngelscriptFGameplayAbilitySpecBinds::SetRemoveAfterActivation);
		FGameplayAbilitySpec_.Method(
			"bool GetbPendingRemove() const",
			&FAngelscriptFGameplayAbilitySpecBinds::GetPendingRemove);
		FGameplayAbilitySpec_.Method(
			"void SetbPendingRemove(bool bValue)",
			&FAngelscriptFGameplayAbilitySpecBinds::SetPendingRemove);
		FGameplayAbilitySpec_.Method(
			"bool GetbActivateOnce() const",
			&FAngelscriptFGameplayAbilitySpecBinds::GetActivateOnce);
		FGameplayAbilitySpec_.Method(
			"void SetbActivateOnce(bool bValue)",
			&FAngelscriptFGameplayAbilitySpecBinds::SetActivateOnce);

		FGameplayAbilitySpec_.Method(
			"UGameplayAbility GetPrimaryInstance() const",
			METHOD_TRIVIAL(FGameplayAbilitySpec, GetPrimaryInstance));
		FGameplayAbilitySpec_.Method(
			"bool ShouldReplicateAbilitySpec() const",
			METHOD_TRIVIAL(FGameplayAbilitySpec, ShouldReplicateAbilitySpec));
		FGameplayAbilitySpec_.Method(
			"TArray<UGameplayAbility> GetAbilityInstances() const",
			METHOD_TRIVIAL(FGameplayAbilitySpec, GetAbilityInstances));
		FGameplayAbilitySpec_.Method(
			"bool IsActive() const",
			METHOD_TRIVIAL(FGameplayAbilitySpec, IsActive));
		FGameplayAbilitySpec_.Method(
			"void PreReplicatedRemoved(const FGameplayAbilitySpecContainer& InArraySerializer)",
			METHODPR_TRIVIAL(
				void,
				FGameplayAbilitySpec,
				PreReplicatedRemove,
				(const FGameplayAbilitySpecContainer&)));
		FGameplayAbilitySpec_.Method(
			"void PostReplicatedAdd(const FGameplayAbilitySpecContainer& InArraySerializer)",
			METHODPR_TRIVIAL(
				void,
				FGameplayAbilitySpec,
				PostReplicatedAdd,
				(const FGameplayAbilitySpecContainer&)));
		FGameplayAbilitySpec_.Method(
			"FString GetDebugString()",
			METHOD_TRIVIAL(FGameplayAbilitySpec, GetDebugString));

	});
