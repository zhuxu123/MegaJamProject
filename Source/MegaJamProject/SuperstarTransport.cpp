#include "SuperstarStory.h"
#include "SuperstarStage.h"
#include "SuperstarMusic.h"
#include "BackstageSfx.h"
#include "Dom/JsonObject.h"
#include "InputKeyEventArgs.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

float ASuperstarStoryHUD::SequenceTime() const
{
    return BeatStarts.IsValidIndex(Beat)?BeatStarts[Beat]+Clock-TimelineOffset:0.f;
}

void ASuperstarStoryHUD::SeekSequence(float Time)
{
    Time=FMath::Clamp(Time,0.f,SequenceDuration)+TimelineOffset;
    int32 Target=LastBeat-1;
    for(int32 I=FirstBeat;I<LastBeat-1;++I)
        if(Time<BeatStarts[I+1]) {Target=I;break;}
    if(Target!=Beat) SelectBeat(Target);
    Clock=FMath::Clamp(Time-BeatStarts[Target],0.f,static_cast<float>(Beats[Target]->GetNumberField(TEXT("duration"))));
    PublishedHold=0.f;
    UpdateActors();
}

void ASuperstarStoryHUD::AdvanceSequence(float Delta,float ScrubAxis)
{
    const bool bShuttle=!FMath::IsNearlyZero(ScrubAxis);
    const bool bCanShuttle=ScrubAxis<0?SequenceTime()>0:SequenceTime()<SequenceDuration;
    if(Sfx) Sfx->TransportMotion(bShuttle && bCanShuttle ? (ScrubAxis>0?1:-1) : 0);
    PlaybackRate=bShuttle?FMath::Clamp(ScrubAxis,-1.f,1.f)*3.f:(bPlaying?1.f:0.f);
    if(bShuttle && bCanShuttle)
    {
        PresentCamera(ScrubAxis<0?TEXT("倒放 ×3"):TEXT("快进 ×3"));
        ShowHint(TEXT("transport"),TEXT("按住方向键三倍快进倒放；空格切换播放。"));
    }
    if(bShuttle)
    {
        // Explicit navigation can inspect any beat; it never sends an evidence card.
        SeekSequence(SequenceTime()+FMath::Max(0.f,Delta)*PlaybackRate);
        if(SequenceTime()<=0.f || SequenceTime()>=SequenceDuration) {bPlaying=false;PlaybackRate=0.f;}
        return;
    }
    if(!bPlaying) return;
    float Remaining=FMath::Max(0.f,Delta);
    if(PublishedHold>0.f)
    {
        const float Hold=FMath::Min(PublishedHold,Remaining);PublishedHold-=Hold;Remaining-=Hold;
    }
    while(Remaining>0.f)
    {
        const float Duration=Beats[Beat]->GetNumberField(TEXT("duration"));
        const float Available=FMath::Max(0.f,Duration-Clock);
        if(Remaining<Available) {Clock+=Remaining;break;}
        Clock=Duration;Remaining-=Available;
        if(!Beats[Beat]->GetStringField(TEXT("evidence")).IsEmpty() && !bPublished)
        {
            PlaybackRate=0.f;break; // automatic playback still waits for explicit evidence submission
        }
        if(Beat+1>=LastBeat)
        {
            bPlaying=false;PlaybackRate=0.f;
            if(bFullStoryDebug) {if(Sfx) Sfx->PlayCue(TEXT("result"));}
            else {OpenAlbum();PhotoNotice=TEXT("本关现场结束：选择3张照片发布；Tab可返回本关回看补拍。");}
            break;
        }
        SelectBeat(Beat+1); // retain overshoot, including when a long frame crosses multiple beats
    }
}

bool ASuperstarStoryHUD::HandleTimelinePointer(FVector2D Position,bool bPressed,bool bDown,bool bReleased)
{
    if(bPressed && Position.X>=35 && Position.X<=1245 && Position.Y>=610 && Position.Y<=645)
    {
        bTimelineDragging=true;bCameraDragging=false;bDragging=false;bPlaying=false;PlaybackRate=0.f;
    }
    if(!bTimelineDragging) return false;
    if(bDown || bPressed || bReleased)
    {
        const float Before=SequenceTime();
        SeekSequence(FMath::Clamp((Position.X-35)/1210.,0.,1.)*SequenceDuration);
        if(Sfx) Sfx->TransportMotion(FMath::IsNearlyEqual(Before,SequenceTime())?0:(SequenceTime()>Before?1:-1));
    }
    if(bReleased || !bDown) { bTimelineDragging=false; if(Sfx) Sfx->TransportMotion(0); }
    // Consume the release frame as well; the scene must not take ownership of this drag.
    return true;
}

