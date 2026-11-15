#include "SuperstarStory.h"
#include "SuperstarStage.h"
#include "SuperstarMusic.h"
#include "BackstageSfx.h"
#include "Dom/JsonObject.h"
#include "Engine/Canvas.h"
#include "Engine/GameViewportClient.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

namespace
{
    const FLinearColor Paper(.86f,.80f,.67f), Gold(.83f,.68f,.30f), Dark(.045f,.043f,.038f), Light(.82f,.85f,.82f);
    bool Hit(FVector2D P,float X,float Y,float W,float H) {return P.X>=X && P.X<X+W && P.Y>=Y && P.Y<Y+H;}
}

bool ASuperstarStoryHUD::LoadLevels()
{
    FString Text;TSharedPtr<FJsonObject> Root;
    if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectContentDir()/TEXT("SuperstarStory/levels.json")))
       || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root) || !Root.IsValid()) return false;
    const TArray<TSharedPtr<FJsonValue>>* Entries=nullptr;
    if(!Root->TryGetArrayField(TEXT("levels"),Entries) || Entries->Num()!=4) return false;
    if(Root->GetIntegerField(TEXT("required_photos"))!=3 || Root->GetIntegerField(TEXT("max_photos"))!=6) return false;
    int32 Next=0;
    for(const auto& Value:*Entries)
    {
        auto Level=Value->AsObject();
        if(!Level || Level->GetIntegerField(TEXT("id"))!=Levels.Num()+1 || Level->GetIntegerField(TEXT("first_beat"))-1!=Next) return false;
        const int32 End=Level->GetIntegerField(TEXT("last_beat"));
        if(End<=Next || End>Beats.Num() || Level->GetArrayField(TEXT("headlines")).Num()!=3) return false;
        for(int32 I=Next;I<End;++I)
            if(Beats[I]->GetIntegerField(TEXT("chapter"))!=Levels.Num()+1) return false;
        Levels.Add(Level);Next=End;
    }
    return Next==Beats.Num();
}

void ASuperstarStoryHUD::ShowLevelSelect()
{
    bPlaying=false;PlaybackRate=0;bTimelineDragging=false;bCameraDragging=false;bDragging=false;
    LevelPhase=ESuperstarLevelPhase::Select;
    if(Music) Music->SetStoryPaused(true);
    if(Sfx) Sfx->TransportMotion(0);
}

void ASuperstarStoryHUD::ChooseLevel(int32 Index)
{
    if(!Levels.IsValidIndex(Index)) return;
    if(ScreenshotHandle.IsValid()) UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
    ScreenshotHandle.Reset();
    LevelIndex=Index;bReferenceView=false;
    FirstBeat=Levels[Index]->GetIntegerField(TEXT("first_beat"))-1;
    LastBeat=Levels[Index]->GetIntegerField(TEXT("last_beat"));
    TimelineOffset=BeatStarts[FirstBeat];
    SequenceDuration=(LastBeat<BeatStarts.Num()?BeatStarts[LastBeat]:FullDuration)-TimelineOffset;
    Photos.Empty();PendingPhoto=FSuperstarPhoto();PhotoSlots.Empty();PhotoSession.Empty();PhotoNotice.Empty();
    PublishedBeats.Empty();bPublished=false;bEvidencePlaced=false;PublishedHold=0;
    bPublishPhotos=false;PreviewClock=0;EditSlot=0;Headline=0;
    bPlaying=false;PlaybackRate=0;bTimelineDragging=false;bCameraDragging=false;bDragging=false;
    ViewPan=TargetPan=ViewTilt=TargetTilt=ViewStation=TargetStation=0;
    ViewZoom=TargetZoom=1;CameraStation=0;
    PhotoPhase=ESuperstarPhotoPhase::Live;LevelPhase=ESuperstarLevelPhase::Briefing;
    SelectBeat(FirstBeat);
    if(Stage) Stage->SetView(0,0,0,1);
    if(Music) Music->SetStoryPaused(true);
    if(Sfx) {Sfx->Motion(0,false,0);Sfx->PlayCue(TEXT("chapter"));}
    UE_LOG(LogTemp,Display,TEXT("SuperstarLevel: briefing level=%d beats=%d-%d duration=%.1f"),Index+1,FirstBeat+1,LastBeat,SequenceDuration);
}

