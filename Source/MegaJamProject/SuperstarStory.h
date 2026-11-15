#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "SuperstarStory.generated.h"

class FJsonObject;
class UTexture2D;
class UFont;
class ASuperstarStage;
class USuperstarMusic;
class UBackstageSfx;

USTRUCT()
struct FSuperstarPhoto
{
    GENERATED_BODY()
    UPROPERTY(Transient) TObjectPtr<UTexture2D> Texture;
    TArray<FColor> Pixels;
    int32 Width = 0, Height = 0, Beat = 0, Station = 0;
    float Time = 0.f, Pan = 0.f, Tilt = 0.f, Zoom = 1.f;
    float CropZoom = 1.f, CropX = .5f, CropY = .5f;
    FString Title;
    FString File;
};

USTRUCT()
struct FSuperstarEdition
{
    GENERATED_BODY()
    int32 Level = -1;
    FString Headline, Path;
    UPROPERTY(Transient) TArray<FSuperstarPhoto> Shots;
};

enum class ESuperstarLevelPhase : uint8 { Select, Briefing, Playing, Newspaper, Archive, Finale };

enum class ESuperstarPhotoPhase : uint8 { Live, Capturing, Confirm, Edit, Preview, Results };

UCLASS()
class MEGAJAMPROJECT_API ASuperstarStoryController : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
};

// Separate mode: never spawns or changes the existing Backstage stage.
UCLASS()
class MEGAJAMPROJECT_API ASuperstarStoryGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ASuperstarStoryGameMode();
};

UCLASS()
class MEGAJAMPROJECT_API ASuperstarStoryHUD : public AHUD
{
    GENERATED_BODY()
public:
    ASuperstarStoryHUD();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void DrawHUD() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    TSharedPtr<FJsonObject> Data;
    TSharedPtr<FJsonObject> HeadAnchors;
    TArray<TSharedPtr<FJsonObject>> Beats;
    TArray<TSharedPtr<FJsonObject>> Levels;
    ESuperstarLevelPhase LevelPhase = ESuperstarLevelPhase::Select;
    static constexpr float NewspaperRevealDuration = 1.4f;
    float NewspaperRevealTime = NewspaperRevealDuration;
    int32 LevelIndex = -1, FirstBeat = 0, LastBeat = 0;
    float TimelineOffset = 0.f, FullDuration = 0.f;
    bool bFullStoryDebug = false;
    bool bLevelsSmoke = false;
    int32 LevelsSmokeTicks = 0, LevelsSmokeStep = 0, LevelsSmokeIndex = 0;
    UPROPERTY(Transient) TMap<FString, TObjectPtr<UTexture2D>> Frames;
    UPROPERTY(Transient) TObjectPtr<UFont> StoryFont;
    UPROPERTY(Transient) TObjectPtr<ASuperstarStage> Stage;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USuperstarMusic> Music;
    UPROPERTY(Transient) TObjectPtr<UBackstageSfx> Sfx;
    bool bReferenceView = false;
    int32 Beat = 0;
    float Clock = 0.f;
    bool bPlaying = true;
    float PlaybackRate = 1.f;
    bool bTimelineDragging = false;
    TArray<float> BeatStarts;
    float SequenceDuration = 0.f;
    TSet<int32> PublishedBeats;
    bool bTransportSmoke = false;
    int32 TransportSmokeTicks = 0;
    float TransportInputTime = 0.f;
    bool bEvidencePlaced = false;
    bool bDragging = false;
    bool bPublished = false;
    float PublishedHold = 0.f;
    int32 SmokeTicks = 0;
    int32 SmokeBeat = 0;
    bool bSmoke = false;
    bool bMusicSmoke = false;
    double MusicSmokeStart = 0.;
    int32 MusicSmokeStep = 0;
    FString Error;
    float Scale = 1.f;
    FVector2D Origin = FVector2D::ZeroVector;
    float ViewPan = 0.f, ViewTilt = 0.f, ViewZoom = 1.f, ViewStation = 0.f;
    float TargetPan = 0.f, TargetTilt = 0.f, TargetZoom = 1.f, TargetStation = 0.f;
    int32 CameraStation = 0;
    bool bCameraDragging = false;
    FVector2D LastDrag;
    ESuperstarPhotoPhase PhotoPhase = ESuperstarPhotoPhase::Live;
    UPROPERTY(Transient) TArray<FSuperstarPhoto> Photos;
    UPROPERTY(Transient) FSuperstarPhoto PendingPhoto;
    TArray<int32> PhotoSlots;
    int32 EditSlot = 0, Headline = 0;
    float PreviewClock = 0.f;
    bool bPublishPhotos = false;
    FDelegateHandle ScreenshotHandle;
    double CaptureStart = 0.;
    FString PhotoSession;
    FString PhotoNotice;
    bool bPhotoSmoke = false;
    bool bMotionSmoke = false;
    bool bRound2Smoke = false;
    int32 Round2SmokeTicks = 0, Round2SmokeStep = 0;
    TSet<FString> Round2Seen;
    int32 MotionSmokeStep = 0, MotionSmokeTicks = 0;
    int32 PhotoSmokeTicks = 0, PhotoSmokeStep = 0;
    float UITime=0, CameraNoticeUntil=0, PhotoArrivalTime=-10, SavedPulseUntil=0, SwapStart=-10, EvidencePulseUntil=0, HintUntil=0;
    FString CameraNotice, HintText;
    TSet<FString> SeenHints;
    TArray<int32> SwapFrom;
    UPROPERTY(Transient) TArray<FSuperstarEdition> Editions;
    int32 ArchiveLevel=0;
    ESuperstarLevelPhase ArchiveReturn=ESuperstarLevelPhase::Select;
    bool bCropDragging=false;
    FVector2D CropDragStart, CropCenterStart;
    void PresentCamera(const FString& Text);
    void ShowHint(const FString& Id, const FString& Text);
    void DrawCameraOverlay();
    void DrawHint();
    void SwapPhotos(int32 A,int32 B);
    void DrawCropGuide(const FSuperstarPhoto& Photo,float X,float Y,float W);
    bool HandleCropPointer(FVector2D Position,bool bPressed,bool bDown,float Wheel);
    bool ReadEdition(const FString& Path);
    void LoadEditions();
    void OpenArchive();
    void DrawCollection();
    bool TickCollection();
    void DrawEdition(const FSuperstarEdition& Edition,float X,float Y,float W);

