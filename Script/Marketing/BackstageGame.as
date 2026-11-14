class ABackstagePawn : APawn
{
    UPROPERTY(DefaultComponent, RootComponent)
    USceneComponent Root;
}

class ABackstageGameMode : AGameModeBase
{
    default DefaultPawnClass = ABackstagePawn;
    default HUDClass = ABackstageHUD;
    default PlayerControllerClass = ABackstageController;
}

class ABackstageController : APlayerController
{
    FBackstageRound Round;
    ABackstageStageActor StageActor = nullptr;
    int EditSlot = 0;
    bool bWasA = false;
    bool bWasD = false;
    bool bWasQ = false;
    bool bWasE = false;
    bool bMapChecked = false;
    bool bInMainMenu = false;
    float MenuClock = 0.0;
    float LiveArmTimer = 0.0;

    void EnsureStage()
    {
        if (StageActor == nullptr)
            StageActor = Cast<ABackstageStageActor>(BackstageFindStageActor());
        if (StageActor == nullptr)
            StageActor = ABackstageStageActor::Spawn(FVector::ZeroVector, FRotator::ZeroRotator);
        if (StageActor != nullptr)
            StageActor.InitializeStage(this, Round.LevelIndex);
    }

    bool Pressed(FName Key)
    {
        return BackstagePressed(Key);
    }

    float CameraAxis(FName Negative, FName Positive, bool&inout bWasNegative, bool&inout bWasPositive, float Delta)
    {
        const bool bNegative = BackstageHeld(Negative);
        const bool bPositive = BackstageHeld(Positive);
        const float Input = Backstage::CameraInput(!bWasNegative && (bNegative || Pressed(Negative)),
            !bWasPositive && (bPositive || Pressed(Positive)), bNegative, bPositive, Delta);
        bWasNegative = bNegative;
        bWasPositive = bPositive;
        return Input;
    }

    void HandleMainMenu()
    {
        const FVector2D Pointer = BackstagePointer();
        if (Pressed(n"Enter") || Pressed(n"SpaceBar") || Pressed(n"One"))
        {
            BackstageSetLevel(0);
            BackstageOpenMap(n"/Game/levels/Backstage");
            return;
        }
        if (Pressed(n"Two"))
        {
            BackstageSetLevel(1);
            BackstageOpenMap(n"/Game/levels/Backstage");
            return;
        }
        if (Pressed(n"Escape"))
        {
            BackstageQuitGame();
            return;
        }
        if (Pressed(n"LeftMouseButton"))
        {
            if (Pointer.X >= 420 && Pointer.X <= 860 && Pointer.Y >= 312 && Pointer.Y <= 382)
            {
                BackstageSetLevel(0);
                BackstageOpenMap(n"/Game/levels/Backstage");
                return;
            }
            if (Pointer.X >= 420 && Pointer.X <= 860 && Pointer.Y >= 402 && Pointer.Y <= 472)
            {
                BackstageSetLevel(1);
                BackstageOpenMap(n"/Game/levels/Backstage");
                return;
            }
            if (Pointer.X >= 420 && Pointer.X <= 860 && Pointer.Y >= 492 && Pointer.Y <= 552)
            {
                BackstageQuitGame();
                return;
            }
        }
    }

    void StartStageRound()
    {
        Round.StartLevel(Round.LevelIndex);
        Round.EnableBraindance();
        BackstageClearPhotos();
        EditSlot = 0;
        LiveArmTimer = 0.18;
    }

    bool ShouldShowPointer()
    {
        return bInMainMenu || Round.Phase == EBackstagePhase::Briefing
            || Round.Phase == EBackstagePhase::Live || Round.Phase == EBackstagePhase::Confirm
            || Round.Phase == EBackstagePhase::Edit || Round.Phase == EBackstagePhase::Results;
    }

    UFUNCTION(BlueprintOverride)
    void Tick(float DeltaSeconds)
    {
        if (!IsLocalController())
            return;

        MenuClock += DeltaSeconds;
        if (!bMapChecked)
        {
            bMapChecked = true;
            bInMainMenu = BackstageIsMainMenuMap();
            if (!bInMainMenu)
                Round.PrepareBriefing(BackstageGetLevel());
            SetBackstagePointer(ShouldShowPointer());
        }

        if (bInMainMenu)
        {
            HandleMainMenu();
            return;
        }

        EnsureStage();

        const auto PreviousPhase = Round.Phase;
        switch (Round.Phase)
        {
            case EBackstagePhase::Briefing:
            {
                if (Pressed(n"Enter"))
                {
                    StartStageRound();
                    break;
                }
                if (Pressed(n"M"))
                {
                    BackstageOpenMap(n"/Game/levels/MainMenu");
                    return;
                }
                if (Pressed(n"LeftMouseButton"))
                {
                    const FVector2D Pointer = BackstagePointer();
                    if (Pointer.X >= 340 && Pointer.X <= 670 && Pointer.Y >= 518 && Pointer.Y <= 568)
                        StartStageRound();
                    else if (Pointer.X >= 700 && Pointer.X <= 940 && Pointer.Y >= 518 && Pointer.Y <= 568)
                    {
                        BackstageOpenMap(n"/Game/levels/MainMenu");
                        return;
                    }
                }
                break;
            }
            case EBackstagePhase::Results:
            {
                if (Pressed(n"Enter"))
                {
                    StartStageRound();
                    break;
                }
                if (Pressed(n"N") && Round.LevelIndex == 0)
                {
                    BackstageSetLevel(1);
                    Round.PrepareBriefing(1);
                    break;
                }
                if (Pressed(n"M"))
                {
                    BackstageOpenMap(n"/Game/levels/MainMenu");
                    return;
                }
                if (Pressed(n"LeftMouseButton"))
                {
                    const FVector2D Pointer = BackstagePointer();
                    if (Pointer.X >= 48 && Pointer.X <= 278 && Pointer.Y >= 644 && Pointer.Y <= 692)
                        StartStageRound();
                    else if (Round.LevelIndex == 0 && Pointer.X >= 296 && Pointer.X <= 564 && Pointer.Y >= 644 && Pointer.Y <= 692)
                    {
                        BackstageSetLevel(1);
                        Round.PrepareBriefing(1);
                        break;
                    }
                    else if (Pointer.X >= 582 && Pointer.X <= 810 && Pointer.Y >= 644 && Pointer.Y <= 692)
                    {
                        BackstageOpenMap(n"/Game/levels/MainMenu");
                        return;
                    }
                }
                break;
            }
            case EBackstagePhase::Live:
                HandleCamera(DeltaSeconds);
                break;
            case EBackstagePhase::Confirm:
                HandleConfirm();
                break;
            case EBackstagePhase::Edit:
                HandleEdit(DeltaSeconds);
                break;
            case EBackstagePhase::Preview:
                if (!Round.bPublishing && Pressed(n"BackSpace"))
                    Round.Phase = EBackstagePhase::Edit;
                break;
        }
        Round.Advance(DeltaSeconds);
        if (Round.Phase != PreviousPhase)
        {
            SetBackstagePointer(ShouldShowPointer());
            if (Round.Phase == EBackstagePhase::Live)
                LiveArmTimer = 0.18;
        }

        if (StageActor != nullptr)
        {
            if (Round.Phase == EBackstagePhase::Preview && Round.Cut.Num() > 0)
            {
                const int Frame = Math::Clamp(
                    int(Round.PreviewTime / Backstage::PreviewSecondsPerFrame),
                    0,
                    Round.Cut.Num() - 1);
                StageActor.SyncToView(this, Round.Cut[Frame], true, Round.LevelIndex);
            }
            else if (Round.Phase == EBackstagePhase::Confirm && Round.bHasPending)
            {
                StageActor.SyncToView(this, Round.PendingPhoto, true, Round.LevelIndex);
            }
            else
            {
                StageActor.SyncToView(this, Round.Camera, true, Round.LevelIndex);
            }
        }
    }