void ASuperstarStoryHUD::StartLevel()
{
    if(LevelPhase!=ESuperstarLevelPhase::Briefing || !Levels.IsValidIndex(LevelIndex)) return;
    LevelPhase=ESuperstarLevelPhase::Playing;bPlaying=true;PlaybackRate=1;
    if(Music) Music->SetStoryPaused(false);
    UE_LOG(LogTemp,Display,TEXT("SuperstarLevel: start level=%d"),LevelIndex+1);
}

void ASuperstarStoryHUD::ContinueLevel()
{
    if(LevelIndex+1<Levels.Num()) ChooseLevel(LevelIndex+1);
    else ShowLevelSelect();
}

FString ASuperstarStoryHUD::PublicationHeadline() const
{
    if(Levels.IsValidIndex(LevelIndex)) return Levels[LevelIndex]->GetArrayField(TEXT("headlines"))[FMath::Clamp(Headline,0,2)]->AsString();
    const TCHAR* Titles[]={TEXT("现场直击：三个时刻的记录"),TEXT("镜头里的善意与援手"),TEXT("聚光灯背后的冲突")};
    return Titles[FMath::Clamp(Headline,0,2)];
}

bool ASuperstarStoryHUD::CanPublishLevel()
{
    if(PhotoSlots.Num()!=3) {PhotoNotice=TEXT("请先选满3张照片。");return false;}
    for(int32 Slot:PhotoSlots)
        if(!Photos.IsValidIndex(Slot) || Photos[Slot].Beat<FirstBeat+1 || Photos[Slot].Beat>LastBeat)
        {PhotoNotice=TEXT("所选照片不属于本关，请重新选择。");return false;}
    if(!bFullStoryDebug)
    {
        int32 Needed=0,Sent=0;
        for(int32 I=FirstBeat;I<LastBeat;++I)
            if(!Beats[I]->GetStringField(TEXT("evidence")).IsEmpty()) {++Needed;if(PublishedBeats.Contains(I)) ++Sent;}
        if(Sent<Needed) {PhotoNotice=FString::Printf(TEXT("请先返回现场发送全部证据（已发送 %d/%d），照片会保留。"),Sent,Needed);return false;}
    }
    return true;
}

bool ASuperstarStoryHUD::TickLevelFlow()
{
    if(LevelPhase==ESuperstarLevelPhase::Playing) return false;
    auto* PC=GetOwningPlayerController();const auto P=Pointer();
    const bool Click=PC->WasInputKeyJustPressed(EKeys::LeftMouseButton);
    const bool Enter=PC->WasInputKeyJustPressed(EKeys::Enter);
    const bool Back=PC->WasInputKeyJustPressed(EKeys::Escape)||PC->WasInputKeyJustPressed(EKeys::M);
    if(LevelPhase==ESuperstarLevelPhase::Select)
    {
        const FKey Keys[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four};
        const FKey Chapters[]={EKeys::F1,EKeys::F2,EKeys::F3,EKeys::F4};
        for(int32 I=0;I<4;++I)
            if(PC->WasInputKeyJustPressed(Keys[I]) || PC->WasInputKeyJustPressed(Chapters[I])
                || (Click&&Hit(P,70+(I%2)*610,185+(I/2)*200,530,175))) {ChooseLevel(I);return true;}
        if(Back || (Click&&Hit(P,70,625,260,48))) UGameplayStatics::OpenLevel(this,TEXT("/Game/levels/MainMenu"),true);
    }
    else if(LevelPhase==ESuperstarLevelPhase::Briefing)
    {
        if(Enter || (Click&&Hit(P,255,550,365,52))) StartLevel();
        else if(Back || (Click&&Hit(P,660,550,365,52))) ShowLevelSelect();
    }
    else if(LevelPhase==ESuperstarLevelPhase::Newspaper)
    {
        if(Enter || (Click&&Hit(P,870,625,300,48))) ContinueLevel();
        else if(PC->WasInputKeyJustPressed(EKeys::R) || (Click&&Hit(P,440,625,300,48))) ChooseLevel(LevelIndex);
        else if(Back || (Click&&Hit(P,80,625,300,48))) ShowLevelSelect();
        else if(PC->WasInputKeyJustPressed(EKeys::BackSpace)) {LevelPhase=ESuperstarLevelPhase::Playing;PhotoPhase=ESuperstarPhotoPhase::Edit;}
    }
    return true; // Enter starting a level must not also fire the camera in the same frame.
}

