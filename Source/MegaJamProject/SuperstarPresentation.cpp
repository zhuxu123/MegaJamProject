#include "SuperstarStory.h"
#include "GameLocalization.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    const FLinearColor Mint(.30f,.85f,.73f), Ink(.025f,.033f,.055f), Paper(.86f,.80f,.67f);
    bool Hit(FVector2D P,float X,float Y,float W,float H) {return P.X>=X && P.X<X+W && P.Y>=Y && P.Y<Y+H;}
}

void ASuperstarStoryHUD::PresentCamera(const FString& Text)
{
    CameraNotice=Text;CameraNoticeUntil=UITime+.65f;
}

void ASuperstarStoryHUD::ShowHint(const FString& Id,const FString& Text)
{
    if(SeenHints.Contains(Id)) return;
    SeenHints.Add(Id);HintText=Text;HintUntil=UITime+4.f;
}

void ASuperstarStoryHUD::DrawHint()
{
    if(UITime>=HintUntil) return;
    // Reserved header strip; never cover the scene's evidence controls or bottom buttons.
    Rect(320,3,640,24,FLinearColor(.035f,.07f,.08f,.95f));
    FitLabel(HintText,330,4,620,22,12,Mint,true);
}

void ASuperstarStoryHUD::DrawCameraOverlay()
{
    const FLinearColor White(.8f,.9f,.9f,.6f);
    for(float X:{45.f,1235.f}) for(float Y:{115.f,505.f})
    {
        Rect(X==45?X:X-22,Y,22,1.5f,White);
        Rect(X,Y==115?Y:Y-22,1.5f,22,White);
    }
    Rect(55,119,255,27,FLinearColor(0,0,0,.55f));
    Rect(63,128,7,7,FLinearColor(.95f,.12f,.2f,bPlaying?(.55f+.45f*FMath::Sin(UITime*5)):1));
    const int32 TimecodeFrames=FMath::FloorToInt(SequenceTime()*30.f);
    FitLabel(FString::Printf(TEXT("REC  %02d:%02d:%02d  |  CAM %d"),TimecodeFrames/1800,(TimecodeFrames/30)%60,TimecodeFrames%30,CameraStation+1),78,120,222,24,13);
    if(UITime<CameraNoticeUntil)
    {
        const float Fade=FMath::Min(1.f,(CameraNoticeUntil-UITime)*5.f);
        Rect(465,120,350,30,FLinearColor(.03f,.10f,.12f,Fade));
        FitLabel(CameraNotice,475,122,330,26,16,FLinearColor(.9f,.8f,.35f,Fade),true);
    }
    int32 Needed=0,Sent=0;
    for(int32 I=FirstBeat;I<LastBeat;++I) if(!Beats[I]->GetStringField(TEXT("evidence")).IsEmpty()) {++Needed;if(PublishedBeats.Contains(I)) ++Sent;}
    if(Needed)
    {
        const bool Pulse=UITime<EvidencePulseUntil;
        Rect(55,153,340,34,Pulse?FLinearColor(.08f,.40f,.28f):FLinearColor(.10f,.04f,.06f,.92f));
        const FString Status=GameLocalization::Translate(Pulse?TEXT("发送成功"):TEXT("直播证据"));
        FitLabel(FString::Printf(TEXT("LIVE  |  %s  %d / %d"),*Status,Sent,Needed),65,156,320,28,17,Mint,true);
    }
    DrawHint();
}

void ASuperstarStoryHUD::SwapPhotos(int32 A,int32 B)
{
    if(!PhotoSlots.IsValidIndex(A)||!PhotoSlots.IsValidIndex(B)) return;
    SwapFrom=PhotoSlots;SwapStart=UITime;PhotoSlots.Swap(A,B);
    if(EditSlot==A) EditSlot=B;else if(EditSlot==B) EditSlot=A;
    PhotoNotice=GameLocalization::Translate(TEXT("已交换顺序"))+FString::Printf(TEXT("  %d ↔ %d"),A+1,B+1);
    ShowHint(TEXT("reorder"),TEXT("照片顺序会改变叙事；Z/X交换相邻照片。"));
}

