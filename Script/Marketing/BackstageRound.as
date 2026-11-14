enum EBackstagePhase
{
    Briefing, Live, Confirm, Edit, Preview, Results
}

enum EBackstageBeat
{
    Arrival, Collision, Argument, Pickup, Handoff, Together
}

enum EBackstageRainBeat
{
    EventIdle, RainStart, SnatchUmbrella, PassingFan, WalkAway
}

enum EBackstageHeadline
{
    Neutral, Helpful, Conflict
}

enum EBackstageReading
{
    Inconclusive, Helpful, Conflict, Accountable
}

struct FBackstageLevelConfig
{
    int LevelIndex = 0;
    FString CodeName = "LEVEL 01";
    FString Title = "雨夜品牌活动外场";
    FString TaskBrief = "拍摄林晚宠粉的温暖瞬间（需确认 1 张）";
    FString TruthBrief = "现场突降大雨，小北撑伞却被林晚嫌伞太小抢走。在林晚经过举牌粉丝旁时找准构图，拍出“把唯一的伞送给粉丝”的新闻图。";
    FString SuccessHeadline = "《雨中暖心举动！林晚把唯一的伞送给粉丝》";
    int RequiredPhotos = 1;
    int MaxShutters = 6;
    float Duration = 36.0;
    float TargetTimeStart = 21.5;
    float TargetTimeEnd = 30.5;
    bool bUsesEditBoard = false;
}

struct FBackstagePhoto
{
    float Time = 0;
    int Station = 0;
    float Pan = 0;
    float Tilt = 0;
    float Zoom = 1;
    float SmoothAngle = 0;
    float SmoothCenter = 0;
    bool bHasSmoothStation = false;
}

struct FBackstageResult
{
    EBackstageReading Reading = EBackstageReading::Inconclusive;
    bool bPassed = false;
    bool bTimeMatched = false;
    bool bSubjectsMatched = false;
    int Attention = 1;
    int CompanyTrust = 0;
    int PublicTrust = 0;
    FString NewspaperHeadline;
    FString NewspaperLead;
    FString CheckTimeText;
    FString CheckSubjectText;
    FString Comment;
    FString Company;
    FString Person;
    FString Evidence;
}

namespace Backstage
{
    const float Duration = 50;
    const int MaxPhotos = 6;
    const int FrameCount = 3;
    const int PendingCaptureSlot = 6;
    const float PreviewSecondsPerFrame = 2;
    const float InitialZoom = 1.45;
    const float ProjectionScale = 0.136;
    const float CameraTap = 0.12;
    const float PanLimit = 2.0;
    const float PanOvershoot = 0.42;
    const float TiltLimit = 1.0;
    const float TiltOvershoot = 0.35;
    const float ZoomMin = 1.0;
    const float ZoomMax = 2.8;
    const float ZoomOvershoot = 0.22;

    FBackstageLevelConfig GetLevelConfig(int LevelIndex)
    {
        FBackstageLevelConfig Cfg;
        if (LevelIndex == 1)
        {
            Cfg.LevelIndex = 1;
            Cfg.CodeName = "LEVEL 02";
            Cfg.Title = "慈善活动后台";
            Cfg.TaskBrief = "拍摄 3 张现场照片并剪辑发布（需确认 3 张）";
            Cfg.TruthBrief = "“我赶着上场，后台如果有乱七八糟的事，麻烦别发出去。”";
            Cfg.SuccessHeadline = "《慈善活动后台直击：真相还是人设？》";
            Cfg.RequiredPhotos = 3;
            Cfg.MaxShutters = 6;
            Cfg.Duration = 50.0;
            Cfg.TargetTimeStart = 0.0;
            Cfg.TargetTimeEnd = 50.0;
            Cfg.bUsesEditBoard = true;
            return Cfg;
        }
        return Cfg;
    }

    float PhotoLeft(int Index) { return 48 + Index * 200; }
    float SlotLeft(int Index) { return 48 + Index * 245; }

    int PhotoUnderPointer(FVector2D Point)
    {
        if (Point.Y < 177 || Point.Y >= 323)
            return -1;
        for (int Index = 0; Index < MaxPhotos; ++Index)
            if (Point.X >= PhotoLeft(Index) && Point.X < PhotoLeft(Index) + 184)
                return Index;
        return -1;
    }

    int SlotUnderPointer(FVector2D Point)
    {
        if (Point.Y < 392 || Point.Y >= 548)
            return -1;
        for (int Index = 0; Index < FrameCount; ++Index)
            if (Point.X >= SlotLeft(Index) && Point.X < SlotLeft(Index) + 222)
                return Index;
        return -1;
    }

