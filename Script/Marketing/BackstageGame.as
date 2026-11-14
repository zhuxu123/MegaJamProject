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
    bool bWasW = false;
    bool bWasS = false;

    void EnsureStage()
    {
        if (StageActor == nullptr)
        {
            StageActor = ABackstageStageActor::Spawn(FVector::ZeroVector, FRotator::ZeroRotator);
            if (StageActor != nullptr)
                StageActor.InitializeStage(this);
        }
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

    UFUNCTION(BlueprintOverride)
    void Tick(float DeltaSeconds)
    {
        if (!IsLocalController())
            return;

        EnsureStage();

        const auto PreviousPhase = Round.Phase;
        switch (Round.Phase)
        {
            case EBackstagePhase::Briefing:
            case EBackstagePhase::Results:
                if (Pressed(n"Enter"))
                {
                    Round.Start();
                    Round.EnableBraindance();
                    BackstageClearPhotos();
                    EditSlot = 0;
                }
                break;
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
            SetBackstagePointer(Round.Phase == EBackstagePhase::Edit);

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
        if (Pressed(n"One")) Round.SetTargetStation(0);
        if (Pressed(n"Two")) Round.SetTargetStation(1);
        if (Pressed(n"Three")) Round.SetTargetStation(2);

        const FVector2D Mouse = BackstageMouseDelta();
        const float Wheel = BackstageWheelDelta();
        const float TiltAxis = CameraAxis(n"S", n"W", bWasS, bWasW, Delta);
        const float ZoomAxis = CameraAxis(n"Q", n"E", bWasQ, bWasE, Delta);
        Round.PushCamera(Mouse.X * 0.28, Mouse.Y * 0.20 + TiltAxis * 1.6, Wheel * 0.28 + ZoomAxis * 1.8);

        const bool bRewind = BackstageHeld(n"A") || BackstageHeld(n"Left");
        const bool bForward = BackstageHeld(n"D") || BackstageHeld(n"Right");
        Round.SetScrubInput((bForward ? 1.0 : 0.0) - (bRewind ? 1.0 : 0.0));

        if (Pressed(n"R") || Pressed(n"RightMouseButton") || Pressed(n"Down"))
            Round.TogglePause();

        if (Pressed(n"BackSpace") || Pressed(n"Home"))
        {
            Round.Camera.Time = 0.0;
            Round.TargetCamera.Time = 0.0;
            Round.PlaybackSpeed = 0.0;
        }

        if (Pressed(n"SpaceBar"))
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
        const FBackstageRound& Round = Controller.Round;

        if (Round.Phase == EBackstagePhase::Live)
            MaskStageWindow(SizeX, SizeY, 48, 125, 1184, 445);
        else if (Round.Phase == EBackstagePhase::Preview)
            MaskStageWindow(SizeX, SizeY, 48, 150, 1184, 440);
        else
            BackstageRect(FLinearColor(Ink.R, Ink.G, Ink.B, 0.84), 0, 0, SizeX, SizeY);

        Text("营销达人  /  慈善活动后台", 48, 34, Mint, 1.15);
        switch (Round.Phase)
        {
            case EBackstagePhase::Briefing: Briefing(); break;
            case EBackstagePhase::Live: Live(Round); break;
            case EBackstagePhase::Edit: Edit(Round, Controller.EditSlot, Controller.BackstagePointer()); break;
            case EBackstagePhase::Preview: Preview(Round); break;
            case EBackstagePhase::Results: Results(Round); break;
        }
    }

    void MaskStageWindow(int SizeX, int SizeY, float X, float Y, float W, float H)
    {
        const float Left = OriginX + X * Scale;
        const float Top = OriginY + Y * Scale;
        const float Right = OriginX + (X + W) * Scale;
        const float Bottom = OriginY + (Y + H) * Scale;
        if (Top > 0)
            BackstageRect(Ink, 0, 0, SizeX, Top);
        if (Bottom < SizeY)
            BackstageRect(Ink, 0, Bottom, SizeX, SizeY - Bottom);
        if (Left > 0)
            BackstageRect(Ink, 0, Top, Left, Bottom - Top);
        if (Right < SizeX)
            BackstageRect(Ink, Right, Top, SizeX - Right, Bottom - Top);
    }

    void Viewfinder(const FBackstagePhoto&in Photo, float X, float Y, float W, float H, bool bLive)
    {
        const FLinearColor Edge(0.36, 0.93, 0.73, 0.35);
        Box(Edge, X, Y, W, 1);
        Box(Edge, X, Y + H - 1, W, 1);
        Box(Edge, X, Y, 1, H);
        Box(Edge, X + W - 1, Y, 1, H);

        const float Bracket = 26;
        const float Thick = 3;
        Box(Mint, X, Y, Bracket, Thick);
        Box(Mint, X, Y, Thick, Bracket);
        Box(Mint, X + W - Bracket, Y, Bracket, Thick);
        Box(Mint, X + W - Thick, Y, Thick, Bracket);
        Box(Mint, X, Y + H - Thick, Bracket, Thick);
        Box(Mint, X, Y + H - Bracket, Thick, Bracket);
        Box(Mint, X + W - Bracket, Y + H - Thick, Bracket, Thick);
        Box(Mint, X + W - Thick, Y + H - Bracket, Thick, Bracket);

        const FLinearColor Guide(0.94, 0.97, 1.0, 0.14);
        Box(Guide, X + W / 3.0, Y + 12, 1, H - 24);
        Box(Guide, X + W * 2.0 / 3.0, Y + 12, 1, H - 24);

        if (bLive)
        {
            Box(Paper, X + W * 0.5 - 10, Y + H * 0.5, 20, 1);
            Box(Paper, X + W * 0.5, Y + H * 0.5 - 10, 1, 20);
            Box(FLinearColor(Ink.R, Ink.G, Ink.B, 0.65), X + 14, Y + 12, 92, 28);
            Text("● REC", X + 22, Y + 17, ArtistColor);
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

    void Briefing()
    {
        Text("活动简报", 48, 96, Paper, 2.6);
        Box(Panel, 48, 186, 1184, 386);
        Text("公司  /  活动回顾", 80, 218, Mint, 1.28);
        Text("去慈善活动后台拍三张照片，剪成一条能体现艺人亲和力的作品。", 80, 262, Paper, 1.48);
        Text("艺人私信", 80, 331, ArtistColor, 1.28);
        Text("我赶着上场。后台如果有乱七八糟的事，麻烦别发出去。", 80, 375, Paper, 1.48);
        Text("50 秒超梦回放（支持倒放与暂停）   ·   6 次快门   ·   发布 3 张", 80, 483, Muted, 1.30);
        Text("[ Enter ]  前往后台", 48, 638, Mint, 1.55);
    }

    void Live(const FBackstageRound&in Round)
    {
        const int WholeSec = int(Round.Camera.Time);
        const int Tenths = int(Round.Camera.Time * 10.0) % 10;
        const FString StatusLabel = Math::Abs(Round.ScrubInput) > 0.05
            ? (Round.ScrubInput < 0 ? "倒放 << 3x" : "快进 >> 3x")
            : (Round.bPaused ? "暂停 ||" : "超梦回放 1x");
        const FLinearColor StatusColor = Math::Abs(Round.ScrubInput) > 0.05 ? Gold : (Round.bPaused ? ArtistColor : Mint);

        Text(f"机位 {Round.Camera.Station + 1}  /  {WholeSec}.{Tenths} 秒", 48, 82, Paper, 1.30);
        Text(StatusLabel, 330, 82, StatusColor, 1.24);
        Text(f"横移 {int(Round.Camera.Pan * 30)}°   俯仰 {int(Round.Camera.Tilt * 15)}°   变焦 {int(Round.Camera.Zoom * 100)}%", 520, 82, Muted, 1.12);
        Text(f"快门 {Round.Shutters} / 6", 1070, 82, Paper, 1.30);

        Viewfinder(Round.Camera, 48, 125, 1184, 435, true);
        if (Round.ReplayNotice > 0)
        {
            Box(Ink, 365, 136, 538, 43);
            Text("已返回超梦时间轴，可继续倒放、微调机位与补拍", 389, 145, Mint, 1.16);
        }

        const float TrackX = 48;
        const float TrackY = 576;
        const float TrackW = 1184;
        Box(Panel, TrackX, TrackY, TrackW, 10);
        Box(StatusColor, TrackX, TrackY, TrackW * (Round.Camera.Time / Backstage::Duration), 10);

        const float MarkScale = TrackW / Backstage::Duration;
        Box(Ink, TrackX + 7.0 * MarkScale - 1, TrackY - 2, 2, 14);
        Box(Ink, TrackX + 14.0 * MarkScale - 1, TrackY - 2, 2, 14);
        Box(Ink, TrackX + 23.0 * MarkScale - 1, TrackY - 2, 2, 14);
        Box(Ink, TrackX + 33.0 * MarkScale - 1, TrackY - 2, 2, 14);
        Box(Ink, TrackX + 42.0 * MarkScale - 1, TrackY - 2, 2, 14);

        for (int PhotoIdx = 0; PhotoIdx < Round.Photos.Num(); ++PhotoIdx)
        {
            const float PinX = TrackX + TrackW * (Round.Photos[PhotoIdx].Time / Backstage::Duration);
            Box(Gold, PinX - 2, TrackY - 4, 4, 18);
        }

        const float HeadX = TrackX + TrackW * (Round.Camera.Time / Backstage::Duration);
        Box(Paper, HeadX - 2, TrackY - 6, 4, 22);

        Text("艺人", 48, 608, ArtistColor, 1.08);
        Text("志愿者", 126, 608, VolunteerColor, 1.08);
        Text("工作人员", 230, 608, CoordinatorColor, 1.08);
        Text("金色刻度 = 已拍照片时间点   |   Backspace 回到 0 秒", 805, 608, Muted, 1.04);
        Text("鼠标 转向/俯仰   滚轮/Q/E 变焦   1–3 机位   A/D 倒放/快进   R/右键 暂停   空格 拍摄   Enter 剪辑", 48, 656, Mint, 1.08);
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
        Text(HeadlineText(Round.Headline), 48, 88, Paper, 1.65);
        Text(Round.bPublishing ? "发布中" : "作品预览", 1084, 92, Mint, 1.22);
        Viewfinder(Round.Cut[Frame], 48, 150, 1184, 440, false);
        Text(f"{Frame + 1} / 3", 48, 621, Mint, 1.32);
        if (!Round.bPublishing) Text("Backspace 返回剪辑", 875, 621, Muted, 1.15);
    }

    void Results(const FBackstageRound&in Round)
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
        Text("[ Enter ]  重新拍摄", 48, 648, Mint, 1.45);
    }

    FString Signed(int Value)
    {
        return Value > 0 ? f"+{Value}" : f"{Value}";
    }
}
