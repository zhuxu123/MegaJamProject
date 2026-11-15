#include "SuperstarStory.h"
#include "GameLocalization.h"
#include "SuperstarStage.h"
#include "SuperstarMusic.h"
#include "BackstageSfx.h"
#include "BackstageArt.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Engine/World.h"
#include "Core/AngelscriptBinds.h"
#include "CanvasItem.h"
#include "Dom/JsonObject.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/SlateRenderer.h"
#include "Engine/GameViewportClient.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

namespace
{
    const FLinearColor Ink(.025f, .033f, .055f, 1.f);
    const FLinearColor Mint(.30f, .85f, .73f, 1.f);
    bool ReadStoryJson(const FString& File, TSharedPtr<FJsonObject>& Out)
    {
        FString Text;
        return FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("SuperstarStory") / File))
            && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out) && Out.IsValid();
    }
    bool Inside(FVector2D P, float X, float Y, float W, float H)
    {
        return P.X >= X && P.X <= X + W && P.Y >= Y && P.Y <= Y + H;
    }
    void OpenSuperstarStory(APlayerController* Controller)
    {
        // Reuse the empty entry map as a host, with an explicit separate game mode.
        // Neither .umap nor the existing stage actors are edited.
        UGameplayStatics::OpenLevel(Controller, TEXT("/Game/levels/MainMenu"), true,
            TEXT("game=/Script/MegaJamProject.SuperstarStoryGameMode"));
    }
}

static const FAngelscriptBind BindSuperstarStory(TEXT("MegaJamProject.SuperstarStory"),
    EAngelscriptBindPhase::ExplicitBindings, [](FAngelscriptBinds& Binds)
    {
        auto Controller = Binds.ExistingClassForTarget("APlayerController");
        Controller.Method("void OpenSuperstarStory()", &OpenSuperstarStory);
    });

ASuperstarStoryGameMode::ASuperstarStoryGameMode()
{
    DefaultPawnClass = nullptr;
    PlayerControllerClass = ASuperstarStoryController::StaticClass();
    HUDClass = ASuperstarStoryHUD::StaticClass();
}

void ASuperstarStoryController::BeginPlay()
{
    Super::BeginPlay();
    bShowMouseCursor = true;
    FInputModeGameAndUI Input;
    Input.SetHideCursorDuringCapture(false);
    Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(Input);
}

ASuperstarStoryHUD::ASuperstarStoryHUD()
{
    PrimaryActorTick.bCanEverTick = true;
    Music = CreateDefaultSubobject<USuperstarMusic>(TEXT("StoryMusic"));
}

void ASuperstarStoryHUD::BeginPlay()
{
    Super::BeginPlay();
    PrimaryActorTick.bCanEverTick = true;
    SetActorTickEnabled(true);
    TSharedPtr<FJsonObject> Clips;
    StoryFont = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
    if (!ReadStoryJson(TEXT("story.json"), Data) || !ReadStoryJson(TEXT("clips.json"), Clips))
    {
        Error = TEXT("剧情数据缺失：请检查 Content/SuperstarStory。按 ESC 返回。");
        UE_LOG(LogTemp, Error, TEXT("SuperstarStory: missing or invalid JSON"));
        return;
    }
    Data->SetObjectField(TEXT("clips"), Clips);
    ReadStoryJson(TEXT("head_anchors.json"), HeadAnchors);
    for (const auto& Value : Data->GetArrayField(TEXT("beats"))) Beats.Add(Value->AsObject());
    for (const auto& StoryBeat : Beats) { BeatStarts.Add(SequenceDuration); SequenceDuration+=StoryBeat->GetNumberField(TEXT("duration")); }
    FullDuration=SequenceDuration;LastBeat=Beats.Num();
    if(!LoadLevels()) {Error=TEXT("四关任务配置无效，请检查 levels.json。");return;}
    bLevelsSmoke=FParse::Param(FCommandLine::Get(),TEXT("SuperstarLevelsSmoke"));
    bRound2Smoke=FParse::Param(FCommandLine::Get(),TEXT("SuperstarRound2Smoke"));
    bTransportSmoke=FParse::Param(FCommandLine::Get(),TEXT("SuperstarTransportSmoke"));
    bSmoke = FParse::Param(FCommandLine::Get(), TEXT("SuperstarSmoke")) || FParse::Param(FCommandLine::Get(), TEXT("SuperstarMenuSmoke"));
    bReferenceView = FParse::Param(FCommandLine::Get(), TEXT("SuperstarReferences"));
    bPhotoSmoke = FParse::Param(FCommandLine::Get(), TEXT("SuperstarPhotoSmoke"));
    bMotionSmoke = FParse::Param(FCommandLine::Get(), TEXT("SuperstarMotionSmoke"));
    Stage = GetWorld()->SpawnActor<ASuperstarStage>();
    if (Stage) GetOwningPlayerController()->SetViewTarget(Stage);
    SelectBeat(0);
    Sfx=UBackstageSfx::For(GetOwningPlayerController());
    if(FParse::Param(FCommandLine::Get(),TEXT("InterfaceSfxSmoke")) && Music) Music->ToggleMute();
    bMusicSmoke = FParse::Param(FCommandLine::Get(), TEXT("SuperstarMusicSmoke"));
    if (bMusicSmoke)
    {
        MusicSmokeStart = FPlatformTime::Seconds();
        UAudioMixerBlueprintLibrary::StartRecordingOutput(this, 90.f);
        UE_LOG(LogTemp, Display, TEXT("SuperstarMusic smoke: recording master output; first cue loops after 48 seconds"));
    }
    bFullStoryDebug=bRound2Smoke || bSmoke || bPhotoSmoke || bMotionSmoke || bTransportSmoke || bMusicSmoke || FParse::Param(FCommandLine::Get(),TEXT("InterfaceSfxSmoke"));
    if(bFullStoryDebug) LevelPhase=ESuperstarLevelPhase::Playing;
    else
    {
        const int32 Requested=FCString::Atoi(GetWorld()->URL.GetOption(TEXT("chapter="),TEXT("0")));
        if(Requested>=1 && Requested<=4) ChooseLevel(Requested-1);else ShowLevelSelect();
    }
    UE_LOG(LogTemp, Display, TEXT("SuperstarStory: loaded %d beats, %d clips at 5 fps"), Beats.Num(), Clips->Values.Num());
}