    void HandleCamera(float Delta)
    {
        LiveArmTimer = Math::Max(0.0, LiveArmTimer - Delta);
        if (Pressed(n"One")) Round.SetTargetStation(0);
        if (Pressed(n"Two")) Round.SetTargetStation(1);
        if (Pressed(n"Three")) Round.SetTargetStation(2);

        const FVector2D DragDelta = BackstageMouseDelta();
        float PanDrag = 0.0;
        float TiltDrag = 0.0;
        if (BackstageHeld(n"LeftMouseButton") && !Pressed(n"LeftMouseButton"))
        {
            PanDrag = -DragDelta.X * 1.15;
            TiltDrag = -DragDelta.Y * 0.90;
        }

        const float Wheel = BackstageWheelDelta();
        const float PanHeld = (BackstageHeld(n"D") ? 1.0 : 0.0) - (BackstageHeld(n"A") ? 1.0 : 0.0);
        const float TiltHeld = (BackstageHeld(n"W") ? 1.0 : 0.0) - (BackstageHeld(n"S") ? 1.0 : 0.0);
        const float ZoomHeld = (BackstageHeld(n"E") ? 1.0 : 0.0) - (BackstageHeld(n"Q") ? 1.0 : 0.0);
        Round.PushCamera(PanHeld * Delta * 0.85 + PanDrag, TiltHeld * Delta * 0.55 + TiltDrag,
            Wheel * 0.06 + ZoomHeld * Delta * 0.55);

        const bool bRewind = BackstageHeld(n"Left") || BackstageHeld(n"Z");
        const bool bForward = BackstageHeld(n"Right") || BackstageHeld(n"C");
        Round.SetScrubInput((bForward ? 1.0 : 0.0) - (bRewind ? 1.0 : 0.0));

        if (Pressed(n"SpaceBar") || Pressed(n"R") || Pressed(n"RightMouseButton"))
            Round.TogglePause();

        if (Pressed(n"BackSpace") || Pressed(n"Home"))
        {
            Round.Camera.Time = 0.0;
            Round.TargetCamera.Time = 0.0;
            Round.PlaybackSpeed = 0.0;
        }

        if (Pressed(n"Enter") && LiveArmTimer <= 0.0 && Round.Shutters > 0)
        {
            Round.CapturePending();
            if (Round.bHasPending && StageActor != nullptr)
            {
                const FBackstagePhoto& Pending = Round.PendingPhoto;
                StageActor.SyncToView(this, Pending, true, Round.LevelIndex);
                BackstageCapturePhoto(StageActor, Backstage::PendingCaptureSlot, Backstage::EffectiveAngle(Pending),
                    Backstage::EffectiveCenter(Pending), Pending.Pan, Pending.Tilt, Pending.Zoom);
            }
        }
    }

    void HandleConfirm()
    {
        bool bAccept = Pressed(n"Enter") || Pressed(n"Y");
        bool bReject = Pressed(n"BackSpace") || Pressed(n"Escape") || Pressed(n"N");
        if (Pressed(n"LeftMouseButton"))
        {
            const FVector2D Pointer = BackstagePointer();
            if (Pointer.X >= 360 && Pointer.X <= 620 && Pointer.Y >= 546 && Pointer.Y <= 600)
                bAccept = true;
            else if (Pointer.X >= 660 && Pointer.X <= 920 && Pointer.Y >= 546 && Pointer.Y <= 600)
                bReject = true;
        }

        if (bAccept)
        {
            const FBackstagePhoto AcceptedShot = Round.PendingPhoto;
            if (Round.AcceptPending() && StageActor != nullptr && Round.Photos.Num() > 0)
            {
                const int NewIndex = Round.Photos.Num() - 1;
                StageActor.SyncToView(this, AcceptedShot, true, Round.LevelIndex);
                BackstageCapturePhoto(StageActor, NewIndex, Backstage::EffectiveAngle(AcceptedShot),
                    Backstage::EffectiveCenter(AcceptedShot), AcceptedShot.Pan, AcceptedShot.Tilt, AcceptedShot.Zoom);
            }
            return;
        }

        if (bReject)
            Round.RejectPending();
    }

    void HandleEdit(float Delta)
    {
        if (Pressed(n"LeftMouseButton"))
        {
            const FVector2D Pointer = BackstagePointer();
            const int Photo = Backstage::PhotoUnderPointer(Pointer);
            const int Slot = Backstage::SlotUnderPointer(Pointer);
            if (Photo >= 0)
                Round.TogglePhoto(Photo);
            if (Round.Slots.IsValidIndex(Slot))
                EditSlot = Slot;
        }
        if (Pressed(n"One")) Round.TogglePhoto(0);
        if (Pressed(n"Two")) Round.TogglePhoto(1);
        if (Pressed(n"Three")) Round.TogglePhoto(2);
        if (Pressed(n"Four")) Round.TogglePhoto(3);
        if (Pressed(n"Five")) Round.TogglePhoto(4);
        if (Pressed(n"Six")) Round.TogglePhoto(5);
        if (Pressed(n"Z")) Round.SwapSlot(0, 1);
        if (Pressed(n"X")) Round.SwapSlot(1, 2);
        if (Pressed(n"C")) EditSlot = (EditSlot + 1) % Round.RequiredFrameCount();
        Round.AdjustCrop(EditSlot, CameraAxis(n"A", n"D", bWasA, bWasD, Delta),
            CameraAxis(n"Q", n"E", bWasQ, bWasE, Delta));
        if (Pressed(n"T"))
            Round.Headline = EBackstageHeadline((int(Round.Headline) + 1) % 3);
        if (Pressed(n"P")) Round.Preview(false);
        if (Pressed(n"Enter")) Round.Preview(true);
        if (Pressed(n"R"))
        {
            if (Round.bBraindanceActive)
                Round.ReturnToBraindance();
            else
                Round.Replay();
        }
        if (Pressed(n"BackSpace"))
        {
            Round.PrepareBriefing(Round.LevelIndex);
            BackstageClearPhotos();
        }
    }
}

