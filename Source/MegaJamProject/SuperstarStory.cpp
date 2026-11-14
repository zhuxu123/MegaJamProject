#include "SuperstarStory.h"
#include "SuperstarStage.h"
#include "SuperstarMusic.h"
#include "BackstageArt.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Engine/World.h"
#include "Core/AngelscriptBinds.h"
#include "CanvasItem.h"
#include "Dom/JsonObject.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
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
    bSmoke = FParse::Param(FCommandLine::Get(), TEXT("SuperstarSmoke")) || FParse::Param(FCommandLine::Get(), TEXT("SuperstarMenuSmoke"));
    bReferenceView = FParse::Param(FCommandLine::Get(), TEXT("SuperstarReferences"));
    bPhotoSmoke = FParse::Param(FCommandLine::Get(), TEXT("SuperstarPhotoSmoke"));
    Stage = GetWorld()->SpawnActor<ASuperstarStage>();
    if (Stage) GetOwningPlayerController()->SetViewTarget(Stage);
    SelectBeat(0);
    bMusicSmoke = FParse::Param(FCommandLine::Get(), TEXT("SuperstarMusicSmoke"));
    if (bMusicSmoke)
    {
        MusicSmokeStart = FPlatformTime::Seconds();
        UAudioMixerBlueprintLibrary::StartRecordingOutput(this, 90.f);
        UE_LOG(LogTemp, Display, TEXT("SuperstarMusic smoke: recording master output; first cue loops after 48 seconds"));
    }
    UE_LOG(LogTemp, Display, TEXT("SuperstarStory: loaded %d beats, %d clips at 5 fps"), Beats.Num(), Clips->Values.Num());
}