UTexture2D* ASuperstarStoryHUD::LoadFrame(const FString& Path)
{
    if (const auto* Found = Frames.Find(Path)) return Found->Get();
    TArray<uint8> Bytes;
    const FString File=GameLocalization::IsEnglish() && Path==TEXT("Static/Partial20261115/EV03_roster.png")
        ? FPaths::ProjectContentDir()/TEXT("Localization/Images/EV03_roster_en.png")
        : FPaths::ProjectContentDir()/TEXT("SuperstarStory")/Path;
    if (!FFileHelper::LoadFileToArray(Bytes, *File))
    {
        UE_LOG(LogTemp, Error, TEXT("SuperstarStory: cannot load %s"), *Path);
        return nullptr;
    }
    auto& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    auto Image = Module.CreateImageWrapper(EImageFormat::PNG);
    TArray<uint8> Raw;
    if (!Image.IsValid() || !Image->SetCompressed(Bytes.GetData(), Bytes.Num()) || !Image->GetRaw(ERGBFormat::BGRA, 8, Raw)) return nullptr;
    UTexture2D* Texture = UTexture2D::CreateTransient(Image->GetWidth(), Image->GetHeight(), PF_B8G8R8A8);
    if (!Texture) return nullptr;
    Texture->SRGB = true;
    Texture->Filter = TF_Bilinear;
    Texture->AddressX = TA_Clamp;
    Texture->AddressY = TA_Clamp;
    auto& Mip = Texture->GetPlatformData()->Mips[0];
    void* Buffer = Mip.BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(Buffer, Raw.GetData(), Raw.Num());
    Mip.BulkData.Unlock();
    Texture->UpdateResource();
    Frames.Add(Path, Texture); // GC tracked; no permanent roots.
    return Texture;
}

void ASuperstarStoryHUD::SelectBeat(int32 Index)
{
    if (!Beats.IsValidIndex(Index) || Index<FirstBeat || Index>=LastBeat) return;
    Beat = Index;
    Clock = 0;
    bPublished=PublishedBeats.Contains(Beat);
    bEvidencePlaced=bPublished;
    bDragging=false;
    PublishedHold = 0.f;
    // Drop only frames no longer needed. Shared idle clips survive adjacent shots.
    TSet<FString> Needed;
    for (const auto& Value : Beats[Beat]->GetArrayField(TEXT("actors")))
    {
        TArray<FString> Ids{Value->AsObject()->GetStringField(TEXT("clip"))};
        FString Walk;
        if(Value->AsObject()->TryGetStringField(TEXT("walk_clip"),Walk)) Ids.Add(Walk);
        const TArray<TSharedPtr<FJsonValue>>* Sequence=nullptr;
        if(Value->AsObject()->TryGetArrayField(TEXT("sequence"),Sequence))
            for(const auto& Part:*Sequence) Ids.Add(Part->AsObject()->GetStringField(TEXT("clip")));
        for(const auto& Id:Ids)
        {
            const auto Clip=Data->GetObjectField(TEXT("clips"))->GetObjectField(Id);
            for(int32 I=0;I<Clip->GetIntegerField(TEXT("frames"));++I) Needed.Add(ClipFramePath(Id,I));
        }
    }
    const TArray<TSharedPtr<FJsonValue>>* Props=nullptr;
    if(Beats[Beat]->TryGetArrayField(TEXT("props"),Props))
        for(const auto& Prop:*Props) Needed.Add(Prop->AsObject()->GetStringField(TEXT("image")));
    FString EvidenceImage;
    if(Beats[Beat]->TryGetStringField(TEXT("evidence_image"),EvidenceImage)) Needed.Add(EvidenceImage);
    const FString Scene = Beats[Beat]->GetStringField(TEXT("scene"));
    if (Music) Music->SelectBeat(Beat + 1);
    if (Stage)
    {
        Stage->ResetCharacters();Stage->SetScene(Scene);
        const TArray<TSharedPtr<FJsonValue>>* Furniture=nullptr;
        if(Beats[Beat]->TryGetArrayField(TEXT("support_boxes"),Furniture)) for(const auto& V:*Furniture)
        {
            const auto Box=V->AsObject();const auto& P=Box->GetArrayField(TEXT("position"));
            const auto& S=Box->GetArrayField(TEXT("size"));const auto& C=Box->GetArrayField(TEXT("color"));
            Stage->AddStoryBox({P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber()},
                {S[0]->AsNumber(),S[1]->AsNumber(),S[2]->AsNumber()},
                FLinearColor(C[0]->AsNumber(),C[1]->AsNumber(),C[2]->AsNumber()));
        }
    }
    FString Ref = Scene;
    if (Scene == TEXT("corridor") || Scene == TEXT("storage")) Ref = TEXT("dressing");
    if (Scene == TEXT("newstudio")) Ref = TEXT("studio");
    Needed.Add(TEXT("References/") + Ref + TEXT(".png"));
    for (auto It = Frames.CreateIterator(); It; ++It) if (!Needed.Contains(It.Key())) It.RemoveCurrent();
    for (const FString& Path : Needed) LoadFrame(Path);
    UpdateActors();
    UE_LOG(LogTemp, Display, TEXT("SuperstarStory: beat %d/%d %s"), Beat + 1, Beats.Num(), *Beats[Beat]->GetStringField(TEXT("title")));
}

void ASuperstarStoryHUD::Publish()
{
    if (!Beats.IsValidIndex(Beat) || Beats[Beat]->GetStringField(TEXT("evidence")).IsEmpty() || !bEvidencePlaced) return;
    bPublished = true;
    PublishedBeats.Add(Beat);
    bPlaying = true;
    Clock = Beats[Beat]->GetNumberField(TEXT("duration"));
    PublishedHold = 1.2f;
    UE_LOG(LogTemp, Display, TEXT("SuperstarStory: evidence published: %s"), *Beats[Beat]->GetStringField(TEXT("evidence")));
}

FVector2D ASuperstarStoryHUD::Pointer() const
{
    float X = -1, Y = -1;
    if (GetOwningPlayerController()) GetOwningPlayerController()->GetMousePosition(X, Y);
    return (FVector2D(X, Y) - Origin) / FMath::Max(Scale, .001f);
}