class ABackstageHUD : AHUD
{
    const FLinearColor Ink = FLinearColor(0.026, 0.045, 0.068, 1);
    const FLinearColor Panel = FLinearColor(0.058, 0.089, 0.125, 1);
    const FLinearColor Paper = FLinearColor(0.94, 0.97, 1, 1);
    const FLinearColor Muted = FLinearColor(0.58, 0.68, 0.77, 1);
    const FLinearColor Mint = FLinearColor(0.36, 0.93, 0.73, 1);
    const FLinearColor ArtistColor = FLinearColor(0.97, 0.36, 0.46, 1);
    const FLinearColor VolunteerColor = FLinearColor(0.30, 0.66, 0.98, 1);
    const FLinearColor CoordinatorColor = FLinearColor(0.37, 0.84, 0.61, 1);
    const FLinearColor Gold = FLinearColor(1, 0.72, 0.30, 1);
    float Scale = 1;
    float OriginX = 0;
    float OriginY = 0;

    UFUNCTION(BlueprintOverride)
    void DrawHUD(int SizeX, int SizeY)
    {
        auto Controller = Cast<ABackstageController>(BackstagePlayer());
        if (Controller == nullptr)
            return;

        Scale = Math::Min(SizeX / 1280.0, SizeY / 720.0);
        OriginX = (SizeX - Scale * 1280) * 0.5;
        OriginY = (SizeY - Scale * 720) * 0.5;

        if (Controller.bInMainMenu)
        {
            BackstageRect(Ink, 0, 0, SizeX, SizeY);
            MainMenu(Controller.BackstagePointer(), Controller.MenuClock);
            return;
        }

        const FBackstageRound& Round = Controller.Round;
        if (Round.Phase == EBackstagePhase::Briefing || Round.Phase == EBackstagePhase::Confirm)
            BackstageRect(FLinearColor(Ink.R, Ink.G, Ink.B, 0.56), 0, 0, SizeX, SizeY);
        else if (Round.Phase == EBackstagePhase::Edit)
            BackstageRect(FLinearColor(Ink.R, Ink.G, Ink.B, 0.88), 0, 0, SizeX, SizeY);
        else if (Round.Phase == EBackstagePhase::Results)
        {
            const float Fade = Math::Clamp(Round.ResultsClock / 0.85, 0.0, 1.0);
            BackstageRect(FLinearColor(Ink.R, Ink.G, Ink.B, 0.92 * Fade), 0, 0, SizeX, SizeY);
        }

        switch (Round.Phase)
        {
            case EBackstagePhase::Briefing: Briefing(Round, Controller.BackstagePointer()); break;
            case EBackstagePhase::Live: Live(Round, Controller.MenuClock); break;
            case EBackstagePhase::Confirm: Confirm(Round, Controller.BackstagePointer()); break;
            case EBackstagePhase::Edit:
                Text(f"营销达人  /  {Round.Config.Title}", 48, 34, Mint, 1.15);
                Edit(Round, Controller.EditSlot, Controller.BackstagePointer());
                break;
            case EBackstagePhase::Preview: Preview(Round); break;
            case EBackstagePhase::Results:
                Results(Round, Controller.BackstagePointer());
                break;
        }
    }

    FString TwoDigit(int Value)
    {
        const int Clamped = Math::Clamp(Value, 0, 99);
        return Clamped < 10 ? f"0{Clamped}" : f"{Clamped}";
    }

    void ShadowText(FString Value, float X, float Y, FLinearColor Color, float Size = 1)
    {
        BackstageText(Value, FLinearColor(0.01, 0.015, 0.025, 0.88), OriginX + (X + 1.5) * Scale,
            OriginY + (Y + 1.5) * Scale, nullptr, Size * Scale, false);
        BackstageText(Value, Color, OriginX + X * Scale, OriginY + Y * Scale, nullptr, Size * Scale, false);
    }

    void CornerBrackets(float X, float Y, float W, float H, float Arm, float Thick, FLinearColor Color)
    {
        Box(Color, X, Y, Arm, Thick);
        Box(Color, X, Y, Thick, Arm);
        Box(Color, X + W - Arm, Y, Arm, Thick);
        Box(Color, X + W - Thick, Y, Thick, Arm);
        Box(Color, X, Y + H - Thick, Arm, Thick);
        Box(Color, X, Y + H - Arm, Thick, Arm);
        Box(Color, X + W - Arm, Y + H - Thick, Arm, Thick);
        Box(Color, X + W - Thick, Y + H - Arm, Thick, Arm);
    }