void ASuperstarStoryHUD::DrawTransport()
{
    const float Fraction=SequenceTime()/FMath::Max(.001f,SequenceDuration);
    Rect(35,618,1210,10,FLinearColor(.14f,.19f,.24f));
    Rect(35,618,1210*Fraction,10,FLinearColor(.30f,.85f,.73f));
    for(int32 I=FirstBeat+1;I<LastBeat;++I)
    {
        const bool Chapter=Beats[I]->GetIntegerField(TEXT("chapter"))!=Beats[I-1]->GetIntegerField(TEXT("chapter"));
        Rect(35+1210*(BeatStarts[I]-TimelineOffset)/SequenceDuration,Chapter?614:618,Chapter?2:1,Chapter?18:10,
            Chapter?FLinearColor(.85f,.9f,1):FLinearColor(.32f,.42f,.48f));
    }
    Rect(32+1210*Fraction,613,6,20,FLinearColor::White);
    FitLabel(TEXT("拖动本关时间线 · ←/Z 倒放  →/C 快进 · 空格/R 暂停"),35,633,770,20,13);
    const FString State=bTimelineDragging?TEXT("定位"):(PlaybackRate<0?TEXT("倒放"):(PlaybackRate>1?TEXT("快进"):(PlaybackRate>0?TEXT("播放"):TEXT("暂停/等待"))));
    FitLabel(FString::Printf(TEXT("%s %.0fx · %.1f / %.1f 秒 · 本段 %.1f 秒"),*State,FMath::Abs(PlaybackRate),SequenceTime(),SequenceDuration,Clock),825,633,420,20,12);
}

