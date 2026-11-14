#include "BackstageStage.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr double FrameAspect = 8.0 / 3.0;
	constexpr double WideWidth = 735;
	constexpr double PanUnits = 100;
	const FVector StageOrigin(0, 0, 1500);
	const FVector FloorParcel(8, -38, 14);

	double Ease(float Time, float Start, float End)
	{
		const double Alpha = FMath::Clamp((Time - Start) / (End - Start), 0.0f, 1.0f);
		return Alpha * Alpha * (3 - 2 * Alpha);
	}

	struct FBlocking
	{
		FVector Artist, Volunteer, Coordinator;
		FVector Parcel, ArtistHand, VolunteerHand;
		double Crouch, ArtistLean, VolunteerLean;
	};

	FBlocking Block(float Time)
	{
		FBlocking Pose{};
		Pose.Artist = FVector(FMath::Lerp(-265.0, -32.0, Ease(Time, 0, 7))
			- 64 * Ease(Time, 7.2f, 9) + 38 * Ease(Time, 22, 25)
			+ 15 * Ease(Time, 32, 35), -36, 0);
		Pose.Volunteer = FVector(25 + 61 * Ease(Time, 7.15f, 9), -12, 0);
		Pose.Coordinator = FVector(240 - 70 * Ease(Time, 30, 38), 10, 0);

		const double Gesture = (Ease(Time, 13.7f, 15) - Ease(Time, 21.5f, 23))
			* (0.83 + 0.17 * FMath::Sin(Time * 3));
		const double Reach = Ease(Time, 23, 25.4f) - Ease(Time, 39.2f, 41.5f);
		const double Transfer = Ease(Time, 35, 37) - Ease(Time, 39.2f, 41.5f);
		Pose.Crouch = Ease(Time, 23, 25.4f) - Ease(Time, 29, 33);
		Pose.ArtistLean = 22 * Pose.Crouch + 12 * Gesture + 13 * (Ease(Time, 6.4f, 7) - Ease(Time, 7, 8));
		Pose.VolunteerLean = -17 * (Ease(Time, 7, 7.35f) - Ease(Time, 7.35f, 9));
		const FVector ArtistRest = Pose.Artist + FVector(16, -31, 84 - 45 * Pose.Crouch);
		const FVector VolunteerRest = Pose.Volunteer + FVector(-24, -23, 95);
		const FVector Lift = FMath::Lerp(FloorParcel, FVector(0, -40, 101), Ease(Time, 29, 33));
		const FVector Exchange(18, -43, 107);
		const FVector ArtistTarget = FMath::Lerp(Lift, Exchange, Ease(Time, 35, 37));
		Pose.ArtistHand = FMath::Lerp(ArtistRest + FVector(52 * Gesture, -4, 46 * Gesture), ArtistTarget, Reach);
		Pose.VolunteerHand = FMath::Lerp(VolunteerRest + FVector(-23 * Gesture, 0, 34 * Gesture), Exchange, Transfer);
		if (Time < 8.2f)
		{
			const FVector Dropped = Pose.Volunteer + FVector(-34, -26, 104);
			const double Fall = FMath::Clamp((Time - 7.1) / 1.1, 0.0, 1.0);
			Pose.Parcel = FMath::Lerp(Dropped, FloorParcel, Fall * Fall);
			Pose.VolunteerHand = FMath::Lerp(Dropped, VolunteerRest, Ease(Time, 7.1f, 8.2f));
		}
		else if (Time < 25.4f)
			Pose.Parcel = FloorParcel;
		else if (Time < 37.5f)
			Pose.Parcel = Pose.ArtistHand;
		else
			Pose.Parcel = Pose.VolunteerHand;
		return Pose;
	}

	FTransform CameraTransform(int32 Station, float Pan)
	{
		const FVector Positions[] = {FVector(0, -630, 252), FVector(-365, -540, 226), FVector(365, -540, 226)};
		const FVector Position = Positions[FMath::Clamp(Station, 0, 2)];
		const FRotator Rotation = (FVector(0, -12, 85) - Position).Rotation();
		return FTransform(Rotation, StageOrigin + Position + Rotation.RotateVector(FVector(0, Pan * PanUnits, 0)));
	}

	void Segment(UStaticMeshComponent* Mesh, FVector Start, FVector End, double Thickness)
	{
		const FVector Delta = End - Start;
		Mesh->SetRelativeLocationAndRotation((Start + End) * 0.5, FRotationMatrix::MakeFromZ(Delta).Rotator());
		Mesh->SetRelativeScale3D(FVector(Thickness, Thickness, Delta.Size() / 100));
	}
}

ABackstageStage::ABackstageStage()
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/Backstage/M_BackstageStage.M_BackstageStage"));
	ShapeMaterial = Material.Object;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Stage")));
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Camera"));
	Capture->SetupAttachment(GetRootComponent());
	Capture->ProjectionType = ECameraProjectionMode::Orthographic;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->CaptureSource = SCS_FinalColorLDR;
	Capture->PostProcessSettings.AutoExposureMethod = AEM_Manual;
	Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	Capture->ShowFlags.SetMotionBlur(false);
}

