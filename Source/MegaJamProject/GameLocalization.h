#pragma once
#include "CoreMinimal.h"

// Presentation-only localization. Chinese story identifiers and scoring data stay unchanged.
struct FSlateFontInfo;
namespace GameLocalization
{
    FString WrapForWidth(const FString& Text, const FSlateFontInfo& Font, float Width);
    bool IsEnglish();
    void Toggle();
    FString Translate(const FString& Source);
}
