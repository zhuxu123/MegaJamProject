#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PaperSprite.h"
#include "SuperstarStage.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UPaperSpriteComponent;
class UMaterialInterface;
class UTexture2D;
class UStaticMesh;

UCLASS()
class USuperstarRuntimeSprite : public UPaperSprite
{
    GENERATED_BODY()
public:
    void Initialize(UTexture2D* Texture, UMaterialInterface* Material);
};

// All geometry belongs to this transient actor. No level package is modified.
UCLASS()
class MEGAJAMPROJECT_API ASuperstarStage : public AActor
{
    GENERATED_BODY()
public:
    ASuperstarStage();
    void SetScene(const FString& Scene);
    void SetCharacter(int32 Index, UTexture2D* Texture, float ScreenX, bool bFlip, float RenderScale = 1.f, float HeightOffset = 0.f, float Depth = 0.f);
    void ResetCharacters();
    void AddStoryBox(FVector Position, FVector Size, FLinearColor Color);
    void SetReferenceView(bool bReference);
    void SetView(float Station, float Pan, float Tilt, float Zoom);
    FVector CharacterPixelWorld(int32 Index, FVector2D Pixel) const;
    void SetProp(int32 Index, UTexture2D* Texture, FVector Center, float Height, bool bFlip = false);
    void SetAttachedProp(int32 Index, UTexture2D* Texture, int32 ActorIndex, FVector2D Hand, FVector2D GripUV, float Height, float Angle = 0.f, bool bMirrorWithHolder = true);
    void SetPropVisible(int32 Index, bool bVisible);
    FVector PropPixelWorld(int32 Index, FVector2D UV) const;
private:
    UPROPERTY() TObjectPtr<UCameraComponent> Camera;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Pieces;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> StoryFurniture;
    UPROPERTY() TArray<TObjectPtr<UPaperSpriteComponent>> Characters;
    UPROPERTY() TArray<TObjectPtr<UTexture2D>> CharacterTextures;
    UPROPERTY() TArray<TObjectPtr<UPaperSpriteComponent>> Props;
    UPROPERTY() TArray<TObjectPtr<UTexture2D>> PropTextures;
    UPROPERTY() TObjectPtr<UMaterialInterface> BaseMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> SpriteMaterial;
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
    UPROPERTY() TObjectPtr<UStaticMesh> Sphere;
    FString CurrentScene;
    void Box(FVector Position, FVector Size, FLinearColor Color, const TCHAR* Shape = TEXT("Cube"));
    void Desk(float X, float Y);
    void Equipment(float X, float Y);
};