void ABackstageStage::BeginPlay()
{
	Super::BeginPlay();
	SetActorLocation(StageOrigin);
	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!ensure(Cube && Sphere && ShapeMaterial))
		return;
	BuildSet();
	Pose(0);
}

UStaticMeshComponent* ABackstageStage::Shape(UStaticMesh* Mesh, FLinearColor Color, FVector Position, FVector Scale)
{
	auto* ShapeComponent = NewObject<UStaticMeshComponent>(this);
	ShapeComponent->SetupAttachment(GetRootComponent());
	ShapeComponent->SetStaticMesh(Mesh);
	ShapeComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShapeComponent->SetRelativeLocation(Position);
	ShapeComponent->SetRelativeScale3D(Scale);
	if (auto* Material = ShapeComponent->CreateDynamicMaterialInstance(0, ShapeMaterial))
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	ShapeComponent->RegisterComponent();
	return ShapeComponent;
}

void ABackstageStage::BuildSet()
{
	const FLinearColor Navy(0.035f, 0.065f, 0.088f);
	const FLinearColor Slate(0.14f, 0.18f, 0.22f);
	const FLinearColor Teal(0.05f, 0.24f, 0.27f);
	const FLinearColor Gold(0.94f, 0.46f, 0.065f);
	Shape(Cube, Slate, FVector(0, 30, -12), FVector(11, 9, 0.24));
	Shape(Cube, Navy, FVector(0, 266, 145), FVector(11, 0.20, 3));
	for (int Index = -6; Index <= 6; ++Index)
		Shape(Cube, Teal, FVector(Index * 82, 246, 156), FVector(0.57, 0.12, 3.12));
	Shape(Cube, Gold, FVector(0, 237, 218), FVector(10, 0.03, 0.05));
	Shape(Cube, Gold, FVector(0, 180, 0.8), FVector(9.9, 0.04, 0.016));
	for (int Side : {-1, 1})
	{
		Shape(Cube, Navy, FVector(Side * 290, 145, 35), FVector(1.17, 0.68, 0.70));
		Shape(Cube, Slate, FVector(Side * 290, 145, 72), FVector(1.22, 0.71, 0.07));
		Shape(Cube, Gold, FVector(Side * 290, 109, 43), FVector(0.46, 0.02, 0.06));
	}

	auto* Key = NewObject<UDirectionalLightComponent>(this);
	Key->SetupAttachment(GetRootComponent());
	Key->SetRelativeRotation(FRotator(-49, 55, 0));
	Key->SetIntensity(3.8f);
	Key->RegisterComponent();
	auto* Fill = NewObject<UPointLightComponent>(this);
	Fill->SetupAttachment(GetRootComponent());
	Fill->SetRelativeLocation(FVector(290, -185, 255));
	Fill->SetIntensity(1800);
	Fill->SetAttenuationRadius(850);
	Fill->SetCastShadows(false);
	Fill->RegisterComponent();

	const FLinearColor Wardrobe[] = {{0.85f, 0.12f, 0.22f}, {0.075f, 0.34f, 0.8f}, {0.09f, 0.57f, 0.36f}};
	const FLinearColor Skin[] = {{0.80f, 0.46f, 0.28f}, {0.64f, 0.33f, 0.19f}, {0.48f, 0.26f, 0.17f}};
	for (int Index = 0; Index < 3; ++Index)
	{
		FFigure& Figure = Figures[Index];
		Figure.Body = Shape(Cube, Wardrobe[Index], FVector::ZeroVector, FVector(0.38, 0.29, 0.55));
		Figure.Head = Shape(Sphere, Skin[Index], FVector::ZeroVector, FVector(0.30, 0.29, 0.34));
		for (int Limb = 0; Limb < 4; ++Limb)
		{
			Figure.Arms[Limb] = Shape(Cube, Limb % 2 == 0 ? Wardrobe[Index] : Skin[Index], FVector::ZeroVector, FVector::OneVector);
			Figure.Legs[Limb] = Shape(Cube, Navy, FVector::ZeroVector, FVector::OneVector);
		}
		for (auto*& Hand : Figure.Hands)
			Hand = Shape(Sphere, Skin[Index], FVector::ZeroVector, FVector(0.14));
	}
	Parcel = Shape(Cube, Gold, FVector::ZeroVector, FVector(0.31, 0.25, 0.26));
}