void ASuperstarStoryHUD::Tick(float Delta)
{
    Super::Tick(Delta);
    auto* PC = GetOwningPlayerController();
    if (!PC) return;
    if (Stage && PC->GetViewTarget() != Stage) PC->SetViewTarget(Stage);
    if (!Error.IsEmpty() || !Beats.IsValidIndex(Beat)) return;
    if(bLevelsSmoke) RunLevelsSmoke();
    if(TickLevelFlow()) return;
    if (PC->WasInputKeyJustPressed(EKeys::Escape) && PhotoPhase == ESuperstarPhotoPhase::Live)
    {
        if(bFullStoryDebug) UGameplayStatics::OpenLevel(this,TEXT("/Game/levels/MainMenu"),true);
        else ShowLevelSelect();
        return;
    }
    if (bTransportSmoke) { RunTransportSmoke(); if(TransportSmokeTicks<73 || TransportSmokeTicks>=205) return; }
    if (bRound2Smoke) {RunRound2Smoke();return;}
    if (bMotionSmoke) { RunMotionSmoke(); return; }
    if (bPhotoSmoke) { RunPhotoSmoke(); UpdateActors(); return; }
    if(PhotoPhase==ESuperstarPhotoPhase::Live && HandleTimelinePointer(Pointer(),PC->WasInputKeyJustPressed(EKeys::LeftMouseButton),PC->IsInputKeyDown(EKeys::LeftMouseButton),PC->WasInputKeyJustReleased(EKeys::LeftMouseButton)))
    {
        if(Music) Music->SetStoryPaused(true);
        UpdateActors();return;
    }
    if (TickPhotography(Delta)) { if(Sfx) Sfx->TransportMotion(0); return; }
    if (bMusicSmoke)
    {
        const double Elapsed = FPlatformTime::Seconds() - MusicSmokeStart;
        const double Times[] = {52., 54., 56., 58., 60., 64., 68., 72., 76., 80., 84., 86., 89.};
        if (MusicSmokeStep < UE_ARRAY_COUNT(Times) && Elapsed >= Times[MusicSmokeStep])
        {
            UE_LOG(LogTemp, Display, TEXT("SuperstarMusic smoke: step=%d elapsed=%.3f"), MusicSmokeStep, Elapsed);
            switch (MusicSmokeStep++)
            {
            case 0: Music->SetStoryPaused(true); break;
            case 1: Music->SetStoryPaused(false); break;
            case 2: case 3: Music->ToggleMute(); break;
            case 4: SelectBeat(4); break;
            case 5: SelectBeat(6); break;
            case 6: SelectBeat(19); break;
            case 7: SelectBeat(23); break;
            case 8: SelectBeat(32); break;
            case 9: SelectBeat(36); break;
            case 10: Music->DestroyComponent(); Music = nullptr; break;
            case 11:
                UAudioMixerBlueprintLibrary::StopRecordingOutput(this, EAudioRecordingExportType::WavFile,
                    TEXT("SuperstarMusic-Smoke"), FPaths::ProjectSavedDir() / TEXT("AudioValidation"));
                break;
            case 12:
                UE_LOG(LogTemp, Display, TEXT("SuperstarMusic smoke: sequence complete; validate recorded WAV before claiming PASS"));
                PC->ConsoleCommand(TEXT("quit"));
                break;
            }
        }
        return;
    }
    if (bSmoke)
    {
        ++SmokeTicks;
        if (SmokeTicks == 90)
        {
            FVector ViewLocation;
            FRotator ViewRotation;
            PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
            UE_LOG(LogTemp, Display, TEXT("Superstar camera: %s / %s target=%s"), *ViewLocation.ToString(), *ViewRotation.ToString(), *GetNameSafe(PC->GetViewTarget()));
            SelectBeat(SmokeBeat);
            Clock = FMath::Min(2.8, Beats[Beat]->GetNumberField(TEXT("duration")) - .2);
            if (!Beats[Beat]->GetStringField(TEXT("evidence")).IsEmpty())
            {
                Publish();
                check(!bPublished); // publishing requires a selected card
                bEvidencePlaced = true;
                Publish();
                check(bPublished);
            }
        }
        if (SmokeTicks == 100)
        {
            FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / (bReferenceView ? TEXT("Screenshots/SuperstarReferences") : TEXT("Screenshots/Superstar3D")) /
                FString::Printf(TEXT("Beat_%02d.png"), SmokeBeat + 1), true, false);
        }
        if (SmokeTicks >= 108)
        {
            ++SmokeBeat;
            SmokeTicks = 89;
            if (SmokeBeat >= Beats.Num())
            {
                UE_LOG(LogTemp, Display, TEXT("SuperstarStory smoke PASS: %d beats, all evidence gates exercised"), Beats.Num());
                PC->ConsoleCommand(TEXT("quit"));
            }
        }
        UpdateActors();
        return;
    }
    if (PC->WasInputKeyJustPressed(EKeys::SpaceBar) || PC->WasInputKeyJustPressed(EKeys::R) || PC->WasInputKeyJustPressed(EKeys::RightMouseButton)) { bPlaying=!bPlaying;PlaybackRate=bPlaying?1.f:0.f; }
    if (PC->WasInputKeyJustPressed(EKeys::M) && Music) Music->ToggleMute();
    if (PC->WasInputKeyJustPressed(EKeys::B)) bReferenceView = !bReferenceView;
    auto Jump=[this](int32 Index,FName Cue)
    {
        if(Beats.IsValidIndex(Index) && Index>=FirstBeat && Index<LastBeat) { if(Sfx) Sfx->PlayCue(Cue); SelectBeat(Index); }
    };
    if (PC->WasInputKeyJustPressed(EKeys::PageUp)) Jump(Beat - 1,TEXT("rewind"));
    if (PC->WasInputKeyJustPressed(EKeys::PageDown)) Jump(Beat + 1,TEXT("forward"));
    if (PC->WasInputKeyJustPressed(EKeys::Home)) { if(Sfx) Sfx->PlayCue(TEXT("rewind")); SeekSequence(0);PlaybackRate=0; }
    if (PC->WasInputKeyJustPressed(EKeys::End)) { if(Sfx) Sfx->PlayCue(TEXT("forward")); SeekSequence(SequenceDuration);bPlaying=false;PlaybackRate=0; }
    const FKey Chapters[] = {EKeys::F1, EKeys::F2, EKeys::F3, EKeys::F4};
    for (int32 C = 0; C < 4; ++C)
        if (PC->WasInputKeyJustPressed(Chapters[C]))
        {
            if(!bFullStoryDebug) {ChooseLevel(C);return;}
            for (int32 I = 0; I < Beats.Num(); ++I)
                if (Beats[I]->GetIntegerField(TEXT("chapter")) == C + 1) { Jump(I,TEXT("chapter")); break; }
        }
    const bool bGate = !Beats[Beat]->GetStringField(TEXT("evidence")).IsEmpty() && !bPublished;
    const auto P = Pointer();
    if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
    {
        if (Inside(P, 35, 655, 110, 40)) Jump(Beat - 1,TEXT("rewind"));
        else if (Inside(P, 155, 655, 110, 40)) bPlaying = !bPlaying;
        else if (Inside(P, 275, 655, 110, 40)) Jump(Beat + 1,TEXT("forward"));
        else if (Inside(P, 405, 655, 110, 40)) Jump(Beat,TEXT("rewind"));
        else if (Inside(P, 875, 655, 185, 40)) bReferenceView = !bReferenceView;
        else if (Inside(P, 1080, 655, 160, 40)) { if(bFullStoryDebug) UGameplayStatics::OpenLevel(this,TEXT("/Game/levels/MainMenu"),true);else ShowLevelSelect();return; }
        else if (bGate && Inside(P, 52, 423, 330, 75)) { bEvidencePlaced = true; bDragging = true; }
        else if (bGate && Inside(P, 995, 445, 185, 48)) Publish();
    }
    if (PC->WasInputKeyJustReleased(EKeys::LeftMouseButton) && bDragging)
    {
        // A click selects the evidence; dragging to the live window also selects it.
        bDragging = false;
    }
    if (bGate && PC->WasInputKeyJustPressed(EKeys::V)) { if (!bEvidencePlaced) bEvidencePlaced = true; else Publish(); }
    const bool bRewind=PC->IsInputKeyDown(EKeys::Left)||PC->IsInputKeyDown(EKeys::Z);
    const bool bForward=PC->IsInputKeyDown(EKeys::Right)||PC->IsInputKeyDown(EKeys::C);
    AdvanceSequence(Delta,(bForward?1.f:0.f)-(bRewind?1.f:0.f));
    // BGM is ambience, not reversed dialogue: pause it while shuttling the picture.
    if (Music) Music->SetStoryPaused(!bPlaying || bRewind || bForward);
    UpdateActors();
}

