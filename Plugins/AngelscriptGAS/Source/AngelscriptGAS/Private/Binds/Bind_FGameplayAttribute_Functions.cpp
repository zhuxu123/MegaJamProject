#include "Bind_FGameplayAttribute_Functions.h"

#include "AttributeSet.h"

UClass* FAngelscriptFGameplayAttributeBinds::GetAttributeSetClass(const FGameplayAttribute& Attribute)
{
	return Attribute.IsValid() ? Attribute.GetAttributeSetClass() : nullptr;
}