void ABackstageStage::Pose(float Time)
{
	const FBlocking Beat = Block(Time);
	const FVector Roots[] = {Beat.Artist, Beat.Volunteer, Beat.Coordinator};
	for (int Index = 0; Index < 3; ++Index)
	{
		auto& Figure = Figures[Index];
		const FVector Root = Roots[Index];
		const double Crouch = Index == 0 ? Beat.Crouch : 0;
		const double Lean = Index == 0 ? Beat.ArtistLean : (Index == 1 ? Beat.VolunteerLean : 0);
		const double Walk = Index == 0 ? 1 - Ease(Time, 6.3f, 7) : (Index == 2 ? Ease(Time, 30, 31) - Ease(Time, 37, 38) : 0);
		const FVector Hip = Root + FVector(0, 0, 82 - 46 * Crouch);
		const FVector Shoulder = Root + FVector(Lean, 0, 134 - 61 * Crouch);
		Figure.Body->SetRelativeLocationAndRotation((Hip + Shoulder) / 2, FRotationMatrix::MakeFromZ(Shoulder - Hip).Rotator());
		Figure.Head->SetRelativeLocation(Shoulder + FVector(Lean * 0.18, 0, 23));

		for (int Side = 0; Side < 2; ++Side)
		{
			const double Sign = Side == 0 ? -1 : 1;
			const double Stride = Sign * Walk * 23 * FMath::Sin(Time * 4.4);
			const FVector Foot = Root + FVector(Stride + 16 * Crouch, Sign * 15, 7 + FMath::Max(0.0, Stride * 0.32));
			const FVector LegTop = Hip + FVector(0, Sign * 12, 0);
			const FVector Knee = (LegTop + Foot) / 2 + FVector(28 * Crouch, -5, 0);
			Segment(Figure.Legs[Side * 2], LegTop, Knee, 0.15);
			Segment(Figure.Legs[Side * 2 + 1], Knee, Foot, 0.12);
			const FVector ArmTop = Shoulder + FVector(0, -20 + Side * 39, -6);
			FVector Hand = Hip + FVector(-Stride * 0.6, -29 + Side * 58, -3);
			if (Side == 0 && Index == 0)
				Hand = Beat.ArtistHand;
			if (Side == 0 && Index == 1)
				Hand = Beat.VolunteerHand;
			if (Side == 0 && Index == 2)
				Hand += FVector(-30, -20, 48) * (Ease(Time, 34, 36) - Ease(Time, 41, 44));
			const FVector Elbow = (ArmTop + Hand) / 2 + FVector(0, -10, -9);
			Segment(Figure.Arms[Side * 2], ArmTop, Elbow, 0.105);
			Segment(Figure.Arms[Side * 2 + 1], Elbow, Hand, 0.085);
			Figure.Hands[Side]->SetRelativeLocation(Hand);
		}
	}
	Parcel->SetRelativeLocationAndRotation(Beat.Parcel, FRotator(0, Time < 8.2f ? Ease(Time, 7.1f, 8.2f) * 150 : 0, 0));
}

UTextureRenderTarget2D* ABackstageStage::Frame(float Time, int32 Station, float Pan, float Zoom, int32 CacheSlot)
{
	if (!Parcel || CacheSlot < 0 || CacheSlot >= 13 || Zoom <= 0)
		return nullptr;
	if (Targets.Num() <= CacheSlot)
	{
		Targets.SetNum(CacheSlot + 1);
		FrameKeys.SetNum(CacheSlot + 1);
	}
	auto& Target = Targets[CacheSlot];
	if (!Target)
	{
		Target = NewObject<UTextureRenderTarget2D>(this);
		Target->InitCustomFormat(CacheSlot == 0 ? 1152 : 768, CacheSlot == 0 ? 432 : 288, PF_B8G8R8A8, false);
		Target->UpdateResourceImmediate(true);
	}
	const FFrameKey Key{Time, Station, Pan, Zoom};
	if (FrameKeys[CacheSlot] != Key)
	{
		Pose(Time);
		Capture->SetWorldTransform(CameraTransform(Station, Pan));
		Capture->OrthoWidth = WideWidth / Zoom;
		Capture->TextureTarget = Target;
		Capture->CaptureScene();
		if (ShapeMaterial->IsComplete())
			FrameKeys[CacheSlot] = Key;
	}
	return Target;
}

int32 ABackstageStage::Evidence(float Time, int32 Station, float Pan, float Zoom)
{
	const FTransform View = CameraTransform(Station, Pan);
	const FBlocking Beat = Block(Time);
	const double HalfWidth = WideWidth / Zoom / 2;
	const double HalfHeight = HalfWidth / FrameAspect;
	const FVector Points[] = {Beat.Artist + FVector(0, 0, 112 - 47 * Beat.Crouch),
		Beat.Volunteer + FVector(0, 0, 112), Beat.Parcel};
	int32 Visible = 0;
	for (int Index = 0; Index < 3; ++Index)
	{
		const FVector Local = View.InverseTransformPosition(Points[Index] + StageOrigin);
		if (FMath::Abs(Local.Y) <= HalfWidth * 0.90 && FMath::Abs(Local.Z) <= HalfHeight * 0.97)
			Visible |= 1 << Index;
	}
	return Visible;
}