void ASuperstarStoryHUD::RunTransportSmoke()
{
    if(++TransportSmokeTicks<60) return;
    if(TransportSmokeTicks==60)
    {
        // Same helpers called by the live keyboard and pointer path, without bypassing seek logic.
        bPlaying=true;SeekSequence(BeatStarts[4]-.1f);AdvanceSequence(.2f,1);
        check(Beat==4 && FMath::IsNearlyEqual(Clock,.5f,.001f));
        AdvanceSequence(.2f,-1);check(Beat==3 && FMath::IsNearlyEqual(Clock,static_cast<float>(Beats[3]->GetNumberField(TEXT("duration")))-.1f,.001f));
        bPlaying=false;const float Held=SequenceTime();AdvanceSequence(.25f,1);AdvanceSequence(.25f,-1);
        check(FMath::IsNearlyEqual(SequenceTime(),Held,.001f));AdvanceSequence(1,0);check(FMath::IsNearlyEqual(SequenceTime(),Held,.001f));
        bPlaying=true;AdvanceSequence(.25f,1);const float Release=SequenceTime();AdvanceSequence(.25f,0);
        check(FMath::IsNearlyEqual(SequenceTime(),Release+.25f,.001f));
        // Continuous mouse drag remains captured even over the scene and outside the bar.
        bCameraDragging=true;
        check(HandleTimelinePointer({35+1210*.2,620},true,true,false));
        check(bTimelineDragging && !bCameraDragging && !bPlaying);
        check(HandleTimelinePointer({35+1210*.7,200},false,true,false));
        check(FMath::IsNearlyEqual(SequenceTime(),SequenceDuration*.7f,.001f));
        check(HandleTimelinePointer({-200,200},false,true,false));check(SequenceTime()==0);
        check(HandleTimelinePointer({1500,700},false,false,true));
        check(SequenceTime()==SequenceDuration && !bTimelineDragging && !bPlaying);
        AdvanceSequence(2,0);check(SequenceTime()==SequenceDuration);
        check(!HandleTimelinePointer({500,200},true,true,false)); // scene drag is not stolen
        // Evidence gates survive backwards seeking and are never implicitly sent by fast-forward.
        SelectBeat(33);const float GateDuration=Beats[33]->GetNumberField(TEXT("duration"));Clock=GateDuration-.1f;bPlaying=true;AdvanceSequence(2,0);
        check(Beat==33 && !bPublished && FMath::IsNearlyEqual(Clock,GateDuration));
        AdvanceSequence(.2f,1);check(Beat==34 && !PublishedBeats.Contains(33));
        SelectBeat(33);bEvidencePlaced=true;Publish();SeekSequence(0);SeekSequence(BeatStarts[33]+1);
        check(bPublished && PublishedBeats.Contains(33));
        // Original multi-clip sequence: forward and reverse across the empty-hand rise/crouch cut.
        SelectBeat(10);Clock=2.5f;
        const auto Actor=Beats[10]->GetArrayField(TEXT("actors"))[1]->AsObject();
        FString Id;int32 Frame;ResolveActorSample(Actor,Id,Frame);check(Id==TEXT("A16_xiaobei_rise_empty"));
        AdvanceSequence(.1f,1);ResolveActorSample(Actor,Id,Frame);check(Id==TEXT("A17_xiaobei_crouch_empty"));
        AdvanceSequence(.1f,-1);ResolveActorSample(Actor,Id,Frame);check(Id==TEXT("A16_xiaobei_rise_empty"));
        // Seek across a handover and back; each pose must reattach from its evaluated time.
        SelectBeat(3);const float Transfer=Beats[3]->GetArrayField(TEXT("props"))[0]->AsObject()->GetNumberField(TEXT("transfer_at"));Clock=Transfer-.1f;UpdateActors();
        const auto Before=Beats[3]->GetArrayField(TEXT("actors"))[1]->AsObject();
        const FVector PreviousHand=Stage->CharacterPixelWorld(1,ActorHand(Before));
        AdvanceSequence(.1f,1);check(Clock>Transfer);AdvanceSequence(.1f,-1);UpdateActors();
        check(FVector::Dist(PreviousHand,Stage->CharacterPixelWorld(1,ActorHand(Before)))<.001f);
        SeekSequence(BeatStarts[4]+1.2f);bPlaying=false;PlaybackRate=0;UpdateActors();
        UE_LOG(LogTemp,Display,TEXT("SuperstarTransport checks PASS: forward/reverse across beats, release state, drag capture, clamps, evidence, sequence cuts and handover seek"));
    }
    if(TransportSmokeTicks==65)
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/SuperstarTransport/Timeline.png"),true,false);
    // Exercise actual PlayerController key states through the normal Tick input branches.
    auto Key=[this](FKey Input,EInputEvent Event)
    {
        GetOwningPlayerController()->InputKey(FInputKeyEventArgs::CreateSimulated(Input,Event,Event==IE_Released?0.f:1.f));
    };
    if(TransportSmokeTicks==73) {SelectBeat(4);Clock=2.f;bPlaying=false;Key(EKeys::Left,IE_Pressed);}
    if(TransportSmokeTicks==83) {check(Beat==4 && Clock<1.98f && !bPlaying);Key(EKeys::Left,IE_Released);}
    if(TransportSmokeTicks==86) TransportInputTime=Clock;
    if(TransportSmokeTicks==90) {check(FMath::IsNearlyEqual(Clock,TransportInputTime,.001f));Key(EKeys::Right,IE_Pressed);}
    if(TransportSmokeTicks==100) {check(Clock>TransportInputTime+.02f && Beat==4);Key(EKeys::Right,IE_Released);}
    if(TransportSmokeTicks==103) {TransportInputTime=Clock;Key(EKeys::Z,IE_Pressed);}
    if(TransportSmokeTicks==113) {check(Clock<TransportInputTime-.02f);Key(EKeys::Z,IE_Released);}
    if(TransportSmokeTicks==117) {TransportInputTime=Clock;Key(EKeys::C,IE_Pressed);}
    if(TransportSmokeTicks==127) {check(Clock>TransportInputTime+.02f);Key(EKeys::C,IE_Released);}
    if(TransportSmokeTicks==130) Key(EKeys::SpaceBar,IE_Pressed);
    if(TransportSmokeTicks==131) Key(EKeys::SpaceBar,IE_Released);
    if(TransportSmokeTicks==133) check(bPlaying);
    if(TransportSmokeTicks==135) Key(EKeys::R,IE_Pressed);
    if(TransportSmokeTicks==136) Key(EKeys::R,IE_Released);
    if(TransportSmokeTicks==138) {check(!bPlaying);TransportInputTime=Clock;}
    if(TransportSmokeTicks==140) check(FMath::IsNearlyEqual(Clock,TransportInputTime,.001f));
    if(TransportSmokeTicks==145) Key(EKeys::F2,IE_Pressed);
    if(TransportSmokeTicks==146) Key(EKeys::F2,IE_Released);
    if(TransportSmokeTicks==149) {check(Beats[Beat]->GetIntegerField(TEXT("chapter"))==2);TransportInputTime=TargetPan;Key(EKeys::D,IE_Pressed);}
    if(TransportSmokeTicks==160) {check(TargetPan>TransportInputTime);Key(EKeys::D,IE_Released);TransportInputTime=TargetZoom;}
    if(TransportSmokeTicks==163) Key(EKeys::E,IE_Pressed);
    if(TransportSmokeTicks==175) {check(TargetZoom>TransportInputTime);Key(EKeys::E,IE_Released);TransportInputTime=TargetZoom;}
    if(TransportSmokeTicks==180) Key(EKeys::Q,IE_Pressed);
    if(TransportSmokeTicks==193) {check(TargetZoom<TransportInputTime);Key(EKeys::Q,IE_Released);}
    if(TransportSmokeTicks==205)
    {
        UE_LOG(LogTemp,Display,TEXT("SuperstarTransport smoke PASS: timeline checks and injected Left/Right/Z/C/Space/R/F2/D/E/Q through live input"));
        GetOwningPlayerController()->ConsoleCommand(TEXT("quit"));
    }
}