UTexture2D* ASuperstarStoryHUD::LoadFrame(const FString& Path)
{
    if (const auto* Found = Frames.Find(Path)) return Found->Get();
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *(FPaths::ProjectContentDir() / TEXT("SuperstarStory") / Path)))
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
    if (!Beats.IsValidIndex(Index)) return;
    Beat = Index;
    Clock = 0;
    bEvidencePlaced = bDragging = bPublished = false;
    PublishedHold = 0.f;
    // Drop only frames no longer needed. Shared idle clips survive adjacent shots.
    TSet<FString> Needed;
    for (const auto& Value : Beats[Beat]->GetArrayField(TEXT("actors")))
    {
        const FString Id = Value->AsObject()->GetStringField(TEXT("clip"));
        const auto Clip = Data->GetObjectField(TEXT("clips"))->GetObjectField(Id);
        for (int32 I = 1; I <= Clip->GetIntegerField(TEXT("frames")); ++I)
            Needed.Add(FString::Printf(TEXT("Animations/%s/%s_%06d.png"), *Id, *Id, I));
    }
    const FString Scene = Beats[Beat]->GetStringField(TEXT("scene"));
    if (Music) Music->SelectBeat(Beat + 1);
    if (Stage) { Stage->ResetCharacters(); Stage->SetScene(Scene); }
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
    if (PC->WasInputKeyJustPressed(EKeys::Escape) && PhotoPhase == ESuperstarPhotoPhase::Live)
    {
        UGameplayStatics::OpenLevel(this, TEXT("/Game/levels/MainMenu"), true);
        return;
    }
    if (!Error.IsEmpty() || !Beats.IsValidIndex(Beat)) return;
    if (bPhotoSmoke) { RunPhotoSmoke(); UpdateActors(); return; }
    if (TickPhotography(Delta)) return;
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
    if (PC->WasInputKeyJustPressed(EKeys::SpaceBar)) bPlaying = !bPlaying;
    if (PC->WasInputKeyJustPressed(EKeys::M) && Music) Music->ToggleMute();
    if (PC->WasInputKeyJustPressed(EKeys::B)) bReferenceView = !bReferenceView;
    if (PC->WasInputKeyJustPressed(EKeys::Left)) SelectBeat(Beat - 1);
    if (PC->WasInputKeyJustPressed(EKeys::Right)) SelectBeat(Beat + 1);
    if (PC->WasInputKeyJustPressed(EKeys::R)) SelectBeat(Beat);
    const FKey Chapters[] = {EKeys::F1, EKeys::F2, EKeys::F3, EKeys::F4};
    for (int32 C = 0; C < 4; ++C)
        if (PC->WasInputKeyJustPressed(Chapters[C]))
            for (int32 I = 0; I < Beats.Num(); ++I)
                if (Beats[I]->GetIntegerField(TEXT("chapter")) == C + 1) { SelectBeat(I); break; }
    const auto Current = Beats[Beat];
    const bool bGate = !Current->GetStringField(TEXT("evidence")).IsEmpty() && !bPublished;
    const auto P = Pointer();
    if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
    {
        if (Inside(P, 35, 655, 110, 40)) SelectBeat(Beat - 1);
        else if (Inside(P, 155, 655, 110, 40)) bPlaying = !bPlaying;
        else if (Inside(P, 275, 655, 110, 40)) SelectBeat(Beat + 1);
        else if (Inside(P, 405, 655, 110, 40)) SelectBeat(Beat);
        else if (Inside(P, 875, 655, 185, 40)) bReferenceView = !bReferenceView;
        else if (Inside(P, 1080, 655, 160, 40)) { UGameplayStatics::OpenLevel(this, TEXT("/Game/levels/MainMenu"), true); return; }
        else if (bGate && Inside(P, 52, 423, 330, 75)) { bEvidencePlaced = true; bDragging = true; }
        else if (bGate && Inside(P, 995, 445, 185, 48)) Publish();
        else if (Inside(P, 35, 618, 1210, 16)) { Clock = FMath::Clamp((P.X - 35) / 1210., 0., .999) * Current->GetNumberField(TEXT("duration")); bPlaying = false; }
    }
    if (PC->WasInputKeyJustReleased(EKeys::LeftMouseButton) && bDragging)
    {
        // A click selects the evidence; dragging to the live window also selects it.
        bDragging = false;
    }
    if (bGate && PC->WasInputKeyJustPressed(EKeys::V)) { if (!bEvidencePlaced) bEvidencePlaced = true; else Publish(); }
    if (bPlaying && bGate)
    {
        // Let the witnesses enter before waiting for the evidence; never skip the gate.
        Clock = FMath::Min(Clock + Delta, static_cast<float>(Beats[Beat]->GetNumberField(TEXT("duration"))));
    }
    else if (bPlaying && PublishedHold > 0.f)
    {
        PublishedHold -= Delta;
    }
    else if (bPlaying)
    {
        Clock += Delta;
        if (Clock >= Beats[Beat]->GetNumberField(TEXT("duration")))
        {
            if (Beat + 1 < Beats.Num()) SelectBeat(Beat + 1);
            else { Clock = Beats[Beat]->GetNumberField(TEXT("duration")); bPlaying = false; }
        }
    }
    if (Music) Music->SetStoryPaused(!bPlaying);
    UpdateActors();
}

void ASuperstarStoryHUD::Rect(float X, float Y, float W, float H, FLinearColor Color)
{
    DrawRect(Color, Origin.X + X * Scale, Origin.Y + Y * Scale, W * Scale, H * Scale);
}
void ASuperstarStoryHUD::Label(const FString& Text, float X, float Y, int32 Size, FLinearColor Color)
{
    if (!Canvas || !StoryFont) return;
    FCanvasTextItem Item(Origin + FVector2D(X, Y) * Scale, FText::FromString(Text), FSlateFontInfo(StoryFont, FMath::Max(8, FMath::RoundToInt(Size * Scale))), Color);
    Canvas->DrawItem(Item);
}

