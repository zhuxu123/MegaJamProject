class ABackstageGameMode : AGameModeBase
{
    default HUDClass = ABackstageHUD;
    default PlayerControllerClass = ABackstageController;
}

class ABackstageController : APlayerController
{
    FBackstageRound Round;
    int EditSlot = 0;
    bool bWasA = false;
    bool bWasD = false;
    bool bWasQ = false;
    bool bWasE = false;

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

        const auto PreviousPhase = Round.Phase;
        switch (Round.Phase)
        {
            case EBackstagePhase::Briefing:
            case EBackstagePhase::Results:
                if (Pressed(n"Enter"))
                {
                    Round.Start();
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
    }

    void HandleCamera(float Delta)
    {
        if (Pressed(n"One")) Round.Camera.Station = 0;
        if (Pressed(n"Two")) Round.Camera.Station = 1;
        if (Pressed(n"Three")) Round.Camera.Station = 2;
        Backstage::AdjustView(Round.Camera, CameraAxis(n"A", n"D", bWasA, bWasD, Delta),
            CameraAxis(n"Q", n"E", bWasQ, bWasE, Delta));
        if (Pressed(n"SpaceBar")) Round.Capture();
        if (Pressed(n"R")) Round.Replay();
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
        if (Pressed(n"R")) Round.Replay();
        if (Pressed(n"BackSpace")) Round = FBackstageRound();
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

        BackstageRect(Ink, 0, 0, SizeX, SizeY);
        Scale = Math::Min(SizeX / 1280.0, SizeY / 720.0);
        OriginX = (SizeX - Scale * 1280) * 0.5;
        OriginY = (SizeY - Scale * 720) * 0.5;
        const FBackstageRound& Round = Controller.Round;
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
        Text("现场 50 秒   ·   6 次快门   ·   1 次回看   ·   发布 3 张", 80, 483, Muted, 1.30);
        Text("[ Enter ]  前往后台", 48, 638, Mint, 1.55);
    }

    void Live(const FBackstageRound&in Round)
    {
        Text(f"机位 {Round.Camera.Station + 1}  /  {int(Round.Camera.Time)} 秒", 48, 82, Paper, 1.36);
        Text(f"转向 {int(Round.Camera.Pan * 30)}°   变焦 {int(Round.Camera.Zoom * 100)}%", 460, 82, Muted, 1.18);
        Text(f"快门 {Round.Shutters} / 6    回看 {Round.Replays} / 1", 898, 82, Paper, 1.36);
        Shot(Round.Camera, 48, 125, 1184, 445, true);
        if (Round.ReplayNotice > 0)
        {
            Box(Ink, 365, 136, 538, 43);
            Text("已回到开场，保留已拍素材和剩余快门", 389, 145, Mint, 1.18);
        }
        Box(Panel, 48, 580, 1184, 5);
        Box(Mint, 48, 580, 1184 * Round.Camera.Time / Backstage::Duration, 5);
        Text("艺人", 48, 607, ArtistColor, 1.08);
        Text("志愿者", 126, 607, VolunteerColor, 1.08);
        Text("工作人员", 230, 607, CoordinatorColor, 1.08);
        Text("1–3 机位    A / D 转向    Q / E 变焦    空格 拍摄    R 回看    Enter 剪辑", 48, 659, Mint, 1.18);
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
                Shot(Round.Photos[Index], X, 177, 184, 110);
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
                Shot(Round.Edits[Round.Slots[Index]], X, 392, 222, 120);
            if (Index == ActiveSlot) Box(Mint, X, 392, 222, 3);
            Text(f"槽 {Index + 1}", X + 12, 519, Index == ActiveSlot ? Mint : Paper);
        }
        Text("标题  /  T 切换", 798, 407, Muted, 1.11);
        Text(HeadlineText(Round.Headline), 798, 449, Paper, 1.30);
        if (Round.Slots.Num() == Backstage::FrameCount)
            Text("P 预览    Enter 发布", 48, 614, Mint, 1.30);
        else
            Text("选满 3 张后可预览或发布。", 48, 614, Paper, 1.30);
        if (Round.Replays > 0)
            Text("R 回看现场（保留素材和剩余快门）", 472, 655, Muted, 1.09);
        Text("Backspace 返回简报", 48, 655, Muted, 1.09);
    }

    void Preview(const FBackstageRound&in Round)
    {
        const int Frame = Math::Min(int(Round.PreviewTime / Backstage::PreviewSecondsPerFrame), Backstage::FrameCount - 1);
        Text(HeadlineText(Round.Headline), 48, 88, Paper, 1.65);
        Text(Round.bPublishing ? "发布中" : "作品预览", 1084, 92, Mint, 1.22);
        Shot(Round.Cut[Frame], 48, 150, 1184, 440);
        Text(f"{Frame + 1} / 3", 48, 621, Mint, 1.32);
        if (!Round.bPublishing) Text("Backspace 返回剪辑", 875, 621, Muted, 1.15);
    }

    void Results(const FBackstageRound&in Round)
    {
        const FBackstageResult Result = Round.Result;
        Text(HeadlineText(Round.Headline), 48, 90, Paper, 1.8);
        for (int Index = 0; Index < Backstage::FrameCount; ++Index)
            Shot(Round.Cut[Index], 48 + float(Index) * 211, 140, 198, 110);
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