    float CameraInput(bool bNegativePressed, bool bPositivePressed, bool bNegativeHeld, bool bPositiveHeld, float Delta)
    {
        const float Step = Math::Max(Delta, CameraTap);
        const float Negative = bNegativePressed ? Step : (bNegativeHeld ? Delta : 0);
        const float Positive = bPositivePressed ? Step : (bPositiveHeld ? Delta : 0);
        return Positive - Negative;
    }

    void AdjustView(FBackstagePhoto&inout View, float PanInput, float ZoomInput, float TiltInput = 0.0)
    {
        View.Pan = Math::Clamp(View.Pan + PanInput * 2.5, -PanLimit, PanLimit);
        View.Tilt = Math::Clamp(View.Tilt + TiltInput * 1.8, -TiltLimit, TiltLimit);
        View.Zoom = Math::Clamp(View.Zoom + ZoomInput * 1.8, ZoomMin, ZoomMax);
    }

    float StationAngle(int Station)
    {
        return Station == 0 ? 0.0 : (Station == 1 ? -0.63 : 0.63);
    }

    float StationCenter(int Station)
    {
        return Station == 0 ? 0.0 : (Station == 1 ? -0.78 : 0.65);
    }

    float EffectiveAngle(const FBackstagePhoto&in Photo)
    {
        return Photo.bHasSmoothStation ? Photo.SmoothAngle : StationAngle(Photo.Station);
    }

    float EffectiveCenter(const FBackstagePhoto&in Photo)
    {
        return Photo.bHasSmoothStation ? Photo.SmoothCenter : StationCenter(Photo.Station);
    }

    float CameraPitch(float Tilt)
    {
        return -2.5 + Tilt * 10.5;
    }

    float DampCurve(float Current, float Target, float Delta, float Speed)
    {
        const float Alpha = 1.0 - Math::Exp(-Speed * Math::Max(Delta, 0.0));
        return Math::Lerp(Current, Target, Alpha);
    }

    float ElasticStep(float Current, float Delta, float MinVal, float MaxVal, float MaxOvershoot)
    {
        if (Delta == 0)
            return Current;
        float Resistance = 1.0;
        if (Current < MinVal && Delta < 0)
        {
            const float T = Math::Clamp((MinVal - Current) / Math::Max(MaxOvershoot, 0.001), 0.0, 1.0);
            Resistance = Math::Max(0.12, (1.0 - T) * (1.0 - T));
        }
        else if (Current > MaxVal && Delta > 0)
        {
            const float T = Math::Clamp((Current - MaxVal) / Math::Max(MaxOvershoot, 0.001), 0.0, 1.0);
            Resistance = Math::Max(0.12, (1.0 - T) * (1.0 - T));
        }
        return Math::Clamp(Current + Delta * Resistance, MinVal - MaxOvershoot, MaxVal + MaxOvershoot);
    }

    float ReboundToRange(float Value, float MinVal, float MaxVal, float Delta, float Rate)
    {
        const float Alpha = 1.0 - Math::Exp(-Rate * Math::Max(Delta, 0.0));
        if (Value < MinVal)
            return Math::Lerp(Value, MinVal, Alpha);
        if (Value > MaxVal)
            return Math::Lerp(Value, MaxVal, Alpha);
        return Value;
    }

    EBackstageBeat BeatAt(float Time)
    {
        if (Time < 7) return EBackstageBeat::Arrival;
        if (Time < 14) return EBackstageBeat::Collision;
        if (Time < 23) return EBackstageBeat::Argument;
        if (Time < 33) return EBackstageBeat::Pickup;
        if (Time < 42) return EBackstageBeat::Handoff;
        return EBackstageBeat::Together;
    }

    FString BeatName(EBackstageBeat Beat)
    {
        switch (Beat)
        {
            case EBackstageBeat::Arrival: return "进场";
            case EBackstageBeat::Collision: return "碰撞";
            case EBackstageBeat::Argument: return "争执";
            case EBackstageBeat::Pickup: return "拾起";
            case EBackstageBeat::Handoff: return "递交";
            default: return "同行";
        }
    }

    FVector2D ArtistAt(float Time)
    {
        if (Time < 7)
            return FVector2D(Math::Lerp(-2.7, -0.55, Time / 7.0), 0.45);
        if (Time < 14)
            return FVector2D(Math::Lerp(-0.55, -1.15, Math::Clamp((Time - 7) / 2.5, 0.0, 1.0)), 0.45);
        if (Time < 42)
            return FVector2D(-1.15, 0.45);
        return FVector2D(-0.84, 0.22);
    }

    FVector2D VolunteerAt(float Time)
    {
        return FVector2D(Time < 7 ? 0.12 : 0.82, -0.12);
    }