void ASuperstarStoryHUD::DrawLevelFlow()
{
    if(LevelPhase==ESuperstarLevelPhase::Newspaper) {DrawNewspaper();return;}
    DrawRect(FLinearColor(.025f,.030f,.032f),0,0,Canvas->SizeX,Canvas->SizeY);
    if(LevelPhase==ESuperstarLevelPhase::Select)
    {
        FitLabel(TEXT("超级巨星  /  四关独立选关"),70,45,1140,55,32,Paper);
        FitLabel(TEXT("选择任意一关开始 · 每关独立拍摄与发布"),70,110,1140,35,19,Gold);
        const auto P=Pointer();
        for(int32 I=0;I<Levels.Num();++I)
        {
            float X=70+(I%2)*610,Y=185+(I/2)*200;
            Rect(X,Y,530,175,Hit(P,X,Y,530,175)?FLinearColor(.12f,.20f,.20f):FLinearColor(.09f,.10f,.10f));
            Rect(X,Y,4,175,Gold);
            const auto& L=Levels[I];
            const int32 First=L->GetIntegerField(TEXT("first_beat"))-1,Last=L->GetIntegerField(TEXT("last_beat"));
            const float Duration=(Last<BeatStarts.Num()?BeatStarts[Last]:FullDuration)-BeatStarts[First];
            FitLabel(FString::Printf(TEXT("[ %d ]  %s"),I+1,*L->GetStringField(TEXT("title"))),X+22,Y+17,486,44,26,Paper);
            FitLabel(L->GetStringField(TEXT("summary")),X+22,Y+72,486,32,17,Light);
            FitLabel(FString::Printf(TEXT("3 张成片  ·  %.1f 秒%s"),Duration,L->GetArrayField(TEXT("interludes")).IsEmpty()?TEXT(""):TEXT("  ·  含间幕")),X+22,Y+120,486,28,16,Gold);
        }
        Button(TEXT("返回主菜单  M / ESC"),70,625,260,48,17,Dark,Paper);
        FitLabel(TEXT("点击任务卡，或按 1–4 选择对应关卡"),380,625,800,48,18,Light);
        return;
    }
    const auto& L=Levels[LevelIndex];
    Rect(140,70,1000,565,FLinearColor(.075f,.070f,.058f));
    Rect(140,70,1000,2,Paper);Rect(140,633,1000,2,Paper);
    FitLabel(FString::Printf(TEXT("LEVEL %02d  //  摄制任务单（目标：3 张成片 · 磁带 %.1f 秒）"),LevelIndex+1,SequenceDuration),185,100,910,35,19,Gold);
    FitLabel(L->GetStringField(TEXT("title")),185,151,910,60,36,Paper);
    FitLabel(TEXT("你是林晚的危机公关顾问。"),185,211,910,30,20,Light);
    Rect(185,254,910,1,Gold);
    FitLabel(TEXT("顾客陈辞："),185,266,910,29,21,Gold);
    FitLabel(L->GetStringField(TEXT("client_statement")),185,296,910,85,21,Paper);
    FitLabel(TEXT("公关目标："),185,392,910,29,21,Gold);
    FitLabel(L->GetStringField(TEXT("objective")),185,422,910,56,20,Paper);
    FitLabel(TEXT("WASD / 拖动构图 · Q/E/滚轮变焦 · 空格暂停 · Enter拍照 · Tab选图"),185,485,910,30,16,Light);
    Button(TEXT("▶ 进入拍摄现场  Enter"),255,550,365,52,20,FLinearColor(.19f,.18f,.15f),Paper);
    Button(TEXT("返回选关  M / ESC"),660,550,365,52,20,FLinearColor(.19f,.18f,.15f),Paper);
}

