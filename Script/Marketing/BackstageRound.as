enum EBackstagePhase
{
    Briefing, Live, Edit, Preview, Results
}

enum EBackstageBeat
{
    Arrival, Collision, Argument, Pickup, Handoff, Together
}

enum EBackstageHeadline
{
    Neutral, Helpful, Conflict
}

enum EBackstageReading
{
    Inconclusive, Helpful, Conflict, Accountable
}

struct FBackstagePhoto
{
    float Time = 0;
    int Station = 0;
    float Pan = 0;
    float Zoom = 1;
}

struct FBackstageResult
{
    EBackstageReading Reading = EBackstageReading::Inconclusive;
    int Attention = 1;
    int CompanyTrust = 0;
    int PublicTrust = 0;
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
    const float PreviewSecondsPerFrame = 2;
    const float InitialZoom = 1.45;
    const float ProjectionScale = 0.136;
    const float CameraTap = 0.12;

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

    void AdjustView(FBackstagePhoto&inout View, float PanInput, float ZoomInput)
    {
        View.Pan = Math::Clamp(View.Pan + PanInput * 2.5, -2.0, 2.0);
        View.Zoom = Math::Clamp(View.Zoom + ZoomInput * 1.8, 1.0, 2.8);
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
        const float Angle = Photo.Station == 0 ? 0.0 : (Photo.Station == 1 ? -0.63 : 0.63);
        const float Center = Photo.Station == 0 ? 0.0 : (Photo.Station == 1 ? -0.78 : 0.65);
        return FVector2D(0.5 + (Point.X * Math::Cos(Angle) + Point.Y * Math::Sin(Angle)
            - Center - Photo.Pan) * ProjectionScale * Photo.Zoom, ProjectHeight(Photo, 0.67 + Point.Y * 0.15));
    }

    float ProjectHeight(const FBackstagePhoto&in Photo, float HeightAtWideAngle)
    {
        return 0.65 + (HeightAtWideAngle - 0.65) * Photo.Zoom;
    }

    bool Visible(const FBackstagePhoto&in Photo, FVector2D Point)
    {
        const double X = Project(Photo, Point).X;
        return X >= 0.11 && X <= 0.89;
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
    EBackstagePhase Phase = EBackstagePhase::Briefing;
    FBackstagePhoto Camera;
    TArray<FBackstagePhoto> Photos;
    TArray<int> Slots;
    TArray<FBackstagePhoto> Edits;
    TArray<FBackstagePhoto> Cut;
    FBackstageResult Result;
    EBackstageHeadline Headline = EBackstageHeadline::Neutral;
    int Shutters = Backstage::MaxPhotos;
    int Replays = 1;
    float PreviewTime = 0;
    float ReplayNotice = 0;
    bool bPublishing = false;

    void Start()
    {
        this = FBackstageRound();
        Camera.Zoom = Backstage::InitialZoom;
        Phase = EBackstagePhase::Live;
    }

    void Advance(float Delta)
    {
        if (Phase == EBackstagePhase::Live)
        {
            ReplayNotice = Math::Max(0, ReplayNotice - Delta);
            Camera.Time = Math::Min(Camera.Time + Delta, Backstage::Duration);
            if (Camera.Time >= Backstage::Duration)
                Phase = EBackstagePhase::Edit;
        }
        else if (Phase == EBackstagePhase::Preview)
        {
            PreviewTime += Delta;
            if (PreviewTime >= Backstage::FrameCount * Backstage::PreviewSecondsPerFrame)
                Phase = bPublishing ? EBackstagePhase::Results : EBackstagePhase::Edit;
        }
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
        else if (Slots.Num() < Backstage::FrameCount)
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
        if (Phase != EBackstagePhase::Edit || Slots.Num() != Backstage::FrameCount)
            return;
        Cut.Reset();
        for (int Index : Slots)
            Cut.Add(Edits[Index]);
        bPublishing = bPublish;
        PreviewTime = 0;
        if (bPublish)
            Result = Backstage::Evaluate(Cut, Headline);
        Phase = EBackstagePhase::Preview;
    }
}
