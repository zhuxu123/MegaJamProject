enum EBackstageRole
{
    Artist,
    Volunteer,
    Coordinator
}

enum EBackstageExpression
{
    Calm,
    Shock,
    Angry,
    Focus,
    Warm
}

enum EBackstageAnimState
{
    Idle,
    Walk,
    Shock,
    Argue,
    Pickup,
    Care,
    Handoff,
    Warm
}

struct FBackstagePuppetPose
{
    EBackstageAnimState State = EBackstageAnimState::Idle;
    EBackstageExpression Expression = EBackstageExpression::Calm;
    FName SequenceKey = n"Artist_Idle";
    FName EmotionBadge = NAME_None;
    int FrameIndex = 0;
    int FrameCount = 4;
    float FacingSign = 1.0;
    bool bCrouching = false;
}

namespace BackstagePuppet
{
    FVector2D RolePositionAt(EBackstageRole Role, float Time)
    {
        if (Role == EBackstageRole::Artist)
            return Backstage::ArtistAt(Time);
        if (Role == EBackstageRole::Volunteer)
            return Backstage::VolunteerAt(Time);
        return Backstage::CoordinatorAt(Time);
    }

    FName BuildSequenceKey(EBackstageRole Role, EBackstageAnimState State)
    {
        if (Role == EBackstageRole::Artist)
        {
            switch (State)
            {
                case EBackstageAnimState::Walk: return n"Artist_Walk";
                case EBackstageAnimState::Shock: return n"Artist_Shock";
                case EBackstageAnimState::Argue: return n"Artist_Argue";
                case EBackstageAnimState::Pickup: return n"Artist_Pickup";
                case EBackstageAnimState::Care: return n"Artist_Care";
                case EBackstageAnimState::Handoff: return n"Artist_Handoff";
                case EBackstageAnimState::Warm: return n"Artist_Warm";
                default: return n"Artist_Idle";
            }
        }
        if (Role == EBackstageRole::Volunteer)
        {
            switch (State)
            {
                case EBackstageAnimState::Walk: return n"Volunteer_Walk";
                case EBackstageAnimState::Shock: return n"Volunteer_Shock";
                case EBackstageAnimState::Argue: return n"Volunteer_Argue";
                case EBackstageAnimState::Pickup: return n"Volunteer_Pickup";
                case EBackstageAnimState::Care: return n"Volunteer_Care";
                case EBackstageAnimState::Handoff: return n"Volunteer_Handoff";
                case EBackstageAnimState::Warm: return n"Volunteer_Warm";
                default: return n"Volunteer_Idle";
            }
        }
        switch (State)
        {
            case EBackstageAnimState::Walk: return n"Coordinator_Walk";
            case EBackstageAnimState::Shock: return n"Coordinator_Shock";
            case EBackstageAnimState::Argue: return n"Coordinator_Argue";
            case EBackstageAnimState::Pickup: return n"Coordinator_Pickup";
            case EBackstageAnimState::Care: return n"Coordinator_Care";
            case EBackstageAnimState::Handoff: return n"Coordinator_Handoff";
            case EBackstageAnimState::Warm: return n"Coordinator_Warm";
            default: return n"Coordinator_Idle";
        }
    }

