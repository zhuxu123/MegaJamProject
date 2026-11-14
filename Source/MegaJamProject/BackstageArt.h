#pragma once
#include "CoreMinimal.h"
class UTexture2D;
// Read-only access to the existing four procedural badges; no change to old stage behavior.
UTexture2D* BackstageGetEmotionTexture(const FName& Kind);
