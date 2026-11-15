#include "SuperstarStage.h"
#include "GameLocalization.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PaperSpriteComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

void USuperstarRuntimeSprite::Initialize(UTexture2D* Texture, UMaterialInterface* Material)
{
    SpriteCollisionDomain = ESpriteCollisionMode::None;
    AlternateMaterialSplitIndex = INDEX_NONE;
    DefaultMaterial = Material;
    BakedSourceTexture = Texture;
    BakedSourceUV = FVector2D::ZeroVector;
    BakedSourceDimension = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
#if WITH_EDITORONLY_DATA
    SourceTexture = Texture;
    SourceUV = FVector2D::ZeroVector;
    SourceDimension = BakedSourceDimension;
    SourceTextureDimension = BakedSourceDimension;
#endif
    constexpr float L = -197.5f, R = 197.5f, T = 351.796875f, B = -43.203125f;
    BakedRenderData = {FVector4(L,T,0,0), FVector4(L,B,0,1), FVector4(R,B,1,1),
        FVector4(L,T,0,0), FVector4(R,B,1,1), FVector4(R,T,1,0)};
}

ASuperstarStage::ASuperstarStage()
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> FlatAsset(TEXT("/Game/LevelPrototyping/Materials/M_FlatCol.M_FlatCol"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> SpriteAsset(TEXT("/Paper2D/TranslucentUnlitSpriteMaterial.TranslucentUnlitSpriteMaterial"));
    Cube = CubeAsset.Object;
    Cylinder = CylinderAsset.Object;
    Sphere = SphereAsset.Object;
    BaseMaterial = FlatAsset.Object;
    SpriteMaterial = SpriteAsset.Object;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("StoryRoot"));
    StageSign = CreateDefaultSubobject<UWidgetComponent>(TEXT("StageSign"));
    StageSign->SetupAttachment(RootComponent);
    StageSign->SetWidgetSpace(EWidgetSpace::World);
    StageSign->SetDrawSize(FVector2D(1024,160));
    StageSign->SetPivot(FVector2D(.5,.5));
    StageSign->SetBlendMode(EWidgetBlendMode::Transparent);
    StageSign->SetBackgroundColor(FLinearColor::Transparent);
    StageSign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    StageSign->SetRelativeLocation(FVector(0,275,325));
    StageSign->SetRelativeRotation(FRotator(0,-90,0));
    StageSign->SetRelativeScale3D(FVector(.28));
    StageSign->SetVisibility(false);
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("StoryCamera"));
    Camera->SetupAttachment(RootComponent);
    Camera->SetRelativeLocation(FVector(0, -1400, 470));
    Camera->SetRelativeRotation((FVector(0,0,160) - FVector(0,-1400,470)).Rotation());
    Camera->ProjectionMode = ECameraProjectionMode::Perspective;
    Camera->FieldOfView = 53.f;
    Camera->bConstrainAspectRatio = true;
    Camera->AspectRatio = 1280.f / 720.f;
    Camera->PostProcessSettings.bOverride_AutoExposureMethod = true;
    Camera->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
    Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
    Camera->PostProcessSettings.AutoExposureBias = 0.f;
    Camera->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
    Camera->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
    Camera->PostProcessSettings.bOverride_MotionBlurAmount = true;
    Camera->PostProcessSettings.MotionBlurAmount = 0.f; // Five-fps sprites and frozen photos must stay readable.
    auto* Key = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("StoryKey"));
    Key->SetupAttachment(RootComponent);
    Key->SetRelativeRotation(FRotator(-55, -30, 0));
    Key->SetIntensity(5.f);
    Key->ForwardShadingPriority = 1;
    Key->SetLightColor(FLinearColor(1.f,.88f,.75f));
    auto* Fill = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("StoryFill"));
    Fill->SetupAttachment(RootComponent);
    Fill->SetRelativeRotation(FRotator(-25, 90, 0));
    Fill->SetIntensity(3.f);
    Fill->SetCastShadows(false);
    Fill->SetLightColor(FLinearColor(.65f,.78f,1.f));
}