    FVector2D CoordinatorAt(float Time)
    {
        return FVector2D(2.85 - Math::Clamp((Time - 31) / 8.0, 0.0, 1.0) * 1.17, 0.7);
    }

    FVector2D ParcelAt(float Time)
    {
        auto Person = VolunteerAt(Time);
        if (Time < 7 || Time >= 42)
            return Person + FVector2D(-0.30, 0);
        if (Time < 23)
            return FVector2D(0.05, 0.40);
        if (Time < 33)
            return FVector2D(-0.37, 0.40);
        return FVector2D(-0.06, 0.18);
    }

    FVector2D Project(const FBackstagePhoto&in Photo, FVector2D Point)
    {
        const float Angle = EffectiveAngle(Photo);
        const float Center = EffectiveCenter(Photo);
        return FVector2D(0.5 + (Point.X * Math::Cos(Angle) + Point.Y * Math::Sin(Angle)
            - Center - Photo.Pan) * ProjectionScale * Photo.Zoom, ProjectHeight(Photo, 0.67 + Point.Y * 0.15));
    }

    float ProjectHeight(const FBackstagePhoto&in Photo, float HeightAtWideAngle)
    {
        return 0.65 + (HeightAtWideAngle - 0.65) * Photo.Zoom + Photo.Tilt * 0.14 * Photo.Zoom;
    }

    bool Visible(const FBackstagePhoto&in Photo, FVector2D Point)
    {
        const double X = Project(Photo, Point).X;
        return X >= 0.11 && X <= 0.89;
    }

    EBackstageRainBeat RainBeatAt(float Time)
    {
        if (Time < 6) return EBackstageRainBeat::EventIdle;
        if (Time < 13) return EBackstageRainBeat::RainStart;
        if (Time < 20) return EBackstageRainBeat::SnatchUmbrella;
        if (Time < 31) return EBackstageRainBeat::PassingFan;
        return EBackstageRainBeat::WalkAway;
    }

    FString RainBeatName(EBackstageRainBeat Beat)
    {
        switch (Beat)
        {
            case EBackstageRainBeat::EventIdle: return "活动开场";
            case EBackstageRainBeat::RainStart: return "突降大雨";
            case EBackstageRainBeat::SnatchUmbrella: return "夺伞离去";
            case EBackstageRainBeat::PassingFan: return "经过粉丝";
            default: return "独自走远";
        }
    }

    bool IsRainingAt(int LevelIndex, float Time)
    {
        return LevelIndex == 0 && Time >= 6.0;
    }

    FVector2D LinWanAt(float Time)
    {
        if (Time < 13)
            return FVector2D(-1.45, 0.32);
        if (Time < 16)
            return FVector2D(Math::Lerp(-1.45, -1.30, (Time - 13.0) / 3.0), 0.32);
        if (Time < 21.5)
            return FVector2D(Math::Lerp(-1.30, 0.45, (Time - 16.0) / 5.5), Math::Lerp(0.32, 0.22, (Time - 16.0) / 5.5));
        if (Time <= 30.5)
            return FVector2D(Math::Lerp(0.45, 1.55, (Time - 21.5) / 9.0), 0.22);
        return FVector2D(Math::Lerp(1.55, 2.85, Math::Clamp((Time - 30.5) / 5.5, 0.0, 1.0)), 0.25);
    }

    FVector2D XiaoBeiAt(float Time)
    {
        if (Time < 6)
            return FVector2D(-2.05, 0.42);
        if (Time < 9)
            return FVector2D(Math::Lerp(-2.05, -1.75, (Time - 6.0) / 3.0), Math::Lerp(0.42, 0.36, (Time - 6.0) / 3.0));
        return FVector2D(-1.75, 0.36);
    }

    FVector2D FanAt(float Time)
    {
        return FVector2D(1.92, -0.08);
    }

    FVector2D UmbrellaAt(float Time)
    {
        const FVector2D XiaoBei = XiaoBeiAt(Time);
        const FVector2D LinWan = LinWanAt(Time);
        if (Time < 13)
            return XiaoBei + FVector2D(0.26, -0.02);
        if (Time < 15)
        {
            const float Alpha = (Time - 13.0) / 2.0;
            return FVector2D(Math::Lerp(XiaoBei.X + 0.26, LinWan.X + 0.12, Alpha), Math::Lerp(XiaoBei.Y, LinWan.Y, Alpha));
        }
        return LinWan + FVector2D(0.12, -0.02);
    }

    FVector2D FanSignAt(float Time)
    {
        return FanAt(Time) + FVector2D(-0.18, -0.06);
    }