void ASuperstarStoryHUD::DrawCropGuide(const FSuperstarPhoto& P,float X,float Y,float W)
{
    DrawPhoto(P,X,Y,W,false);
    const float H=W*P.Height/P.Width, Span=1.f/P.CropZoom;
    const float L=X+(P.CropX-Span*.5f)*W,T=Y+(P.CropY-Span*.5f)*H;
    const float R=L+Span*W,B=T+Span*H;
    const FLinearColor Shade(0,0,0,.65f), Edge(.98f,.28f,.38f);
    Rect(X,Y,W,T-Y,Shade);Rect(X,B,W,Y+H-B,Shade);
    Rect(X,T,L-X,B-T,Shade);Rect(R,T,X+W-R,B-T,Shade);
    Rect(L,T,R-L,2,Edge);Rect(L,B-2,R-L,2,Edge);Rect(L,T,2,B-T,Edge);Rect(R-2,T,2,B-T,Edge);
    for(float PX:{L,R}) for(float PY:{T,B}) Rect(PX-3,PY-3,6,6,FLinearColor::White);
}

bool ASuperstarStoryHUD::ReadEdition(const FString& Path)
{
    FString Text;TSharedPtr<FJsonObject> Root;
    if(!FFileHelper::LoadFileToString(Text,*Path)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root)||!Root) return false;
    bool bTestRun=false;
    Root->TryGetBoolField(TEXT("test_run"),bTestRun);
    if(bTestRun && !bLevelsSmoke) return false;
    double Number=0;FString EditionHeadline;
    const TArray<TSharedPtr<FJsonValue>>* Shots=nullptr;
    if(!Root->TryGetNumberField(TEXT("level"),Number)||Number<1||Number>4||!Root->TryGetStringField(TEXT("headline"),EditionHeadline)
        ||!Root->TryGetArrayField(TEXT("shots"),Shots)||Shots->Num()!=3) return false;
    const int32 Index=static_cast<int32>(Number)-1;
    FSuperstarEdition E;E.Level=Index;E.Headline=EditionHeadline;E.Path=Path;
    for(const auto& V:*Shots)
    {
        const auto J=V->AsObject();FString File;double BeatNumber=0,Time=0,Station=0;
        if(!J||!J->TryGetStringField(TEXT("image"),File)||FPaths::GetCleanFilename(File)!=File
            ||!J->TryGetNumberField(TEXT("beat"),BeatNumber)||!J->TryGetNumberField(TEXT("time"),Time)||!J->TryGetNumberField(TEXT("station"),Station)) return false;
        if(BeatNumber<Levels[Index]->GetIntegerField(TEXT("first_beat"))||BeatNumber>Levels[Index]->GetIntegerField(TEXT("last_beat"))) return false;
        FSuperstarPhoto P;P.Texture=FImageUtils::ImportFileAsTexture2D(FPaths::GetPath(Path)/File);
        if(!P.Texture) return false;
        P.Width=P.Texture->GetSizeX();P.Height=P.Texture->GetSizeY();P.Beat=BeatNumber;P.Time=Time;P.Station=Station;
        P.Title=Beats[P.Beat-1]->GetStringField(TEXT("title"));E.Shots.Add(MoveTemp(P));
    }
    Editions.SetNum(4);Editions[Index]=MoveTemp(E);return true;
}

void ASuperstarStoryHUD::LoadEditions()
{
    Editions.SetNum(4);
    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(Files,*(FPaths::ProjectSavedDir()/TEXT("SuperstarPhotos")),TEXT("publication.json"),true,false);
    Files.Sort([](const FString& A,const FString& B){return IFileManager::Get().GetTimeStamp(*A)>IFileManager::Get().GetTimeStamp(*B);});
    for(const auto& File:Files)
    {
        // Inspect level before loading textures: keep only the latest valid edition per level.
        FString Text;TSharedPtr<FJsonObject> J;double L=0;
        if(FFileHelper::LoadFileToString(Text,*File)&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),J)&&J
            &&J->TryGetNumberField(TEXT("level"),L)&&L>=1&&L<=4&&Editions[static_cast<int32>(L)-1].Shots.IsEmpty()) ReadEdition(File);
    }
}

void ASuperstarStoryHUD::OpenArchive()
{
    ArchiveReturn=LevelPhase;ArchiveLevel=FMath::Clamp(LevelIndex,0,3);LevelPhase=ESuperstarLevelPhase::Archive;
}

