#include "SuperstarMusic.h"
#include "Audio.h"
#include "Components/AudioComponent.h"
#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool USuperstarMusicWave::LoadLoop(const FString& File)
{
    TArray<uint8> Bytes;
    FWaveModInfo Info;
    if (!FFileHelper::LoadFileToArray(Bytes, *File) || !Info.ReadWaveInfo(Bytes.GetData(), Bytes.Num())
        || *Info.pFormatTag != FWaveModInfo::WAVE_INFO_FORMAT_PCM || *Info.pBitsPerSample != 16
        || *Info.pChannels != 2 || *Info.pSamplesPerSec != 48000 || Info.SampleDataSize < 4
        || Info.SampleDataSize % 4 != 0) return false;
    PCM.Append(Info.SampleDataStart, Info.SampleDataSize);
    NumChannels = 2;
    SetSampleRate(48000);
    Duration = INDEFINITELY_LOOPING_DURATION;
    bLooping = false; // Circular PCM supplies the loop; no game-thread retrigger or silent gap.
    bCanProcessAsync = true;
    VirtualizationMode = EVirtualizationMode::PlayWhenSilent; // Fade-in starts at zero, mute retains musical position.
    return true;
}

int32 USuperstarMusicWave::OnGeneratePCMAudio(TArray<uint8>& OutAudio, int32 NumSamples)
{
    if (PCM.IsEmpty() || NumSamples <= 0) return 0;
    const int32 Bytes = NumSamples * sizeof(int16);
    OutAudio.SetNumUninitialized(Bytes);
    int32 Written = 0;
    while (Written < Bytes)
    {
        const int32 Count = FMath::Min(Bytes - Written, PCM.Num() - Cursor);
        FMemory::Memcpy(OutAudio.GetData() + Written, PCM.GetData() + Cursor, Count);
        Written += Count;
        Cursor = (Cursor + Count) % PCM.Num();
    }
    return NumSamples;
}

USuperstarMusic::USuperstarMusic()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void USuperstarMusic::BeginPlay()
{
    Super::BeginPlay();
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("SuperstarStory/Audio/music.json")))
        || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Manifest) || !Manifest.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("SuperstarMusic: missing/invalid manifest"));
        return;
    }
    MasterVolume = FMath::Clamp(static_cast<float>(Manifest->GetNumberField(TEXT("volume"))), 0.f, 1.f);
    Crossfade = FMath::Max(.1f, static_cast<float>(Manifest->GetNumberField(TEXT("crossfade_seconds"))));
    Voices.SetNum(2);
    for (auto& Voice : Voices)
    {
        Voice.Audio = NewObject<UAudioComponent>(GetOwner());
        Voice.Audio->bAutoActivate = false;
        Voice.Audio->bAutoDestroy = false;
        Voice.Audio->bStopWhenOwnerDestroyed = true;
        Voice.Audio->bAllowSpatialization = false;
        Voice.Audio->bIsUISound = true;
        Voice.Audio->bIsMusic = true;
        Voice.Audio->RegisterComponent();
    }
}

void USuperstarMusic::SelectBeat(int32 OneBasedBeat)
{
    if (!Manifest.IsValid() || Voices.Num() != 2) return;
    FString Id;
    if (!Manifest->GetObjectField(TEXT("beat_to_track"))->TryGetStringField(FString::FromInt(OneBasedBeat), Id)) return;
    if (Id == CurrentId) return; // Adjacent shots keep the musical phrase running.
    for (const auto& Value : Manifest->GetArrayField(TEXT("tracks")))
    {
        const auto Track = Value->AsObject();
        if (Track->GetStringField(TEXT("id")) != Id) continue;
        auto* Wave = NewObject<USuperstarMusicWave>(this);
        const FString File = Track->GetStringField(TEXT("file"));
        if (!Wave->LoadLoop(FPaths::ProjectContentDir() / TEXT("SuperstarStory") / File))
        {
            UE_LOG(LogTemp, Error, TEXT("SuperstarMusic: invalid PCM16 stereo 48k WAV %s"), *File);
            return;
        }
        const int32 Next = Active == 0 ? 1 : 0;
        auto& Voice = Voices[Next];
        Voice.Audio->Stop();
        Voice.Wave = Wave;
        Voice.Audio->SetSound(Wave);
        Voice.Gain = 0.f;
        Voice.Target = 1.f;
        Voice.Audio->SetVolumeMultiplier(0.f);
        Voice.Audio->Play();
        Voice.Audio->SetPaused(bPaused);
        if (Active >= 0) Voices[Active].Target = 0.f;
        Active = Next;
        CurrentId = Id;
        CurrentTitle = Track->GetStringField(TEXT("title"));
        UE_LOG(LogTemp, Display, TEXT("SuperstarMusic: cue=%s beat=%d playing=%d"), *Id, OneBasedBeat, Voice.Audio->IsPlaying());
        return;
    }
}

void USuperstarMusic::SetStoryPaused(bool bPause)
{
    if (bPaused == bPause) return;
    bPaused = bPause;
    for (auto& Voice : Voices) Voice.Audio->SetPaused(bPaused);
    UE_LOG(LogTemp, Display, TEXT("SuperstarMusic: paused=%d"), bPaused);
}

void USuperstarMusic::ToggleMute()
{
    bMuted = !bMuted;
    UE_LOG(LogTemp, Display, TEXT("SuperstarMusic: muted=%d"), bMuted);
}

FString USuperstarMusic::Status() const
{
    if (CurrentId.IsEmpty()) return TEXT("BGM 未载入");
    return FString::Printf(TEXT("M %s · %s"), bMuted ? TEXT("开启音乐") : TEXT("静音"), *CurrentTitle);
}

void USuperstarMusic::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(Delta, TickType, TickFunction);
    MuteGain = FMath::FInterpConstantTo(MuteGain, bMuted ? 0.f : 1.f, Delta, 5.f);
    for (auto& Voice : Voices)
    {
        if (!bPaused) Voice.Gain = FMath::FInterpConstantTo(Voice.Gain, Voice.Target, Delta, 1.f / Crossfade);
        Voice.Audio->SetVolumeMultiplier(Voice.Gain * MasterVolume * MuteGain);
        if (Voice.Target == 0.f && Voice.Gain == 0.f && Voice.Wave)
        {
            Voice.Audio->Stop();
            Voice.Audio->SetSound(nullptr);
            Voice.Wave = nullptr;
        }
    }
}

void USuperstarMusic::EndPlay(const EEndPlayReason::Type Reason)
{
    for (auto& Voice : Voices)
    {
        Voice.Audio->Stop();
        Voice.Audio->SetSound(nullptr);
        Voice.Audio->DestroyComponent();
        Voice.Wave = nullptr;
    }
    Voices.Empty();
    Super::EndPlay(Reason);
}