void ASuperstarStoryHUD::Background(const FString& Scene)
{
    if (!bReferenceView && Scene != TEXT("newspaper"))
    {
        if (PhotoPhase == ESuperstarPhotoPhase::Capturing) return;
        Rect(45, 115, 310, 30, Ink);
        Label(TEXT("3D 布景初版  ·  B 对照参考图"), 55, 120, 16, Mint);
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
    }
    if (Scene == TEXT("corridor"))
    {
        Rect(55, 155, 230, 185, Ink);
        Label(TEXT("选手照片墙"), 80, 165, 18, Mint);
        for (int32 I = 0; I < 6; ++I)
        {
            Rect(78 + (I % 3) * 65, 205 + (I / 3) * 62, 48, 52, FLinearColor(.65f,.70f,.75f));
            Label(FString::FromInt(I + 1), 96 + (I % 3) * 65, 220 + (I / 3) * 62, 16, Ink);
        }
    }
    if (Scene == TEXT("storage"))
    {
        Rect(850, 150, 170, 350, Ink);
        Label(TEXT("杂物间"), 892, 180, 20, Mint);
        Label(TEXT("李谋 · 苏瑶"), 874, 240, 18);
        Label(TEXT("提及小北"), 885, 285, 18);
    }
    if (Scene == TEXT("newspaper"))
    {
        // Composite over the three green photo slots; leave the newspaper masthead and columns visible.
        Rect(567, 153, 147, 94, Ink);
        Rect(567, 270, 147, 89, Ink);
        Rect(567, 384, 147, 94, Ink);
        Label(TEXT("李谋"), 619, 177, 16, Mint);
        Label(TEXT("被警方带走"), 588, 205, 16, Mint);
        Label(TEXT("苏瑶"), 619, 294, 16, Mint);
        Label(TEXT("被行业封杀"), 588, 322, 16, Mint);
        Label(TEXT("《扶摇直上》"), 580, 407, 16, Mint);
        Label(TEXT("换导重拍"), 601, 437, 16, Mint);
    }
}

void ASuperstarStoryHUD::UpdateActors()
{
    if (!Stage || !Beats.IsValidIndex(Beat)) return;
    const auto Current = Beats[Beat];
    int32 Index = 0;
    for (const auto& Value : Current->GetArrayField(TEXT("actors")))
    {
        const auto Actor = Value->AsObject();
        const FString Id = Actor->GetStringField(TEXT("clip"));
        const auto Clip = Data->GetObjectField(TEXT("clips"))->GetObjectField(Id);
        const int32 Count = Clip->GetIntegerField(TEXT("frames"));
        const int32 Sample = FMath::FloorToInt(FMath::Max(0.f, Clock)*5.f);
        const int32 Frame = Clip->GetBoolField(TEXT("loop")) ? Sample%Count : FMath::Min(Sample,Count-1);
        auto* Texture = LoadFrame(FString::Printf(TEXT("Animations/%s/%s_%06d.png"), *Id,*Id,Frame+1));
        const float X = FMath::Lerp(Actor->GetNumberField(TEXT("x")),Actor->GetNumberField(TEXT("to")),
            FMath::Clamp(Clock/Current->GetNumberField(TEXT("duration")),0.,1.));
        Stage->SetCharacter(Index++,Texture,X,Actor->GetBoolField(TEXT("flip")));
    }
    Stage->SetReferenceView(bReferenceView || Current->GetStringField(TEXT("scene")) == TEXT("newspaper"));
}