void ASuperstarStoryHUD::Rect(float X, float Y, float W, float H, FLinearColor Color)
{
    DrawRect(Color, Origin.X + X * Scale, Origin.Y + Y * Scale, W * Scale, H * Scale);
}
void ASuperstarStoryHUD::Label(const FString& Text, float X, float Y, int32 Size, FLinearColor Color)
{
    FitLabel(Text, X, Y, FMath::Max(1.f, 1260.f-X), Size*1.8f, Size, Color, false);
}

void ASuperstarStoryHUD::FitLabel(const FString& Source, float X, float Y, float W, float H, int32 Size, FLinearColor Color, bool bCenter)
{
    const FString Translated = GameLocalization::Translate(Source);
    FString Text = Translated;
    if (!Canvas || !StoryFont || Text.IsEmpty() || W <= 0 || H <= 0) return;
    // Measure the actual composite font, including Chinese fallback glyphs, in viewport pixels.
    // Scale and Origin are the same transform used by drawing and pointer hit testing.
    const auto Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    FSlateFontInfo Font(StoryFont, FMath::Max(8, FMath::RoundToInt(Size * Scale)));
    const bool bWrap=GameLocalization::IsEnglish() && !bCenter && W>400 && H>=30 && Size<=21;
    if(bWrap) Text=GameLocalization::WrapForWidth(Translated,Font,W*Scale);
    FVector2D Extent = Measure->Measure(Text, Font);
    const FVector2D Available(W * Scale, H * Scale);
    while (Font.Size > 8 && (Extent.X > Available.X || Extent.Y > Available.Y))
    {
        --Font.Size;
        if(bWrap) Text=GameLocalization::WrapForWidth(Translated,Font,W*Scale);
        Extent = Measure->Measure(Text, Font);
    }
    const float Fit = FMath::Min(1.f, FMath::Min(Available.X / FMath::Max(1., Extent.X), Available.Y / FMath::Max(1., Extent.Y)));
    Extent *= Fit;
    const FVector2D Padding(bCenter ? (Available.X - Extent.X) * .5 : 0., (Available.Y - Extent.Y) * .5);
    FCanvasTextItem Item(Origin + FVector2D(X,Y) * Scale + Padding, FText::FromString(Text), Font, Color);
    Item.Scale = FVector2D(Fit,Fit);
    Canvas->DrawItem(Item);
}

void ASuperstarStoryHUD::Button(const FString& Text, float X, float Y, float W, float H, int32 Size, FLinearColor Background, FLinearColor Color)
{
    Rect(X,Y,W,H,Background);
    FitLabel(Text,X+8,Y+5,W-16,H-10,Size,Color,true);
}

void ASuperstarStoryHUD::RunMotionSmoke()
{
    // Render three gait cycles at three camera stations. Deliberately seek away and back.
    static const int32 Samples[]={0,3,4,5,6,14,15,23,25,26,27,28,30,31,32,34,37,38};
    if(++MotionSmokeTicks<60) return;
    if(MotionSmokeStep>=UE_ARRAY_COUNT(Samples)*18)
    {
        UE_LOG(LogTemp,Display,TEXT("SuperstarMotion smoke PASS: 324 samples, 3 stations, direction, hand grip and reverse-seek checks"));
        GetOwningPlayerController()->ConsoleCommand(TEXT("quit"));return;
    }
    const int32 Phase=(MotionSmokeTicks-60)%4;
    if(Phase==0)
    {
        const int32 Target=Samples[MotionSmokeStep/18];
        if(Beat!=Target || MotionSmokeStep==0) SelectBeat(Target);
        Clock=(MotionSmokeStep%18)*.2f+.001f;
        ViewStation=TargetStation=((MotionSmokeStep%18)/6)-1;Stage->SetView(ViewStation,0,0,1);
        UpdateActors();
        const float SavedClock=Clock;
        TArray<FVector> Hands;
        for(int32 I=0;I<Beats[Beat]->GetArrayField(TEXT("actors")).Num();++I)
        {
            const auto A=Beats[Beat]->GetArrayField(TEXT("actors"))[I]->AsObject();
            Hands.Add(Stage->CharacterPixelWorld(I,ActorHand(A)));
            FString Walk;
            if(A->TryGetStringField(TEXT("walk_clip"),Walk) && Clock>=A->GetNumberField(TEXT("move_start")) && Clock<A->GetNumberField(TEXT("move_end")))
            {
                FString Id;int32 Frame;ResolveActorSample(A,Id,Frame);
                FString Facing=TEXT("right");Data->GetObjectField(TEXT("clips"))->GetObjectField(Id)->TryGetStringField(TEXT("screen_facing"),Facing);
                const bool Left=(Facing==TEXT("left"))!=ActorFlip(A,Id);
                checkf(Left==(A->GetNumberField(TEXT("to"))<A->GetNumberField(TEXT("x"))),TEXT("Actor walks backward"));
            }
        }
        Clock=Beats[Beat]->GetNumberField(TEXT("duration"));UpdateActors();Clock=SavedClock;UpdateActors();
        for(int32 I=0;I<Hands.Num();++I)
            checkf(FVector::Dist(Hands[I],Stage->CharacterPixelWorld(I,ActorHand(Beats[Beat]->GetArrayField(TEXT("actors"))[I]->AsObject())))<.001,TEXT("Non-deterministic actor pose"));
        const TArray<TSharedPtr<FJsonValue>>* Props=nullptr;
        if(Beats[Beat]->TryGetArrayField(TEXT("props"),Props)) for(int32 I=0;I<Props->Num();++I)
        {
            const auto P=(*Props)[I]->AsObject();double HolderIndex=0.;
            if(!P->TryGetNumberField(TEXT("attach_actor"),HolderIndex)) continue;
            double Transfer=0.;if(P->TryGetNumberField(TEXT("transfer_at"),Transfer)&&Clock>=Transfer) HolderIndex=P->GetNumberField(TEXT("transfer_actor"));
            const auto A=Beats[Beat]->GetArrayField(TEXT("actors"))[static_cast<int32>(HolderIndex)]->AsObject();
            const FVector2D Grip=PropGrip(P);
            const FVector GripError=Stage->PropPixelWorld(I,Grip)-Stage->CharacterPixelWorld(HolderIndex,ActorHand(A));
            checkf(GripError.Size()<.11,TEXT("Detached prop: %s error=%s"),*P->GetStringField(TEXT("image")),*GripError.ToString());
        }
    }
    if(Phase==2) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/SuperstarMotion")/
        FString::Printf(TEXT("Beat_%02d_%02d.png"),Beat+1,MotionSmokeStep%18),true,false);
    if(Phase==3) ++MotionSmokeStep;
}