    FBackstagePuppetPose EvaluatePose(EBackstageRole Role, float Time)
    {
        FBackstagePuppetPose Pose;
        const float ClampedTime = Math::Max(Time, 0.0);
        const float PrevTime = Math::Max(0.0, ClampedTime - 0.12);
        const float NextTime = Math::Min(Backstage::Duration, ClampedTime + 0.12);
        const FVector2D CurPos = RolePositionAt(Role, ClampedTime);
        const FVector2D PrevPos = RolePositionAt(Role, PrevTime);
        const FVector2D NextPos = RolePositionAt(Role, NextTime);
        const float Span = Math::Max(NextTime - PrevTime, 0.01);
        const float VelX = (NextPos.X - PrevPos.X) / Span;
        const float Speed = Math::Abs(VelX);

        if (Speed > 0.08)
            Pose.FacingSign = VelX >= 0.0 ? 1.0 : -1.0;
        else
            Pose.FacingSign = CurPos.X <= 0.0 ? 1.0 : -1.0;

        const auto Beat = Backstage::BeatAt(ClampedTime);
        switch (Beat)
        {
            case EBackstageBeat::Arrival:
                Pose.State = Speed > 0.18 ? EBackstageAnimState::Walk : EBackstageAnimState::Idle;
                Pose.Expression = EBackstageExpression::Calm;
                break;

            case EBackstageBeat::Collision:
                Pose.State = EBackstageAnimState::Shock;
                if (Role == EBackstageRole::Coordinator)
                {
                    Pose.Expression = EBackstageExpression::Focus;
                }
                else
                {
                    Pose.Expression = EBackstageExpression::Shock;
                    Pose.EmotionBadge = n"Emotion_Shock";
                }
                break;

            case EBackstageBeat::Argument:
                if (Role == EBackstageRole::Artist)
                {
                    Pose.State = EBackstageAnimState::Argue;
                    Pose.Expression = EBackstageExpression::Angry;
                    Pose.EmotionBadge = n"Emotion_Anger";
                    Pose.FacingSign = 1.0;
                }
                else if (Role == EBackstageRole::Volunteer)
                {
                    Pose.State = EBackstageAnimState::Shock;
                    Pose.Expression = EBackstageExpression::Shock;
                    Pose.EmotionBadge = n"Emotion_Shock";
                    Pose.FacingSign = -1.0;
                }
                else
                {
                    Pose.State = EBackstageAnimState::Argue;
                    Pose.Expression = EBackstageExpression::Focus;
                    Pose.FacingSign = -1.0;
                }
                break;

            case EBackstageBeat::Pickup:
                if (Role == EBackstageRole::Artist)
                {
                    Pose.State = EBackstageAnimState::Pickup;
                    Pose.Expression = EBackstageExpression::Focus;
                    Pose.bCrouching = true;
                    Pose.EmotionBadge = n"Emotion_Care";
                    Pose.FacingSign = 1.0;
                }
                else if (Role == EBackstageRole::Volunteer)
                {
                    Pose.State = EBackstageAnimState::Care;
                    Pose.Expression = EBackstageExpression::Calm;
                    Pose.EmotionBadge = n"Emotion_Care";
                    Pose.FacingSign = -1.0;
                }
                else
                {
                    Pose.State = EBackstageAnimState::Idle;
                    Pose.Expression = EBackstageExpression::Focus;
                    Pose.FacingSign = -1.0;
                }
                break;

            case EBackstageBeat::Handoff:
                if (Role == EBackstageRole::Coordinator)
                {
                    Pose.State = EBackstageAnimState::Warm;
                    Pose.Expression = EBackstageExpression::Warm;
                }
                else
                {
                    Pose.State = EBackstageAnimState::Handoff;
                    Pose.Expression = EBackstageExpression::Warm;
                    Pose.EmotionBadge = n"Emotion_Warm";
                }
                break;

            case EBackstageBeat::Together:
                Pose.State = EBackstageAnimState::Warm;
                Pose.Expression = EBackstageExpression::Warm;
                Pose.EmotionBadge = Role == EBackstageRole::Coordinator ? NAME_None : n"Emotion_Warm";
                break;
        }

        float FPS = 6.0;
        if (Pose.State == EBackstageAnimState::Walk || Pose.State == EBackstageAnimState::Pickup || Pose.State == EBackstageAnimState::Handoff)
            FPS = 8.0;
        else if (Pose.State == EBackstageAnimState::Shock || Pose.State == EBackstageAnimState::Argue)
            FPS = 10.0;

        Pose.FrameIndex = Math::Clamp(Math::FloorToInt(ClampedTime * FPS) % Pose.FrameCount, 0, Pose.FrameCount - 1);
        Pose.SequenceKey = BuildSequenceKey(Role, Pose.State);
        return Pose;
    }
}

class ABackstagePuppet : AActor
{
    UPROPERTY(DefaultComponent, RootComponent)
    USceneComponent Root;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent BodySprite;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent ShadowSprite;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent EmotionSprite;

    EBackstageRole Role = EBackstageRole::Artist;
    FLinearColor RoleColor = FLinearColor(0.97, 0.36, 0.46, 1.0);
    FName LastEmotionBadge = NAME_None;