    FBackstageResult EvaluateLevel1(const FBackstageLevelConfig&in Cfg, const TArray<FBackstagePhoto>&in Sequence)
    {
        FBackstageResult Result;
        if (Sequence.Num() == 0)
        {
            Result.NewspaperHeadline = "《投稿无效：尚未提交照片》";
            Result.NewspaperLead = "编辑部未收到有效成片，请先拍摄并确认 1 张照片。";
            return Result;
        }

        const FBackstagePhoto Photo = Sequence[0];
        const bool bTimeOk = Photo.Time >= Cfg.TargetTimeStart && Photo.Time <= Cfg.TargetTimeEnd;
        const bool bLinWan = Visible(Photo, LinWanAt(Photo.Time));
        const bool bUmbrella = Visible(Photo, UmbrellaAt(Photo.Time));
        const bool bFan = Visible(Photo, FanAt(Photo.Time));
        const bool bFanSign = Visible(Photo, FanSignAt(Photo.Time));
        const bool bXiaoBei = Visible(Photo, XiaoBeiAt(Photo.Time));

        Result.bTimeMatched = bTimeOk;
        Result.bSubjectsMatched = bLinWan && bUmbrella && bFan && bFanSign && !bXiaoBei;
        Result.CheckTimeText = bTimeOk
            ? f"✓ 时间段吻合（{int(Photo.Time)}s · 处于林晚经过粉丝的黄金窗口）"
            : f"✕ 时间段不符（当前 {int(Photo.Time)}s · 需在 21.5–30.5s 雨中经过粉丝时拍摄）";

        if (Result.bSubjectsMatched)
        {
            Result.CheckSubjectText = "✓ 人物与道具构图到位（含林晚、雨伞、举牌粉丝，且已裁掉淋雨的助理小北）";
        }
        else if (!bLinWan || !bUmbrella)
        {
            Result.CheckSubjectText = "✕ 画面缺少林晚或雨伞主体";
        }
        else if (!bFan || !bFanSign)
        {
            Result.CheckSubjectText = "✕ 画面未包含举着林晚应援牌的粉丝";
        }
        else
        {
            Result.CheckSubjectText = "✕ 画面左方拍到了被留在雨里的小北（请放大或右移裁掉小北）";
        }

        if (Result.bTimeMatched && Result.bSubjectsMatched)
        {
            Result.bPassed = true;
            Result.Reading = EBackstageReading::Helpful;
            Result.Attention = 6;
            Result.CompanyTrust = 5;
            Result.PublicTrust = 4;
            Result.NewspaperHeadline = "《雨中暖心举动!林晚把唯一的伞送给粉丝》";
            Result.NewspaperLead = "昨夜品牌户外活动突发急雨，当红女星林晚被镜头捕捉到撑着唯一的雨伞走向举牌守候的粉丝身旁，温情画面迅速登上热搜头版。";
            Result.Evidence = Result.CheckTimeText + "  |  " + Result.CheckSubjectText;
            Result.Comment = "“姐姐雨这么大还优先照顾粉丝，路转粉了！”";
            Result.Company = "团队：“完美的角度！公关危机直接变成年度暖心热搜！”";
            Result.Person = "小北（画外音）：“……没人看到我在后面淋透了吗？”";
        }
        else
        {
            Result.bPassed = false;
            Result.Reading = bXiaoBei ? EBackstageReading::Conflict : EBackstageReading::Inconclusive;
            Result.Attention = bXiaoBei ? 4 : 1;
            Result.CompanyTrust = bXiaoBei ? -3 : -1;
            Result.PublicTrust = -1;
            Result.NewspaperHeadline = bXiaoBei
                ? "《公关翻车：淋雨助理与抢伞同框曝光》"
                : "《废稿退回：新闻主题说服力不足》";
            Result.NewspaperLead = !Result.bTimeMatched
                ? "成片拍摄时机尚未形成“雨中靠近粉丝送伞”的错位视觉，报社无法按预设暖心稿件刊登。"
                : (bXiaoBei
                    ? "成片将淋雨发愣的助理小北一并拍进画框，评论区一眼看穿“唯一雨伞”另有隐情！"
                    : "画面缺少关键人物或应援牌特征，读者无法认出林晚与粉丝互动。");
            Result.Evidence = Result.CheckTimeText + "  |  " + Result.CheckSubjectText;
            Result.Comment = bXiaoBei ? "网友：“后面那个浑身湿透的小北是谁？”" : "网友：“这拍的是什么，完全看不懂。”";
            Result.Company = "主编：“重拍！时间窗口要选林晚经过粉丝时，构图必须框住她、伞和粉丝且裁掉小北。”";
            Result.Person = "提示：使用 ←/→ 拖到 22–30s，滚轮放大或按住左键向右拖拽构图，按 Enter 重拍。";
        }
        return Result;
    }

