#include "Core/AngelscriptBinds.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"

namespace
{
	bool BackstagePressed(const APlayerController* Controller, const FName& KeyName)
	{
		return Controller->WasInputKeyJustPressed(FKey(KeyName));
	}

	bool BackstageHeld(const APlayerController* Controller, const FName& KeyName)
	{
		return Controller->IsInputKeyDown(FKey(KeyName));
	}

	void SetBackstagePointer(APlayerController* Controller, bool bVisible)
	{
		Controller->SetShowMouseCursor(bVisible);
		if (bVisible)
		{
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			Controller->SetInputMode(Mode);
		}
		else
		{
			Controller->SetInputMode(FInputModeGameOnly());
		}
	}

	FVector2D BackstagePointer(const APlayerController* Controller)
	{
		float X, Y;
		int32 Width, Height;
		Controller->GetViewportSize(Width, Height);
		if (Width <= 0 || Height <= 0 || !Controller->GetMousePosition(X, Y))
		{
			return FVector2D(-1, -1);
		}

		const double Scale = FMath::Min(Width / 1280.0, Height / 720.0);
		const FVector2D Origin((Width - 1280 * Scale) * 0.5, (Height - 720 * Scale) * 0.5);
		return (FVector2D(X, Y) - Origin) / Scale;
	}
}

static const FAngelscriptBind BindBackstageControls(
	TEXT("MegaJamProject.BackstageControls"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto Controller = Binds.ExistingClassForTarget("APlayerController");
		Controller.Method("bool BackstagePressed(const FName& KeyName) const", &BackstagePressed);
		Controller.Method("bool BackstageHeld(const FName& KeyName) const", &BackstageHeld);
		Controller.Method("void SetBackstagePointer(bool bVisible)", &SetBackstagePointer);
		Controller.Method("FVector2D BackstagePointer() const", &BackstagePointer);

		auto HUD = Binds.ExistingClassForTarget("AHUD");
		HUD.Method("APlayerController BackstagePlayer() const", METHOD_TRIVIAL(AHUD, GetOwningPlayerController));
		HUD.Method("void BackstageRect(FLinearColor Color, float32 X, float32 Y, float32 W, float32 H)", METHOD_TRIVIAL(AHUD, DrawRect));
		HUD.Method("void BackstageText(const FString& Text, FLinearColor Color, float32 X, float32 Y, UFont Font, float32 Scale, bool bScalePosition)", METHOD_TRIVIAL(AHUD, DrawText));
	});
