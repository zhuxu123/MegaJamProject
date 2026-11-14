#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BackstageStage.generated.h"

class USceneCaptureComponent2D;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

UCLASS(NotPlaceable)
class ABackstageStage : public AActor
{
	GENERATED_BODY()

public:
	ABackstageStage();
	UTextureRenderTarget2D* Frame(float Time, int32 Station, float Pan, float Zoom, int32 CacheSlot);
	static int32 Evidence(float Time, int32 Station, float Pan, float Zoom);

protected:
	virtual void BeginPlay() override;

private:
	struct FFigure
	{
		UStaticMeshComponent* Body;
		UStaticMeshComponent* Head;
		UStaticMeshComponent* Arms[4];
		UStaticMeshComponent* Legs[4];
		UStaticMeshComponent* Hands[2];
	};

	struct FFrameKey
	{
		float Time = -1;
		int32 Station = -1;
		float Pan = 0;
		float Zoom = 0;
		bool operator==(const FFrameKey&) const = default;
	};

	UPROPERTY()
	TObjectPtr<USceneCaptureComponent2D> Capture;
	UPROPERTY()
	TObjectPtr<UStaticMesh> Cube;
	UPROPERTY()
	TObjectPtr<UStaticMesh> Sphere;
	UPROPERTY()
	TObjectPtr<UMaterialInterface> ShapeMaterial;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextureRenderTarget2D>> Targets;
	TArray<FFrameKey> FrameKeys;
	FFigure Figures[3] = {};
	UStaticMeshComponent* Parcel = nullptr;

	UStaticMeshComponent* Shape(UStaticMesh* Mesh, FLinearColor Color, FVector Position, FVector Scale);
	void BuildSet();
	void Pose(float Time);
};
