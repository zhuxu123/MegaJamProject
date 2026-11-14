#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "SuperstarMusic.generated.h"

class UAudioComponent;
class FJsonObject;

// PCM is initialized before playback, then immutable. Cursor belongs only to the audio thread.
UCLASS()
class USuperstarMusicWave : public USoundWaveProcedural
{
    GENERATED_BODY()
public:
    bool LoadLoop(const FString& File);
    virtual int32 OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples) override;
private:
    TArray<uint8> PCM;
    int32 Cursor = 0;
};

USTRUCT()
struct FSuperstarMusicVoice
{
    GENERATED_BODY()
    UPROPERTY(Transient) TObjectPtr<UAudioComponent> Audio;
    UPROPERTY(Transient) TObjectPtr<USuperstarMusicWave> Wave;
    float Gain = 0.f;
    float Target = 0.f;
};

UCLASS()
class USuperstarMusic : public UActorComponent
{
    GENERATED_BODY()
public:
    USuperstarMusic();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
    void SelectBeat(int32 OneBasedBeat);
    void SetStoryPaused(bool bPause);
    void ToggleMute();
    FString Status() const;
private:
    UPROPERTY(Transient) TArray<FSuperstarMusicVoice> Voices;
    TSharedPtr<FJsonObject> Manifest;
    FString CurrentId;
    FString CurrentTitle;
    int32 Active = -1;
    float MasterVolume = .48f;
    float Crossfade = 1.4f;
    float MuteGain = 1.f;
    bool bPaused = false;
    bool bMuted = false;
};
