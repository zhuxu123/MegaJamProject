enum EBackstageRole
{
    Artist,
    Volunteer,
    Coordinator,
    LinWan,
    XiaoBei,
    Fan
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
        if (Role == EBackstageRole::Artist) return Backstage::ArtistAt(Time);
        if (Role == EBackstageRole::Volunteer) return Backstage::VolunteerAt(Time);
        if (Role == EBackstageRole::Coordinator) return Backstage::CoordinatorAt(Time);
        if (Role == EBackstageRole::LinWan) return Backstage::LinWanAt(Time);
        if (Role == EBackstageRole::XiaoBei) return Backstage::XiaoBeiAt(Time);
        return Backstage::FanAt(Time);
    }

    FName BuildSequenceKey(EBackstageRole Role, EBackstageAnimState State)
    {
        FString Prefix = "Artist";
        if (Role == EBackstageRole::Volunteer) Prefix = "Volunteer";
        else if (Role == EBackstageRole::Coordinator) Prefix = "Coordinator";
        else if (Role == EBackstageRole::LinWan) Prefix = "LinWan";
        else if (Role == EBackstageRole::XiaoBei) Prefix = "XiaoBei";
        else if (Role == EBackstageRole::Fan) Prefix = "Fan";

        switch (State)
        {
            case EBackstageAnimState::Walk: return FName(Prefix + "_Walk");
            case EBackstageAnimState::Shock: return FName(Prefix + "_Shock");
            case EBackstageAnimState::Argue: return FName(Prefix + "_Argue");
            case EBackstageAnimState::Pickup: return FName(Prefix + "_Pickup");
            case EBackstageAnimState::Care: return FName(Prefix + "_Care");
            case EBackstageAnimState::Handoff: return FName(Prefix + "_Handoff");
            case EBackstageAnimState::Warm: return FName(Prefix + "_Warm");
            default: return FName(Prefix + "_Idle");
        }
    }

    FBackstagePuppetPose EvaluateLevel1Pose(EBackstageRole Role, float Time)
    {
        FBackstagePuppetPose Pose;
        const float ClampedTime = Math::Max(Time, 0.0);
        const float PrevTime = Math::Max(0.0, ClampedTime - 0.12);
        const float NextTime = Math::Min(36.0, ClampedTime + 0.12);
        const float VelX = (RolePositionAt(Role, NextTime).X - RolePositionAt(Role, PrevTime).X) / Math::Max(NextTime - PrevTime, 0.01);
        const float Speed = Math::Abs(VelX);
        const auto Beat = Backstage::RainBeatAt(ClampedTime);

        if (Role == EBackstageRole::LinWan)
        {
            Pose.FacingSign = 1.0;
            if (Beat == EBackstageRainBeat::EventIdle || Beat == EBackstageRainBeat::RainStart)
            {
                Pose.State = Beat == EBackstageRainBeat::EventIdle ? EBackstageAnimState::Warm : EBackstageAnimState::Idle;
                Pose.Expression = Beat == EBackstageRainBeat::EventIdle ? EBackstageExpression::Warm : EBackstageExpression::Calm;
            }
            else if (Beat == EBackstageRainBeat::SnatchUmbrella)
            {
                Pose.State = ClampedTime < 16.0 ? EBackstageAnimState::Argue : EBackstageAnimState::Walk;
                Pose.Expression = ClampedTime < 16.0 ? EBackstageExpression::Angry : EBackstageExpression::Focus;
                if (ClampedTime < 15.5)
                {
                    Pose.FacingSign = -1.0;
                    Pose.EmotionBadge = n"Emotion_Anger";
                }
            }
            else
            {
                Pose.State = Speed > 0.08 ? EBackstageAnimState::Walk : EBackstageAnimState::Idle;
                Pose.Expression = Beat == EBackstageRainBeat::PassingFan ? EBackstageExpression::Calm : EBackstageExpression::Focus;
            }
        }
        else if (Role == EBackstageRole::XiaoBei)
        {
            Pose.FacingSign = 1.0;
            if (Beat == EBackstageRainBeat::EventIdle)
            {
                Pose.State = EBackstageAnimState::Idle;
                Pose.Expression = EBackstageExpression::Calm;
            }
            else if (Beat == EBackstageRainBeat::RainStart)
            {
                Pose.State = Speed > 0.08 ? EBackstageAnimState::Walk : EBackstageAnimState::Care;
                Pose.Expression = EBackstageExpression::Focus;
                Pose.EmotionBadge = n"Emotion_Care";
            }
            else
            {
                Pose.State = EBackstageAnimState::Shock;
                Pose.Expression = EBackstageExpression::Shock;
                if (ClampedTime < 22.0)
                    Pose.EmotionBadge = n"Emotion_Shock";
            }
        }
        else
        {
            Pose.FacingSign = -1.0;
            if (Beat == EBackstageRainBeat::PassingFan || Beat == EBackstageRainBeat::EventIdle)
            {
                Pose.State = EBackstageAnimState::Warm;
                Pose.Expression = EBackstageExpression::Warm;
                if (Beat == EBackstageRainBeat::PassingFan)
                    Pose.EmotionBadge = n"Emotion_Warm";
            }
            else
            {
                Pose.State = EBackstageAnimState::Care;
                Pose.Expression = EBackstageExpression::Focus;
            }
        }

        float FPS = 6.0;
        if (Pose.State == EBackstageAnimState::Walk) FPS = 8.0;
        else if (Pose.State == EBackstageAnimState::Shock || Pose.State == EBackstageAnimState::Argue) FPS = 9.0;

        Pose.FrameIndex = Math::Clamp(Math::FloorToInt(ClampedTime * FPS) % Pose.FrameCount, 0, Pose.FrameCount - 1);
        Pose.SequenceKey = BuildSequenceKey(Role, Pose.State);
        return Pose;
    }

    FBackstagePuppetPose EvaluatePose(EBackstageRole Role, float Time)
    {
        if (Role == EBackstageRole::LinWan || Role == EBackstageRole::XiaoBei || Role == EBackstageRole::Fan)
            return EvaluateLevel1Pose(Role, Time);
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
    FName LastSequenceKey = NAME_None;
    int LastFrameIndex = -1;
    float LastFacingSign = 0.0;
    bool bPuppetVisible = true;

    void SetPuppetVisible(bool bVisible)
    {
        bPuppetVisible = bVisible;
        ShadowSprite.BackstageSetVisible(bVisible);
        BodySprite.BackstageSetVisible(bVisible);
        if (!bVisible)
            EmotionSprite.BackstageSetVisible(false);
    }

    void Setup(EBackstageRole InRole, FLinearColor InColor)
    {
        Role = InRole;
        RoleColor = InColor;
        LastEmotionBadge = NAME_None;
        LastSequenceKey = NAME_None;
        LastFrameIndex = -1;
        LastFacingSign = 0.0;
        bPuppetVisible = true;

        ShadowSprite.BackstageSetupPart(n"Shadow", FVector2D(74.0, 38.0), FVector2D(0.5, 0.5),
            FLinearColor(0.02, 0.04, 0.06, 0.72), false, false);
        ShadowSprite.SetRelativeLocation(FVector(0.0, 0.0, 1.5));
        ShadowSprite.SetRelativeRotation(FRotator(0.0, 0.0, -90.0));

        BodySprite.SetRelativeLocation(FVector(0.0, 0.0, 0.0));
        EmotionSprite.SetRelativeLocation(FVector(0.0, 6.0, 166.0));
        EmotionSprite.BackstageSetVisible(false);
    }

    void Sync(float Time, FVector WorldFootPos, float CameraYawDeg, float CameraPitchDeg = 0.0)
    {
        if (!bPuppetVisible)
            return;
        BackstageSetTransform(WorldFootPos, FRotator(0.0, CameraYawDeg + 90.0, CameraPitchDeg * 0.55));

        const FBackstagePuppetPose Pose = BackstagePuppet::EvaluatePose(Role, Time);
        const bool bIllustrated = (Role == EBackstageRole::LinWan || Role == EBackstageRole::XiaoBei);
        const FVector2D SpriteSize = bIllustrated ? FVector2D(82.0, 162.0) : FVector2D(104.0, 156.0);
        if (Pose.SequenceKey != LastSequenceKey || Pose.FrameIndex != LastFrameIndex)
        {
            BodySprite.BackstageSetSpriteFrame(Pose.SequenceKey, Pose.FrameIndex, SpriteSize,
                FVector2D(0.5, 1.0), RoleColor, true, false);
            LastSequenceKey = Pose.SequenceKey;
            LastFrameIndex = Pose.FrameIndex;
        }
        const float WalkBob = (Pose.SequenceKey == n"LinWan_Walk" || Pose.SequenceKey == n"XiaoBei_Walk"
            || Pose.SequenceKey == n"Artist_Walk" || Pose.SequenceKey == n"Coordinator_Walk")
            ? Math::Abs(Math::Sin(Time * 8.5)) * 4.5
            : Math::Sin(Time * 2.4) * 1.2;
        BodySprite.SetRelativeLocation(FVector(0.0, 0.0, WalkBob));
        if (Math::Abs(Pose.FacingSign - LastFacingSign) > 0.01)
        {
            BodySprite.BackstageSetScale(FVector(Pose.FacingSign, 1.0, 1.0));
            LastFacingSign = Pose.FacingSign;
        }

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
    UStaticMeshComponent StageCarpetMesh;

    UPROPERTY(DefaultComponent, Attach = Root)
    UStaticMeshComponent StageBorderMesh;

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

    UPROPERTY(DefaultComponent, Attach = Root)
    UStaticMeshComponent Pillar0;

    UPROPERTY(DefaultComponent, Attach = Root)
    UStaticMeshComponent Pillar1;

    UPROPERTY(DefaultComponent, Attach = Root)
    UStaticMeshComponent Pillar2;

    UPROPERTY(DefaultComponent, Attach = Root)
    UStaticMeshComponent Pillar3;

    UPROPERTY(DefaultComponent, Attach = Root)
    UStaticMeshComponent Pillar4;

    UPROPERTY(DefaultComponent, Attach = Root)
    UStaticMeshComponent LightCan0;

    UPROPERTY(DefaultComponent, Attach = Root)
    UStaticMeshComponent LightCan1;

    UPROPERTY(DefaultComponent, Attach = Root)
    UStaticMeshComponent LightCan2;

    UPROPERTY(DefaultComponent, Attach = Root)
    UStaticMeshComponent LightCan3;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent EditorArtistSprite;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent EditorVolunteerSprite;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent EditorCoordinatorSprite;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent UmbrellaSprite;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent UmbrellaShadow;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent FanSignSprite;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent BrandBannerSprite;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent BackdropSprite;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent RainSprite0;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent RainSprite1;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent RainSprite2;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent RainSprite3;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent RainSprite4;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent RainSprite5;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent RainSprite6;

    UPROPERTY(DefaultComponent, Attach = Root)
    UPaperSpriteComponent RainSprite7;

    ABackstagePuppet ArtistPuppet = nullptr;
    ABackstagePuppet VolunteerPuppet = nullptr;
    ABackstagePuppet CoordinatorPuppet = nullptr;
    ABackstagePuppet LinWanPuppet = nullptr;
    ABackstagePuppet XiaoBeiPuppet = nullptr;
    ABackstagePuppet FanPuppet = nullptr;
    bool bInitialized = false;
    int ActiveLevelIndex = -1;
    bool bLastUmbrellaOpen = false;
    bool bHasSetUmbrella = false;

    FVector StagePoint(FVector2D Pt, float HeightUU)
    {
        return FVector(-Pt.Y * 100.0, Pt.X * 100.0, HeightUU);
    }

    void ConfigureStageMesh(UStaticMeshComponent Mesh, FString MeshPath, FVector RelLoc, FVector RelScale,
        FLinearColor Color, float Roughness, float Metallic, bool bCastShadow)
    {
        Mesh.BackstageSetupStageMesh(MeshPath, Color, Roughness, Metallic, bCastShadow);
        Mesh.SetRelativeLocation(RelLoc);
        Mesh.BackstageSetScale(RelScale);
    }

    void SetRainVisible(bool bShow)
    {
        RainSprite0.BackstageSetVisible(bShow);
        RainSprite1.BackstageSetVisible(bShow);
        RainSprite2.BackstageSetVisible(bShow);
        RainSprite3.BackstageSetVisible(bShow);
        RainSprite4.BackstageSetVisible(bShow);
        RainSprite5.BackstageSetVisible(bShow);
        RainSprite6.BackstageSetVisible(bShow);
        RainSprite7.BackstageSetVisible(bShow);
    }

    void UpdateRainDrop(UPaperSpriteComponent Drop, int Index, float Time, FRotator RainRot)
    {
        if (Drop == nullptr)
            return;
        const float Phase = Math::Fmod(Time * (1.8 + float(Index % 3) * 0.35) + float(Index) * 0.21, 1.0);
        const float StageX = -2.4 + float(Index) * 0.68 + Math::Sin(Time * 1.4 + float(Index)) * 0.14;
        const float StageY = -0.35 + float(Index % 3) * 0.48;
        const float HeightUU = 340.0 - Phase * 310.0;
        Drop.SetRelativeLocation(StagePoint(FVector2D(StageX, StageY), HeightUU));
        Drop.SetRelativeRotation(RainRot);
    }

    void UpdateRain(float Time, bool bRaining, float CameraYaw, float CameraPitch)
    {
        SetRainVisible(bRaining);
        if (!bRaining)
            return;

        const FRotator RainRot(-14.0, CameraYaw + 90.0, CameraPitch * 0.35);
        UpdateRainDrop(RainSprite0, 0, Time, RainRot);
        UpdateRainDrop(RainSprite1, 1, Time, RainRot);
        UpdateRainDrop(RainSprite2, 2, Time, RainRot);
        UpdateRainDrop(RainSprite3, 3, Time, RainRot);
        UpdateRainDrop(RainSprite4, 4, Time, RainRot);
        UpdateRainDrop(RainSprite5, 5, Time, RainRot);
        UpdateRainDrop(RainSprite6, 6, Time, RainRot);
        UpdateRainDrop(RainSprite7, 7, Time, RainRot);
    }

    void BuildStageLayout(int LevelIndex = 0)
    {
        const FString CubePath = "/Engine/BasicShapes/Cube.Cube";
        const FString CylinderPath = "/Engine/BasicShapes/Cylinder.Cylinder";
        const bool bRainLevel = LevelIndex == 0;

        if (bRainLevel)
        {
            ConfigureStageMesh(FloorMesh, CubePath, FVector(120.0, 0.0, -20.0), FVector(12.0, 18.0, 0.4),
                FLinearColor(0.015, 0.020, 0.030, 1.0), 0.72, 0.02, false);
            ConfigureStageMesh(StageBorderMesh, CubePath, FVector(15.0, 0.0, -0.5), FVector(5.1, 7.6, 0.03),
                FLinearColor(0.05, 0.06, 0.08, 1.0), 0.65, 0.15, false);
            ConfigureStageMesh(StageCarpetMesh, CubePath, FVector(15.0, 0.0, 1.2), FVector(4.8, 7.2, 0.03),
                FLinearColor(0.12, 0.02, 0.035, 1.0), 0.85, 0.02, false);
            FloorMesh.BackstageSetVisible(false);
            StageBorderMesh.BackstageSetVisible(false);
            StageCarpetMesh.BackstageSetVisible(false);
            ConfigureStageMesh(BackWallMesh, CubePath, FVector(280.0, 0.0, 300.0), FVector(0.4, 18.0, 6.0),
                FLinearColor(0.03, 0.05, 0.095, 1.0), 0.78, 0.08, false);
            ConfigureStageMesh(LeftCurtainMesh, CubePath, FVector(20.0, -680.0, 260.0), FVector(5.2, 0.5, 5.2),
                FLinearColor(0.025, 0.04, 0.075, 1.0), 0.88, 0.0, false);
            ConfigureStageMesh(RightCurtainMesh, CubePath, FVector(20.0, 680.0, 260.0), FVector(5.2, 0.5, 5.2),
                FLinearColor(0.025, 0.04, 0.075, 1.0), 0.88, 0.0, false);
            ConfigureStageMesh(TrussMesh, CubePath, FVector(-140.0, 0.0, 420.0), FVector(0.35, 16.0, 0.35),
                FLinearColor(0.12, 0.15, 0.20, 1.0), 0.45, 0.65, false);
        }
        else
        {
            ConfigureStageMesh(FloorMesh, CubePath, FVector(120.0, 0.0, -20.0), FVector(12.0, 18.0, 0.4),
                FLinearColor(0.035, 0.048, 0.068, 1.0), 0.75, 0.04, false);
            ConfigureStageMesh(StageBorderMesh, CubePath, FVector(15.0, 0.0, -0.5), FVector(5.1, 7.6, 0.03),
                FLinearColor(0.10, 0.13, 0.18, 1.0), 0.60, 0.25, false);
            ConfigureStageMesh(StageCarpetMesh, CubePath, FVector(15.0, 0.0, 1.2), FVector(4.8, 7.2, 0.03),
                FLinearColor(0.07, 0.10, 0.15, 1.0), 0.82, 0.02, false);
            ConfigureStageMesh(BackWallMesh, CubePath, FVector(280.0, 0.0, 300.0), FVector(0.4, 18.0, 6.0),
                FLinearColor(0.05, 0.08, 0.12, 1.0), 0.85, 0.05, false);
            ConfigureStageMesh(LeftCurtainMesh, CubePath, FVector(20.0, -680.0, 260.0), FVector(5.2, 0.5, 5.2),
                FLinearColor(0.035, 0.05, 0.08, 1.0), 0.92, 0.0, false);
            ConfigureStageMesh(RightCurtainMesh, CubePath, FVector(20.0, 680.0, 260.0), FVector(5.2, 0.5, 5.2),
                FLinearColor(0.035, 0.05, 0.08, 1.0), 0.92, 0.0, false);
            ConfigureStageMesh(TrussMesh, CubePath, FVector(-140.0, 0.0, 420.0), FVector(0.35, 16.0, 0.35),
                FLinearColor(0.10, 0.13, 0.18, 1.0), 0.45, 0.65, false);
        }

        ConfigureStageMesh(Pillar0, CylinderPath, StagePoint(FVector2D(-4.0, -0.9), 210.0), FVector(0.32, 0.32, 4.2),
            FLinearColor(0.12, 0.16, 0.22, 1.0), 0.58, 0.25, false);
        ConfigureStageMesh(Pillar1, CylinderPath, StagePoint(FVector2D(-2.0, -0.9), 210.0), FVector(0.32, 0.32, 4.2),
            FLinearColor(0.12, 0.16, 0.22, 1.0), 0.58, 0.25, false);
        ConfigureStageMesh(Pillar2, CylinderPath, StagePoint(FVector2D(0.0, -0.9), 210.0), FVector(0.32, 0.32, 4.2),
            FLinearColor(0.12, 0.16, 0.22, 1.0), 0.58, 0.25, false);
        ConfigureStageMesh(Pillar3, CylinderPath, StagePoint(FVector2D(2.0, -0.9), 210.0), FVector(0.32, 0.32, 4.2),
            FLinearColor(0.12, 0.16, 0.22, 1.0), 0.58, 0.25, false);
        ConfigureStageMesh(Pillar4, CylinderPath, StagePoint(FVector2D(4.0, -0.9), 210.0), FVector(0.32, 0.32, 4.2),
            FLinearColor(0.12, 0.16, 0.22, 1.0), 0.58, 0.25, false);

        // Hide primitive background walls/pillars in favor of the illustrated stage backdrop
        BackWallMesh.BackstageSetVisible(false);
        LeftCurtainMesh.BackstageSetVisible(false);
        RightCurtainMesh.BackstageSetVisible(false);
        Pillar0.BackstageSetVisible(false);
        Pillar1.BackstageSetVisible(false);
        Pillar2.BackstageSetVisible(false);
        Pillar3.BackstageSetVisible(false);
        Pillar4.BackstageSetVisible(false);

        BackdropSprite.BackstageSetupPart(bRainLevel ? n"Backdrop_Rain" : n"Backdrop_Charity",
            FVector2D(1560.0, 878.0), FVector2D(0.5, 0.5), FLinearColor::White, false, false);
        BackdropSprite.SetRelativeLocation(FVector(250.0, 0.0, 210.0));
        BackdropSprite.SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
        BackdropSprite.BackstageSetVisible(true);

        ConfigureStageMesh(LightCan0, CylinderPath, FVector(-140.0, -360.0, 396.0), FVector(0.24, 0.24, 0.38),
            FLinearColor(0.22, 0.26, 0.32, 1.0), 0.28, 0.85, false);
        LightCan0.SetRelativeRotation(FRotator(-24.0, 0.0, 0.0));
        ConfigureStageMesh(LightCan1, CylinderPath, FVector(-140.0, -120.0, 396.0), FVector(0.24, 0.24, 0.38),
            FLinearColor(0.22, 0.26, 0.32, 1.0), 0.28, 0.85, false);
        LightCan1.SetRelativeRotation(FRotator(-24.0, 0.0, 0.0));
        ConfigureStageMesh(LightCan2, CylinderPath, FVector(-140.0, 120.0, 396.0), FVector(0.24, 0.24, 0.38),
            FLinearColor(0.22, 0.26, 0.32, 1.0), 0.28, 0.85, false);
        LightCan2.SetRelativeRotation(FRotator(-24.0, 0.0, 0.0));
        ConfigureStageMesh(LightCan3, CylinderPath, FVector(-140.0, 360.0, 396.0), FVector(0.24, 0.24, 0.38),
            FLinearColor(0.22, 0.26, 0.32, 1.0), 0.28, 0.85, false);
        LightCan3.SetRelativeRotation(FRotator(-24.0, 0.0, 0.0));

        if (bRainLevel)
        {
            KeySpot.BackstageSetupSpotLight(FVector(-120.0, -20.0, 415.0), FRotator(-65.0, 10.0, 0.0),
                FLinearColor(0.86, 0.94, 1.0, 1.0), 1800.0, 2600.0, 30.0, 62.0, false);
            RimSpotLeft.BackstageSetupSpotLight(FVector(160.0, -440.0, 320.0), FRotator(-28.0, 110.0, 0.0),
                FLinearColor(0.32, 0.68, 1.0, 1.0), 1100.0, 2200.0, 24.0, 52.0, false);
            RimSpotRight.BackstageSetupSpotLight(FVector(160.0, 440.0, 320.0), FRotator(-28.0, -110.0, 0.0),
                FLinearColor(1.0, 0.42, 0.56, 1.0), 1100.0, 2200.0, 24.0, 52.0, false);
            StageFill.BackstageSetupPointLight(FVector(-260.0, 0.0, 220.0),
                FLinearColor(0.60, 0.76, 0.98, 1.0), 450.0, 2200.0, false);
            BounceDir.BackstageSetupDirectionalLight(FRotator(-35.0, 18.0, 0.0),
                FLinearColor(0.70, 0.84, 1.0, 1.0), 0.22, false);
            StageFog.BackstageSetupFog(0.0005, FLinearColor(0.04, 0.07, 0.13, 1.0), false);
        }
        else
        {
            KeySpot.BackstageSetupSpotLight(FVector(-120.0, -40.0, 410.0), FRotator(-66.0, 14.0, 0.0),
                FLinearColor(1.0, 0.93, 0.82, 1.0), 1600.0, 2400.0, 28.0, 56.0, false);
            RimSpotLeft.BackstageSetupSpotLight(FVector(160.0, -440.0, 320.0), FRotator(-28.0, 110.0, 0.0),
                FLinearColor(0.26, 0.64, 1.0, 1.0), 950.0, 2200.0, 22.0, 50.0, false);
            RimSpotRight.BackstageSetupSpotLight(FVector(160.0, 440.0, 320.0), FRotator(-28.0, -110.0, 0.0),
                FLinearColor(1.0, 0.34, 0.48, 1.0), 950.0, 2200.0, 22.0, 50.0, false);
            StageFill.BackstageSetupPointLight(FVector(-260.0, 0.0, 220.0),
                FLinearColor(0.68, 0.80, 0.98, 1.0), 420.0, 2000.0, false);
            BounceDir.BackstageSetupDirectionalLight(FRotator(-35.0, 18.0, 0.0),
                FLinearColor(0.75, 0.85, 0.98, 1.0), 0.20, false);
            StageFog.BackstageSetupFog(0.0004, FLinearColor(0.05, 0.09, 0.14, 1.0), false);
        }

        ParcelShadow.BackstageSetupPart(n"Shadow", FVector2D(46.0, 26.0), FVector2D(0.5, 0.5),
            FLinearColor(0.02, 0.04, 0.06, 0.65), false, false);
        ParcelShadow.SetRelativeRotation(FRotator(0.0, 0.0, -90.0));
        const FVector InitParcelLoc = StagePoint(Backstage::ParcelAt(0.0), 1.5);
        ParcelShadow.SetRelativeLocation(FVector(InitParcelLoc.X - 6.0, InitParcelLoc.Y, 1.5));

        const FLinearColor Gold(1.0, 0.78, 0.38, 1.0);
        ParcelSprite.BackstageSetupPart(n"Parcel", FVector2D(36.0, 36.0), FVector2D(0.5, 0.5), Gold, true, false);
        ParcelSprite.SetRelativeLocation(FVector(InitParcelLoc.X - 8.0, InitParcelLoc.Y, 80.0));
        ParcelSprite.SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
        ScatterLeft.BackstageSetupPart(n"Parcel", FVector2D(20.0, 20.0), FVector2D(0.5, 0.5), Gold, true, false);
        ScatterRight.BackstageSetupPart(n"Parcel", FVector2D(22.0, 22.0), FVector2D(0.5, 0.5), Gold, true, false);
        ParcelShadow.BackstageSetVisible(!bRainLevel);
        ParcelSprite.BackstageSetVisible(!bRainLevel);
        ScatterLeft.BackstageSetVisible(false);
        ScatterRight.BackstageSetVisible(false);

        BrandBannerSprite.BackstageSetupPart(n"BrandBanner", FVector2D(220.0, 110.0), FVector2D(0.5, 0.5),
            FLinearColor::White, false, false);
        BrandBannerSprite.SetRelativeLocation(StagePoint(FVector2D(0.0, -0.78), 145.0));
        BrandBannerSprite.SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
        BrandBannerSprite.BackstageSetVisible(false);

        UmbrellaShadow.BackstageSetupPart(n"Shadow", FVector2D(62.0, 24.0), FVector2D(0.5, 0.5),
            FLinearColor(0.02, 0.04, 0.06, 0.60), false, false);
        UmbrellaShadow.SetRelativeRotation(FRotator(0.0, 0.0, -90.0));
        UmbrellaShadow.BackstageSetVisible(bRainLevel);

        bLastUmbrellaOpen = false;
        bHasSetUmbrella = false;
        UmbrellaSprite.BackstageSetupPart(n"UmbrellaFolded", FVector2D(82.0, 74.0), FVector2D(0.5, 0.5),
            FLinearColor::White, true, false);
        UmbrellaSprite.SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
        UmbrellaSprite.BackstageSetVisible(bRainLevel);

        FanSignSprite.BackstageSetupPart(n"FanSign", FVector2D(68.0, 52.0), FVector2D(0.5, 0.5),
            FLinearColor::White, true, false);
        FanSignSprite.SetRelativeLocation(StagePoint(Backstage::FanSignAt(0.0), 124.0));
        FanSignSprite.SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
        FanSignSprite.BackstageSetVisible(bRainLevel);

        const FLinearColor RainTint(0.75, 0.90, 1.0, 0.85);
        RainSprite0.BackstageSetupPart(n"RainStreak", FVector2D(24.0, 88.0), FVector2D(0.5, 0.5), RainTint, false, false);
        RainSprite1.BackstageSetupPart(n"RainStreak", FVector2D(24.0, 88.0), FVector2D(0.5, 0.5), RainTint, false, false);
        RainSprite2.BackstageSetupPart(n"RainStreak", FVector2D(24.0, 88.0), FVector2D(0.5, 0.5), RainTint, false, false);
        RainSprite3.BackstageSetupPart(n"RainStreak", FVector2D(24.0, 88.0), FVector2D(0.5, 0.5), RainTint, false, false);
        RainSprite4.BackstageSetupPart(n"RainStreak", FVector2D(24.0, 88.0), FVector2D(0.5, 0.5), RainTint, false, false);
        RainSprite5.BackstageSetupPart(n"RainStreak", FVector2D(24.0, 88.0), FVector2D(0.5, 0.5), RainTint, false, false);
        RainSprite6.BackstageSetupPart(n"RainStreak", FVector2D(24.0, 88.0), FVector2D(0.5, 0.5), RainTint, false, false);
        RainSprite7.BackstageSetupPart(n"RainStreak", FVector2D(24.0, 88.0), FVector2D(0.5, 0.5), RainTint, false, false);
        SetRainVisible(false);

        if (bRainLevel)
        {
            EditorArtistSprite.BackstageSetSpriteFrame(n"LinWan_Idle", 0, FVector2D(104.0, 156.0),
                FVector2D(0.5, 1.0), FLinearColor::White, true, false);
            EditorArtistSprite.SetRelativeLocation(StagePoint(Backstage::LinWanAt(0.0), 0.0));
            EditorArtistSprite.SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
            EditorArtistSprite.BackstageSetScale(FVector(1.0, 1.0, 1.0));
            EditorArtistSprite.BackstageSetVisible(!bInitialized);

            EditorVolunteerSprite.BackstageSetSpriteFrame(n"XiaoBei_Idle", 0, FVector2D(104.0, 156.0),
                FVector2D(0.5, 1.0), FLinearColor::White, true, false);
            EditorVolunteerSprite.SetRelativeLocation(StagePoint(Backstage::XiaoBeiAt(0.0), 0.0));
            EditorVolunteerSprite.SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
            EditorVolunteerSprite.BackstageSetScale(FVector(1.0, 1.0, 1.0));
            EditorVolunteerSprite.BackstageSetVisible(!bInitialized);

            EditorCoordinatorSprite.BackstageSetSpriteFrame(n"Fan_Warm", 0, FVector2D(104.0, 156.0),
                FVector2D(0.5, 1.0), FLinearColor::White, true, false);
            EditorCoordinatorSprite.SetRelativeLocation(StagePoint(Backstage::FanAt(0.0), 0.0));
            EditorCoordinatorSprite.SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
            EditorCoordinatorSprite.BackstageSetScale(FVector(-1.0, 1.0, 1.0));
            EditorCoordinatorSprite.BackstageSetVisible(!bInitialized);
        }
        else
        {
            EditorArtistSprite.BackstageSetSpriteFrame(n"Artist_Idle", 0, FVector2D(104.0, 156.0),
                FVector2D(0.5, 1.0), FLinearColor(0.97, 0.36, 0.46, 1.0), true, false);
            EditorArtistSprite.SetRelativeLocation(StagePoint(FVector2D(-0.95, 0.45), 0.0));
            EditorArtistSprite.SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
            EditorArtistSprite.BackstageSetVisible(!bInitialized);

            EditorVolunteerSprite.BackstageSetSpriteFrame(n"Volunteer_Idle", 0, FVector2D(104.0, 156.0),
                FVector2D(0.5, 1.0), FLinearColor(0.30, 0.66, 0.98, 1.0), true, false);
            EditorVolunteerSprite.SetRelativeLocation(StagePoint(Backstage::VolunteerAt(12.0), 0.0));
            EditorVolunteerSprite.SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
            EditorVolunteerSprite.BackstageSetScale(FVector(-1.0, 1.0, 1.0));
            EditorVolunteerSprite.BackstageSetVisible(!bInitialized);

            EditorCoordinatorSprite.BackstageSetSpriteFrame(n"Coordinator_Idle", 0, FVector2D(104.0, 156.0),
                FVector2D(0.5, 1.0), FLinearColor(0.37, 0.84, 0.61, 1.0), true, false);
            EditorCoordinatorSprite.SetRelativeLocation(StagePoint(FVector2D(1.90, 0.70), 0.0));
            EditorCoordinatorSprite.SetRelativeRotation(FRotator(0.0, 90.0, 0.0));
            EditorCoordinatorSprite.BackstageSetScale(FVector(-1.0, 1.0, 1.0));
            EditorCoordinatorSprite.BackstageSetVisible(!bInitialized);
        }
    }

    UFUNCTION(BlueprintOverride)
    void UserConstructionScript()
    {
        BuildStageLayout(0);
    }

    void InitializeStage(APlayerController Controller, int LevelIndex = 0)
    {
        if (Controller == nullptr)
            return;
        if (bInitialized && ActiveLevelIndex == LevelIndex)
            return;

        if (!bInitialized)
        {
            bInitialized = true;
            Controller.BackstagePrewarmSprites();
        }
        ActiveLevelIndex = LevelIndex;
        BuildStageLayout(LevelIndex);

        EditorArtistSprite.BackstageSetVisible(false);
        EditorVolunteerSprite.BackstageSetVisible(false);
        EditorCoordinatorSprite.BackstageSetVisible(false);

        if (LinWanPuppet == nullptr)
        {
            LinWanPuppet = ABackstagePuppet::Spawn(FVector::ZeroVector, FRotator::ZeroRotator);
            LinWanPuppet.Setup(EBackstageRole::LinWan, FLinearColor::White);
        }
        if (XiaoBeiPuppet == nullptr)
        {
            XiaoBeiPuppet = ABackstagePuppet::Spawn(FVector::ZeroVector, FRotator::ZeroRotator);
            XiaoBeiPuppet.Setup(EBackstageRole::XiaoBei, FLinearColor::White);
        }
        if (FanPuppet == nullptr)
        {
            FanPuppet = ABackstagePuppet::Spawn(FVector::ZeroVector, FRotator::ZeroRotator);
            FanPuppet.Setup(EBackstageRole::Fan, FLinearColor::White);
        }

        if (ArtistPuppet == nullptr)
        {
            ArtistPuppet = ABackstagePuppet::Spawn(FVector::ZeroVector, FRotator::ZeroRotator);
            ArtistPuppet.Setup(EBackstageRole::Artist, FLinearColor(0.97, 0.36, 0.46, 1.0));
        }
        if (VolunteerPuppet == nullptr)
        {
            VolunteerPuppet = ABackstagePuppet::Spawn(FVector::ZeroVector, FRotator::ZeroRotator);
            VolunteerPuppet.Setup(EBackstageRole::Volunteer, FLinearColor(0.30, 0.66, 0.98, 1.0));
        }
        if (CoordinatorPuppet == nullptr)
        {
            CoordinatorPuppet = ABackstagePuppet::Spawn(FVector::ZeroVector, FRotator::ZeroRotator);
            CoordinatorPuppet.Setup(EBackstageRole::Coordinator, FLinearColor(0.37, 0.84, 0.61, 1.0));
        }

        const bool bRainLevel = LevelIndex == 0;
        LinWanPuppet.SetPuppetVisible(bRainLevel);
        XiaoBeiPuppet.SetPuppetVisible(bRainLevel);
        FanPuppet.SetPuppetVisible(bRainLevel);
        ArtistPuppet.SetPuppetVisible(!bRainLevel);
        VolunteerPuppet.SetPuppetVisible(!bRainLevel);
        CoordinatorPuppet.SetPuppetVisible(!bRainLevel);
    }

    void SyncToView(APlayerController Controller, const FBackstagePhoto&in View, bool bActivateCamera, int LevelIndex = 0)
    {
        if (Controller == nullptr)
            return;
        InitializeStage(Controller, LevelIndex);

        const float CamAngleRad = Backstage::EffectiveAngle(View);
        const float CamCenter = Backstage::EffectiveCenter(View);
        const float CameraYaw = Math::RadiansToDegrees(CamAngleRad);
        const float CameraPitch = Backstage::CameraPitch(View.Tilt);
        Controller.BackstageSyncCamera(this, CamAngleRad, CamCenter, View.Pan, View.Tilt, View.Zoom, bActivateCamera);

        const FRotator FacingCam(0.0, CameraYaw + 90.0, CameraPitch * 0.55);

        if (LevelIndex == 0)
        {
            if (LinWanPuppet != nullptr)
                LinWanPuppet.Sync(View.Time, Controller.BackstageWorldPoint(Backstage::LinWanAt(View.Time), 0.0), CameraYaw, CameraPitch);
            if (XiaoBeiPuppet != nullptr)
                XiaoBeiPuppet.Sync(View.Time, Controller.BackstageWorldPoint(Backstage::XiaoBeiAt(View.Time), 0.0), CameraYaw, CameraPitch);
            if (FanPuppet != nullptr)
                FanPuppet.Sync(View.Time, Controller.BackstageWorldPoint(Backstage::FanAt(View.Time), 0.0), CameraYaw, CameraPitch);

            const bool bRaining = Backstage::IsRainingAt(0, View.Time);
            const bool bUmbrellaOpen = View.Time >= 6.0;
            if (!bHasSetUmbrella || bUmbrellaOpen != bLastUmbrellaOpen)
            {
                UmbrellaSprite.BackstageSetupPart(bUmbrellaOpen ? n"Umbrella" : n"UmbrellaFolded",
                    FVector2D(bUmbrellaOpen ? 92.0 : 48.0, bUmbrellaOpen ? 82.0 : 68.0),
                    FVector2D(0.5, 0.5), FLinearColor::White, true, false);
                bLastUmbrellaOpen = bUmbrellaOpen;
                bHasSetUmbrella = true;
            }

            const FVector2D UmbStage = Backstage::UmbrellaAt(View.Time);
            const FVector UmbWorld = Controller.BackstageWorldPoint(UmbStage, bUmbrellaOpen ? 196.0 : 108.0);
            const FVector UmbFloor = Controller.BackstageWorldPoint(UmbStage, 1.5);
            UmbrellaShadow.SetRelativeLocation(FVector(UmbFloor.X - 4.0, UmbFloor.Y, 1.5));
            UmbrellaSprite.SetRelativeLocation(FVector(UmbWorld.X + 4.0, UmbWorld.Y, UmbWorld.Z));
            UmbrellaSprite.SetRelativeRotation(FacingCam);

            const FVector2D SignStage = Backstage::FanSignAt(View.Time);
            const float SignBob = Math::Sin(View.Time * 3.8) * 4.0;
            const FVector SignWorld = Controller.BackstageWorldPoint(SignStage, 126.0 + SignBob);
            FanSignSprite.SetRelativeLocation(FVector(SignWorld.X - 6.0, SignWorld.Y, SignWorld.Z));
            FanSignSprite.SetRelativeRotation(FacingCam);

            UpdateRain(View.Time, bRaining, CameraYaw, CameraPitch);
            return;
        }

        if (ArtistPuppet != nullptr)
            ArtistPuppet.Sync(View.Time, Controller.BackstageWorldPoint(Backstage::ArtistAt(View.Time), 0.0), CameraYaw, CameraPitch);
        if (VolunteerPuppet != nullptr)
            VolunteerPuppet.Sync(View.Time, Controller.BackstageWorldPoint(Backstage::VolunteerAt(View.Time), 0.0), CameraYaw, CameraPitch);
        if (CoordinatorPuppet != nullptr)
        {
            FVector CoordPos = Controller.BackstageWorldPoint(Backstage::CoordinatorAt(View.Time), 0.0);
            CoordPos.X += 4.0;
            CoordinatorPuppet.Sync(View.Time, CoordPos, CameraYaw, CameraPitch);
        }

        const auto Beat = Backstage::BeatAt(View.Time);
        const bool bDropped = Beat == EBackstageBeat::Collision || Beat == EBackstageBeat::Argument;
        const float ParcelHeight = bDropped ? 18.0 : (Beat == EBackstageBeat::Pickup ? 40.0 : 80.0);
        const FVector2D ParcelStage = Backstage::ParcelAt(View.Time);
        const FVector ParcelFloor = Controller.BackstageWorldPoint(ParcelStage, 1.5);

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
