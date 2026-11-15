#include "BackstageSfx.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Components/AudioComponent.h"
#include "Core/AngelscriptBinds.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Sound/SoundWave.h"

namespace
{
    struct FCue { const TCHAR* Id; int32 Channel; float Volume; double Cooldown; };
    // Transport, camera, shutter and result each have one voice. Camera cannot accumulate voices.
    const FCue Cues[] = {
        {TEXT("chapter"),-1,.55f,.12}, {TEXT("forward"),0,.42f,.10}, {TEXT("rewind"),0,.42f,.10},
        {TEXT("shutter"),2,.65f,.14}, {TEXT("pan"),1,.18f,.28},
        {TEXT("zoom_in"),1,.24f,.30}, {TEXT("zoom_out"),1,.24f,.30}, {TEXT("result"),3,.55f,1.9}
    };
    void PlayControllerCue(APlayerController* PC,const FName& Id) { if(auto* S=UBackstageSfx::For(PC)) S->PlayCue(Id); }
    void ControllerMotion(APlayerController* PC,int32 Transport,bool bPan,int32 Zoom) { if(auto* S=UBackstageSfx::For(PC)) S->Motion(Transport,bPan,Zoom); }
    const FAngelscriptBind BindInterfaceAudio(TEXT("MegaJamProject.InterfaceAudio"),EAngelscriptBindPhase::ExplicitBindings,[](FAngelscriptBinds& Binds)
    {
        auto Controller=Binds.ExistingClassForTarget("APlayerController");
        Controller.Method("void BackstagePlayCue(const FName& Id)",&PlayControllerCue);
        Controller.Method("void BackstageAudioMotion(int Transport, bool bPan, int Zoom)",&ControllerMotion);
    });
}

UBackstageSfx::UBackstageSfx()
{
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.bStartWithTickEnabled=false;
}

UBackstageSfx* UBackstageSfx::For(APlayerController* PC)
{
    if(!PC || !PC->IsLocalController() || PC->GetNetMode()==NM_DedicatedServer) return nullptr;
    auto* Existing=PC->FindComponentByClass<UBackstageSfx>();
    if(Existing) return Existing;
    auto* New=NewObject<UBackstageSfx>(PC);
    PC->AddInstanceComponent(New);
    New->RegisterComponent();
    return New;
}

void UBackstageSfx::BeginPlay()
{
    Super::BeginPlay();
    for(const auto& Cue:Cues)
    {
        const FString Path=FString::Printf(TEXT("/Game/Audio/Interface/SFX_%s.SFX_%s"),Cue.Id,Cue.Id);
        auto* Sound=LoadObject<USoundWave>(nullptr,*Path);
        if(Sound) Sounds.Add(FName(Cue.Id),Sound);
        else UE_LOG(LogTemp,Error,TEXT("InterfaceSfx: missing %s"),*Path);
    }
    for(int32 I=0;I<4;++I)
    {
        auto* Audio=NewObject<UAudioComponent>(GetOwner());
        Audio->bAutoActivate=false;Audio->bAutoDestroy=false;Audio->bStopWhenOwnerDestroyed=true;
        Audio->bAllowSpatialization=false;Audio->bIsUISound=true;Audio->bIsMusic=false;
        Audio->RegisterComponent();Voices.Add(Audio);
    }
    bSmoke=FParse::Param(FCommandLine::Get(),TEXT("InterfaceSfxSmoke"));
    if(bSmoke)
    {
        check(Sounds.Num()==UE_ARRAY_COUNT(Cues));
        SmokeStart=FPlatformTime::Seconds();SetComponentTickEnabled(true);
        UAudioMixerBlueprintLibrary::StartRecordingOutput(this,15.f);
        UE_LOG(LogTemp,Display,TEXT("InterfaceSfx smoke: master recording started; 8 cues loaded"));
    }
}