    FBackstageResult EvaluateLevel(const FBackstageLevelConfig&in Cfg, const TArray<FBackstagePhoto>&in Sequence, EBackstageHeadline Headline)
    {
        if (Cfg.LevelIndex == 0)
            return EvaluateLevel1(Cfg, Sequence);

        FBackstageResult Result = Evaluate(Sequence, Headline);
        Result.bTimeMatched = Sequence.Num() >= Cfg.RequiredPhotos;
        Result.bSubjectsMatched = Result.Reading != EBackstageReading::Inconclusive;
        Result.bPassed = Result.bTimeMatched && Result.bSubjectsMatched;
        Result.CheckTimeText = Result.bTimeMatched ? f"✓ 已提交 {Sequence.Num()}/{Cfg.RequiredPhotos} 张连环组图" : "✕ 照片张数不足";
        Result.CheckSubjectText = Result.bSubjectsMatched ? "✓ 画面人物与证据链有效" : "✕ 顺序或构图遗漏关键人物证据";
        if (Headline == EBackstageHeadline::Helpful)
            Result.NewspaperHeadline = "《慈善后台暖心瞬间：顶流蹲身帮志愿者整理物资》";
        else if (Headline == EBackstageHeadline::Conflict)
            Result.NewspaperHeadline = "《慈善夜幕后黑脸？艺人疑与工作人员起争执》";
        else
            Result.NewspaperHeadline = "《慈善活动后台纪实：现场突发状况全记录》";
        Result.NewspaperLead = Result.Comment + "  " + Result.Evidence;
        return Result;
    }

    FBackstageResult Evaluate(const TArray<FBackstagePhoto>&in Sequence, EBackstageHeadline Headline)
    {
        int Collision = -1;
        int Argument = -1;
        int Scattered = -1;
        int Pickup = -1;
        int Handoff = -1;
        int Together = -1;
        for (int Index = 0; Index < Sequence.Num(); ++Index)
        {
            const FBackstagePhoto Photo = Sequence[Index];
            const auto Beat = BeatAt(Photo.Time);
            const bool Artist = Visible(Photo, ArtistAt(Photo.Time));
            const bool Volunteer = Visible(Photo, VolunteerAt(Photo.Time));
            const bool Parcel = Visible(Photo, ParcelAt(Photo.Time));
            if (Beat == EBackstageBeat::Collision && Artist && Volunteer && Parcel && Collision < 0 && Photo.Time < 9)
                Collision = Index;
            if (Beat == EBackstageBeat::Argument && Artist && Volunteer && Argument < 0)
                Argument = Index;
            if ((Beat == EBackstageBeat::Collision || Beat == EBackstageBeat::Argument) && Parcel && Scattered < 0 && Argument >= 0 && Index > Argument)
                Scattered = Index;
            if (Beat == EBackstageBeat::Pickup && Artist && Parcel && Pickup < 0)
                Pickup = Index;
            if (Beat == EBackstageBeat::Handoff && Artist && Volunteer && Parcel && Handoff < 0)
                Handoff = Index;
            if (Beat == EBackstageBeat::Together && Artist && Volunteer && Together < 0)
                Together = Index;
        }

        FBackstageResult Result;
        if (Collision >= 0 && Argument > Collision && Pickup > Argument)
        {
            Result.Reading = EBackstageReading::Accountable;
            Result.Attention = 3;
            Result.CompanyTrust = -1;
            Result.PublicTrust = 4;
            Result.Evidence = f"第{Collision + 1}张交代碰撞，第{Argument + 1}张保留争执，第{Pickup + 1}张拍到补救。";
            Result.Comment = "她碰倒了东西，后来也蹲下来帮忙。这段至少交代清楚了。";
            Result.Company = "事实完整，但失态也跟着公开了。";
            Result.Person = "艺人：我希望你发布前先和我说一声。";
        }
        else if (Pickup >= 0 && Handoff > Pickup && Together > Handoff)
        {
            Result.Reading = EBackstageReading::Helpful;
            Result.Attention = 2;
            Result.CompanyTrust = 4;
            Result.PublicTrust = 1;
            Result.Evidence = f"第{Pickup + 1}张捡起物品，第{Handoff + 1}张交还，第{Together + 1}张两人同框。";
            Result.Comment = "她还蹲下来帮忙捡，看起来挺照顾工作人员。";
            Result.Company = "亲和力很自然。我们会用这条做活动回顾。";
            Result.Person = "志愿者：前面东西为什么掉地上，你也拍到了吧？";
        }
        else if (Argument >= 0 && Scattered > Argument && Pickup < 0 && Handoff < 0)
        {
            Result.Reading = EBackstageReading::Conflict;
            Result.Attention = 5;
            Result.CompanyTrust = -4;
            Result.PublicTrust = -1;
            Result.Evidence = f"第{Argument + 1}张出现争执，第{Scattered + 1}张接上散落物品，成片省略了补救。";
            Result.Comment = "她刚指着人说话，下一秒就是东西散了一地，发生了什么？";
            Result.Company = "合作方暂停了转发，想先了解事情经过。";
            Result.Person = "艺人：我后来帮忙收拾了，你为什么没发那段？";
        }
        else
        {
            Result.Evidence = "顺序或构图遗漏了把动作连起来的证据。";
            Result.Comment = "看出了几个片段，可是它们之间发生了什么？";
            Result.Company = "下次把人物、动作和相关物件交代清楚。";
            Result.Person = "志愿者：如果有人问，我可以告诉他们现场发生了什么。";
        }

        if ((Headline == EBackstageHeadline::Helpful && Result.Reading == EBackstageReading::Helpful)
            || (Headline == EBackstageHeadline::Conflict && Result.Reading == EBackstageReading::Conflict))
            Result.Attention += 1;
        else if (Headline != EBackstageHeadline::Neutral)
            Result.PublicTrust -= 1;
        return Result;
    }
}