// Render translated signage as UI panels over the reference art. Source PNGs stay intact.
// Normalized rectangles keep the overlay aligned with the image at every viewport size.
void ASuperstarStoryHUD::LocalizedArtOverlay(const FString& Path, float X, float Y, float W, float H)
{
    if (!GameLocalization::IsEnglish()) return;
    auto Panel = [&](float L,float T,float Width,float Height,const TCHAR* Text,FLinearColor BG,FLinearColor FG)
    {
        Rect(X+L*W,Y+T*H,Width*W,Height*H,BG);
        FitLabel(Text,X+L*W+2,Y+T*H+1,Width*W-4,Height*H-2,18,FG,true);
    };
    const FLinearColor Blue(.08f,.12f,.3f), White(.9f,.95f,1.f), Paper(.78f,.75f,.68f), Black(.025f,.025f,.03f);
    if(Path==TEXT("References/stage.png"))
    {
        Panel(.012f,.315f,.222f,.14f,TEXT("RISE TO STARDOM"),Blue,White);
        Panel(.39f,.325f,.20f,.122f,TEXT("RISE TO STARDOM"),Blue,White);
        Panel(.725f,.315f,.235f,.145f,TEXT("RISE TO STARDOM"),Blue,White);
    }
    else if(Path==TEXT("References/dressing.png"))
    {
        Panel(.229f,.208f,.089f,.093f,TEXT("RISE TO STARDOM"),Blue,White);
        Panel(.785f,.443f,.027f,.045f,TEXT("RISE"),Blue,White);
        Panel(.937f,.447f,.033f,.047f,TEXT("RISE"),Blue,White);
    }
    else if(Path==TEXT("References/newspaper.png"))
        Panel(.275f,.022f,.447f,.07f,TEXT("JUNFENG ENTERTAINMENT"),Paper,Black);
}

void ASuperstarStoryHUD::Background(const FString& Scene)
{
    if (!bReferenceView && Scene != TEXT("newspaper"))
    {
        if (PhotoPhase == ESuperstarPhotoPhase::Capturing) return;
        Rect(45, 115, 310, 30, Ink);
        FitLabel(bFullStoryDebug?TEXT("3D 布景初版  ·  B 对照参考图"):TEXT("拍摄现场  ·  B 对照参考图"), 55, 118, 290, 24, 16, Mint);
        if (Scene == TEXT("storage")) Label(TEXT("杂物间：李谋 · 苏瑶 / 提及小北"), 820, 180, 17);
        if (Scene == TEXT("stage")) Label(TEXT("扶 摇 直 上"), 550, 180, 25, Mint);
        return;
    }
    FString Ref = Scene;
    if (Scene == TEXT("corridor") || Scene == TEXT("storage")) Ref = TEXT("dressing");
    if (Scene == TEXT("newstudio")) Ref = TEXT("studio");
    auto* Texture = LoadFrame(TEXT("References/") + Ref + TEXT(".png"));
    Rect(35, 105, 1210, 410, FLinearColor(.12f, .15f, .22f));
    if (Texture)
    {
        const float Fit = FMath::Min(1210.f / Texture->GetSizeX(), 410.f / Texture->GetSizeY());
        const float W = Texture->GetSizeX() * Fit, H = Texture->GetSizeY() * Fit;
        DrawTexture(Texture, Origin.X + (640 - W / 2) * Scale, Origin.Y + (310 - H / 2) * Scale,
            W * Scale, H * Scale, 0, 0, 1, 1, FLinearColor::White, BLEND_Translucent);
        LocalizedArtOverlay(TEXT("References/")+Ref+TEXT(".png"),640-W/2,310-H/2,W,H);
    }
    if (Scene == TEXT("corridor"))
    {
        Rect(55, 155, 230, 185, Ink);
        FitLabel(TEXT("选手照片墙"), 65, 165, 210, 28, 18, Mint,true);
        for (int32 I = 0; I < 6; ++I)
        {
            Rect(78 + (I % 3) * 65, 205 + (I / 3) * 62, 48, 52, FLinearColor(.65f,.70f,.75f));
            Label(FString::FromInt(I + 1), 96 + (I % 3) * 65, 220 + (I / 3) * 62, 16, Ink);
        }
    }
    if (Scene == TEXT("storage"))
    {
        Rect(850, 150, 170, 350, Ink);
        FitLabel(TEXT("杂物间"), 858, 180, 154, 32, 18, Mint, true);
        FitLabel(TEXT("李谋 · 苏瑶"), 858, 240, 154, 32, 18, Mint, true);
        FitLabel(TEXT("提及小北"), 858, 285, 154, 32, 18, Mint, true);
    }
    if (Scene == TEXT("newspaper"))
    {
        // Composite over the three green photo slots; leave the newspaper masthead and columns visible.
        Rect(567, 153, 147, 94, Ink);
        Rect(567, 270, 147, 89, Ink);
        Rect(567, 384, 147, 94, Ink);
        FitLabel(TEXT("李谋"), 571, 177, 139, 26, 16, Mint, true);
        FitLabel(TEXT("被警方带走"), 571, 205, 139, 26, 16, Mint, true);
        FitLabel(TEXT("苏瑶"), 571, 294, 139, 26, 16, Mint, true);
        FitLabel(TEXT("被行业封杀"), 571, 322, 139, 26, 16, Mint, true);
        FitLabel(TEXT("《扶摇直上》"), 571, 407, 139, 26, 16, Mint, true);
        FitLabel(TEXT("换导重拍"), 571, 437, 139, 26, 16, Mint, true);
    }
}

