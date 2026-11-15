#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BackstageSfx.generated.h"

class APlayerController;
class UAudioComponent;
class USoundWave;

// Shared by the existing AngelScript rounds and the native Superstar preview.
UCLASS()
class UBackstageSfx : public UActorComponent
{
    GENERATED_BODY()
public:
    UBackstageSfx();
    static UBackstageSfx* For(APlayerController* Controller);
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
    void PlayCue(FName Id);
    void Motion(int32 Transport, bool bPan, int32 Zoom);
    void TransportMotion(int32 Direction);
    void CameraMotion(bool bPan, int32 Zoom);
private:
    UPROPERTY(Transient) TMap<FName,TObjectPtr<USoundWave>> Sounds;
    UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> Voices;
    TMap<FName,double> LastPlayed;
    int32 LastTransport = 0;
    int32 PlayedCount = 0;
    bool bSmoke = false;
    double SmokeStart = 0.;
    int32 SmokeStep = 0;
    void FadeChannel(int32 Channel);
};