struct FBackstageRound
{
    int LevelIndex = 0;
    FBackstageLevelConfig Config = Backstage::GetLevelConfig(0);
    EBackstagePhase Phase = EBackstagePhase::Briefing;
    FBackstagePhoto Camera;
    FBackstagePhoto TargetCamera;
    FBackstagePhoto PendingPhoto;
    bool bHasPending = false;
    TArray<FBackstagePhoto> Photos;
    TArray<int> Slots;
    TArray<FBackstagePhoto> Edits;
    TArray<FBackstagePhoto> Cut;
    FBackstageResult Result;
    EBackstageHeadline Headline = EBackstageHeadline::Neutral;
    int Shutters = Backstage::MaxPhotos;
    int Replays = 1;
    float PreviewTime = 0;
    float ConfirmClock = 0;
    float ResultsClock = 0;
    float ReplayNotice = 0;
    bool bPublishing = false;
    bool bBraindanceActive = false;
    bool bSmoothCamera = false;
    bool bPaused = false;
    float PlaybackSpeed = 1.0;
    float ScrubInput = 0.0;
    bool bPushingPan = false;
    bool bPushingTilt = false;
    bool bPushingZoom = false;

    float ActiveDuration() const
    {
        return Math::Max(Config.Duration, 1.0);
    }

    int RequiredFrameCount() const
    {
        return Math::Clamp(Config.RequiredPhotos, 1, Backstage::FrameCount);
    }

    void PrepareBriefing(int InLevelIndex)
    {
        this = FBackstageRound();
        LevelIndex = Math::Clamp(InLevelIndex, 0, 1);
        Config = Backstage::GetLevelConfig(LevelIndex);
        Shutters = Config.MaxShutters;
        Camera.Zoom = Backstage::InitialZoom;
        TargetCamera = Camera;
        Phase = EBackstagePhase::Briefing;
    }

    void StartLevel(int InLevelIndex)
    {
        PrepareBriefing(InLevelIndex);
        Phase = EBackstagePhase::Live;
    }

    void Start()
    {
        StartLevel(1);
    }

    void EnableBraindance()
    {
        bBraindanceActive = true;
        bSmoothCamera = true;
        bPaused = false;
        PlaybackSpeed = 1.0;
        ScrubInput = 0.0;
        Camera.SmoothAngle = Backstage::StationAngle(Camera.Station);
        Camera.SmoothCenter = Backstage::StationCenter(Camera.Station);
        Camera.bHasSmoothStation = true;
        TargetCamera = Camera;
    }

    void SetTargetStation(int Station)
    {
        const int ClampedStation = Math::Clamp(Station, 0, 2);
        if (!bSmoothCamera)
        {
            TargetCamera = Camera;
            bSmoothCamera = true;
        }
        if (!Camera.bHasSmoothStation)
        {
            Camera.SmoothAngle = Backstage::StationAngle(Camera.Station);
            Camera.SmoothCenter = Backstage::StationCenter(Camera.Station);
            Camera.bHasSmoothStation = true;
        }
        TargetCamera.Station = ClampedStation;
        TargetCamera.SmoothAngle = Backstage::StationAngle(ClampedStation);
        TargetCamera.SmoothCenter = Backstage::StationCenter(ClampedStation);
        TargetCamera.bHasSmoothStation = true;
        Camera.Station = ClampedStation;
    }