FString ASuperstarStoryHUD::ClipFramePath(const FString& Id,int32 Frame) const
{
    const auto Clip=Data->GetObjectField(TEXT("clips"))->GetObjectField(Id);
    return Clip->GetStringField(TEXT("path"))/FString::Printf(TEXT("%s_%06d.png"),*Id,Frame+1);
}

void ASuperstarStoryHUD::ResolveActorSample(const TSharedPtr<FJsonObject>& Actor,FString& Id,int32& Frame) const
{
    Id=Actor->GetStringField(TEXT("clip"));
    double Delay=0.;Actor->TryGetNumberField(TEXT("delay"),Delay);
    const double ActorTime=FMath::Max(0.,static_cast<double>(Clock)-Delay);
    double LocalTime=ActorTime;
    const TArray<TSharedPtr<FJsonValue>>* Sequence=nullptr;
    if(Actor->TryGetArrayField(TEXT("sequence"),Sequence))
        for(const auto& Part:*Sequence)
        {
            const auto P=Part->AsObject();const double At=P->GetNumberField(TEXT("at"));
            if(ActorTime>=At) {Id=P->GetStringField(TEXT("clip"));LocalTime=ActorTime-At;}
        }
    double Start=0.,End=Beats[Beat]->GetNumberField(TEXT("duration"));
    Actor->TryGetNumberField(TEXT("move_start"),Start);Actor->TryGetNumberField(TEXT("move_end"),End);
    FString Walk;
    const bool bWalking=Actor->TryGetStringField(TEXT("walk_clip"),Walk) && Clock>=Start && Clock<End;
    if(bWalking) {Id=Walk;LocalTime=Clock-Start;}
    const auto Clip=Data->GetObjectField(TEXT("clips"))->GetObjectField(Id);
    const int32 Count=Clip->GetIntegerField(TEXT("frames"));
    const int32 Sample=FMath::FloorToInt(LocalTime*Clip->GetNumberField(TEXT("fps"))+.00001);
    Frame=Clip->GetBoolField(TEXT("loop"))?Sample%Count:FMath::Min(Sample,Count-1);
    double Hold=0.;if(!bWalking && Actor->TryGetNumberField(TEXT("hold_frame"),Hold)) Frame=FMath::Clamp(static_cast<int32>(Hold),0,Count-1);
}

float ASuperstarStoryHUD::ActorScreenX(const TSharedPtr<FJsonObject>& Actor) const
{
    double Start=0.,End=Beats[Beat]->GetNumberField(TEXT("duration"));
    Actor->TryGetNumberField(TEXT("move_start"),Start);Actor->TryGetNumberField(TEXT("move_end"),End);
    return FMath::Lerp(Actor->GetNumberField(TEXT("x")),Actor->GetNumberField(TEXT("to")),FMath::Clamp((Clock-Start)/FMath::Max(.01,End-Start),0.,1.))+ActorOffset(Actor).X;
}

bool ASuperstarStoryHUD::ActorFlip(const TSharedPtr<FJsonObject>& Actor,const FString& Id) const
{
    FString Walk;
    double Start=0,End=Beats[Beat]->GetNumberField(TEXT("duration"));
    Actor->TryGetNumberField(TEXT("move_start"),Start);Actor->TryGetNumberField(TEXT("move_end"),End);
    if(Actor->TryGetStringField(TEXT("walk_clip"),Walk) && Clock>=Start && Clock<End)
    {
        const bool bWantLeft=Actor->GetNumberField(TEXT("to"))<Actor->GetNumberField(TEXT("x"));
        FString Facing=TEXT("right");Data->GetObjectField(TEXT("clips"))->GetObjectField(Id)->TryGetStringField(TEXT("screen_facing"),Facing);
        return bWantLeft!=(Facing==TEXT("left"));
    }
    FString Desired,Facing=TEXT("right");
    if(Actor->TryGetStringField(TEXT("facing"),Desired))
    {
        Data->GetObjectField(TEXT("clips"))->GetObjectField(Id)->TryGetStringField(TEXT("screen_facing"),Facing);
        return (Desired==TEXT("left"))!=(Facing==TEXT("left"));
    }
    return Actor->GetBoolField(TEXT("flip"));
}

FVector2D ASuperstarStoryHUD::ActorHand(const TSharedPtr<FJsonObject>& Actor) const
{
    FString Id;int32 Frame;ResolveActorSample(Actor,Id,Frame);
    const TArray<TSharedPtr<FJsonValue>>* Points=nullptr;
    if(Data->GetObjectField(TEXT("clips"))->GetObjectField(Id)->TryGetArrayField(TEXT("hand_anchors"),Points) && Points->IsValidIndex(Frame))
    {
        const auto& XY=(*Points)[Frame]->AsArray();return FVector2D(XY[0]->AsNumber(),XY[1]->AsNumber());
    }
    if(Actor->TryGetArrayField(TEXT("hand"),Points)) return FVector2D((*Points)[0]->AsNumber(),(*Points)[1]->AsNumber());
    return FVector2D(380,345);
}

float ASuperstarStoryHUD::PropScreenX(const TSharedPtr<FJsonObject>& Prop) const
{
    double Index=0.;
    if(Prop->TryGetNumberField(TEXT("actor"),Index))
    {
        const auto& Actors=Beats[Beat]->GetArrayField(TEXT("actors"));
        if(Actors.IsValidIndex(static_cast<int32>(Index)))
        {
            const auto A=Actors[static_cast<int32>(Index)]->AsObject();
            return ActorScreenX(A)+Prop->GetNumberField(TEXT("offset_x"));
        }
    }
    return Prop->GetNumberField(TEXT("x"));
}