    void Setup(EBackstageRole InRole, FLinearColor InColor)
    {
        Role = InRole;
        RoleColor = InColor;
        LastEmotionBadge = NAME_None;

        ShadowSprite.BackstageSetupPart(n"Shadow", FVector2D(74.0, 38.0), FVector2D(0.5, 0.5),
            FLinearColor(0.02, 0.04, 0.06, 0.72), false, false);
        ShadowSprite.SetRelativeLocation(FVector(0.0, 0.0, 1.5));
        ShadowSprite.SetRelativeRotation(FRotator(0.0, 0.0, -90.0));

        BodySprite.SetRelativeLocation(FVector(0.0, 0.0, 0.0));
        EmotionSprite.SetRelativeLocation(FVector(0.0, 6.0, 166.0));
        EmotionSprite.BackstageSetVisible(false);
    }

    void Sync(float Time, FVector WorldFootPos, float CameraYawDeg)
    {
        BackstageSetTransform(WorldFootPos, FRotator(0.0, CameraYawDeg + 90.0, 0.0));

        const FBackstagePuppetPose Pose = BackstagePuppet::EvaluatePose(Role, Time);
        BodySprite.BackstageSetSpriteFrame(Pose.SequenceKey, Pose.FrameIndex, FVector2D(104.0, 156.0),
            FVector2D(0.5, 1.0), RoleColor, true, true);
        BodySprite.BackstageSetScale(FVector(Pose.FacingSign, 1.0, 1.0));

        if (Pose.EmotionBadge != NAME_None)
        {
            if (Pose.EmotionBadge != LastEmotionBadge)
            {
                EmotionSprite.BackstageSetupPart(Pose.EmotionBadge, FVector2D(34.0, 34.0), FVector2D(0.5, 0.5),
                    FLinearColor::White, false, false);
                LastEmotionBadge = Pose.EmotionBadge;
            }
            const float HoverZ = (Pose.bCrouching ? 136.0 : 166.0) + Math::Sin(Time * 6.0) * 4.0;
            EmotionSprite.SetRelativeLocation(FVector(Pose.FacingSign * 14.0, 6.0, HoverZ));
            EmotionSprite.BackstageSetVisible(true);
        }
        else
        {
            EmotionSprite.BackstageSetVisible(false);
        }
    }
}

class ABackstageStageActor : AActor
{
    UPROPERTY(DefaultComponent, RootComponent)
    USceneComponent Root;

    UPROPERTY(DefaultComponent, Attach = Root)
    UStaticMeshComponent FloorMesh;

    UPROPERTY(DefaultComponent, Attach = Root)
    UStaticMeshComponent BackWallMesh;

    UPROPERTY(DefaultComponent, Attach = Root)
    UStaticMeshComponent LeftCurtainMesh;

    UPROPERTY(DefaultComponent, Attach = Root)
    UStaticMeshComponent RightCurtainMesh;

    UPROPERTY(DefaultComponent, Attach = Root)
    UStaticMeshComponent TrussMesh;

    UPROPERTY(DefaultComponent, Attach = Root)
    USpotLightComponent KeySpot;

    UPROPERTY(DefaultComponent, Attach = Root)
    USpotLightComponent RimSpotLeft;

    UPROPERTY(DefaultComponent, Attach = Root)
    USpotLightComponent RimSpotRight;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPointLightComponent StageFill;

    UPROPERTY(DefaultComponent, Attach = Root)
    UDirectionalLightComponent BounceDir;

    UPROPERTY(DefaultComponent, Attach = Root)
    UExponentialHeightFogComponent StageFog;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent ParcelSprite;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent ParcelShadow;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent ScatterLeft;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent ScatterRight;

    TArray<UStaticMeshComponent> Pillars;
    ABackstagePuppet ArtistPuppet = nullptr;
    ABackstagePuppet VolunteerPuppet = nullptr;
    ABackstagePuppet CoordinatorPuppet = nullptr;
    bool bInitialized = false;

    void ConfigureStageMesh(UStaticMeshComponent Mesh, FString MeshPath, FVector RelLoc, FVector RelScale,
        FLinearColor Color, float Roughness, float Metallic, bool bCastShadow)
    {
        Mesh.BackstageSetupStageMesh(MeshPath, Color, Roughness, Metallic, bCastShadow);
        Mesh.SetRelativeLocation(RelLoc);
        Mesh.BackstageSetScale(RelScale);
    }