    bool LoadLevels();
    void ChooseLevel(int32 Index);
    void StartLevel();
    void ShowLevelSelect();
    void ContinueLevel();
    bool TickLevelFlow();
    void DrawLevelFlow();
    void DrawNewspaper();
    void RunLevelsSmoke();
    FString PublicationHeadline() const;
    bool CanPublishLevel();
    void SelectBeat(int32 Index);
    void Publish();
    void Rect(float X, float Y, float W, float H, FLinearColor Color);
    void Label(const FString& Text, float X, float Y, int32 Size = 18, FLinearColor Color = FLinearColor::White);
    void FitLabel(const FString& Text, float X, float Y, float W, float H, int32 Size = 18, FLinearColor Color = FLinearColor::White, bool bCenter = false);
    void Button(const FString& Text, float X, float Y, float W, float H, int32 Size, FLinearColor Background, FLinearColor Color = FLinearColor::White);
    void LocalizedArtOverlay(const FString& Path, float X, float Y, float W, float H);
    void Background(const FString& Scene);
    void DrawActors();
    void UpdateActors();
    void ResolveActorSample(const TSharedPtr<FJsonObject>& Actor, FString& Id, int32& Frame) const;
    FString ClipFramePath(const FString& Id, int32 Frame) const;
    float PropScreenX(const TSharedPtr<FJsonObject>& Prop) const;
    float ActorScreenX(const TSharedPtr<FJsonObject>& Actor) const;
    bool ActorFlip(const TSharedPtr<FJsonObject>& Actor, const FString& Id) const;
    FVector2D ActorHand(const TSharedPtr<FJsonObject>& Actor) const;
    void RunMotionSmoke();
    void RunRound2Smoke();
    float ActorScale(const TSharedPtr<FJsonObject>& Actor) const;
    FVector2D ActorOffset(const TSharedPtr<FJsonObject>& Actor) const;
    bool PropVisible(const TSharedPtr<FJsonObject>& Prop) const;
    FVector2D PropGrip(const TSharedPtr<FJsonObject>& Prop) const;
    bool PropMirrors(const TSharedPtr<FJsonObject>& Prop) const;
    float SequenceTime() const;
    void SeekSequence(float Time);
    void AdvanceSequence(float Delta, float ScrubAxis);
    bool HandleTimelinePointer(FVector2D Position, bool bPressed, bool bDown, bool bReleased);
    void DrawTransport();
    void RunTransportSmoke();
    void UpdateCamera(float Delta);
    void TakePhoto();
    void OnPhotoCaptured(int32 Width, int32 Height, const TArray<FColor>& Pixels);
    void AcceptPhoto();
    void RejectPhoto(bool bResumePlayback = false);
    void OpenAlbum();
    void TogglePhoto(int32 Index);
    void StartPhotoPreview(bool bPublish);
    void SavePublication();
    bool TickPhotography(float Delta);
    void DrawPhotography();
    void DrawViewfinder();
    void DrawRecentPhotos();
    void DrawPhoto(const FSuperstarPhoto& Photo, float X, float Y, float W, bool bCrop);
    void RunPhotoSmoke();
    FVector2D Pointer() const;
    UTexture2D* LoadFrame(const FString& RelativePath);
};