void ASuperstarStoryHUD::DrawNewspaper()
{
    DrawRect(FLinearColor(.035f,.032f,.028f),0,0,Canvas->SizeX,Canvas->SizeY);
    Rect(50,28,1180,665,Paper);
    FitLabel(TEXT("骏 风 娱 乐"),80,48,1120,58,38,Dark,true);
    Rect(80,111,1120,3,Dark);
    FitLabel(FString::Printf(TEXT("第 %02d 期  /  %s  /  本关现场报道"),LevelIndex+1,*Levels[LevelIndex]->GetStringField(TEXT("title"))),80,119,1120,30,16,Dark,true);
    FitLabel(PublicationHeadline(),80,163,1120,51,30,Dark,true);
    for(int32 I=0;I<3;++I)
    {
        const float X=80+I*380;
        const auto& Shot=Photos[PhotoSlots[I]];
        Rect(X-2,236,364,126,Dark);DrawPhoto(Shot,X,238,360,true);
        FitLabel(FString::Printf(TEXT("%02d  %s"),I+1,*Shot.Title),X,374,360,33,17,Dark);
        FitLabel(FString::Printf(TEXT("本关第 %d 段 · %.1f 秒 · 机位 %d"),Shot.Beat-FirstBeat,Shot.Time,Shot.Station+1),X,409,360,24,13,Dark);
    }
    Rect(80,452,1120,1,Dark);
    const auto& Lead=Levels[LevelIndex]->GetArrayField(TEXT("paper_lead"));
    for(int32 I=0;I<Lead.Num();++I) FitLabel(Lead[I]->AsString(),80,470+I*34,1120,30,18,Dark);
    FitLabel(TEXT("照片、裁切与排序来自本关拍摄  ·  Backspace 返回编辑"),80,555,1120,30,15,Dark);
    Button(TEXT("返回选关  M"),80,625,300,48,18,Dark,Paper);
    Button(TEXT("重新拍摄本关  R"),440,625,300,48,18,Dark,Paper);
    Button(LevelIndex==3?TEXT("完成 · 返回选关  Enter"):TEXT("继续下一关  Enter"),870,625,300,48,18,Dark,Paper);
}