    void InitializeStage(APlayerController Controller)
    {
        if (bInitialized || Controller == nullptr)
            return;
        bInitialized = true;

        const FString CubePath = "/Engine/BasicShapes/Cube.Cube";
        const FString CylinderPath = "/Engine/BasicShapes/Cylinder.Cylinder";

        ConfigureStageMesh(FloorMesh, CubePath, FVector(120.0, 0.0, -20.0), FVector(12.0, 18.0, 0.4),
            FLinearColor(0.08, 0.11, 0.15, 1.0), 0.62, 0.08, false);
        ConfigureStageMesh(BackWallMesh, CubePath, FVector(280.0, 0.0, 300.0), FVector(0.4, 18.0, 6.0),
            FLinearColor(0.05, 0.08, 0.12, 1.0), 0.85, 0.05, false);
        ConfigureStageMesh(LeftCurtainMesh, CubePath, FVector(20.0, -680.0, 260.0), FVector(5.2, 0.5, 5.2),
            FLinearColor(0.035, 0.05, 0.08, 1.0), 0.92, 0.0, false);
        ConfigureStageMesh(RightCurtainMesh, CubePath, FVector(20.0, 680.0, 260.0), FVector(5.2, 0.5, 5.2),
            FLinearColor(0.035, 0.05, 0.08, 1.0), 0.92, 0.0, false);
        ConfigureStageMesh(TrussMesh, CubePath, FVector(-140.0, 0.0, 420.0), FVector(0.35, 16.0, 0.35),
            FLinearColor(0.14, 0.17, 0.22, 1.0), 0.35, 0.78, true);

        Pillars.Empty();
        for (int Index = -2; Index <= 2; ++Index)
        {
            UStaticMeshComponent Pillar = CreateComponent(UStaticMeshComponent);
            Pillar.BackstageAttachTo(Root);
            const FVector PillarLoc = Controller.BackstageWorldPoint(FVector2D(Index * 2.0, -0.9), 210.0);
            ConfigureStageMesh(Pillar, CylinderPath, PillarLoc, FVector(0.32, 0.32, 4.2),
                FLinearColor(0.12, 0.16, 0.22, 1.0), 0.58, 0.25, true);
            Pillars.Add(Pillar);
        }

        KeySpot.BackstageSetupSpotLight(FVector(-120.0, -40.0, 410.0), FRotator(-66.0, 14.0, 0.0),
            FLinearColor(1.0, 0.93, 0.82, 1.0), 65000.0, 2400.0, 28.0, 56.0, true);
        RimSpotLeft.BackstageSetupSpotLight(FVector(160.0, -440.0, 320.0), FRotator(-28.0, 110.0, 0.0),
            FLinearColor(0.26, 0.64, 1.0, 1.0), 38000.0, 2200.0, 22.0, 50.0, false);
        RimSpotRight.BackstageSetupSpotLight(FVector(160.0, 440.0, 320.0), FRotator(-28.0, -110.0, 0.0),
            FLinearColor(1.0, 0.34, 0.48, 1.0), 38000.0, 2200.0, 22.0, 50.0, false);
        StageFill.BackstageSetupPointLight(FVector(-260.0, 0.0, 220.0),
            FLinearColor(0.68, 0.80, 0.98, 1.0), 22000.0, 2000.0, false);
        BounceDir.BackstageSetupDirectionalLight(FRotator(-35.0, 18.0, 0.0),
            FLinearColor(0.75, 0.85, 0.98, 1.0), 1.8, false);
        StageFog.BackstageSetupFog(0.0045, FLinearColor(0.05, 0.09, 0.14, 1.0), true);

        ParcelShadow.BackstageSetupPart(n"Shadow", FVector2D(46.0, 26.0), FVector2D(0.5, 0.5),
            FLinearColor(0.02, 0.04, 0.06, 0.65), false, false);
        ParcelShadow.SetRelativeRotation(FRotator(0.0, 0.0, -90.0));

        const FLinearColor Gold(1.0, 0.78, 0.38, 1.0);
        ParcelSprite.BackstageSetupPart(n"Parcel", FVector2D(36.0, 36.0), FVector2D(0.5, 0.5), Gold, true, true);
        ScatterLeft.BackstageSetupPart(n"Parcel", FVector2D(20.0, 20.0), FVector2D(0.5, 0.5), Gold, true, true);
        ScatterRight.BackstageSetupPart(n"Parcel", FVector2D(22.0, 22.0), FVector2D(0.5, 0.5), Gold, true, true);

        ArtistPuppet = ABackstagePuppet::Spawn(FVector::ZeroVector, FRotator::ZeroRotator);
        ArtistPuppet.Setup(EBackstageRole::Artist, FLinearColor(0.97, 0.36, 0.46, 1.0));

        VolunteerPuppet = ABackstagePuppet::Spawn(FVector::ZeroVector, FRotator::ZeroRotator);
        VolunteerPuppet.Setup(EBackstageRole::Volunteer, FLinearColor(0.30, 0.66, 0.98, 1.0));

        CoordinatorPuppet = ABackstagePuppet::Spawn(FVector::ZeroVector, FRotator::ZeroRotator);
        CoordinatorPuppet.Setup(EBackstageRole::Coordinator, FLinearColor(0.37, 0.84, 0.61, 1.0));
    }