void ASuperstarStage::Box(FVector Position, FVector Size, FLinearColor Color, const TCHAR* Shape)
{
    Position.X = -Position.X; // Author layouts from the viewer's left to right.
    auto* Mesh = NewObject<UStaticMeshComponent>(this);
    Mesh->SetupAttachment(RootComponent);
    Mesh->SetStaticMesh(FCString::Strcmp(Shape, TEXT("Cylinder")) == 0 ? Cylinder.Get() : (FCString::Strcmp(Shape, TEXT("Sphere")) == 0 ? Sphere.Get() : Cube.Get()));
    Mesh->SetRelativeLocation(Position);
    Mesh->SetRelativeScale3D(Size / 100.);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (BaseMaterial)
    {
        auto* Material = UMaterialInstanceDynamic::Create(BaseMaterial, Mesh);
        Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
        Material->SetVectorParameterValue(TEXT("Base Color"), Color);
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Material->SetScalarParameterValue(TEXT("Roughness"), .72f);
        Mesh->SetMaterial(0, Material);
    }
    Mesh->RegisterComponent();
    Pieces.Add(Mesh);
}

void ASuperstarStage::Desk(float X, float Y)
{
    const FLinearColor Wood(.35f,.23f,.15f), Metal(.09f,.13f,.18f);
    Box({X,Y,105}, {250,100,12}, Wood);
    for (float DX : {-105.f,105.f}) for (float DY : {-35.f,35.f}) Box({X+DX,Y+DY,50}, {10,10,100}, Metal);
    Box({X+50,Y+12,146}, {70,10,60}, Metal);
    Box({X+50,Y+5,146}, {60,3,47}, FLinearColor(.18f,.45f,.58f));
    Box({X-50,Y-10,117}, {65,40,8}, FLinearColor(.8f,.83f,.78f));
}

void ASuperstarStage::Equipment(float X, float Y)
{
    const FLinearColor Dark(.035f,.05f,.075f);
    Box({X,Y,110},{8,8,220},Dark,TEXT("Cylinder"));
    Box({X,Y,225},{90,18,65},Dark);
    Box({X,Y-12,225},{75,6,50},FLinearColor(.95f,.88f,.62f));
    for (float DX : {-32.f,32.f}) Box({X+DX,Y,10},{60,8,8},Dark);
    Box({X+60,Y+30,35},{85,60,70},Dark);
    Box({X+60,Y-1,35},{72,3,4},FLinearColor(.5f,.55f,.6f));
}

