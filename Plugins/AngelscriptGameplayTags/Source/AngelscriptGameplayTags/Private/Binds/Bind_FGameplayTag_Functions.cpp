#include "Bind_FGameplayTag_Functions.h"

bool FAngelscriptFGameplayTagBinds::RemoveTag(
	FGameplayTagContainer& Container,
	const FGameplayTag& TagToRemove)
{
	return Container.RemoveTag(TagToRemove, false);
}

void FAngelscriptFGameplayTagBinds::AppendToString(void* Value, FString& String)
{
	String += static_cast<FGameplayTag*>(Value)->ToString();
}
