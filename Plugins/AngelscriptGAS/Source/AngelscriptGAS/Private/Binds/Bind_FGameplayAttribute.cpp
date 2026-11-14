#include "AngelscriptBinds.h"

#include "AttributeSet.h"
#include "Bind_FGameplayAttribute_Functions.h"

AS_FORCE_LINK const FAngelscriptBind Bind_FGameplayAttribute(
	TEXT("FGameplayAttribute"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FGameplayAttribute_ = Binds.ExistingClassForTarget("FGameplayAttribute");
		FGameplayAttribute_.Method("bool IsValid() const", METHOD_TRIVIAL(FGameplayAttribute, IsValid));
		FGameplayAttribute_.Method(
			"UClass GetAttributeSetClass() const",
			&FAngelscriptFGameplayAttributeBinds::GetAttributeSetClass);

	});