void ASuperstarStage::SetScene(const FString& Scene)
{
    if (CurrentScene == Scene) return;
    CurrentScene = Scene;
    for (UStaticMeshComponent* Mesh : Pieces) if (Mesh) Mesh->DestroyComponent();
    Pieces.Reset();
    if (!BaseMaterial) BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/LevelPrototyping/Materials/M_FlatCol.M_FlatCol"));
    const bool bOutdoor = Scene == TEXT("outdoor") || Scene == TEXT("newstudio");
    const bool bDayExterior = Scene == TEXT("newstudio");
    const bool bStage = Scene == TEXT("stage");
    StageSign->SetVisibility(bStage);
    if (bStage)
    {
        auto* Font=LoadObject<UFont>(nullptr,TEXT("/Engine/EngineFonts/Roboto.Roboto"));
        StageSign->SetSlateWidget(SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center)
            [SNew(STextBlock)
                .Text(FText::FromString(GameLocalization::Translate(TEXT("扶 摇 直 上"))))
                .Font(FSlateFontInfo(Font,64))
                .ColorAndOpacity(FSlateColor(FLinearColor(.30f,.85f,.73f))) ]);
    }
    const FLinearColor Wall = bDayExterior ? FLinearColor(.38f,.57f,.70f) : (bOutdoor ? FLinearColor(.09f,.15f,.25f) : FLinearColor(.28f,.36f,.44f));
    const FLinearColor Trim(.07f,.12f,.18f), Light(.5f,.78f,.83f);
    Box({0,70,-16}, {1500,850,30}, bOutdoor ? FLinearColor(.13f,.19f,.26f) : FLinearColor(.32f,.38f,.43f));
    Box({0,330,270},{1500,25,550},Wall);
    if (!bOutdoor)
    {
        Box({-740,60,270},{25,550,550},Trim);
        Box({740,60,270},{25,550,550},Trim);
    }
    Box({0,310,32},{1500,12,32},Trim);
    if (!bOutdoor) Box({0,310,460},{1500,15,12},Light);
    // Floor seams make the geometry depth legible without a texture dependency.
    for (int32 I=-3; I<=3; ++I) Box({I*190.,60,.1},{2,800,1},Trim);
    for (int32 I=-2; I<=2; ++I) Box({0,I*170.,.1},{1500,2,1},Trim);
    if (bOutdoor)
    {
        for (int32 I=-3; I<=3; ++I) for (int32 R=0; R<2; ++R)
        {
            Box({I*200.,309.,70.+R*65.},{160,10,45},Trim);
            Box({I*200.,302.,70.+R*65.},{140,4,30},FLinearColor(.15f,.26f,.38f));
        }
        Box({-470,75,170},{12,12,340},Trim,TEXT("Cylinder"));
        Box({-470,75,340},{90,90,25},Trim,TEXT("Cylinder"));
        Box({-470,75,317},{58,58,30},FLinearColor(1.f,.83f,.45f),TEXT("Cylinder"));
        for (int32 I=0; I<3; ++I) { Box({200.+I*125.,240,55},{60,60,110},Trim,TEXT("Cylinder")); Box({200.+I*125.,240,113},{70,70,10},Light,TEXT("Cylinder")); }
        // A low studio boundary and open sky keep the exterior readable from all stations.
        Box({0,280,115},{1500,24,12},Light);
        Box({0,140,3},{1450,80,4},FLinearColor(.08f,.13f,.19f));
        for (int32 I=0; I<(bDayExterior ? 0 : 24); ++I)
            Box({-700.+I*59.,250,220.+(I%4)*65.},{1.5,1.5,25},FLinearColor(.38f,.55f,.67f));
    }
    else if (bStage)
    {
        for (int32 I=-1; I<=1; ++I)
        {
            Box({I*380.,305,280},{350,14,225},Trim);
            Box({I*380.,295,280},{320,8,195},FLinearColor(.12f,.17f,.42f));
            for(int32 J=0; J<7; ++J) Box({I*380.-135.+J*45.,285,280},{4,4,165},FLinearColor(.37f,.23f,.66f));
        }
        Box({0,150,25},{980,290,50},Trim);
        Box({0,-10,8},{1100,60,16},Light);
        Box({0,0,28},{1000,7,6},FLinearColor(.22f,.65f,.9f));
        Desk(500,190);
        for (int32 I=-2; I<=2; ++I) { Box({I*250.,70,450},{60,70,35},Trim); Box({I*250.,35,430},{40,20,20},Light); }
    }
    else if (Scene == TEXT("corridor") || Scene == TEXT("storage"))
    {
        for (float X : {-220.f,250.f,590.f}) { Box({X,307,150},{150,10,290},Trim); Box({X+50,295,130},{9,8,9},Light); }
        // The roster is a textured story prop. Do not layer the old six-person
        // placeholder board behind it, where side cameras expose the mismatch.
        Box({590,285,360},{130,10,35},FLinearColor(.13f,.5f,.36f));
        if (Scene == TEXT("storage"))
        {
            Box({-435,315,235},{590,12,450},FLinearColor(.12f,.25f,.36f));
            Box({310,315,235},{880,12,450},FLinearColor(.40f,.25f,.13f));
            Box({-145,80,220},{24,480,440},Trim);
            Box({-160,-70,175},{12,120,350},FLinearColor(.16f,.21f,.25f));
            Box({-168,-135,170},{5,8,320},FLinearColor(.87f,.62f,.25f));
            Box({-440,100,1},{560,420,3},FLinearColor(.14f,.27f,.37f));
            Box({310,100,1},{850,420,3},FLinearColor(.42f,.28f,.15f));
            for (int32 I=0; I<3; ++I) Box({420.+I*80.,170,40.+I%2*40.},{75,75,80},FLinearColor(.37f,.28f,.20f));
        }
        Equipment(480,180);
    }
    else if (Scene == TEXT("dressing"))
    {
        Desk(-170,200);
        Box({-170,290,230},{240,14,210},Trim);
        Box({-170,279,230},{210,8,180},FLinearColor(.45f,.67f,.74f));
        for (int32 I=0; I<5; ++I) for (float X:{-295.f,-45.f}) Box({X,270,150.+I*40.},{14,12,14},FLinearColor(1.f,.86f,.50f),TEXT("Sphere"));
        Box({440,240,250},{260,12,12},Trim);
        for (float X : {315.f,565.f}) Box({X,240,125},{8,8,250},Trim);
        for (int32 I=0; I<5; ++I) Box({345.+I*45.,240,185},{30,20,110},FLinearColor(.22f+I*.1f,.28f,.34f));
        Equipment(-540,180);
    }
    else
    {
        // Office film set / brighter new production set, based on the supplied studio reference.
        Desk(-390,160);
        Box({-410,300,280},{460,12,250},Trim);
        Box({-410,289,280},{425,7,215},FLinearColor(.34f,.58f,.64f));
        Box({-410,280,280},{8,8,225},Trim);
        Equipment(350,180);
        Equipment(600,95);
        Desk(470,230);
        if (Scene == TEXT("newstudio")) { Box({0,302,300},{320,15,120},FLinearColor(.77f,.70f,.49f)); Box({-700,250,100},{55,55,200},FLinearColor(.2f,.43f,.28f),TEXT("Cylinder")); }
    }
    UE_LOG(LogTemp, Display, TEXT("SuperstarStage: built %s with %d mesh components"), *Scene, Pieces.Num());
}