    void PushCamera(float PanDelta, float TiltDelta, float ZoomDelta)
    {
        if (!bSmoothCamera)
        {
            TargetCamera = Camera;
            bSmoothCamera = true;
        }
        if (Math::Abs(PanDelta) > 0.0001)
        {
            TargetCamera.Pan = Backstage::ElasticStep(TargetCamera.Pan, PanDelta,
                -Backstage::PanLimit, Backstage::PanLimit, Backstage::PanOvershoot);
            bPushingPan = true;
        }
        if (Math::Abs(TiltDelta) > 0.0001)
        {
            TargetCamera.Tilt = Backstage::ElasticStep(TargetCamera.Tilt, TiltDelta,
                -Backstage::TiltLimit, Backstage::TiltLimit, Backstage::TiltOvershoot);
            bPushingTilt = true;
        }
        if (Math::Abs(ZoomDelta) > 0.0001)
        {
            TargetCamera.Zoom = Backstage::ElasticStep(TargetCamera.Zoom, ZoomDelta,
                Backstage::ZoomMin, Backstage::ZoomMax, Backstage::ZoomOvershoot);
            bPushingZoom = true;
        }
    }

    void SetScrubInput(float Input)
    {
        ScrubInput = Math::Clamp(Input, -1.0, 1.0);
    }

    void TogglePause()
    {
        if (Phase != EBackstagePhase::Live)
            return;
        bPaused = !bPaused;
        if (!bPaused && Camera.Time >= ActiveDuration() - 0.01)
            Camera.Time = 0.0;
    }

    void ReturnToBraindance()
    {
        if (Phase != EBackstagePhase::Edit && Phase != EBackstagePhase::Live && Phase != EBackstagePhase::Confirm)
            return;
        Phase = EBackstagePhase::Live;
        bHasPending = false;
        bBraindanceActive = true;
        bSmoothCamera = true;
        bPaused = true;
        PlaybackSpeed = 0.0;
        ScrubInput = 0.0;
        ReplayNotice = 1.8;
    }

    void Advance(float Delta)
    {
        if (Phase == EBackstagePhase::Live)
        {
            ReplayNotice = Math::Max(0, ReplayNotice - Delta);

            if (bSmoothCamera)
            {
                if (!bPushingPan)
                    TargetCamera.Pan = Backstage::ReboundToRange(TargetCamera.Pan, -Backstage::PanLimit, Backstage::PanLimit, Delta, 10.0);
                if (!bPushingTilt)
                    TargetCamera.Tilt = Backstage::ReboundToRange(TargetCamera.Tilt, -Backstage::TiltLimit, Backstage::TiltLimit, Delta, 10.0);
                if (!bPushingZoom)
                    TargetCamera.Zoom = Backstage::ReboundToRange(TargetCamera.Zoom, Backstage::ZoomMin, Backstage::ZoomMax, Delta, 10.0);

                bPushingPan = false;
                bPushingTilt = false;
                bPushingZoom = false;

                Camera.Pan = Backstage::DampCurve(Camera.Pan, TargetCamera.Pan, Delta, 13.5);
                Camera.Tilt = Backstage::DampCurve(Camera.Tilt, TargetCamera.Tilt, Delta, 13.5);
                Camera.Zoom = Backstage::DampCurve(Camera.Zoom, TargetCamera.Zoom, Delta, 12.0);
                if (Camera.bHasSmoothStation)
                {
                    Camera.SmoothAngle = Backstage::DampCurve(Camera.SmoothAngle, TargetCamera.SmoothAngle, Delta, 9.5);
                    Camera.SmoothCenter = Backstage::DampCurve(Camera.SmoothCenter, TargetCamera.SmoothCenter, Delta, 9.5);
                }
            }

            const float Dur = ActiveDuration();
            if (bBraindanceActive)
            {
                float TargetSpeed = bPaused ? 0.0 : 1.0;
                if (Math::Abs(ScrubInput) > 0.05)
                    TargetSpeed = ScrubInput * 3.0;

                PlaybackSpeed = Backstage::DampCurve(PlaybackSpeed, TargetSpeed, Delta, 12.0);
                Camera.Time = Math::Clamp(Camera.Time + Delta * PlaybackSpeed, 0.0, Dur);
                TargetCamera.Time = Camera.Time;

                if (Camera.Time >= Dur && !bPaused && ScrubInput >= 0.0)
                {
                    bPaused = true;
                    PlaybackSpeed = 0.0;
                }
            }
            else
            {
                Camera.Time = Math::Min(Camera.Time + Delta, Dur);
                if (Camera.Time >= Dur)
                    Phase = EBackstagePhase::Edit;
            }
        }
        else if (Phase == EBackstagePhase::Confirm)
        {
            ConfirmClock += Delta;
        }
        else if (Phase == EBackstagePhase::Preview)
        {
            PreviewTime += Delta;
            if (PreviewTime >= RequiredFrameCount() * Backstage::PreviewSecondsPerFrame)
            {
                if (bPublishing)
                    ResultsClock = 0.0;
                Phase = bPublishing ? EBackstagePhase::Results : EBackstagePhase::Edit;
            }
        }
        else if (Phase == EBackstagePhase::Results)
        {
            ResultsClock += Delta;
        }
    }