void ASuperstarStoryHUD::RunLevelsSmoke()
{
    if(++LevelsSmokeTicks<20) return;LevelsSmokeTicks=0;
    if(PhotoPhase==ESuperstarPhotoPhase::Capturing) {check(FPlatformTime::Seconds()-CaptureStart<10);return;}
    auto Shot=[this](FString Name) {FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/SuperstarLevels")/Name,true,false);};
    auto Key=[this](EInputEvent Event) {GetOwningPlayerController()->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Enter,Event,Event==IE_Released?0.f:1.f));};
    const FKey Chapters[]={EKeys::F1,EKeys::F2,EKeys::F3,EKeys::F4};
    if(LevelsSmokeStep==0) {ShowLevelSelect();Shot(TEXT("Select.png"));bReferenceView=true;}
    else if(LevelsSmokeStep==1) GetOwningPlayerController()->InputKey(FInputKeyEventArgs::CreateSimulated(Chapters[LevelsSmokeIndex],IE_Pressed,1.f));
    else if(LevelsSmokeStep==2)
    {
        GetOwningPlayerController()->InputKey(FInputKeyEventArgs::CreateSimulated(Chapters[LevelsSmokeIndex],IE_Released,0.f));
        check(LevelPhase==ESuperstarLevelPhase::Briefing && LevelIndex==LevelsSmokeIndex);
        check(Photos.IsEmpty() && PublishedBeats.IsEmpty() && !bReferenceView);
        for(int32 I=FirstBeat;I<LastBeat;++I) check(Beats[I]->GetIntegerField(TEXT("chapter"))==LevelIndex+1);
        Shot(FString::Printf(TEXT("L%d-Briefing.png"),LevelIndex+1));
    }
    else if(LevelsSmokeStep==3) Key(IE_Pressed);
    else if(LevelsSmokeStep==4)
    {
        Key(IE_Released);check(LevelPhase==ESuperstarLevelPhase::Playing && PhotoPhase==ESuperstarPhotoPhase::Live);
        bPlaying=false;SeekSequence(-100);check(Beat==FirstBeat && Clock==0);
        SeekSequence(10000);check(Beat==LastBeat-1);const int32 Before=Beat;
        SelectBeat(LastBeat);check(Beat==Before);SelectBeat(FirstBeat-1);check(Beat==Before);
        SeekSequence(SequenceDuration-.05f);bPlaying=true;AdvanceSequence(.2f,0);
        check(Beat==LastBeat-1 && !bPlaying && PhotoPhase==ESuperstarPhotoPhase::Edit);
        PhotoPhase=ESuperstarPhotoPhase::Live;
        check(HandleTimelinePointer({-50,620},false,false,false)==false);
        HandleTimelinePointer({640,620},true,true,false);
        check(FMath::IsNearlyEqual(SequenceTime(),SequenceDuration*.5f,.001f));
        HandleTimelinePointer({1500,620},false,true,false);check(Beat==LastBeat-1);
        HandleTimelinePointer({-50,620},false,false,true);check(Beat==FirstBeat && Clock==0 && !bTimelineDragging);
        SeekSequence(SequenceDuration*.3f);const float T=SequenceTime();AdvanceSequence(.1f,1);AdvanceSequence(.1f,-1);check(FMath::IsNearlyEqual(T,SequenceTime(),.001f));
        Shot(FString::Printf(TEXT("L%d-Live.png"),LevelIndex+1));
    }
    else if(LevelsSmokeStep>=5 && LevelsSmokeStep<=10)
    {
        if(LevelsSmokeStep%2==1)
        {
            const int32 I=(LevelsSmokeStep-5)/2;
            SeekSequence(SequenceDuration*(I==0?.08f:I==1?.50f:.92f));
            CameraStation=I;ViewStation=TargetStation=I-1;Stage->SetView(ViewStation,0,0,1);TakePhoto();
        }
        else {check(PhotoPhase==ESuperstarPhotoPhase::Confirm);AcceptPhoto();}
    }
    else if(LevelsSmokeStep==11)
    {
        check(Photos.Num()==3);OpenAlbum();TogglePhoto(0);TogglePhoto(1);TogglePhoto(2);
        if(LevelIndex==3)
        {
            StartPhotoPreview(true);check(PhotoPhase==ESuperstarPhotoPhase::Edit);
            for(int32 I=FirstBeat;I<LastBeat;++I)
                if(!Beats[I]->GetStringField(TEXT("evidence")).IsEmpty()) {SelectBeat(I);bEvidencePlaced=true;Publish();}
            check(PublishedBeats.Num()==3);bPlaying=false;
        }
        StartPhotoPreview(true);check(PhotoPhase==ESuperstarPhotoPhase::Preview);
    }
    else if(LevelsSmokeStep==12)
    {
        PreviewClock=5.99f;TickPhotography(.1f);check(LevelPhase==ESuperstarLevelPhase::Newspaper);
        Shot(FString::Printf(TEXT("L%d-Newspaper.png"),LevelIndex+1));
    }
    else if(LevelsSmokeStep==13)
    {
        ContinueLevel();
        if(++LevelsSmokeIndex<4) {check(LevelPhase==ESuperstarLevelPhase::Briefing && LevelIndex==LevelsSmokeIndex);LevelsSmokeStep=0;}
        else
        {
            check(LevelPhase==ESuperstarLevelPhase::Select);
            UE_LOG(LogTemp,Display,TEXT("SuperstarLevels smoke PASS: F1-F4 selection, 4 independent briefings, Enter input, scoped seek/reverse/drag, end-stop, 12 real photos, 4 newspapers, evidence gate, continuation and reset"));
            GetOwningPlayerController()->ConsoleCommand(TEXT("quit"));return;
        }
    }
    ++LevelsSmokeStep;
}