void ASuperstarStage::ResetCharacters()
{
    for(UStaticMeshComponent* Mesh:StoryFurniture) if(Mesh) Mesh->DestroyComponent();
    StoryFurniture.Reset();
    for(UPaperSpriteComponent* Character : Characters) if(Character) Character->DestroyComponent();
    Characters.Reset();
    CharacterTextures.Reset();
    for (UPaperSpriteComponent* Prop : Props) if (Prop) Prop->DestroyComponent();
    Props.Reset();
    PropTextures.Reset();
}

void ASuperstarStage::SetCharacter(int32 Index, UTexture2D* Texture, float ScreenX, bool bFlip, float RenderScale, float HeightOffset, float Depth)
{
    if (!Texture) return;
    while (Characters.Num() <= Index)
    {
        auto* Component = NewObject<UPaperSpriteComponent>(this);
        Component->SetupAttachment(RootComponent);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCastShadow(false);
        Component->RegisterComponent();
        Characters.Add(Component);
        Component->SetTranslucentSortPriority(10);
        CharacterTextures.Add(nullptr);
    }
    if (!SpriteMaterial) SpriteMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Paper2D/TranslucentUnlitSpriteMaterial.TranslucentUnlitSpriteMaterial"));
    UPaperSpriteComponent* Component = Characters[Index];
    if (CharacterTextures[Index] != Texture)
    {
        auto* Sprite = NewObject<USuperstarRuntimeSprite>(Component);
        Sprite->Initialize(Texture, SpriteMaterial);
        Component->SetSprite(Sprite);
        CharacterTextures[Index] = Texture;
    }
    Component->SetTranslucentSortPriority(Depth > 0.f ? 5 : 10);
    Component->SetRelativeLocation(FVector((640.f-ScreenX)*1400.f/1280.f, -45.f - Index*.5f + Depth, 3.f + HeightOffset));
    Component->SetRelativeScale3D(FVector((bFlip ? 1.f : -1.f)*RenderScale, 1.f, RenderScale));
}

