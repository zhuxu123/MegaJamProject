#include "GameLocalization.h"
#include "SuperstarStory.h"
#include "SuperstarStage.h"
#include "SuperstarMusic.h"
#include "BackstageSfx.h"
#include "Camera/PlayerCameraManager.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "Dom/JsonObject.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "InputCoreTypes.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

namespace
{
    const FLinearColor PhotoInk(.025f,.033f,.055f,1.f), PhotoMint(.30f,.85f,.73f,1.f);
    bool Hit(FVector2D P,float X,float Y,float W,float H) { return P.X>=X && P.X<X+W && P.Y>=Y && P.Y<Y+H; }
    void CropBounds(FSuperstarPhoto& P)
    {
        P.CropZoom=FMath::Clamp(P.CropZoom,1.f,3.f);
        const float Half=.5f/P.CropZoom;
        P.CropX=FMath::Clamp(P.CropX,Half,1.f-Half);
        P.CropY=FMath::Clamp(P.CropY,Half,1.f-Half);
    }
    bool WritePhoto(const FString& Path,int32 W,int32 H,const TArray<FColor>& Pixels)
    {
        TArray64<uint8> PNG;
        FImageUtils::PNGCompressImageArray(W,H,Pixels,PNG);
        return FFileHelper::SaveArrayToFile(PNG,*Path);
    }
}

void ASuperstarStoryHUD::UpdateCamera(float Delta)
{
    auto* PC=GetOwningPlayerController();
    if (!PC || !Stage || bReferenceView) return;
    const float PreviousPan=TargetPan,PreviousTilt=TargetTilt,PreviousZoom=TargetZoom,PreviousStation=TargetStation;
    if (PC->WasInputKeyJustPressed(EKeys::One)) { CameraStation=0; TargetStation=0; }
    if (PC->WasInputKeyJustPressed(EKeys::Two)) { CameraStation=1; TargetStation=-1; }
    if (PC->WasInputKeyJustPressed(EKeys::Three)) { CameraStation=2; TargetStation=1; }
    auto Axis=[PC,Delta](FKey Negative,FKey Positive)
    {
        const float P=PC->WasInputKeyJustPressed(Positive)?FMath::Max(Delta,.075f):(PC->IsInputKeyDown(Positive)?Delta:0.f);
        const float N=PC->WasInputKeyJustPressed(Negative)?FMath::Max(Delta,.075f):(PC->IsInputKeyDown(Negative)?Delta:0.f);
        return P-N;
    };
    TargetPan+=Axis(EKeys::A,EKeys::D)*.85f;
    TargetTilt+=Axis(EKeys::S,EKeys::W)*.65f;
    TargetZoom+=Axis(EKeys::Q,EKeys::E)*.65f;
    if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp)) TargetZoom+=.12f;
    if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown)) TargetZoom-=.12f;
    const auto P=Pointer();
    const bool bEvidenceOverlay=!Beats[Beat]->GetStringField(TEXT("evidence")).IsEmpty() && P.Y>410;
    if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton) && !bTimelineDragging)
    {
        bCameraDragging=Hit(P,35,105,1210,410) && !bEvidenceOverlay;
        LastDrag=P;
    }
    if (!PC->IsInputKeyDown(EKeys::LeftMouseButton) || bTimelineDragging) bCameraDragging=false;
    if (bCameraDragging)
    {
        const auto Drag=P-LastDrag;
        TargetPan-=Drag.X/400.f;
        TargetTilt+=Drag.Y/250.f;
        LastDrag=P;
    }
    TargetPan=FMath::Clamp(TargetPan,-1.5f,1.5f);
    TargetTilt=FMath::Clamp(TargetTilt,-1.f,1.f);
    TargetZoom=FMath::Clamp(TargetZoom,1.f,2.6f);
    if(!FMath::IsNearlyEqual(PreviousZoom,TargetZoom)) PresentCamera(TargetZoom>PreviousZoom?TEXT("拉近镜头"):TEXT("拉远镜头"));
    else if(!FMath::IsNearlyEqual(PreviousPan,TargetPan)||!FMath::IsNearlyEqual(PreviousTilt,TargetTilt)||PreviousStation!=TargetStation) PresentCamera(TEXT("调整构图"));
    if(Sfx) Sfx->CameraMotion(!FMath::IsNearlyEqual(PreviousPan,TargetPan) || !FMath::IsNearlyEqual(PreviousTilt,TargetTilt) || PreviousStation!=TargetStation,
        FMath::IsNearlyEqual(PreviousZoom,TargetZoom)?0:(TargetZoom>PreviousZoom?1:-1));
    ViewPan=FMath::FInterpTo(ViewPan,TargetPan,Delta,9.f);
    ViewTilt=FMath::FInterpTo(ViewTilt,TargetTilt,Delta,9.f);
    ViewZoom=FMath::FInterpTo(ViewZoom,TargetZoom,Delta,9.f);
    ViewStation=FMath::FInterpTo(ViewStation,TargetStation,Delta,7.f);
    Stage->SetView(ViewStation,ViewPan,ViewTilt,ViewZoom);
    PC->PlayerCameraManager->UpdateCamera(0.f);
}