    void MainMenu(FVector2D Pointer, float Clock)
    {
        Box(FLinearColor(0.035, 0.055, 0.085, 1.0), 0, 0, 1280, 720);
        for (int Row = 0; Row < 6; ++Row)
            Box(FLinearColor(0.05, 0.08, 0.12, 0.35), 28, 80 + float(Row) * 108.0, 1224, 2);

        CornerBrackets(28, 22, 1224, 676, 36, 2.5, Mint);
        const bool bBlink = (int(Clock * 2.0) % 2) == 0;
        ShadowText(bBlink ? "● STANDBY" : "  STANDBY", 54, 42, ArtistColor, 1.20);
        ShadowText("HI-8 MASTER DECK  //  MARKETING FOCUS", 210, 42, Muted, 1.12);
        ShadowText("VHS SP   BAT [■■■]", 1042, 42, Mint, 1.12);

        ShadowText("营销达人  /  MARKETING MASTER", 336, 126, Paper, 2.45);
        ShadowText("90s CAMCORDER  ×  BRAINDANCE REWIND PROTOTYPE", 392, 194, Mint, 1.14);
        ShadowText("选好时机与构图按下快门  ·  用取景器里的画面重写新闻头条", 342, 234, Muted, 1.12);

        const bool bHoverLv1 = Pointer.X >= 420 && Pointer.X <= 860 && Pointer.Y >= 312 && Pointer.Y <= 382;
        const bool bHoverLv2 = Pointer.X >= 420 && Pointer.X <= 860 && Pointer.Y >= 402 && Pointer.Y <= 472;
        const bool bHoverQuit = Pointer.X >= 420 && Pointer.X <= 860 && Pointer.Y >= 492 && Pointer.Y <= 552;

        Box(bHoverLv1 ? FLinearColor(0.11, 0.24, 0.24, 0.95) : Panel, 420, 312, 440, 70);
        CornerBrackets(420, 312, 440, 70, 16, 2, bHoverLv1 ? Mint : Muted);
        ShadowText("▶  第一关：林晚雨中暖心新闻 (拍1张)", 462, 334, bHoverLv1 ? Mint : Paper, 1.24);

        Box(bHoverLv2 ? FLinearColor(0.11, 0.20, 0.28, 0.95) : Panel, 420, 402, 440, 70);
        CornerBrackets(420, 402, 440, 70, 16, 2, bHoverLv2 ? VolunteerColor : Muted);
        ShadowText("▶  第二关：慈善活动后台纪实 (拍3张)", 462, 424, bHoverLv2 ? VolunteerColor : Paper, 1.24);

        Box(bHoverQuit ? FLinearColor(0.22, 0.09, 0.12, 0.95) : Panel, 420, 492, 440, 60);
        CornerBrackets(420, 492, 440, 60, 14, 2, bHoverQuit ? ArtistColor : Muted);
        ShadowText("■  退出游戏  (ESC)", 565, 510, bHoverQuit ? ArtistColor : Muted, 1.18);

        ShadowText("操作：WASD 移动/俯仰  |  按住左键拖拽场景  |  滚轮/Q/E 缩放  |  1-3 机位  |  空格 暂停  |  Enter 拍照并确认",
            122, 622, Muted, 1.00);
        ShadowText("[ Enter / 1 ] 进入第一关   ·   [ 2 ] 进入第二关", 456, 654, Mint, 1.12);
    }

    void Viewfinder(const FBackstagePhoto&in Photo, float X, float Y, float W, float H, bool bLive)
    {
        const FLinearColor Edge(0.36, 0.93, 0.73, 0.22);
        Box(Edge, X, Y, W, 1);
        Box(Edge, X, Y + H - 1, W, 1);
        Box(Edge, X, Y, 1, H);
        Box(Edge, X + W - 1, Y, 1, H);
        CornerBrackets(X, Y, W, H, 28, 2, Mint);

        if (bLive)
        {
            const float Cx = X + W * 0.5;
            const float Cy = Y + H * 0.5;
            const FLinearColor Reticle(0.94, 0.97, 1.0, 0.48);
            Box(Reticle, Cx - 22, Cy, 12, 1.5);
            Box(Reticle, Cx + 10, Cy, 12, 1.5);
            Box(Reticle, Cx, Cy - 18, 1.5, 9);
            Box(Reticle, Cx, Cy + 9, 1.5, 9);

            const float LadderX = X + W - 34;
            const float LadderY = Cy;
            for (int Tick = -2; Tick <= 2; ++Tick)
            {
                const float Ty = LadderY + float(Tick) * 24.0;
                const float Tw = Tick == 0 ? 14.0 : 8.0;
                Box(FLinearColor(0.58, 0.68, 0.77, 0.45), LadderX - Tw, Ty, Tw, 1.5);
            }
            const float PipY = LadderY - Photo.Tilt * 44.0;
            Box(Gold, LadderX - 18, PipY - 2, 16, 4);
        }
    }

    void DrawPhotoOrShot(const FBackstagePhoto&in Original, const FBackstagePhoto&in Edited, int PhotoIndex,
        float X, float Y, float W, float H)
    {
        if (!BackstageDrawPhoto(PhotoIndex, OriginX + X * Scale, OriginY + Y * Scale, W * Scale, H * Scale,
            Original.Pan, Original.Zoom, Edited.Pan, Edited.Zoom))
        {
            Shot(Edited, X, Y, W, H);
        }
    }

    void Box(FLinearColor Color, float X, float Y, float W, float H)
    {
        BackstageRect(Color, OriginX + X * Scale, OriginY + Y * Scale, W * Scale, H * Scale);
    }

    void Text(FString Value, float X, float Y, FLinearColor Color, float Size = 1)
    {
        BackstageText(Value, Color, OriginX + X * Scale, OriginY + Y * Scale, nullptr, Size * Scale, false);
    }

    void Briefing(const FBackstageRound&in Round, FVector2D Pointer)
    {
        Box(FLinearColor(0.03, 0.05, 0.08, 0.84), 210, 120, 860, 480);
        CornerBrackets(210, 120, 860, 480, 26, 2.5, Mint);

        ShadowText(f"{Round.Config.CodeName}  //  任务简报   (目标：{Round.Config.RequiredPhotos} 张成片 · 回放 {int(Round.ActiveDuration())} 秒)",
            248, 154, Mint, 1.18);
        ShadowText(Round.Config.Title, 248, 192, Paper, 2.02);
        ShadowText("公关目标：", 248, 268, Mint, 1.16);
        ShadowText(Round.Config.TaskBrief, 248, 298, Paper, 1.22);
        ShadowText("内部情报：", 248, 356, ArtistColor, 1.16);
        ShadowText(Round.Config.TruthBrief, 248, 386, Paper, 1.20);
        ShadowText("操作：WASD 上下左右移动  ·  按住左键拖拽场景  ·  滚轮/Q/E 缩放  ·  空格 暂停  ·  Enter 拍摄并确认",
            248, 452, Muted, 1.04);

        const bool bHoverStart = Pointer.X >= 340 && Pointer.X <= 670 && Pointer.Y >= 518 && Pointer.Y <= 568;
        const bool bHoverMenu = Pointer.X >= 700 && Pointer.X <= 940 && Pointer.Y >= 518 && Pointer.Y <= 568;

        Box(bHoverStart ? FLinearColor(0.11, 0.25, 0.24, 0.96) : Panel, 340, 518, 330, 50);
        CornerBrackets(340, 518, 330, 50, 12, 2, bHoverStart ? Mint : Muted);
        ShadowText("▶ 进入拍摄现场  (Enter)", 382, 532, bHoverStart ? Mint : Paper, 1.22);

        Box(bHoverMenu ? FLinearColor(0.16, 0.12, 0.14, 0.96) : Panel, 700, 518, 240, 50);
        CornerBrackets(700, 518, 240, 50, 12, 2, bHoverMenu ? Gold : Muted);
        ShadowText("返回主菜单  (M)", 748, 532, bHoverMenu ? Gold : Muted, 1.16);
    }