    void CapturePending()
    {
        if (Phase != EBackstagePhase::Live || Shutters <= 0)
            return;
        PendingPhoto = Camera;
        bHasPending = true;
        ConfirmClock = 0.0;
        bPaused = true;
        PlaybackSpeed = 0.0;
        ScrubInput = 0.0;
        Phase = EBackstagePhase::Confirm;
    }

    bool AcceptPending()
    {
        if (Phase != EBackstagePhase::Confirm || !bHasPending)
            return false;
        bHasPending = false;
        Photos.Add(PendingPhoto);
        Edits.Add(PendingPhoto);
        if (Shutters > 0)
            --Shutters;

        const int Needed = RequiredFrameCount();
        if (Photos.Num() >= Needed)
        {
            if (!Config.bUsesEditBoard)
            {
                Slots.Reset();
                Cut.Reset();
                for (int Idx = 0; Idx < Needed && Photos.IsValidIndex(Idx); ++Idx)
                {
                    Slots.Add(Idx);
                    Cut.Add(Edits[Idx]);
                }
                Headline = EBackstageHeadline::Helpful;
                Result = Backstage::EvaluateLevel(Config, Cut, Headline);
                ResultsClock = 0.0;
                Phase = EBackstagePhase::Results;
            }
            else
            {
                Slots.Reset();
                for (int Idx = 0; Idx < Needed && Photos.IsValidIndex(Idx); ++Idx)
                    Slots.Add(Idx);
                Phase = EBackstagePhase::Edit;
            }
        }
        else
        {
            Phase = EBackstagePhase::Live;
            ReplayNotice = 1.6;
        }
        return true;
    }

    void RejectPending()
    {
        if (Phase != EBackstagePhase::Confirm)
            return;
        bHasPending = false;
        Phase = EBackstagePhase::Live;
    }

    void Capture()
    {
        if (Phase != EBackstagePhase::Live || Shutters == 0)
            return;
        Photos.Add(Camera);
        Edits.Add(Camera);
        --Shutters;
    }

    void Replay()
    {
        if (bBraindanceActive)
        {
            ReturnToBraindance();
            return;
        }
        if ((Phase != EBackstagePhase::Edit && Phase != EBackstagePhase::Live) || Replays == 0)
            return;
        --Replays;
        Camera.Time = 0;
        ReplayNotice = 2;
        Phase = EBackstagePhase::Live;
    }

    void TogglePhoto(int Index)
    {
        if (!Photos.IsValidIndex(Index) || Phase != EBackstagePhase::Edit)
            return;
        const int Existing = Slots.FindIndex(Index);
        if (Existing >= 0)
            Slots.RemoveAt(Existing);
        else if (Slots.Num() < RequiredFrameCount())
            Slots.Add(Index);
    }

    void SwapSlot(int From, int To)
    {
        if (!Slots.IsValidIndex(From) || !Slots.IsValidIndex(To))
            return;
        const int Original = Slots[From];
        Slots[From] = Slots[To];
        Slots[To] = Original;
    }

    void AdjustCrop(int Slot, float PanInput, float ZoomInput)
    {
        if (!Slots.IsValidIndex(Slot) || (PanInput == 0 && ZoomInput == 0))
            return;
        const int Index = Slots[Slot];
        const FBackstagePhoto Original = Photos[Index];
        FBackstagePhoto& Photo = Edits[Index];
        Photo.Zoom = Math::Clamp(Photo.Zoom + ZoomInput * 1.8, Original.Zoom, Original.Zoom * 1.8);
        // Keep the cropped frame within the original photo.
        const float Travel = 0.5 / Backstage::ProjectionScale * (1 / Original.Zoom - 1 / Photo.Zoom);
        Photo.Pan = Math::Clamp(Photo.Pan + PanInput * 2.5, Original.Pan - Travel, Original.Pan + Travel);
    }

    void Preview(bool bPublish)
    {
        if (Phase != EBackstagePhase::Edit || Slots.Num() != RequiredFrameCount())
            return;
        Cut.Reset();
        for (int Index : Slots)
            Cut.Add(Edits[Index]);
        bPublishing = bPublish;
        PreviewTime = 0;
        if (bPublish)
        {
            Result = Backstage::EvaluateLevel(Config, Cut, Headline);
            ResultsClock = 0.0;
        }
        Phase = EBackstagePhase::Preview;
    }
}