void ASuperstarStoryHUD::TakePhoto()
{
    if (LevelPhase!=ESuperstarLevelPhase::Playing || PhotoPhase!=ESuperstarPhotoPhase::Live || Photos.Num()>=6 || bReferenceView
        || Beats[Beat]->GetStringField(TEXT("scene"))==TEXT("newspaper")) return;
    if(Sfx) {Sfx->TransportMotion(0);Sfx->PlayCue(TEXT("shutter"));}
    PendingPhoto=FSuperstarPhoto();
    PendingPhoto.Beat=Beat+1; PendingPhoto.Time=Clock; PendingPhoto.Station=CameraStation;
    PendingPhoto.Pan=ViewPan; PendingPhoto.Tilt=ViewTilt; PendingPhoto.Zoom=ViewZoom;
    PendingPhoto.Title=Beats[Beat]->GetStringField(TEXT("title"));
    TargetPan=ViewPan; TargetTilt=ViewTilt; TargetZoom=ViewZoom; TargetStation=ViewStation;
    bPlaying=false; PlaybackRate=0; bCameraDragging=false; bTimelineDragging=false;
    if (Music) Music->SetStoryPaused(true);
    UpdateActors();
    GetOwningPlayerController()->PlayerCameraManager->SetGameCameraCutThisFrame();
    PhotoPhase=ESuperstarPhotoPhase::Capturing;
    CaptureStart=FPlatformTime::Seconds();
    ScreenshotHandle=UGameViewportClient::OnScreenshotCaptured().AddUObject(this,&ASuperstarStoryHUD::OnPhotoCaptured);
    FScreenshotRequest::RequestScreenshot(false,true);
}

void ASuperstarStoryHUD::OnPhotoCaptured(int32 Width,int32 Height,const TArray<FColor>& Pixels)
{
    UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
    ScreenshotHandle.Reset();
    if (PhotoPhase!=ESuperstarPhotoPhase::Capturing || Pixels.Num()!=Width*Height) return;
    const float Fit=FMath::Min(Width/1280.f,Height/720.f);
    const FVector2D Offset((Width-1280*Fit)/2,(Height-720*Fit)/2);
    const int32 L=FMath::RoundToInt(Offset.X+35*Fit),T=FMath::RoundToInt(Offset.Y+105*Fit);
    PendingPhoto.Width=FMath::RoundToInt(1210*Fit); PendingPhoto.Height=FMath::RoundToInt(410*Fit);
    if (L<0 || T<0 || L+PendingPhoto.Width>Width || T+PendingPhoto.Height>Height) { RejectPhoto(); return; }
    PendingPhoto.Pixels.SetNumUninitialized(PendingPhoto.Width*PendingPhoto.Height);
    for (int32 Y=0;Y<PendingPhoto.Height;++Y)
        FMemory::Memcpy(PendingPhoto.Pixels.GetData()+Y*PendingPhoto.Width,Pixels.GetData()+(Y+T)*Width+L,PendingPhoto.Width*sizeof(FColor));
    for (auto& Pixel:PendingPhoto.Pixels) Pixel.A=255;
    PendingPhoto.Texture=UTexture2D::CreateTransient(PendingPhoto.Width,PendingPhoto.Height,PF_B8G8R8A8);
    auto& Mip=PendingPhoto.Texture->GetPlatformData()->Mips[0];
    void* Buffer=Mip.BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(Buffer,PendingPhoto.Pixels.GetData(),PendingPhoto.Pixels.Num()*sizeof(FColor));
    Mip.BulkData.Unlock();
    PendingPhoto.Texture->SRGB=true;
    PendingPhoto.Texture->UpdateResource();
    PhotoPhase=ESuperstarPhotoPhase::Confirm;
    PhotoArrivalTime=UITime;
    ShowHint(TEXT("confirm"),TEXT("保留或重拍后，现场会自动继续播放。"));
    UE_LOG(LogTemp,Display,TEXT("SuperstarPhoto: captured beat=%d time=%.2f %dx%d"),PendingPhoto.Beat,PendingPhoto.Time,PendingPhoto.Width,PendingPhoto.Height);
}