void ASuperstarStoryHUD::DrawActors()
{
    const auto Current = Beats[Beat];
    int32 ActorIndex = 0;
    for (const auto& Value : Current->GetArrayField(TEXT("actors")))
    {
        const auto Actor = Value->AsObject();
        const FString Id = Actor->GetStringField(TEXT("clip"));
        const auto Clip = Data->GetObjectField(TEXT("clips"))->GetObjectField(Id);
        const int32 Count = Clip->GetIntegerField(TEXT("frames"));
        const int32 Sample = FMath::FloorToInt(FMath::Max(0.f, Clock) * 5.f);
        const int32 Frame = Clip->GetBoolField(TEXT("loop")) ? Sample % Count : FMath::Min(Sample, Count - 1);
        auto* Texture = LoadFrame(FString::Printf(TEXT("Animations/%s/%s_%06d.png"), *Id, *Id, Frame + 1));
        const float Progress = FMath::Clamp(Clock / Current->GetNumberField(TEXT("duration")), 0., 1.);
        const float X = FMath::Lerp(Actor->GetNumberField(TEXT("x")), Actor->GetNumberField(TEXT("to")), Progress);
        const bool bFlip = Actor->GetBoolField(TEXT("flip"));
        constexpr float Size = 360.f;
        const int32 Index = ActorIndex++;
        if (Texture && bReferenceView)
            DrawTexture(Texture, Origin.X + (X - Size * .5f) * Scale, Origin.Y + (494 - Size * .890625f) * Scale,
                Size * Scale, Size * Scale, bFlip ? 1.f : 0.f, 0, bFlip ? -1.f : 1.f, 1, FLinearColor::White, BLEND_Translucent);
        FVector2D HeadPixel(320,100);
        const TArray<TSharedPtr<FJsonValue>>* Anchors = nullptr;
        if (HeadAnchors.IsValid() && HeadAnchors->TryGetArrayField(Id, Anchors) && Anchors->IsValidIndex(Frame))
        {
            const auto& XY = (*Anchors)[Frame]->AsArray();
            HeadPixel = FVector2D(XY[0]->AsNumber(),XY[1]->AsNumber());
        }
        FVector2D Head(X + ((bFlip ? 640-HeadPixel.X : HeadPixel.X)-320)*Size/640.,
            494-Size*.890625+HeadPixel.Y*Size/640.);
        FVector2D Foot(X,494);
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
            Label(Actor->GetStringField(TEXT("name")), Foot.X-45, Foot.Y, 16);
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
                Label(ActiveEmotion->GetStringField(TEXT("label")), Head.X-30, Head.Y-13, 13, Mint);
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
    if (PhotoPhase != ESuperstarPhotoPhase::Live && PhotoPhase != ESuperstarPhotoPhase::Capturing)
    {
        DrawPhotography();
        return;
    }
    const auto Current = Beats[Beat];
    Label(TEXT("超级巨星  /  剧情预演"), 35, 26, 28, Mint);
    Label(FString::Printf(TEXT("第 %d 幕   %02d / %02d   ·   5 FPS"), Current->GetIntegerField(TEXT("chapter")), Beat + 1, Beats.Num()), 850, 35, 20);
    Label(Current->GetStringField(TEXT("title")), 35, 73, 21);
    if (Music) Label(Music->Status(), 850, 77, 14, Mint);
    Background(Current->GetStringField(TEXT("scene")));
    DrawActors();
    if (PhotoPhase == ESuperstarPhotoPhase::Capturing) return;
    Label(Current->GetStringField(TEXT("description")), 35, 535, 18);
    Label(Current->GetStringField(TEXT("note")), 35, 573, 15, FLinearColor(.9f,.72f,.42f));
    const FString Evidence = Current->GetStringField(TEXT("evidence"));
    if (!Evidence.IsEmpty())
    {
        Rect(45, 416, 1190, 95, Ink);
        Label(TEXT("证据卡（原图待补）"), 60, 425, 17, Mint);
        Label(Evidence, 60, 457, 19);
        Rect(450, 435, 490, 62, FLinearColor(.08f,.16f,.18f));
        Label(bPublished ? TEXT("已发送到直播") : (bEvidencePlaced ? Evidence : TEXT("拖入或点击左侧证据卡")), 470, 454, 18, Mint);
        Rect(995, 445, 185, 48, bEvidencePlaced ? FLinearColor(.13f,.42f,.35f) : FLinearColor(.15f,.17f,.20f));
        Label(bPublished ? TEXT("已发送") : TEXT("发送 / V"), 1018, 458, 18);
        if (bDragging) { const auto P = Pointer(); Label(Evidence, P.X + 14, P.Y - 28, 18, Mint); }
    }
    Rect(35, 618, 1210, 8, FLinearColor(.14f,.19f,.24f));
    Rect(35, 618, 1210 * FMath::Clamp(Clock / Current->GetNumberField(TEXT("duration")), 0., 1.), 8, Mint);
    const FString Buttons[] = {TEXT("← 上一段"), bPlaying ? TEXT("暂停 空格") : TEXT("播放 空格"), TEXT("下一段 →"), TEXT("重播 R")};
    for (int32 I = 0; I < 4; ++I) { const int32 X = I == 3 ? 405 : 35 + I * 120; Rect(X, 655, 110, 40, FLinearColor(.10f,.17f,.21f)); Label(Buttons[I], X + 8, 666, 16); }
    Label(TEXT("F1—F4 跳幕 · Tab 相册"), 550, 669, 16, Mint);
    Label(FString::Printf(TEXT("机位 %d · WASD 构图 · Q/E/滚轮变焦 · Enter 拍照 · 已存 %d/6"),CameraStation+1,Photos.Num()),35,598,13,Mint);
    Rect(875, 655, 185, 40, FLinearColor(.10f,.17f,.21f));
    Label(bReferenceView ? TEXT("B 切换 3D 布景") : TEXT("B 查看参考图片"), 885, 666, 16);
    Rect(1080, 655, 160, 40, FLinearColor(.10f,.17f,.21f));
    Label(TEXT("返回主菜单 ESC"), 1090, 666, 16);
}
