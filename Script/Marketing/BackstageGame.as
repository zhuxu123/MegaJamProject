class ABackstageGameMode : AGameModeBase
{
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
            StageActor.InitializeStage(this);
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
        if (Pressed(n"Enter") || Pressed(n"SpaceBar"))
        {
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
            if (Pointer.X >= 420 && Pointer.X <= 860 && Pointer.Y >= 360 && Pointer.Y <= 430)
            {
                BackstageOpenMap(n"/Game/levels/Backstage");
                return;
            }
            if (Pointer.X >= 420 && Pointer.X <= 860 && Pointer.Y >= 455 && Pointer.Y <= 515)
            {
                BackstageQuitGame();
                return;
            }
        }
    }

    void StartStageRound()
    {
        Round.Start();
        Round.EnableBraindance();
        BackstageClearPhotos();
        EditSlot = 0;
        LiveArmTimer = 0.18;
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
            SetBackstagePointer(bInMainMenu || Round.Phase == EBackstagePhase::Briefing
                || Round.Phase == EBackstagePhase::Edit || Round.Phase == EBackstagePhase::Results);
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
                if (Pressed(n"M"))
                {
                    BackstageOpenMap(n"/Game/levels/MainMenu");
                    return;
                }
                if (Pressed(n"LeftMouseButton"))
                {
                    const FVector2D Pointer = BackstagePointer();
                    if (Pointer.X >= 48 && Pointer.X <= 290 && Pointer.Y >= 636 && Pointer.Y <= 684)
                        StartStageRound();
                    else if (Pointer.X >= 320 && Pointer.X <= 560 && Pointer.Y >= 636 && Pointer.Y <= 684)
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
            SetBackstagePointer(Round.Phase == EBackstagePhase::Briefing
                || Round.Phase == EBackstagePhase::Edit || Round.Phase == EBackstagePhase::Results);
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
                StageActor.SyncToView(this, Round.Cut[Frame], true);
            }
            else
            {
                StageActor.SyncToView(this, Round.Camera, true);
            }
        }
    }

    void HandleCamera(float Delta)
    {
        LiveArmTimer = Math::Max(0.0, LiveArmTimer - Delta);
        if (Pressed(n"One")) Round.SetTargetStation(0);
        if (Pressed(n"Two")) Round.SetTargetStation(1);
        if (Pressed(n"Three")) Round.SetTargetStation(2);

        const float Wheel = BackstageWheelDelta();
        const float PanHeld = (BackstageHeld(n"D") ? 1.0 : 0.0) - (BackstageHeld(n"A") ? 1.0 : 0.0);
        const float TiltHeld = (BackstageHeld(n"W") ? 1.0 : 0.0) - (BackstageHeld(n"S") ? 1.0 : 0.0);
        const float ZoomHeld = (BackstageHeld(n"E") ? 1.0 : 0.0) - (BackstageHeld(n"Q") ? 1.0 : 0.0);
        Round.PushCamera(PanHeld * Delta * 0.80, TiltHeld * Delta * 0.45, Wheel * 0.06 + ZoomHeld * Delta * 0.55);

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

        if (Pressed(n"LeftMouseButton") && LiveArmTimer <= 0.0)
        {
            const int PrevCount = Round.Photos.Num();
            Round.Capture();
            if (Round.Photos.Num() > PrevCount && StageActor != nullptr)
            {
                const int NewIndex = Round.Photos.Num() - 1;
                const FBackstagePhoto& Captured = Round.Photos[NewIndex];
                StageActor.SyncToView(this, Captured, true);
                BackstageCapturePhoto(StageActor, NewIndex, Backstage::EffectiveAngle(Captured),
                    Backstage::EffectiveCenter(Captured), Captured.Pan, Captured.Tilt, Captured.Zoom);
            }
        }
        if (Pressed(n"Enter") && Round.Photos.Num() >= Backstage::FrameCount) Round.Phase = EBackstagePhase::Edit;
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
        if (Pressed(n"C")) EditSlot = (EditSlot + 1) % Backstage::FrameCount;
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
            Round = FBackstageRound();
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
        if (Round.Phase == EBackstagePhase::Briefing)
            BackstageRect(FLinearColor(Ink.R, Ink.G, Ink.B, 0.52), 0, 0, SizeX, SizeY);
        else if (Round.Phase == EBackstagePhase::Edit || Round.Phase == EBackstagePhase::Results)
            BackstageRect(FLinearColor(Ink.R, Ink.G, Ink.B, 0.88), 0, 0, SizeX, SizeY);

        switch (Round.Phase)
        {
            case EBackstagePhase::Briefing: Briefing(Controller.BackstagePointer()); break;
            case EBackstagePhase::Live: Live(Round, Controller.MenuClock); break;
            case EBackstagePhase::Edit:
                Text("营销达人  /  慈善活动后台", 48, 34, Mint, 1.15);
                Edit(Round, Controller.EditSlot, Controller.BackstagePointer());
                break;
            case EBackstagePhase::Preview: Preview(Round); break;
            case EBackstagePhase::Results:
                Text("营销达人  /  慈善活动后台", 48, 34, Mint, 1.15);
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
        // Subtle 90s CRT monitor bands
        for (int Row = 0; Row < 6; ++Row)
            Box(FLinearColor(0.05, 0.08, 0.12, 0.35), 28, 80 + float(Row) * 108.0, 1224, 2);

        CornerBrackets(28, 22, 1224, 676, 36, 2.5, Mint);
        const bool bBlink = (int(Clock * 2.0) % 2) == 0;
        ShadowText(bBlink ? "● STANDBY" : "  STANDBY", 54, 42, ArtistColor, 1.20);
        ShadowText("HI-8 MASTER DECK  //  CH-01 BACKSTAGE", 210, 42, Muted, 1.12);
        ShadowText("VHS SP   BAT [■■■]", 1042, 42, Mint, 1.12);

        ShadowText("焦点背面  /  BACKSTAGE FOCUS", 340, 150, Paper, 2.55);
        ShadowText("90s CAMCORDER  ×  BRAINDANCE REWIND PROTOTYPE", 392, 222, Mint, 1.16);
        ShadowText("透过取景器捕捉真实瞬间  ·  自由倒放超梦时间轴  ·  用三帧剪辑重塑真相", 312, 264, Muted, 1.12);

        const bool bHoverStart = Pointer.X >= 420 && Pointer.X <= 860 && Pointer.Y >= 360 && Pointer.Y <= 430;
        const bool bHoverQuit = Pointer.X >= 420 && Pointer.X <= 860 && Pointer.Y >= 455 && Pointer.Y <= 515;

        Box(bHoverStart ? FLinearColor(0.11, 0.24, 0.24, 0.95) : Panel, 420, 360, 440, 70);
        CornerBrackets(420, 360, 440, 70, 16, 2, bHoverStart ? Mint : Muted);
        ShadowText("▶  进入第一关：慈善活动后台", 495, 381, bHoverStart ? Mint : Paper, 1.38);

        Box(bHoverQuit ? FLinearColor(0.22, 0.09, 0.12, 0.95) : Panel, 420, 455, 440, 60);
        CornerBrackets(420, 455, 440, 60, 14, 2, bHoverQuit ? ArtistColor : Muted);
        ShadowText("■  退出游戏  (ESC)", 565, 473, bHoverQuit ? ArtistColor : Muted, 1.20);

        ShadowText("第一关操作一览：WASD 移动视角  |  滚轮 缩放  |  1–3 机位  |  空格 暂停  |  左键 拍照  |  ←/→(Z/C) 倒放/快进", 150, 628, Muted, 1.02);
        ShadowText("[ Enter ] 或鼠标点击按钮进入第一关", 482, 658, Mint, 1.12);
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
            // Minimal segmented center reticle (keeps center subject unobstructed)
            const float Cx = X + W * 0.5;
            const float Cy = Y + H * 0.5;
            const FLinearColor Reticle(0.94, 0.97, 1.0, 0.48);
            Box(Reticle, Cx - 22, Cy, 12, 1.5);
            Box(Reticle, Cx + 10, Cy, 12, 1.5);
            Box(Reticle, Cx, Cy - 18, 1.5, 9);
            Box(Reticle, Cx, Cy + 9, 1.5, 9);

            // Right-side 90s vertical tilt ladder
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

    void Briefing(FVector2D Pointer)
    {
        Box(FLinearColor(0.03, 0.05, 0.08, 0.82), 230, 130, 820, 460);
        CornerBrackets(230, 130, 820, 460, 26, 2.5, Mint);

        ShadowText("LEVEL 01  //  任务简报", 268, 162, Mint, 1.24);
        ShadowText("慈善活动后台纪实", 268, 198, Paper, 2.10);
        ShadowText("公司回顾任务：", 268, 274, Mint, 1.18);
        ShadowText("前往后台拍摄三张能体现艺人亲和力的现场照片并剪辑发布。", 268, 304, Paper, 1.28);
        ShadowText("艺人私信：", 268, 362, ArtistColor, 1.18);
        ShadowText("“我赶着上场，后台如果有乱七八糟的事，麻烦别发出去。”", 268, 392, Paper, 1.28);
        ShadowText("90s 超梦摄像机：50 秒回放（WASD 视角 · 滚轮缩放 · 空格暂停 · 左键拍照 · ←/→ 倒放/快进）", 268, 456, Muted, 1.08);

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

        // Top-Left 90s Camcorder OSD
        ShadowText(ModeText, 48, 40, ModeColor, 1.24);
        ShadowText(f"CAM-0{Round.Camera.Station + 1}   PAN {int(Round.Camera.Pan * 30)}°   TILT {int(Round.Camera.Tilt * 15)}°",
            48, 68, Paper, 1.05);

        // Top-Right Shutter Pips & Zoom Meter
        ShadowText(f"SHUTTER  {Round.Shutters}/6", 988, 40, Paper, 1.16);
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
            ShadowText("已返回超梦时间轴，可继续倒放、调整视角与补拍", 432, 44, Mint, 1.08);
        }

        // Bottom-Left 90s VHS Timecode & Beat Tag
        const int WholeSec = Math::Clamp(int(Round.Camera.Time), 0, 50);
        const int Frames = Math::Clamp(int((Round.Camera.Time - float(WholeSec)) * 30.0), 0, 29);
        ShadowText(f"TC 00:00:{TwoDigit(WholeSec)}:{TwoDigit(Frames)}", 48, 634, Paper, 1.18);
        ShadowText(f"[ {Backstage::BeatName(Backstage::BeatAt(Round.Camera.Time))} ]", 242, 634, Gold, 1.14);

        ShadowText("● 艺人", 1020, 636, ArtistColor, 0.98);
        ShadowText("● 志愿者", 1084, 636, VolunteerColor, 0.98);
        ShadowText("● 场务", 1164, 636, CoordinatorColor, 0.98);

        // Slim Full-Width Braindance Timeline at Bottom Edge
        const float TrackX = 48;
        const float TrackY = 666;
        const float TrackW = 1184;
        Box(FLinearColor(0.03, 0.05, 0.08, 0.65), TrackX, TrackY, TrackW, 4);
        Box(ModeColor, TrackX, TrackY, TrackW * (Round.Camera.Time / Backstage::Duration), 4);

        const float MarkScale = TrackW / Backstage::Duration;
        Box(Ink, TrackX + 7.0 * MarkScale - 1, TrackY - 2, 2, 8);
        Box(Ink, TrackX + 14.0 * MarkScale - 1, TrackY - 2, 2, 8);
        Box(Ink, TrackX + 23.0 * MarkScale - 1, TrackY - 2, 2, 8);
        Box(Ink, TrackX + 33.0 * MarkScale - 1, TrackY - 2, 2, 8);
        Box(Ink, TrackX + 42.0 * MarkScale - 1, TrackY - 2, 2, 8);

        for (int PhotoIdx = 0; PhotoIdx < Round.Photos.Num(); ++PhotoIdx)
        {
            const float PinX = TrackX + TrackW * (Round.Photos[PhotoIdx].Time / Backstage::Duration);
            Box(Gold, PinX - 1.5, TrackY - 4, 3, 12);
        }

        const float HeadX = TrackX + TrackW * (Round.Camera.Time / Backstage::Duration);
        Box(Paper, HeadX - 1.5, TrackY - 5, 3, 14);

        ShadowText("WASD 移动视角   滚轮/Q/E 缩放   1-3 机位   空格 暂停   左键 拍照   ←/→(Z/C) 倒放/快进   Backspace 归零   Enter 剪辑",
            48, 678, Muted, 0.94);
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
        for (int Index = 0; Index < Backstage::FrameCount; ++Index)
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
        if (Round.Slots.Num() == Backstage::FrameCount)
            Text("P 预览    Enter 发布", 48, 614, Mint, 1.30);
        else
            Text("选满 3 张后可预览或发布。", 48, 614, Paper, 1.30);
        if (Round.bBraindanceActive)
            Text("R 返回超梦时间轴继续倒放与补拍", 472, 655, Muted, 1.09);
        else if (Round.Replays > 0)
            Text("R 回看现场（保留素材和剩余快门）", 472, 655, Muted, 1.09);
        Text("Backspace 返回简报", 48, 655, Muted, 1.09);
    }

    void Preview(const FBackstageRound&in Round)
    {
        const int Frame = Math::Min(int(Round.PreviewTime / Backstage::PreviewSecondsPerFrame), Backstage::FrameCount - 1);
        Viewfinder(Round.Cut[Frame], 28, 22, 1224, 676, false);
        ShadowText(Round.bPublishing ? "▶ VCR PUBLISHING  //  发布中" : "▶ VCR PLAYBACK  //  成片预览", 48, 40, Mint, 1.20);
        ShadowText(HeadlineText(Round.Headline), 48, 68, Paper, 1.48);
        ShadowText(f"FRAME  0{Frame + 1} / 03", 48, 656, Mint, 1.24);
        if (!Round.bPublishing) ShadowText("Backspace 返回剪辑", 1048, 658, Muted, 1.12);
    }

    void Results(const FBackstageRound&in Round, FVector2D Pointer)
    {
        const FBackstageResult Result = Round.Result;
        Text(HeadlineText(Round.Headline), 48, 90, Paper, 1.8);
        for (int Index = 0; Index < Backstage::FrameCount; ++Index)
        {
            const int PhotoIndex = Round.Slots.IsValidIndex(Index) ? Round.Slots[Index] : -1;
            const FBackstagePhoto& Original = Round.Photos.IsValidIndex(PhotoIndex) ? Round.Photos[PhotoIndex] : Round.Cut[Index];
            DrawPhotoOrShot(Original, Round.Cut[Index], PhotoIndex, 48 + float(Index) * 211, 140, 198, 110);
        }
        Text(f"关注度  {Signed(Result.Attention)}", 730, 149, Mint, 1.30);
        Text(f"公司信任  {Signed(Result.CompanyTrust)}", 730, 188, Paper, 1.30);
        Text(f"公众信任  {Signed(Result.PublicTrust)}", 972, 188, Paper, 1.30);
        Box(Panel, 48, 277, 1184, 328);
        Text("画面证据", 78, 308, Mint, 1.22);
        Text(Result.Evidence, 78, 345, Paper, 1.33);
        Text("观众：" + Result.Comment, 78, 412, Paper, 1.30);
        Text("公司：" + Result.Company, 78, 464, Paper, 1.30);
        Text(Result.Person, 78, 516, Paper, 1.30);

        const bool bHoverReplay = Pointer.X >= 48 && Pointer.X <= 290 && Pointer.Y >= 636 && Pointer.Y <= 684;
        const bool bHoverMenu = Pointer.X >= 320 && Pointer.X <= 560 && Pointer.Y >= 636 && Pointer.Y <= 684;

        Box(bHoverReplay ? FLinearColor(0.11, 0.25, 0.24, 0.96) : Panel, 48, 636, 242, 48);
        CornerBrackets(48, 636, 242, 48, 12, 2, bHoverReplay ? Mint : Muted);
        ShadowText("▶ 重新拍摄  (Enter)", 76, 650, bHoverReplay ? Mint : Paper, 1.20);

        Box(bHoverMenu ? FLinearColor(0.16, 0.12, 0.14, 0.96) : Panel, 320, 636, 240, 48);
        CornerBrackets(320, 636, 240, 48, 12, 2, bHoverMenu ? Gold : Muted);
        ShadowText("返回主菜单  (M)", 368, 650, bHoverMenu ? Gold : Muted, 1.16);
    }

    FString Signed(int Value)
    {
        return Value > 0 ? f"+{Value}" : f"{Value}";
    }
}