void ASuperstarStoryHUD::AcceptPhoto()
{
    if (PhotoPhase!=ESuperstarPhotoPhase::Confirm || !PendingPhoto.Texture || Photos.Num()>=6) return;
    if (PhotoSession.IsEmpty())
    {
        PhotoSession=FPaths::ProjectSavedDir()/TEXT("SuperstarPhotos")/FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))+TEXT("-")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
        IFileManager::Get().MakeDirectory(*PhotoSession,true);
    }
    PendingPhoto.File=FString::Printf(TEXT("Shot_%02d.png"),Photos.Num()+1);
    if (!WritePhoto(PhotoSession/PendingPhoto.File,PendingPhoto.Width,PendingPhoto.Height,PendingPhoto.Pixels))
    {
        PhotoNotice=TEXT("照片写入失败，请检查 Saved 目录空间。"); return;
    }
    Photos.Add(MoveTemp(PendingPhoto));
    PendingPhoto=FSuperstarPhoto();
    PhotoPhase=ESuperstarPhotoPhase::Live;
    PhotoNotice=FString::Printf(TEXT("已保存 %d/6 张；Tab 打开选图与发布。"),Photos.Num());
    // A successfully kept shot returns to normal playback, even if framed while paused.
    bPlaying=true;
    PlaybackRate=1.f;
    SavedPulseUntil=UITime+1.f;
    if (Music) Music->SetStoryPaused(false);
}

void ASuperstarStoryHUD::RejectPhoto(bool bResumePlayback)
{
    if (ScreenshotHandle.IsValid()) UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
    ScreenshotHandle.Reset();
    PendingPhoto=FSuperstarPhoto();
    PhotoPhase=ESuperstarPhotoPhase::Live;
    if (bResumePlayback)
    {
        bPlaying=true;
        PlaybackRate=1.f;
        if (Music) Music->SetStoryPaused(false);
    }
}

void ASuperstarStoryHUD::OpenAlbum()
{
    bPlaying=false;
    if (Music) Music->SetStoryPaused(true);
    PhotoPhase=ESuperstarPhotoPhase::Edit;
    bCropDragging=false;
    ShowHint(TEXT("edit"),TEXT("选满三张；拖动红框调整裁切，滚轮缩放。"));
    PhotoNotice=TEXT("选3张、决定顺序与裁切，再发布你的现场报道。");
}

void ASuperstarStoryHUD::TogglePhoto(int32 Index)
{
    if (!Photos.IsValidIndex(Index)) return;
    if (PhotoSlots.Contains(Index)) PhotoSlots.Remove(Index);
    else if (PhotoSlots.Num()<3) PhotoSlots.Add(Index);
    EditSlot=FMath::Clamp(EditSlot,0,FMath::Max(0,PhotoSlots.Num()-1));
    PhotoNotice=PhotoSlots.Num()==3 ? TEXT("已选满3张；调整顺序与裁切后即可预览或发布。") : FString::Printf(TEXT("已选 %d/3 张；点击上方照片继续选择。"),PhotoSlots.Num());
}

void ASuperstarStoryHUD::StartPhotoPreview(bool bPublish)
{
    if (PhotoSlots.Num()!=3) { PhotoNotice=TEXT("请先选满3张照片。"); return; }
    if(bPublish && !CanPublishLevel()) return;
    PreviewClock=0;
    bPublishPhotos=bPublish;
    PhotoPhase=ESuperstarPhotoPhase::Preview;
}