void ASuperstarStage::SetReferenceView(bool bReference)
{
    StageSign->SetVisibility(!bReference && CurrentScene==TEXT("stage"));
    for(UStaticMeshComponent* Mesh:StoryFurniture) Mesh->SetVisibility(!bReference);
    for(UPaperSpriteComponent* Character : Characters) Character->SetVisibility(!bReference);
    for(UPaperSpriteComponent* Prop : Props) Prop->SetVisibility(!bReference);
}

void ASuperstarStage::SetProp(int32 Index, UTexture2D* Texture, FVector Center, float Height, bool bFlip)
{
    if (!Texture) return;
    while (Props.Num()<=Index)
    {
        auto* Component=NewObject<UPaperSpriteComponent>(this);
        Component->SetupAttachment(RootComponent);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetCastShadow(false);
        Component->RegisterComponent();
        Props.Add(Component);PropTextures.Add(nullptr);
        Component->SetTranslucentSortPriority(9);
    }
    UPaperSpriteComponent* Component=Props[Index];
    if(PropTextures[Index]!=Texture)
    {
        auto* Sprite=NewObject<USuperstarRuntimeSprite>(Component);
        Sprite->Initialize(Texture,SpriteMaterial);
        Component->SetSprite(Sprite);PropTextures[Index]=Texture;
    }
    Component->SetRelativeScale3D(FVector((bFlip?1.f:-1.f)*Height/395.f*Texture->GetSizeX()/Texture->GetSizeY(),1.f,Height/395.f));
    Component->SetRelativeLocation(Center-FVector(0,0,Height*.390625f));
    Component->SetRelativeRotation(FRotator::ZeroRotator);
}

void ASuperstarStage::SetAttachedProp(int32 Index,UTexture2D* Texture,int32 ActorIndex,FVector2D Hand,FVector2D GripUV,float Height,float Angle,bool bMirrorWithHolder)
{
    if(!Texture || !Characters.IsValidIndex(ActorIndex)) return;
    const bool bFlip=bMirrorWithHolder && Characters[ActorIndex]->GetRelativeScale3D().X>0;
    // Behind the character by a tenth of a unit: the drawn fingers cover the shaft/handle.
    const FVector HandWorld=CharacterPixelWorld(ActorIndex,Hand);
    SetProp(Index,Texture,HandWorld,Height,bFlip);
    Props[Index]->SetRelativeRotation(FRotator(bFlip?-Angle:Angle,0,0));
    Props[Index]->AddWorldOffset(HandWorld+FVector(0,.1,0)-PropPixelWorld(Index,GripUV));
}

FVector ASuperstarStage::PropPixelWorld(int32 Index,FVector2D UV) const
{
    if(!Props.IsValidIndex(Index)) return FVector::ZeroVector;
    return Props[Index]->GetComponentTransform().TransformPosition(FVector((UV.X-.5)*395.,0,(.890625-UV.Y)*395.));
}

void ASuperstarStage::SetView(float Station, float Pan, float Tilt, float Zoom)
{
    const float Angle = Station * .24f;
    const FVector Target(-Pan * 230.f, 0.f, 160.f + Tilt * 115.f);
    const FVector Location = Target + FVector(FMath::Sin(Angle)*1400.f, -FMath::Cos(Angle)*1400.f, 310.f);
    Camera->SetRelativeLocation(Location);
    Camera->SetRelativeRotation((Target-Location).Rotation());
    Camera->SetFieldOfView(FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(53.f)*.5f) / Zoom)));
}

FVector ASuperstarStage::CharacterPixelWorld(int32 Index, FVector2D Pixel) const
{
    if (!Characters.IsValidIndex(Index)) return FVector::ZeroVector;
    const FVector Local((Pixel.X-320.)*395./640., 0., (570.-Pixel.Y)*395./640.);
    return Characters[Index]->GetComponentTransform().TransformPosition(Local);
}

void ASuperstarStage::SetPropVisible(int32 Index,bool bVisible)
{
    if(Props.IsValidIndex(Index)) Props[Index]->SetVisibility(bVisible);
}

void ASuperstarStage::AddStoryBox(FVector Position,FVector Size,FLinearColor Color)
{
    Box(Position,Size,Color);
    StoryFurniture.Add(Pieces.Pop());
}