    void Live(const FBackstageRound&in Round, float Clock)
    {
        if (Backstage::IsRainingAt(Round.LevelIndex, Round.Camera.Time))
        {
            for (int r = 0; r < 14; ++r)
            {
                const float Rx = Math::Fmod(float(r) * 117.0 + Clock * 95.0, 1220.0) + 30.0;
                const float Ry = Math::Fmod(float(r) * 173.0 + Clock * 520.0, 630.0) + 36.0;
                Box(FLinearColor(0.72, 0.86, 1.0, 0.14), Rx, Ry, 2.0, 22.0);
            }
        }

        Viewfinder(Round.Camera, 28, 22, 1224, 676, true);

        const bool bBlink = (int(Clock * 2.0) % 2) == 0;
        FString ModeText = bBlink ? "● PLAY 1.0x  超梦" : "  PLAY 1.0x  超梦";
        FLinearColor ModeColor = Mint;
        if (Math::Abs(Round.ScrubInput) > 0.05)
        {
            ModeText = Round.ScrubInput < 0 ? "<< REW 3.0x  倒放" : ">> FF 3.0x   快进";
            ModeColor = Gold;
        }
        else if (Round.bPaused)
        {
            ModeText = "|| PAUSE     暂停";
            ModeColor = ArtistColor;
        }

        ShadowText(ModeText, 48, 40, ModeColor, 1.24);
        ShadowText(f"CAM-0{Round.Camera.Station + 1}   PAN {int(Round.Camera.Pan * 30)}°   TILT {int(Round.Camera.Tilt * 15)}°",
            48, 68, Paper, 1.05);

        ShadowText(f"GOAL  {Round.Photos.Num()}/{Round.RequiredFrameCount()}   SHUTTER {Round.Shutters}/6", 880, 40, Paper, 1.12);
        for (int i = 0; i < Backstage::MaxPhotos; ++i)
        {
            const bool bAvail = i < Round.Shutters;
            Box(bAvail ? Mint : FLinearColor(0.25, 0.30, 0.36, 0.55), 1126 + float(i) * 18, 44, 12, 14);
        }
        ShadowText(f"W        T  {int(Round.Camera.Zoom * 100)}%", 988, 69, Muted, 1.02);
        Box(FLinearColor(0.58, 0.68, 0.77, 0.55), 1012, 76, 68, 2);
        const float ZoomAlpha = Math::Clamp((Round.Camera.Zoom - Backstage::ZoomMin) / (Backstage::ZoomMax - Backstage::ZoomMin), 0.0, 1.0);
        Box(Mint, 1010 + ZoomAlpha * 64.0, 72, 6, 10);
        ShadowText("BAT [■■]", 1166, 69, Mint, 0.98);

        if (Round.ReplayNotice > 0)
        {
            Box(FLinearColor(0.02, 0.04, 0.06, 0.72), 400, 38, 480, 32);
            ShadowText(Round.LevelIndex == 0 ? "已收入照片 / 可继续调整时机并按下 Enter" : "已返回超梦时间轴，可继续调整视角并拍照",
                438, 44, Mint, 1.06);
        }

        const float Dur = Round.ActiveDuration();
        const int WholeSec = Math::Clamp(int(Round.Camera.Time), 0, int(Dur));
        const int Frames = Math::Clamp(int((Round.Camera.Time - float(WholeSec)) * 30.0), 0, 29);
        ShadowText(f"TC 00:00:{TwoDigit(WholeSec)}:{TwoDigit(Frames)}", 48, 634, Paper, 1.18);
        const FString CurrentBeat = Round.LevelIndex == 0
            ? Backstage::RainBeatName(Backstage::RainBeatAt(Round.Camera.Time))
            : Backstage::BeatName(Backstage::BeatAt(Round.Camera.Time));
        ShadowText(f"[ {CurrentBeat} ]", 242, 634, Gold, 1.14);

        if (Round.LevelIndex == 0)
        {
            ShadowText("● 林晚", 1010, 636, ArtistColor, 0.98);
            ShadowText("● 小北", 1084, 636, VolunteerColor, 0.98);
            ShadowText("● 粉丝", 1158, 636, CoordinatorColor, 0.98);
        }
        else
        {
            ShadowText("● 艺人", 1020, 636, ArtistColor, 0.98);
            ShadowText("● 志愿者", 1084, 636, VolunteerColor, 0.98);
            ShadowText("● 场务", 1164, 636, CoordinatorColor, 0.98);
        }

        const float TrackX = 48;
        const float TrackY = 666;
        const float TrackW = 1184;
        Box(FLinearColor(0.03, 0.05, 0.08, 0.65), TrackX, TrackY, TrackW, 4);
        Box(ModeColor, TrackX, TrackY, TrackW * (Round.Camera.Time / Dur), 4);

        const float MarkScale = TrackW / Dur;
        if (Round.LevelIndex == 0)
        {
            Box(Ink, TrackX + 6.0 * MarkScale - 1, TrackY - 2, 2, 8);
            Box(Ink, TrackX + 13.0 * MarkScale - 1, TrackY - 2, 2, 8);
            Box(Ink, TrackX + 20.0 * MarkScale - 1, TrackY - 2, 2, 8);
            Box(Ink, TrackX + 31.0 * MarkScale - 1, TrackY - 2, 2, 8);
        }
        else
        {
            Box(Ink, TrackX + 7.0 * MarkScale - 1, TrackY - 2, 2, 8);
            Box(Ink, TrackX + 14.0 * MarkScale - 1, TrackY - 2, 2, 8);
            Box(Ink, TrackX + 23.0 * MarkScale - 1, TrackY - 2, 2, 8);
            Box(Ink, TrackX + 33.0 * MarkScale - 1, TrackY - 2, 2, 8);
            Box(Ink, TrackX + 42.0 * MarkScale - 1, TrackY - 2, 2, 8);
        }

        for (int PhotoIdx = 0; PhotoIdx < Round.Photos.Num(); ++PhotoIdx)
        {
            const float PinX = TrackX + TrackW * (Round.Photos[PhotoIdx].Time / Dur);
            Box(Gold, PinX - 1.5, TrackY - 4, 3, 12);
        }

        const float HeadX = TrackX + TrackW * (Round.Camera.Time / Dur);
        Box(Paper, HeadX - 1.5, TrackY - 5, 3, 14);

        ShadowText("WASD 移动/俯仰   按住左键拖拽场景   滚轮/Q/E 缩放   1-3 机位   空格 暂停   ←/→(Z/C) 倒放/快进   Enter 拍照确认",
            48, 678, Muted, 0.94);
    }