void ASuperstarStoryHUD::SavePublication()
{
    auto Manifest=MakeShared<FJsonObject>();
    Manifest->SetBoolField(TEXT("test_run"),bLevelsSmoke||bFullStoryDebug);
    Manifest->SetStringField(TEXT("headline"),PublicationHeadline());
    Manifest->SetNumberField(TEXT("level"),LevelIndex+1);
    Manifest->SetNumberField(TEXT("first_beat"),FirstBeat+1);
    Manifest->SetNumberField(TEXT("last_beat"),LastBeat);
    if(Levels.IsValidIndex(LevelIndex)) Manifest->SetStringField(TEXT("level_title"),Levels[LevelIndex]->GetStringField(TEXT("title")));
    Manifest->SetStringField(TEXT("scope"),TEXT("SuperstarStory player photo sequence; no legacy level score is inferred."));
    TArray<TSharedPtr<FJsonValue>> Shots;
    const FString Edition=TEXT("Publication-")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Directory=PhotoSession/Edition;
    IFileManager::Get().MakeDirectory(*Directory,true);
    for (int32 I=0;I<PhotoSlots.Num();++I)
    {
        const auto& P=Photos[PhotoSlots[I]];
        const int32 W=FMath::Max(1,FMath::RoundToInt(P.Width/P.CropZoom));
        const int32 H=FMath::Max(1,FMath::RoundToInt(P.Height/P.CropZoom));
        const int32 L=FMath::Clamp(FMath::RoundToInt(P.CropX*P.Width-W*.5),0,P.Width-W);
        const int32 T=FMath::Clamp(FMath::RoundToInt(P.CropY*P.Height-H*.5),0,P.Height-H);
        TArray<FColor> Cut;Cut.SetNumUninitialized(W*H);
        for(int32 Y=0;Y<H;++Y) FMemory::Memcpy(Cut.GetData()+Y*W,P.Pixels.GetData()+(T+Y)*P.Width+L,W*sizeof(FColor));
        const FString File=FString::Printf(TEXT("Frame_%02d.png"),I+1);
        if (!WritePhoto(Directory/File,W,H,Cut)) { PhotoNotice=TEXT("发布文件写入失败；原照片已保留。");PhotoPhase=ESuperstarPhotoPhase::Edit;return; }
        auto J=MakeShared<FJsonObject>();
        J->SetStringField(TEXT("original"),TEXT("../")+P.File);J->SetStringField(TEXT("image"),File);
        J->SetNumberField(TEXT("beat"),P.Beat);J->SetNumberField(TEXT("time"),P.Time);J->SetNumberField(TEXT("station"),P.Station);
        J->SetNumberField(TEXT("pan"),P.Pan);J->SetNumberField(TEXT("tilt"),P.Tilt);J->SetNumberField(TEXT("zoom"),P.Zoom);
        J->SetNumberField(TEXT("crop_left"),L);J->SetNumberField(TEXT("crop_top"),T);J->SetNumberField(TEXT("crop_width"),W);J->SetNumberField(TEXT("crop_height"),H);
        Shots.Add(MakeShared<FJsonValueObject>(J));
    }
    Manifest->SetArrayField(TEXT("shots"),Shots);
    Manifest->SetStringField(TEXT("language"), GameLocalization::IsEnglish()?TEXT("en"):TEXT("zh"));
    Manifest->SetStringField(TEXT("display_headline"), GameLocalization::Translate(PublicationHeadline()));
    TArray<TSharedPtr<FJsonValue>> Evidence;
    for(int32 I=FirstBeat;I<LastBeat;++I) if(PublishedBeats.Contains(I)) Evidence.Add(MakeShared<FJsonValueNumber>(I+1));
    Manifest->SetArrayField(TEXT("published_evidence_beats"),Evidence);
    FString Json;FJsonSerializer::Serialize(Manifest,TJsonWriterFactory<>::Create(&Json));
    if (!FFileHelper::SaveStringToFile(Json,*(Directory/TEXT("publication.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) { PhotoNotice=TEXT("发布记录写入失败。");PhotoPhase=ESuperstarPhotoPhase::Edit;return; }
    PhotoNotice=TEXT("原图与发布序列已保存到 Saved/SuperstarPhotos。");
    if(Sfx) Sfx->PlayCue(TEXT("result"));
    if(!bFullStoryDebug) {LevelPhase=ESuperstarLevelPhase::Newspaper;NewspaperRevealTime=0;bPlaying=false;if(Music) Music->SetStoryPaused(true);}
    if(!bFullStoryDebug) ReadEdition(Directory/TEXT("publication.json"));
    UE_LOG(LogTemp,Display,TEXT("SuperstarPhoto: publication saved %s"),*Directory);
}

bool ASuperstarStoryHUD::TickPhotography(float Delta)
{
    auto* PC=GetOwningPlayerController();
    const auto P=Pointer();
    const bool Click=PC->WasInputKeyJustPressed(EKeys::LeftMouseButton);
    const bool Back=PC->WasInputKeyJustPressed(EKeys::BackSpace)||PC->WasInputKeyJustPressed(EKeys::Escape);
    if (PhotoPhase==ESuperstarPhotoPhase::Capturing)
    {
        if (FPlatformTime::Seconds()-CaptureStart>5.) { RejectPhoto();PhotoNotice=TEXT("拍照超时，请重试。"); }
        return true;
    }
    if (PhotoPhase==ESuperstarPhotoPhase::Confirm)
    {
        if (PC->WasInputKeyJustPressed(EKeys::Enter)||PC->WasInputKeyJustPressed(EKeys::Y)||(Click&&Hit(P,340,560,260,48))) AcceptPhoto();
        else if (Back||PC->WasInputKeyJustPressed(EKeys::N)||(Click&&Hit(P,680,560,260,48))) RejectPhoto(true);
        return true;
    }
    if (PhotoPhase==ESuperstarPhotoPhase::Preview)
    {
        if (Back) {PhotoPhase=ESuperstarPhotoPhase::Edit;return true;}
        PreviewClock+=Delta;
        if(PreviewClock>=6.f)
        {
            PhotoPhase=bPublishPhotos?ESuperstarPhotoPhase::Results:ESuperstarPhotoPhase::Edit;
            if(bPublishPhotos) SavePublication();
        }
        return true;
    }
    if (PhotoPhase==ESuperstarPhotoPhase::Results)
    {
        if(Back||PC->WasInputKeyJustPressed(EKeys::Enter)) PhotoPhase=ESuperstarPhotoPhase::Edit;
        if(PC->WasInputKeyJustPressed(EKeys::Tab)) PhotoPhase=ESuperstarPhotoPhase::Live;
        return true;
    }
    if (PhotoPhase==ESuperstarPhotoPhase::Edit)
    {
        if(Back||PC->WasInputKeyJustPressed(EKeys::Tab)) {PhotoPhase=ESuperstarPhotoPhase::Live;return true;}
        const FKey Keys[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six};
        for(int32 I=0;I<6;++I) if(PC->WasInputKeyJustPressed(Keys[I])||(Click&&Hit(P,35+I*203,145,190,110))) TogglePhoto(I);
        for(int32 I=0;I<PhotoSlots.Num();++I) if(Click&&Hit(P,35+I*410,345,390,150)) EditSlot=I;
        if(PC->WasInputKeyJustPressed(EKeys::C)&&PhotoSlots.Num()) EditSlot=(EditSlot+1)%PhotoSlots.Num();
        if(PC->WasInputKeyJustPressed(EKeys::Z)) SwapPhotos(0,1);
        if(PC->WasInputKeyJustPressed(EKeys::X)) SwapPhotos(1,2);
        if(PhotoSlots.IsValidIndex(EditSlot))
        {
            auto& Shot=Photos[PhotoSlots[EditSlot]];
            auto Axis=[PC,Delta](FKey N,FKey K) { return (PC->WasInputKeyJustPressed(K)?FMath::Max(Delta,.1f):(PC->IsInputKeyDown(K)?Delta:0.f))-(PC->WasInputKeyJustPressed(N)?FMath::Max(Delta,.1f):(PC->IsInputKeyDown(N)?Delta:0.f)); };
            Shot.CropZoom+=Axis(EKeys::Q,EKeys::E);
            Shot.CropX+=Axis(EKeys::A,EKeys::D)*.25f;
            Shot.CropY+=Axis(EKeys::W,EKeys::S)*.25f;
            const float Wheel=(PC->WasInputKeyJustPressed(EKeys::MouseScrollUp)?1.f:0.f)-(PC->WasInputKeyJustPressed(EKeys::MouseScrollDown)?1.f:0.f);
            HandleCropPointer(P,Click,PC->IsInputKeyDown(EKeys::LeftMouseButton),Wheel);
            CropBounds(Shot);
        }
        if(PC->WasInputKeyJustPressed(EKeys::T)) Headline=(Headline+1)%3;
        if(PC->WasInputKeyJustPressed(EKeys::P)||(Click&&Hit(P,640,615,180,45))) StartPhotoPreview(false);
        if(PC->WasInputKeyJustPressed(EKeys::Enter)||(Click&&Hit(P,850,615,180,45))) StartPhotoPreview(true);
        if(Click&&Hit(P,1060,615,180,45)) PhotoPhase=ESuperstarPhotoPhase::Live;
        return true;
    }
    UpdateCamera(Delta);
    if(PC->WasInputKeyJustPressed(EKeys::Tab)) {OpenAlbum();return true;}
    if(PC->WasInputKeyJustPressed(EKeys::Enter)) {TakePhoto();return PhotoPhase!=ESuperstarPhotoPhase::Live;}
    return false;
}

bool ASuperstarStoryHUD::HandleCropPointer(FVector2D Position,bool bPressed,bool bDown,float Wheel)
{
    if(!PhotoSlots.IsValidIndex(EditSlot)) {bCropDragging=false;return false;}
    auto& Shot=Photos[PhotoSlots[EditSlot]];
    const float X=35+EditSlot*410,H=390.f*Shot.Height/Shot.Width;
    const bool Inside=Hit(Position,X,348,390,H);
    if(bPressed&&Inside) {bCropDragging=true;CropDragStart=Position;CropCenterStart=FVector2D(Shot.CropX,Shot.CropY);}
    if(!bDown) bCropDragging=false;
    if(bCropDragging) {Shot.CropX=CropCenterStart.X+(Position.X-CropDragStart.X)/390.f;Shot.CropY=CropCenterStart.Y+(Position.Y-CropDragStart.Y)/H;}
    if(Inside) Shot.CropZoom+=Wheel*.15f;
    CropBounds(Shot);
    return Inside||bCropDragging;
}

void ASuperstarStoryHUD::DrawPhoto(const FSuperstarPhoto& P,float X,float Y,float W,bool bCrop)
{
    if(!P.Texture) return;
    const float Span=bCrop?1.f/P.CropZoom:1.f;
    DrawTexture(P.Texture,Origin.X+X*Scale,Origin.Y+Y*Scale,W*Scale,W*P.Height/P.Width*Scale,
        bCrop?P.CropX-Span*.5f:0.f,bCrop?P.CropY-Span*.5f:0.f,Span,Span,FLinearColor::White,BLEND_Opaque);
}

void ASuperstarStoryHUD::DrawViewfinder()
{
    if (bReferenceView || Beats[Beat]->GetStringField(TEXT("scene"))==TEXT("newspaper")) return;
    // Match the reference's fine cyan corners, centered on the actual photo area.
    const FLinearColor Cyan=UITime<SavedPulseUntil?FLinearColor(.25f,1.f,.45f):UITime<CameraNoticeUntil?FLinearColor(1.f,.8f,.2f):FLinearColor(.22f,.78f,.82f,.85f);
    constexpr float Left=440, Right=840, Top=205, Bottom=415, Arm=23, Stroke=1.5f;
    for (float X : {Left,Right}) for (float Y : {Top,Bottom})
    {
        Rect(X==Left?X:X-Arm,Y,Arm,Stroke,Cyan);
        Rect(X,Y==Top?Y:Y-Arm,Stroke,Arm,Cyan);
    }
    Rect(630,309.25f,20,Stroke,Cyan);
    Rect(639.25f,300,Stroke,20,Cyan);
}

void ASuperstarStoryHUD::DrawRecentPhotos()
{
    // Pending shots are visible immediately, but disappear on rejection.
    const bool bPending=PhotoPhase==ESuperstarPhotoPhase::Confirm && PendingPhoto.Texture;
    const int32 Count=Photos.Num()+(bPending?1:0);
    const int32 First=FMath::Max(0,Count-3);
    for (int32 I=First;I<Count;++I)
    {
        const auto& Photo=I<Photos.Num()?Photos[I]:PendingPhoto;
        if (!Photo.Texture) continue;
        const int32 Row=I-First;
        const float Arrival=I==Count-1?FMath::Clamp((UITime-PhotoArrivalTime)/.45f,0.f,1.f):1.f;
        const float X=1030.f-(Row%2)*12.f+220.f*FMath::Square(1.f-Arrival), Y=105.f+Row*102.f;
        const float Angle=Row%2?-2.f:2.f;
        const float Radians=FMath::DegreesToRadians(Angle);
        auto Position=[&](float DX,float DY)
        {
            return Origin+FVector2D(X+DX*FMath::Cos(Radians)-DY*FMath::Sin(Radians),
                Y+DX*FMath::Sin(Radians)+DY*FMath::Cos(Radians))*Scale;
        };
        auto Paper=[&](float DX,float DY,float W,float H,FLinearColor Color)
        {
            FCanvasTileItem Tile(Position(DX,DY),FVector2D(W,H)*Scale,Color);
            Tile.Rotation=FRotator(0,Angle,0);
            Tile.BlendMode=SE_BLEND_Translucent;
            Canvas->DrawItem(Tile);
        };
        Paper(4,5,198,96,FLinearColor(0,0,0,.45f));
        Paper(0,0,198,96,FLinearColor(.89f,.88f,.81f));
        const FVector2D ImagePosition=Position(7,7);
        DrawTexture(Photo.Texture,ImagePosition.X,ImagePosition.Y,184*Scale,184.f*Photo.Height/Photo.Width*Scale,
            0,0,1,1,FLinearColor::White,BLEND_Opaque,1,false,Angle);
        Paper(78,-5,42,11,FLinearColor(.67f,.73f,.65f,.85f));
        const FVector2D Pivot=Origin+FVector2D(X,Y)*Scale;
        Canvas->Canvas->PushRelativeTransform(FTranslationMatrix(FVector(-Pivot.X,-Pivot.Y,0))*FRotationMatrix(FRotator(0,Angle,0))*FTranslationMatrix(FVector(Pivot.X,Pivot.Y,0)));
        const FString Status=GameLocalization::Translate(bPending&&I==Count-1?TEXT("待确认"):TEXT("已保存"));
        FitLabel(FString::Printf(TEXT("#%02d  %s"),I+1,*Status),X+8,Y+76,182,17,11,PhotoInk);
        Canvas->Canvas->PopTransform();
    }
}

void ASuperstarStoryHUD::DrawPhotography()
{
    Rect(0,0,1280,720,PhotoInk);
    if(PhotoPhase==ESuperstarPhotoPhase::Confirm)
    {
        Label(TEXT("确认这张现场照片"),35,35,28,PhotoMint);
        Label(FString::Printf(TEXT("第%d段 · %.1fs · 机位%d · 确认后占用一张名额"),PendingPhoto.Beat,PendingPhoto.Time,PendingPhoto.Station+1),35,85,18);
        DrawPhoto(PendingPhoto,85,160,1110,false);
        Button(TEXT("Enter / Y   保留"),340,560,260,48,20,PhotoMint,PhotoInk);
        Button(TEXT("Backspace / N   重拍"),680,560,260,48,20,FLinearColor(.3f,.2f,.2f));
        FitLabel(PhotoNotice,35,670,1210,28,15,PhotoMint);
        DrawRecentPhotos();
        DrawHint();
        return;
    }
    if(PhotoPhase==ESuperstarPhotoPhase::Preview)
    {
        const int32 I=FMath::Clamp(FMath::FloorToInt(PreviewClock/2),0,2);
        Label(PublicationHeadline(),35,35,28,PhotoMint);
        DrawPhoto(Photos[PhotoSlots[I]],35,150,1210,true);
        Label(FString::Printf(TEXT("%d / 3 · %s · Backspace 取消"),I+1,bPublishPhotos?TEXT("发布预览"):TEXT("仅预览")),35,610,20);
        return;
    }
    const bool bResults=PhotoPhase==ESuperstarPhotoPhase::Results;
    Label(bResults?TEXT("现场报道已发布"):TEXT("选图与剪辑 · 保留3个时刻"),35,30,28,PhotoMint);
    Label(PublicationHeadline(),35,83,21);
    for(int32 I=0;I<6;++I)
    {
        const float X=35+I*203;
        Rect(X-3,142,196,112,PhotoSlots.Contains(I)?PhotoMint:FLinearColor(.12f,.16f,.20f));
        if(Photos.IsValidIndex(I)) {DrawPhoto(Photos[I],X,147,190,false);FitLabel(FString::Printf(TEXT("%d  第%d段 / %.1fs"),I+1,Photos[I].Beat,Photos[I].Time),X+3,220,184,29,14,PhotoInk);}
    }
    Label(TEXT("发布顺序（点击槽位选择裁切目标）"),35,300,20,PhotoMint);
    for(int32 I=0;I<3;++I)
    {
        const int32 Previous=PhotoSlots.IsValidIndex(I)?SwapFrom.IndexOfByKey(PhotoSlots[I]):INDEX_NONE;
        const float Alpha=FMath::Clamp((UITime-SwapStart)/.32f,0.f,1.f);
        const float X=35+410*(Previous>=0?FMath::Lerp(static_cast<float>(Previous),static_cast<float>(I),Alpha*Alpha*(3-2*Alpha)):I);
        Rect(X-3,342,396,153,I==EditSlot?PhotoMint:FLinearColor(.15f,.19f,.24f));
        if(PhotoSlots.IsValidIndex(I)) {if(I==EditSlot&&!bResults) DrawCropGuide(Photos[PhotoSlots[I]],X,348,390);else DrawPhoto(Photos[PhotoSlots[I]],X,348,390,true);Rect(X,477,390,18,FLinearColor(.12f,.16f,.20f));FitLabel(FString::Printf(TEXT("槽%d · 原片%d"),I+1,PhotoSlots[I]+1),X+8,477,374,18,12);}
    }
    FitLabel(PhotoNotice,35,530,1210,32,17,PhotoMint);
    if(bResults)
    {
        TSet<int32> Moments;for(int32 I:PhotoSlots) Moments.Add(Photos[I].Beat);
        FitLabel(FString::Printf(TEXT("3张现场图 · %d个剧情段落 · Enter 返回剪辑 / Tab 继续拍摄"),Moments.Num()),35,595,1210,38,20);
        Label(TEXT("选图顺序、裁切和标题，会让同一现场呈现不同的含义。"),35,650,16);
    }
    else
    {
        FitLabel(TEXT("1—6 选片 · C 选槽 · Z/X 调序 · WASD 裁切位置 · Q/E 裁切缩放 · T 标题"),35,570,1210,32,16);
        const TCHAR* Labels[]={TEXT("P 预览"),TEXT("Enter 发布"),TEXT("Tab 继续拍摄")};
        for(int32 I=0;I<3;++I) Button(Labels[I],640+I*210,615,180,45,18,FLinearColor(.12f,.25f,.27f));
    }
    DrawHint();
}

void ASuperstarStoryHUD::EndPlay(const EEndPlayReason::Type Reason)
{
    if(ScreenshotHandle.IsValid()) UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
    ScreenshotHandle.Reset();
    Super::EndPlay(Reason);
}

void ASuperstarStoryHUD::RunPhotoSmoke()
{
    ++PhotoSmokeTicks;
    if(PhotoSmokeTicks<20) return;
    PhotoSmokeTicks=0;
    if(PhotoPhase==ESuperstarPhotoPhase::Capturing) {check(FPlatformTime::Seconds()-CaptureStart<10.);return;}
    if(PhotoSmokeStep==0)
    {
        static bool bCapturedViewfinder=false;
        if(!bCapturedViewfinder) {FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/SuperstarPhoto/Viewfinder.png"),true,false);bCapturedViewfinder=true;return;}
        bPlaying=false;SelectBeat(10);Clock=2.4f;UpdateActors();TakePhoto();++PhotoSmokeStep;
    }
    else if(PhotoSmokeStep==1)
    {
        check(PhotoPhase==ESuperstarPhotoPhase::Confirm);
        static bool bCapturedConfirm=false;
        if(!bCapturedConfirm) {FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/SuperstarPhoto/Confirm.png"),true,false);bCapturedConfirm=true;return;}
        RejectPhoto(true);
        check(Photos.IsEmpty() && PhotoPhase==ESuperstarPhotoPhase::Live && bPlaying && PlaybackRate==1.f);
        const float Before=SequenceTime();
        AdvanceSequence(.1f,0.f);
        check(FMath::IsNearlyEqual(SequenceTime(),Before+.1f,.001f));
        ++PhotoSmokeStep;
    }
    else if(PhotoSmokeStep>=2&&PhotoSmokeStep<=7)
    {
        if(PhotoSmokeStep%2==0)
        {
            const int32 Shot=(PhotoSmokeStep-2)/2;
            const int32 ShotBeats[]={10,16,18};
            SelectBeat(ShotBeats[Shot]);Clock=Shot==0?4.6f:2.6f;
            ViewStation=Shot-1;ViewPan=.15f*Shot;ViewTilt=.1f;ViewZoom=1.1f+.12f*Shot;
            CameraStation=Shot==0?1:(Shot==1?0:2);Stage->SetView(ViewStation,ViewPan,ViewTilt,ViewZoom);
            GetOwningPlayerController()->PlayerCameraManager->UpdateCamera(0.f);
            UpdateActors();TakePhoto();
        }
        else
        {
            check(PhotoPhase==ESuperstarPhotoPhase::Confirm && !bPlaying);
            AcceptPhoto();
            check(PhotoPhase==ESuperstarPhotoPhase::Live && bPlaying && PlaybackRate==1.f);
            const float Before=SequenceTime();
            AdvanceSequence(.1f,0.f);
            check(FMath::IsNearlyEqual(SequenceTime(),Before+.1f,.001f));
        }
        ++PhotoSmokeStep;
    }
    else if(PhotoSmokeStep==8)
    {
        static bool bCapturedRecent=false;
        if(!bCapturedRecent) {FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/SuperstarPhoto/RecentPhotos.png"),true,false);bCapturedRecent=true;return;}
        check(Photos.Num()==3);OpenAlbum();StartPhotoPreview(true);check(PhotoPhase==ESuperstarPhotoPhase::Edit);
        TogglePhoto(0);TogglePhoto(1);TogglePhoto(2);SwapPhotos(0,2);
        check(PhotoSlots[0]==2 && PhotoSlots[2]==0 && EditSlot==2);
        UITime=SwapStart+.16f;
        auto& P=Photos[PhotoSlots[1]];P.CropZoom=1.5f;P.CropX=.6f;CropBounds(P);
        EditSlot=1;
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/SuperstarPhoto/Edit.png"),true,false);++PhotoSmokeStep;
    }
    else if(PhotoSmokeStep==9)
    {
        auto& P=Photos[PhotoSlots[EditSlot]];
        check(!HandleCropPointer({10,10},true,true,1));
        const float Zoom=P.CropZoom;
        check(HandleCropPointer({600,400},true,true,1));check(P.CropZoom>Zoom);
        HandleCropPointer({2000,-500},false,true,0);
        const float Half=.5f/P.CropZoom;
        check(FMath::IsNearlyEqual(P.CropX,1.f-Half) && FMath::IsNearlyEqual(P.CropY,Half));
        HandleCropPointer({2000,-500},false,false,0);check(!bCropDragging);
        P.CropZoom=1.5f;P.CropX=.6f;P.CropY=.5f;CropBounds(P);
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/SuperstarPhoto/CropGuide.png"),true,false);
        ++PhotoSmokeStep;
    }
    else if(PhotoSmokeStep==10)
    {
        StartPhotoPreview(false);check(PhotoPhase==ESuperstarPhotoPhase::Preview);
        PreviewClock=5.99f;TickPhotography(.1f);check(PhotoPhase==ESuperstarPhotoPhase::Edit);
        StartPhotoPreview(true);PreviewClock=5.99f;TickPhotography(.1f);check(PhotoPhase==ESuperstarPhotoPhase::Results);
        FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/SuperstarPhoto/Results.png"),true,false);
        ++PhotoSmokeStep;
    }
    else
    {
        UE_LOG(LogTemp,Display,TEXT("SuperstarPhoto smoke PASS: rejection, 3 captures, selection, crop, reorder, preview, publication; directory=%s"),*PhotoSession);
        GetOwningPlayerController()->ConsoleCommand(TEXT("quit"));
    }
}