    void SyncToView(APlayerController Controller, const FBackstagePhoto&in View, bool bActivateCamera)
    {
        if (Controller == nullptr)
            return;
        InitializeStage(Controller);

        const float CameraYaw = Controller.BackstageCameraYaw(View.Station);
        Controller.BackstageSyncCamera(this, View.Station, View.Pan, View.Zoom, bActivateCamera);

        if (ArtistPuppet != nullptr)
            ArtistPuppet.Sync(View.Time, Controller.BackstageWorldPoint(Backstage::ArtistAt(View.Time), 0.0), CameraYaw);
        if (VolunteerPuppet != nullptr)
            VolunteerPuppet.Sync(View.Time, Controller.BackstageWorldPoint(Backstage::VolunteerAt(View.Time), 0.0), CameraYaw);
        if (CoordinatorPuppet != nullptr)
        {
            FVector CoordPos = Controller.BackstageWorldPoint(Backstage::CoordinatorAt(View.Time), 0.0);
            CoordPos.X += 4.0;
            CoordinatorPuppet.Sync(View.Time, CoordPos, CameraYaw);
        }

        const auto Beat = Backstage::BeatAt(View.Time);
        const bool bDropped = Beat == EBackstageBeat::Collision || Beat == EBackstageBeat::Argument;
        const float ParcelHeight = bDropped ? 18.0 : (Beat == EBackstageBeat::Pickup ? 40.0 : 80.0);
        const FVector2D ParcelStage = Backstage::ParcelAt(View.Time);
        const FVector ParcelFloor = Controller.BackstageWorldPoint(ParcelStage, 1.5);
        const FRotator FacingCam(0.0, CameraYaw + 90.0, 0.0);

        ParcelShadow.SetRelativeLocation(FVector(ParcelFloor.X - 6.0, ParcelFloor.Y, 1.5));
        ParcelSprite.SetRelativeLocation(FVector(ParcelFloor.X - 8.0, ParcelFloor.Y, ParcelHeight));
        ParcelSprite.SetRelativeRotation(FacingCam);

        if (bDropped)
        {
            const float AngleRad = Math::DegreesToRadians(CameraYaw);
            const FVector RightDir(-Math::Sin(AngleRad), Math::Cos(AngleRad), 0.0);
            ScatterLeft.SetRelativeLocation(FVector(ParcelFloor.X - 6.0, ParcelFloor.Y, 10.0) - RightDir * 36.0);
            ScatterLeft.SetRelativeRotation(FRotator(-18.0, CameraYaw + 90.0, 0.0));
            ScatterRight.SetRelativeLocation(FVector(ParcelFloor.X - 6.0, ParcelFloor.Y, 12.0) + RightDir * 36.0);
            ScatterRight.SetRelativeRotation(FRotator(14.0, CameraYaw + 90.0, 0.0));
            ScatterLeft.BackstageSetVisible(true);
            ScatterRight.BackstageSetVisible(true);
        }
        else
        {
            ScatterLeft.BackstageSetVisible(false);
            ScatterRight.BackstageSetVisible(false);
        }
    }
}