    void Confirm(const FBackstageRound&in Round, FVector2D Pointer)
    {
        Box(FLinearColor(0.03, 0.05, 0.085, 0.94), 250, 88, 780, 536);
        CornerBrackets(250, 88, 780, 536, 26, 2.5, Mint);

        ShadowText(f"CONFIRM CAPTURE  //  是否选用这张成片？ (第 {Round.Photos.Num() + 1} / {Round.RequiredFrameCount()} 张)",
            284, 114, Mint, 1.18);

        DrawPhotoOrShot(Round.PendingPhoto, Round.PendingPhoto, Backstage::PendingCaptureSlot, 320, 158, 640, 292);
        CornerBrackets(320, 158, 640, 292, 18, 2, Paper);

        const int WholeSec = Math::Clamp(int(Round.PendingPhoto.Time), 0, int(Round.ActiveDuration()));
        const int Tenths = Math::Clamp(int((Round.PendingPhoto.Time - float(WholeSec)) * 10.0), 0, 9);
        const FString BeatLabel = Round.LevelIndex == 0
            ? Backstage::RainBeatName(Backstage::RainBeatAt(Round.PendingPhoto.Time))
            : Backstage::BeatName(Backstage::BeatAt(Round.PendingPhoto.Time));

        ShadowText(f"拍摄时刻：{WholeSec}.{Tenths} 秒   ·   节点：[ {BeatLabel} ]   ·   缩放 {int(Round.PendingPhoto.Zoom * 100)}%",
            320, 470, Paper, 1.16);
        ShadowText(Round.LevelIndex == 0
            ? "确认后将立即送审并刊登报纸；不满意可按 BackSpace / 点击重拍调整位置、缩放与时间。"
            : "确认后将收入素材库，集齐 3 张后自动进入剪辑台。",
            320, 502, Muted, 1.02);

        const bool bHoverAccept = Pointer.X >= 360 && Pointer.X <= 620 && Pointer.Y >= 546 && Pointer.Y <= 600;
        const bool bHoverReject = Pointer.X >= 660 && Pointer.X <= 920 && Pointer.Y >= 546 && Pointer.Y <= 600;

        Box(bHoverAccept ? FLinearColor(0.11, 0.26, 0.24, 0.98) : Panel, 360, 546, 260, 54);
        CornerBrackets(360, 546, 260, 54, 12, 2, bHoverAccept ? Mint : Muted);
        ShadowText("✓  确认选用  (Enter)", 400, 562, bHoverAccept ? Mint : Paper, 1.22);

        Box(bHoverReject ? FLinearColor(0.24, 0.10, 0.12, 0.98) : Panel, 660, 546, 260, 54);
        CornerBrackets(660, 546, 260, 54, 12, 2, bHoverReject ? ArtistColor : Muted);
        ShadowText("↺  放弃重拍  (BackSpace)", 690, 562, bHoverReject ? ArtistColor : Paper, 1.16);
    }

    void Shot(const FBackstagePhoto&in Photo, float X, float Y, float W, float H, bool bLive = false)
    {
        Box(FLinearColor(0.10, 0.15, 0.22, 1), X, Y, W, H);
        const float Floor = Math::Clamp(Backstage::ProjectHeight(Photo, 0.62), 0.0, 1.0);
        Box(FLinearColor(0.14, 0.20, 0.25, 1), X, Y + H * Floor, W, H * (1 - Floor));
        for (int Index = -2; Index <= 2; ++Index)
        {
            const float Post = Backstage::Project(Photo, FVector2D(Index * 2.0, -0.9)).X;
            ShotBox(FLinearColor(0.19, 0.25, 0.31, 1), Post - 0.006 * Photo.Zoom, 0,
                0.012 * Photo.Zoom, Floor, X, Y, W, H);
        }
        const auto Beat = Backstage::BeatAt(Photo.Time);
        Person(Photo, Backstage::CoordinatorAt(Photo.Time), CoordinatorColor, false, false, X, Y, W, H);
        Person(Photo, Backstage::VolunteerAt(Photo.Time), VolunteerColor, false, Beat == EBackstageBeat::Handoff, X, Y, W, H);
        Person(Photo, Backstage::ArtistAt(Photo.Time), ArtistColor, Beat == EBackstageBeat::Pickup,
            Beat == EBackstageBeat::Collision || Beat == EBackstageBeat::Argument || Beat == EBackstageBeat::Handoff, X, Y, W, H);

        const FVector2D Parcel = Backstage::ParcelAt(Photo.Time);
        if (Backstage::Visible(Photo, Parcel))
        {
            const FVector2D Position = Backstage::Project(Photo, Parcel);
            const bool bDropped = Beat == EBackstageBeat::Collision || Beat == EBackstageBeat::Argument;
            const float Z = Photo.Zoom;
            const float ParcelY = Position.Y - Z * (bDropped ? 0.02 : (Beat == EBackstageBeat::Pickup ? 0.12 : 0.24));
            ShotBox(Gold, Position.X - Z * 0.016, ParcelY, Z * 0.032, Z * 0.068, X, Y, W, H);
            if (bDropped)
            {
                ShotBox(Gold, Position.X - Z * 0.043, ParcelY + Z * 0.04, Z * 0.018, Z * 0.041, X, Y, W, H);
                ShotBox(Gold, Position.X + Z * 0.028, ParcelY + Z * 0.025, Z * 0.019, Z * 0.045, X, Y, W, H);
            }
        }
        if (bLive)
        {
            Box(Paper, X + W * 0.5 - 10, Y + H * 0.5, 20, 1);
            Box(Paper, X + W * 0.5, Y + H * 0.5 - 10, 1, 20);
            Text("● REC", X + 20, Y + 18, ArtistColor);
        }
    }

