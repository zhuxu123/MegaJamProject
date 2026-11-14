#pragma once

#include "CoreMinimal.h"

struct FGameplayAttribute;

struct FAngelscriptFGameplayAttributeBinds
{
	static UClass* GetAttributeSetClass(const FGameplayAttribute& Attribute);
};