void ASuperstarStoryHUD::UpdateActors()
{
    if (!Stage || !Beats.IsValidIndex(Beat)) return;
    const auto Current = Beats[Beat];
    int32 Index = 0;
    for (const auto& Value : Current->GetArrayField(TEXT("actors")))
    {
        const auto Actor = Value->AsObject();
        FString Id;int32 Frame;ResolveActorSample(Actor,Id,Frame);
        auto* Texture = LoadFrame(ClipFramePath(Id,Frame));
        Stage->SetCharacter(Index++,Texture,ActorScreenX(Actor),ActorFlip(Actor,Id),ActorScale(Actor),ActorOffset(Actor).Y);
    }
    const TArray<TSharedPtr<FJsonValue>>* Props=nullptr;
    if(Current->TryGetArrayField(TEXT("props"),Props))
    {
        int32 PropIndex=0;
        for(const auto& Value:*Props)
        {
            const auto P=Value->AsObject();
            double HolderIndex=0.;
            if(P->TryGetNumberField(TEXT("attach_actor"),HolderIndex))
            {
                double Transfer=0.;if(P->TryGetNumberField(TEXT("transfer_at"),Transfer) && Clock>=Transfer) HolderIndex=P->GetNumberField(TEXT("transfer_actor"));
                const auto Actor=Current->GetArrayField(TEXT("actors"))[static_cast<int32>(HolderIndex)]->AsObject();
                const FVector2D Grip=PropGrip(P);
                double Angle=0.;P->TryGetNumberField(TEXT("angle"),Angle);
                Stage->SetAttachedProp(PropIndex++,LoadFrame(P->GetStringField(TEXT("image"))),HolderIndex,ActorHand(Actor),Grip,P->GetNumberField(TEXT("size")),Angle,PropMirrors(P));
                continue;
            }
            Stage->SetProp(PropIndex++,LoadFrame(P->GetStringField(TEXT("image"))),
                FVector((640.f-PropScreenX(P))*1400.f/1280.f,P->GetNumberField(TEXT("depth")),P->GetNumberField(TEXT("z"))),P->GetNumberField(TEXT("size")));
        }
    }
    const bool bHide=bReferenceView || Current->GetStringField(TEXT("scene"))==TEXT("newspaper");
    Stage->SetReferenceView(bHide);
    if(Props) for(int32 I=0;I<Props->Num();++I) Stage->SetPropVisible(I,!bHide && PropVisible((*Props)[I]->AsObject()));
}

void ASuperstarStoryHUD::DrawActors()
{
    const auto Current = Beats[Beat];
    const TArray<TSharedPtr<FJsonValue>>* Props=nullptr;
    if(bReferenceView&&Current->TryGetArrayField(TEXT("props"),Props))
        for(const auto& Value:*Props)
        {
            const auto P=Value->AsObject();if(!PropVisible(P)) continue;auto* Texture=LoadFrame(P->GetStringField(TEXT("image")));
            if(!Texture) continue;
            const float H=P->GetNumberField(TEXT("size"))*360.f/395.f,W=H*Texture->GetSizeX()/Texture->GetSizeY();
            double HolderIndex=0.;
            if(P->TryGetNumberField(TEXT("attach_actor"),HolderIndex))
            {
                double Transfer=0.;if(P->TryGetNumberField(TEXT("transfer_at"),Transfer) && Clock>=Transfer) HolderIndex=P->GetNumberField(TEXT("transfer_actor"));
                const auto Actor=Current->GetArrayField(TEXT("actors"))[static_cast<int32>(HolderIndex)]->AsObject();
                FString Id;int32 Frame;ResolveActorSample(Actor,Id,Frame);const bool bActorFlip=ActorFlip(Actor,Id);const bool bFlip=PropMirrors(P) && bActorFlip;
                FVector2D Hand=ActorHand(Actor);if(bActorFlip) Hand.X=640-Hand.X;
                const FVector2D Grip=PropGrip(P);
                const float U=bFlip?1-Grip.X:Grip.X;
                const FVector2D Pos(ActorScreenX(Actor)+(Hand.X-320)*360*ActorScale(Actor)/640-U*W,494-ActorOffset(Actor).Y*360/395+(Hand.Y-570)*360*ActorScale(Actor)/640-Grip.Y*H);
                double Angle=0.;P->TryGetNumberField(TEXT("angle"),Angle);
                DrawTexture(Texture,Origin.X+Pos.X*Scale,Origin.Y+Pos.Y*Scale,W*Scale,H*Scale,bFlip?1:0,0,bFlip?-1:1,1,FLinearColor::White,BLEND_Translucent,1,false,bFlip?-Angle:Angle,FVector2D(U,Grip.Y));
                continue;
            }
            DrawTexture(Texture,Origin.X+(PropScreenX(P)-W/2)*Scale,Origin.Y+(494-P->GetNumberField(TEXT("z"))*360/395-H/2)*Scale,
                W*Scale,H*Scale,0,0,1,1,FLinearColor::White,BLEND_Translucent);
        }
    int32 ActorIndex = 0;
    TMap<FString,FVector2D> DrawnRoleLabels;
    for (const auto& Value : Current->GetArrayField(TEXT("actors")))
    {
        const auto Actor = Value->AsObject();
        FString Id;int32 Frame;ResolveActorSample(Actor,Id,Frame);
        auto* Texture = LoadFrame(ClipFramePath(Id,Frame));
        const float X = ActorScreenX(Actor);
        const bool bFlip = ActorFlip(Actor,Id);
        const float Size=360.f*ActorScale(Actor);
        const float Floor=494.f-ActorOffset(Actor).Y*360.f/395.f;
        const int32 Index = ActorIndex++;
        if (Texture && bReferenceView)
            DrawTexture(Texture, Origin.X + (X - Size * .5f) * Scale, Origin.Y + (Floor - Size * .890625f) * Scale,
                Size * Scale, Size * Scale, bFlip ? 1.f : 0.f, 0, bFlip ? -1.f : 1.f, 1, FLinearColor::White, BLEND_Translucent);
        FVector2D HeadPixel(320,100);
        const TArray<TSharedPtr<FJsonValue>>* Anchors = nullptr;
        if (HeadAnchors.IsValid() && HeadAnchors->TryGetArrayField(Id, Anchors) && Anchors->IsValidIndex(Frame))
        {
            const auto& XY = (*Anchors)[Frame]->AsArray();
            HeadPixel = FVector2D(XY[0]->AsNumber(),XY[1]->AsNumber());
        }
        FVector2D Head(X + ((bFlip ? 640-HeadPixel.X : HeadPixel.X)-320)*Size/640.,
            Floor-Size*.890625+HeadPixel.Y*Size/640.);
        FVector2D Foot(X,Floor);
        bool bProjected = true;
        if (!bReferenceView && Stage)
        {
            FVector2D ScreenHead, ScreenFoot;
            bProjected = GetOwningPlayerController()->ProjectWorldLocationToScreen(Stage->CharacterPixelWorld(Index,HeadPixel),ScreenHead);
            GetOwningPlayerController()->ProjectWorldLocationToScreen(Stage->CharacterPixelWorld(Index,{320,570}),ScreenFoot);
            Head = (ScreenHead-Origin)/Scale;
            Foot = (ScreenFoot-Origin)/Scale;
        }
        if (PhotoPhase != ESuperstarPhotoPhase::Capturing && Inside(Foot,70,125,1140,380))
        {
            const FString Name=Actor->GetStringField(TEXT("name"));
            const FVector2D* Previous=DrawnRoleLabels.Find(Name);
            // Crowded extras with the same role share one readable label.
            if(!Previous || FVector2D::Distance(*Previous,Foot)>100)
            {
                FitLabel(Name, Foot.X-45, Foot.Y, 90, 24, 16, FLinearColor::White, true);
                DrawnRoleLabels.Add(Name,Foot);
            }
        }
        bool bEmotionPlaceholder = false;
        const TArray<TSharedPtr<FJsonValue>>* Emotions = nullptr;
        if (Actor->TryGetBoolField(TEXT("emotion_placeholder"), bEmotionPlaceholder) && bEmotionPlaceholder
            && Actor->TryGetArrayField(TEXT("emotions"), Emotions))
        {
            TSharedPtr<FJsonObject> ActiveEmotion;
            for (const auto& Emotion : *Emotions)
                if (Emotion->AsObject()->GetNumberField(TEXT("at")) <= Clock) ActiveEmotion = Emotion->AsObject();
            if (ActiveEmotion && bProjected && Inside(Head,65,165,1150,340))
            {
                if (auto* Badge = BackstageGetEmotionTexture(FName(*ActiveEmotion->GetStringField(TEXT("icon")))))
                    DrawTexture(Badge, Origin.X + (Head.X - 24) * Scale, Origin.Y + (Head.Y - 57) * Scale,
                        48 * Scale, 48 * Scale, 0, 0, 1, 1, FLinearColor::White, BLEND_Translucent);
                FitLabel(ActiveEmotion->GetStringField(TEXT("label")), Head.X-48, Head.Y-13, 96, 22, 13, Mint, true);
            }
        }
    }

}

void ASuperstarStoryHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas) return;
    Scale = FMath::Min(Canvas->SizeX / 1280.f, Canvas->SizeY / 720.f);
    Origin = FVector2D((Canvas->SizeX - 1280 * Scale) / 2, (Canvas->SizeY - 720 * Scale) / 2);
    if (bReferenceView || !Error.IsEmpty()) DrawRect(Ink, 0, 0, Canvas->SizeX, Canvas->SizeY);
    else
    {
        DrawRect(Ink, 0, 0, Canvas->SizeX, Origin.Y + 105 * Scale);
        DrawRect(Ink, 0, Origin.Y + 515 * Scale, Canvas->SizeX, Canvas->SizeY);
        DrawRect(Ink, 0, 0, Origin.X + 35 * Scale, Canvas->SizeY);
        DrawRect(Ink, Origin.X + 1245 * Scale, 0, Canvas->SizeX, Canvas->SizeY);
    }
    if (!Error.IsEmpty()) { Label(Error, 60, 200, 24); return; }
    if (!Beats.IsValidIndex(Beat)) return;
    if(LevelPhase!=ESuperstarLevelPhase::Playing) {DrawLevelFlow();return;}
    if (PhotoPhase != ESuperstarPhotoPhase::Live && PhotoPhase != ESuperstarPhotoPhase::Capturing)
    {
        DrawPhotography();
        return;
    }
    const auto Current = Beats[Beat];
    FitLabel(bFullStoryDebug?TEXT("超级巨星  /  剧情预演"):FString::Printf(TEXT("超级巨星  /  %s"),*Levels[LevelIndex]->GetStringField(TEXT("title"))),35,26,780,44,28,Mint);
    Label(FString::Printf(TEXT("第 %d 关   %02d / %02d   ·   5 FPS"), Current->GetIntegerField(TEXT("chapter")), Beat - FirstBeat + 1, LastBeat-FirstBeat), 850, 35, 20);
    FitLabel(Current->GetStringField(TEXT("title")), 35, 73, 780, 30, 21);
    if (Music) Label(Music->Status(), 850, 77, 14, Mint);
    Background(Current->GetStringField(TEXT("scene")));
    DrawActors();
    if (PhotoPhase == ESuperstarPhotoPhase::Capturing) return;
    FitLabel(Current->GetStringField(TEXT("description")), 35, 530, 1210, 34, 18);
    FitLabel(bFullStoryDebug?Current->GetStringField(TEXT("note")):TEXT("任务：")+Levels[LevelIndex]->GetStringField(TEXT("objective")), 35, 568, 1210, 26, 15, FLinearColor(.9f,.72f,.42f));
    const FString Evidence = Current->GetStringField(TEXT("evidence"));
    if (!Evidence.IsEmpty())
    {
        Rect(45, 416, 1190, 95, Ink);
        FString EvidencePath;
        if(Current->TryGetStringField(TEXT("evidence_image"),EvidencePath))
        {
            if(auto* Image=LoadFrame(EvidencePath)) DrawTexture(Image,Origin.X+60*Scale,Origin.Y+428*Scale,128*Scale,72*Scale,0,0,1,1,FLinearColor::White,BLEND_Opaque);
            Label(TEXT("剧情证据图"),205,432,17,Mint);
            FitLabel(Evidence,205,462,235,32,16);
        }
        else
        {
            Label(TEXT("证据卡（原图待补）"), 60, 425, 17, Mint);
            FitLabel(Evidence, 60, 457, 375, 36, 19);
        }
        Rect(450, 435, 490, 62, FLinearColor(.08f,.16f,.18f));
        FitLabel(bPublished ? TEXT("已发送到直播") : (bEvidencePlaced ? Evidence : TEXT("拖入或点击左侧证据卡")), 465, 443, 460, 46, 18, Mint, true);
        Button(bPublished ? TEXT("已发送") : TEXT("发送 / V"), 995, 445, 185, 48, 18, bEvidencePlaced ? FLinearColor(.13f,.42f,.35f) : FLinearColor(.15f,.17f,.20f));
        if (bDragging) { const auto P = Pointer(); Label(Evidence, P.X + 14, P.Y - 28, 18, Mint); }
    }
    DrawTransport();
    const FString Buttons[] = {TEXT("上一段 PgUp"), bPlaying ? TEXT("暂停 空格") : TEXT("播放 空格"), TEXT("下一段 PgDn"), TEXT("本段重播")};
    for (int32 I = 0; I < 4; ++I) { const int32 X = I == 3 ? 405 : 35 + I * 120; Button(Buttons[I], X, 655, 110, 40, 16, FLinearColor(.10f,.17f,.21f)); }
    FitLabel(TEXT("F1—F4 任务单 · Tab 相册"), 535, 655, 320, 40, 16, Mint, true);
    FitLabel(FString::Printf(TEXT("机位 %d · WASD 构图 · Q/E/滚轮变焦 · Enter 拍照 · 已存 %d/6"),CameraStation+1,Photos.Num()),35,595,1210,20,13,Mint);
    Button(bReferenceView ? TEXT("B 切换 3D 布景") : TEXT("B 查看参考图片"), 875, 655, 185, 40, 16, FLinearColor(.10f,.17f,.21f));
    Button(bFullStoryDebug?TEXT("返回主菜单 ESC"):TEXT("返回选关 ESC"), 1080, 655, 160, 40, 16, FLinearColor(.10f,.17f,.21f));
}