    void ShotBox(FLinearColor Color, float Left, float Top, float Width, float Height, float X, float Y, float W, float H)
    {
        const float Right = Math::Min(1, Left + Width);
        const float Bottom = Math::Min(1, Top + Height);
        const float ClippedLeft = Math::Max(0, Left);
        const float ClippedTop = Math::Max(0, Top);
        if (ClippedLeft < Right && ClippedTop < Bottom)
            Box(Color, X + W * ClippedLeft, Y + H * ClippedTop, W * (Right - ClippedLeft), H * (Bottom - ClippedTop));
    }

    void Person(const FBackstagePhoto&in Photo, FVector2D Point, FLinearColor Color, bool bCrouch,
        bool bReach, float X, float Y, float W, float H)
    {
        if (!Backstage::Visible(Photo, Point))
            return;
        const FVector2D Position = Backstage::Project(Photo, Point);
        const float BodyX = Position.X;
        const float Feet = Position.Y;
        const float Height = (bCrouch ? 0.18 : 0.29) * Photo.Zoom;
        const float Width = 0.034 * Photo.Zoom;
        ShotBox(Color, BodyX - Width * 0.31, Feet - Height, Width * 0.62, Height * 0.24, X, Y, W, H);
        ShotBox(Color, BodyX - Width * 0.5, Feet - Height * 0.69, Width, Height * 0.47, X, Y, W, H);
        ShotBox(Color, BodyX - Width * 0.40, Feet - Height * 0.25, Width * 0.30, Height * 0.25, X, Y, W, H);
        ShotBox(Color, BodyX + Width * 0.10, Feet - Height * 0.25, Width * 0.30, Height * 0.25, X, Y, W, H);
        if (bReach || bCrouch)
        {
            const float Arm = Width * 1.15;
            const float ArmX = Point.X < 0 ? BodyX : BodyX - Arm;
            ShotBox(Color, ArmX, Feet - Height * (bCrouch ? 0.32 : 0.62), Arm, Height * 0.13, X, Y, W, H);
        }
    }

    FString HeadlineText(EBackstageHeadline Headline)
    {
        if (Headline == EBackstageHeadline::Helpful) return "后台暖心一幕";
        if (Headline == EBackstageHeadline::Conflict) return "慈善活动后台起争执";
        return "慈善活动后台纪实";
    }

    void Edit(const FBackstageRound&in Round, int ActiveSlot, FVector2D Pointer)
    {
        const int Need = Round.RequiredFrameCount();
        Text("编辑作品", 48, 80, Paper, 1.8);
        Text("素材  /  点击缩略图或按 1–6 选入、移除", 48, 138, Muted, 1.18);
        const int HoveredPhoto = Backstage::PhotoUnderPointer(Pointer);
        for (int Index = 0; Index < Backstage::MaxPhotos; ++Index)
        {
            const float X = Backstage::PhotoLeft(Index);
            Box(Panel, X, 177, 184, 146);
            if (Round.Photos.IsValidIndex(Index))
            {
                DrawPhotoOrShot(Round.Photos[Index], Round.Photos[Index], Index, X, 177, 184, 110);
                const int Slot = Round.Slots.FindIndex(Index);
                if (HoveredPhoto == Index || Slot >= 0) Box(Mint, X, 177, 184, 3);
                Text(f"{Index + 1}   {int(Round.Photos[Index].Time)} 秒", X + 11, 296, Slot >= 0 ? Mint : Paper);
                if (Slot >= 0) Text(f"槽 {Slot + 1}", X + 118, 296, Mint);
            }
            else Text(f"{Index + 1}   空", X + 11, 296, Muted);
        }
        Text("Z / X 交换顺序    点击槽位或按 C 选槽    A / D 移动裁切    Q / E 调整大小", 48, 351, Muted, 1.10);
        for (int Index = 0; Index < Need; ++Index)
        {
            const float X = Backstage::SlotLeft(Index);
            Box(Panel, X, 392, 222, 156);
            if (Round.Slots.IsValidIndex(Index))
            {
                const int PhotoIndex = Round.Slots[Index];
                DrawPhotoOrShot(Round.Photos[PhotoIndex], Round.Edits[PhotoIndex], PhotoIndex, X, 392, 222, 120);
            }
            if (Index == ActiveSlot) Box(Mint, X, 392, 222, 3);
            Text(f"槽 {Index + 1}", X + 12, 519, Index == ActiveSlot ? Mint : Paper);
        }
        Text("标题  /  T 切换", 798, 407, Muted, 1.11);
        Text(HeadlineText(Round.Headline), 798, 449, Paper, 1.30);
        if (Round.Slots.Num() == Need)
            Text("P 预览    Enter 发布", 48, 614, Mint, 1.30);
        else
            Text(f"选满 {Need} 张后可预览或发布。", 48, 614, Paper, 1.30);
        if (Round.bBraindanceActive)
            Text("R 返回超梦时间轴继续倒放与补拍", 472, 655, Muted, 1.09);
        else if (Round.Replays > 0)
            Text("R 回看现场（保留素材和剩余快门）", 472, 655, Muted, 1.09);
        Text("Backspace 返回简报", 48, 655, Muted, 1.09);
    }

    void Preview(const FBackstageRound&in Round)
    {
        const int Need = Round.RequiredFrameCount();
        const int Frame = Math::Min(int(Round.PreviewTime / Backstage::PreviewSecondsPerFrame), Need - 1);
        Viewfinder(Round.Cut[Frame], 28, 22, 1224, 676, false);
        ShadowText(Round.bPublishing ? "▶ VCR PUBLISHING  //  发布中" : "▶ VCR PLAYBACK  //  成片预览", 48, 40, Mint, 1.20);
        ShadowText(HeadlineText(Round.Headline), 48, 68, Paper, 1.48);
        ShadowText(f"FRAME  0{Frame + 1} / 0{Need}", 48, 656, Mint, 1.24);
        if (!Round.bPublishing) ShadowText("Backspace 返回剪辑", 1048, 658, Muted, 1.12);
    }