void UBackstageSfx::PlayCue(FName Id)
{
    const auto* Found=Sounds.Find(Id);
    if(!Found || !*Found || Voices.Num()!=4) return;
    for(const auto& Cue:Cues)
    {
        if(Id!=FName(Cue.Id)) continue;
        const double Now=FPlatformTime::Seconds();
        if(const auto* Last=LastPlayed.Find(Id); Last && Now-*Last<Cue.Cooldown) return;
        // Movement requests during zoom never replace the more useful focus feedback.
        if(Id==TEXT("pan") && Voices[1]->IsPlaying() && Voices[1]->Sound!=*Found) return;
        LastPlayed.Add(Id,Now);++PlayedCount;
        if(Cue.Channel<0)
        {
            // A short select click finishes across OpenLevel, then the finite SoundWave auto-destroys.
            UGameplayStatics::SpawnSound2D(this,*Found,Cue.Volume,1.f,0.f,nullptr,true,true);
        }
        else
        {
            auto* Voice=Voices[Cue.Channel].Get();Voice->Stop();Voice->SetSound(*Found);
            Voice->SetVolumeMultiplier(Cue.Volume);Voice->Play();
        }
        UE_LOG(LogTemp,Verbose,TEXT("InterfaceSfx: cue=%s"),*Id.ToString());
        if(bSmoke) UE_LOG(LogTemp,Display,TEXT("InterfaceSfx smoke: cue=%s time=%.3f"),*Id.ToString(),Now-SmokeStart);
        return;
    }
}

void UBackstageSfx::FadeChannel(int32 Channel)
{
    if(Voices.IsValidIndex(Channel) && Voices[Channel]->IsPlaying()) Voices[Channel]->FadeOut(.035f,0.f);
}

void UBackstageSfx::Motion(int32 Transport,bool bPan,int32 Zoom)
{
    TransportMotion(Transport);
    CameraMotion(bPan,Zoom);
}

void UBackstageSfx::TransportMotion(int32 Transport)
{
    Transport=FMath::Clamp(Transport,-1,1);
    if(Transport!=LastTransport)
    {
        if(Transport) PlayCue(Transport>0?TEXT("forward"):TEXT("rewind"));
        else FadeChannel(0);
        LastTransport=Transport;
    }
}

void UBackstageSfx::CameraMotion(bool bPan,int32 Zoom)
{
    if(Zoom) PlayCue(Zoom>0?TEXT("zoom_in"):TEXT("zoom_out"));
    else if(bPan) PlayCue(TEXT("pan"));
    // Let discrete wheel/short camera taps finish; no retrigger when interpolation coasts to rest.
}

void UBackstageSfx::EndPlay(const EEndPlayReason::Type Reason)
{
    for(const auto& Voice:Voices) {Voice->Stop();Voice->SetSound(nullptr);Voice->DestroyComponent();}
    Voices.Empty();Sounds.Empty();LastPlayed.Empty();
    Super::EndPlay(Reason);
}

void UBackstageSfx::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Type,Function);
    if(!bSmoke) return;
    const double Elapsed=FPlatformTime::Seconds()-SmokeStart;
    if(SmokeStep<8 && Elapsed>=1.+SmokeStep)
    {
        PlayCue(FName(Cues[SmokeStep].Id));++SmokeStep;
    }
    if(SmokeStep==8 && Elapsed>=10.5)
    {
        for(const auto& Voice:Voices) check(!Voice->IsPlaying()); // finite one-shots really completed
        const int32 Count=PlayedCount;for(int32 I=0;I<100;++I) PlayCue(TEXT("pan"));
        check(PlayedCount==Count+1);++SmokeStep;
    }
    if(SmokeStep==9 && Elapsed>=11.5)
    {
        for(const auto& Voice:Voices) check(!Voice->IsPlaying());
        UAudioMixerBlueprintLibrary::StopRecordingOutput(this,EAudioRecordingExportType::WavFile,TEXT("InterfaceSfx-Smoke"),FPaths::ProjectSavedDir()/TEXT("AudioValidation"));
        UE_LOG(LogTemp,Display,TEXT("InterfaceSfx smoke PASS: 8 finite cues, 4 bounded voices, burst cooldown, recording exported"));++SmokeStep;
    }
    if(SmokeStep==10 && Elapsed>=12.5) CastChecked<APlayerController>(GetOwner())->ConsoleCommand(TEXT("quit"));
}
