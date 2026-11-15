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
    UPROPERTY(Transient) TMap<FString, TObjectPtr<UTexture2D>> Frames;
    UPROPERTY(Transient) TObjectPtr<UFont> StoryFont;
    UPROPERTY(Transient) TObjectPtr<ASuperstarStage> Stage;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USuperstarMusic> Music;
    bool bReferenceView = false;
    int32 Beat = 0;
    float Clock = 0.f;
    bool bPlaying = true;
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
    int32 PhotoSmokeTicks = 0, PhotoSmokeStep = 0;

    void SelectBeat(int32 Index);
    void Publish();
    void Rect(float X, float Y, float W, float H, FLinearColor Color);
    void Label(const FString& Text, float X, float Y, int32 Size = 18, FLinearColor Color = FLinearColor::White);
    void Background(const FString& Scene);
    void DrawActors();
    void UpdateActors();
    void ResolveActorSample(const TSharedPtr<FJsonObject>& Actor, FString& Id, int32& Frame) const;
    FString ClipFramePath(const FString& Id, int32 Frame) const;
    float PropScreenX(const TSharedPtr<FJsonObject>& Prop) const;
    void UpdateCamera(float Delta);
    void TakePhoto();
    void OnPhotoCaptured(int32 Width, int32 Height, const TArray<FColor>& Pixels);
    void AcceptPhoto();
    void RejectPhoto();
    void OpenAlbum();
    void TogglePhoto(int32 Index);
    void StartPhotoPreview(bool bPublish);
    void SavePublication();
    bool TickPhotography(float Delta);
    void DrawPhotography();
    void DrawPhoto(const FSuperstarPhoto& Photo, float X, float Y, float W, bool bCrop);
    void RunPhotoSmoke();
    FVector2D Pointer() const;
    UTexture2D* LoadFrame(const FString& RelativePath);
};