    void Results(const FBackstageRound&in Round, FVector2D Pointer)
    {
        const FBackstageResult Result = Round.Result;
        const float Fade = Math::Clamp(Round.ResultsClock / 0.85, 0.0, 1.0);
        const float SlideOffset = (1.0 - Fade) * 26.0;

        const FLinearColor Newsprint(0.95, 0.92, 0.85, Fade);
        const FLinearColor PrintInk(0.09, 0.11, 0.14, Fade);
        const FLinearColor PrintMuted(0.32, 0.35, 0.39, Fade);
        const FLinearColor PrintCrimson(0.72, 0.11, 0.15, Fade);
        const FLinearColor PrintGreen(0.08, 0.45, 0.28, Fade);

        const float SheetX = 56;
        const float SheetY = 30 + SlideOffset;
        const float SheetW = 1168;
        const float SheetH = 592;

        // Newspaper Front Page Card (fades in smoothly after shoot)
        Box(FLinearColor(0.0, 0.0, 0.0, 0.35 * Fade), SheetX + 8, SheetY + 8, SheetW, SheetH);
        Box(Newsprint, SheetX, SheetY, SheetW, SheetH);
        Box(PrintInk, SheetX + 24, SheetY + 18, SheetW - 48, 4);
        Box(PrintInk, SheetX + 24, SheetY + 62, SheetW - 48, 2);

        Text("娱乐快报  ·  ENTERTAINMENT DAILY", SheetX + 32, SheetY + 28, PrintInk, 1.38);
        Text(Result.bPassed ? "今日头条特刊  //  PASS" : "编辑部退稿评审  //  RETRY",
            SheetX + 860, SheetY + 30, Result.bPassed ? PrintGreen : PrintCrimson, 1.20);

        const FString MainHeadline = Result.NewspaperHeadline.IsEmpty()
            ? HeadlineText(Round.Headline)
            : Result.NewspaperHeadline;
        Text(MainHeadline, SheetX + 32, SheetY + 76, PrintInk, 1.72);
        if (!Result.NewspaperLead.IsEmpty())
            Text(Result.NewspaperLead, SheetX + 32, SheetY + 124, PrintMuted, 1.08);
        Box(PrintInk, SheetX + 24, SheetY + 154, SheetW - 48, 1);

        // Newspaper Photograph Block
        const int FrameNum = Math::Max(1, Round.Cut.Num());
        if (FrameNum == 1)
        {
            const int PhotoIndex = Round.Slots.IsValidIndex(0) ? Round.Slots[0] : 0;
            const FBackstagePhoto& Original = Round.Photos.IsValidIndex(PhotoIndex) ? Round.Photos[PhotoIndex] : Round.Cut[0];
            Box(PrintInk, SheetX + 30, SheetY + 170, 520, 290);
            DrawPhotoOrShot(Original, Round.Cut[0], PhotoIndex, SheetX + 36, SheetY + 176, 508, 260);
            Text("图：现场独家直击（玩家拍摄成片）", SheetX + 168, SheetY + 440, Newsprint, 0.94);
        }
        else
        {
            for (int Index = 0; Index < FrameNum && Index < 3; ++Index)
            {
                const int PhotoIndex = Round.Slots.IsValidIndex(Index) ? Round.Slots[Index] : -1;
                const FBackstagePhoto& Original = Round.Photos.IsValidIndex(PhotoIndex) ? Round.Photos[PhotoIndex] : Round.Cut[Index];
                DrawPhotoOrShot(Original, Round.Cut[Index], PhotoIndex, SheetX + 32 + float(Index) * 180, SheetY + 176, 170, 110);
            }
            Text(f"现场三格连拍组图：{HeadlineText(Round.Headline)}", SheetX + 32, SheetY + 300, PrintMuted, 1.02);
        }

        // Verification & Editorial Columns on Right Side
        const float ColX = SheetX + 576;
        Text("▶ 采编判定结果", ColX, SheetY + 170, PrintInk, 1.24);
        Text(Result.CheckTimeText, ColX, SheetY + 206, Result.bTimeMatched ? PrintGreen : PrintCrimson, 1.08);
        Text(Result.CheckSubjectText, ColX, SheetY + 236, Result.bSubjectsMatched ? PrintGreen : PrintCrimson, 1.08);
        Text(Result.Evidence, ColX, SheetY + 268, PrintMuted, 1.04);

        Box(PrintInk, ColX, SheetY + 304, 540, 1);
        Text(f"关注度 {Signed(Result.Attention)}    公司信任 {Signed(Result.CompanyTrust)}    公众信任 {Signed(Result.PublicTrust)}",
            ColX, SheetY + 318, PrintInk, 1.18);
        Text("热搜评论：" + Result.Comment, ColX, SheetY + 358, PrintInk, 1.04);
        Text("公关反馈：" + Result.Company, ColX, SheetY + 392, PrintInk, 1.04);
        Text(Result.Person, ColX, SheetY + 426, PrintMuted, 1.02);

        Box(FLinearColor(0.08, 0.10, 0.12, 0.08 * Fade), SheetX + 32, SheetY + 478, SheetW - 64, 90);
        Text(Result.bPassed
            ? "▶ 本关达成：舆论走势已按照公关预期发酵，头版报道顺利发出！"
            : "▶ 未过审提示：请点击下方【重新拍摄本关】，调整时机、缩放或左右构图后再试。",
            SheetX + 48, SheetY + 498, Result.bPassed ? PrintGreen : PrintCrimson, 1.18);

        if (Fade < 0.35)
            return;

        const bool bHoverReplay = Pointer.X >= 48 && Pointer.X <= 278 && Pointer.Y >= 644 && Pointer.Y <= 692;
        Box(bHoverReplay ? FLinearColor(0.11, 0.25, 0.24, 0.96) : Panel, 48, 644, 230, 48);
        CornerBrackets(48, 644, 230, 48, 12, 2, bHoverReplay ? Mint : Muted);
        ShadowText("▶ 重新拍摄本关 (Enter)", 70, 658, bHoverReplay ? Mint : Paper, 1.12);

        if (Round.LevelIndex == 0)
        {
            const bool bHoverNext = Pointer.X >= 296 && Pointer.X <= 564 && Pointer.Y >= 644 && Pointer.Y <= 692;
            Box(bHoverNext ? FLinearColor(0.11, 0.24, 0.18, 0.96) : Panel, 296, 644, 268, 48);
            CornerBrackets(296, 644, 268, 48, 12, 2, bHoverNext ? Mint : Muted);
            ShadowText("▶ 下一关：慈善后台 (N)", 324, 658, bHoverNext ? Mint : Paper, 1.12);
        }

        const bool bHoverMenu = Pointer.X >= 582 && Pointer.X <= 810 && Pointer.Y >= 644 && Pointer.Y <= 692;
        Box(bHoverMenu ? FLinearColor(0.16, 0.12, 0.14, 0.96) : Panel, 582, 644, 228, 48);
        CornerBrackets(582, 644, 228, 48, 12, 2, bHoverMenu ? Gold : Muted);
        ShadowText("返回主菜单  (M)", 626, 658, bHoverMenu ? Gold : Muted, 1.12);
    }

    FString Signed(int Value)
    {
        return Value > 0 ? f"+{Value}" : f"{Value}";
    }
}