bool ASuperstarStoryHUD::TickCollection()
{
    if(LevelPhase!=ESuperstarLevelPhase::Archive && LevelPhase!=ESuperstarLevelPhase::Finale) return false;
    auto* PC=GetOwningPlayerController();const auto P=Pointer();const bool Click=PC->WasInputKeyJustPressed(EKeys::LeftMouseButton);
    if(PC->WasInputKeyJustPressed(EKeys::Escape)||(Click&&Hit(P,940,647,280,46))) {ShowLevelSelect();return true;}
    if(LevelPhase==ESuperstarLevelPhase::Archive)
    {
        if(PC->WasInputKeyJustPressed(EKeys::BackSpace)||(Click&&Hit(P,40,647,280,46))) {LevelPhase=ArchiveReturn;return true;}
        const FKey Keys[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four};
        for(int32 I=0;I<4;++I) if(PC->WasInputKeyJustPressed(Keys[I])||(Click&&Hit(P,40+I*305,70,285,42))) ArchiveLevel=I;
    }
    else
    {
        if(PC->WasInputKeyJustPressed(EKeys::Enter)||(Click&&Hit(P,40,647,280,46))) ChooseLevel(0);
        if(PC->WasInputKeyJustPressed(EKeys::H)||(Click&&Hit(P,490,647,280,46))) OpenArchive();
    }
    return true;
}

void ASuperstarStoryHUD::DrawEdition(const FSuperstarEdition& E,float X,float Y,float W)
{
    const float S=W/1180.f;
    Rect(X+5,Y+7,W,460*S,FLinearColor(0,0,0,.4f));Rect(X,Y,W,460*S,Paper);
    FitLabel(TEXT("骏 风 娱 乐"),X+20*S,Y+12*S,W-40*S,48*S,32*S,Ink,true);
    Rect(X+25*S,Y+65*S,W-50*S,1,Ink);
    FitLabel(E.Headline,X+25*S,Y+83*S,W-50*S,60*S,25*S,Ink,true);
    for(int32 I=0;I<E.Shots.Num();++I)
    {
        const float PX=X+(25+I*380)*S;
        DrawPhoto(E.Shots[I],PX,Y+160*S,365*S,false);
        FitLabel(E.Shots[I].Title,PX,Y+295*S,365*S,36*S,17*S,Ink);
    }
    if(Levels.IsValidIndex(E.Level))
    {
        const auto& Lead=Levels[E.Level]->GetArrayField(TEXT("paper_lead"));
        for(int32 I=0;I<Lead.Num();++I) FitLabel(Lead[I]->AsString(),X+25*S,Y+(350+I*34)*S,W-50*S,32*S,17*S,Ink);
    }
}

void ASuperstarStoryHUD::DrawCollection()
{
    Rect(0,0,1280,720,Ink);
    if(LevelPhase==ESuperstarLevelPhase::Archive)
    {
        FitLabel(TEXT("已发布报纸 · 每关最新一期"),40,25,1200,35,24,Mint);
        for(int32 I=0;I<4;++I) Button(Levels[I]->GetStringField(TEXT("title")),40+I*305,70,285,42,15,I==ArchiveLevel?Mint:FLinearColor(.15f,.19f,.22f),I==ArchiveLevel?Ink:FLinearColor::White);
        if(Editions.IsValidIndex(ArchiveLevel)&&Editions[ArchiveLevel].Shots.Num()==3) DrawEdition(Editions[ArchiveLevel],50,140,1180);
        else FitLabel(TEXT("本关尚未发布报纸"),100,280,1080,60,28,Paper,true);
        Button(TEXT("返回  Backspace"),40,647,280,46,18,Mint,Ink);
    }
    else
    {
        FitLabel(TEXT("FRAMING FAME"),40,25,1200,60,42,Paper,true);
        FitLabel(TEXT("扶摇直上 · 四期报道已完成"),40,90,1200,45,26,Mint,true);
        FitLabel(TEXT("同一个现场，四期由你决定的故事。"),40,139,1200,30,18,Paper,true);
        for(int32 I=0;I<4;++I) if(Editions.IsValidIndex(I)&&Editions[I].Shots.Num()==3)
            DrawEdition(Editions[I],65+(I%2)*610,190+(I/2)*225,540);
        Button(TEXT("重新开始  Enter"),40,647,280,46,18,Mint,Ink);
        Button(TEXT("回看报道  H"),490,647,280,46,18,Mint,Ink);
    }
    Button(TEXT("返回选关  ESC"),940,647,280,46,18,FLinearColor(.16f,.20f,.24f));
}
