UCLASS(meta=(AngelscriptTestFlags="CommandletContext;EngineFilter"))
class UBackstageRoundTests : UAngelscriptTestSuite
{
    UFUNCTION(meta=(AngelscriptTest))
    void CameraMovesInBothDirectionsAndReplayKeepsTheShot()
    {
        FBackstageRound Round;
        Round.Start();
        const float InitialZoom = Round.Camera.Zoom;
        const float LeftTap = Backstage::CameraInput(true, false, false, false, 0.001);
        Backstage::AdjustView(Round.Camera, LeftTap, LeftTap);
        AssertTrue(Round.Camera.Pan < 0);
        AssertTrue(Round.Camera.Zoom < InitialZoom);
        const float RightHold = Backstage::CameraInput(false, false, false, true, 0.2);
        Backstage::AdjustView(Round.Camera, RightHold, RightHold);
        AssertTrue(Round.Camera.Pan > 0);
        AssertTrue(Round.Camera.Zoom > InitialZoom);
        Round.Advance(9);
        Round.Capture();
        Round.Replay();
        AssertTrue(Round.Camera.Time == 0 && Round.ReplayNotice > 0);
        AssertEquals(1, Round.Photos.Num());
        AssertEquals(0, Round.Replays);
    }

    UFUNCTION(meta=(AngelscriptTest))
    void ThumbnailAndSlotHitTargetsMatchTheEditor()
    {
        AssertEquals(0, Backstage::PhotoUnderPointer(FVector2D(60, 180)));
        AssertEquals(5, Backstage::PhotoUnderPointer(FVector2D(1055, 290)));
        AssertEquals(-1, Backstage::PhotoUnderPointer(FVector2D(236, 180)));
        AssertEquals(-1, Backstage::PhotoUnderPointer(FVector2D(60, 392)));
        AssertEquals(1, Backstage::SlotUnderPointer(FVector2D(300, 410)));
        AssertEquals(-1, Backstage::SlotUnderPointer(FVector2D(275, 410)));
    }

    FBackstagePhoto Photo(float Time, float Pan = 0, float Zoom = 1)
    {
        FBackstagePhoto Shot;
        Shot.Time = Time;
        Shot.Pan = Pan;
        Shot.Zoom = Zoom;
        return Shot;
    }

    UFUNCTION(meta=(AngelscriptTest))
    void EvidenceAndOrderChangeTheReading()
    {
        TArray<FBackstagePhoto> Photos;
        Photos.Add(Photo(8));
        Photos.Add(Photo(17));
        Photos.Add(Photo(27));
        AssertTrue(Backstage::Evaluate(Photos, EBackstageHeadline::Neutral).Reading == EBackstageReading::Accountable);
        Photos.Swap(0, 1);
        AssertTrue(Backstage::Evaluate(Photos, EBackstageHeadline::Neutral).Reading == EBackstageReading::Inconclusive);
        Photos[2] = Photo(20);
        AssertTrue(Backstage::Evaluate(Photos, EBackstageHeadline::Neutral).Reading == EBackstageReading::Conflict);
    }

    UFUNCTION(meta=(AngelscriptTest))
    void CroppingOutTheVolunteerChangesTheReading()
    {
        TArray<FBackstagePhoto> Photos;
        Photos.Add(Photo(27));
        Photos.Add(Photo(37));
        Photos.Add(Photo(46));
        AssertTrue(Backstage::Evaluate(Photos, EBackstageHeadline::Helpful).Reading == EBackstageReading::Helpful);
        Photos[1] = Photo(37, -1.7, 2.8);
        const auto Result = Backstage::Evaluate(Photos, EBackstageHeadline::Helpful);
        AssertTrue(Result.Reading == EBackstageReading::Inconclusive);
        AssertEquals(-1, Result.PublicTrust);
    }

    UFUNCTION(meta=(AngelscriptTest))
    void ReplaysAndPublishingRespectTheRoundLimits()
    {
        FBackstageRound Round;
        Round.Start();
        Round.Advance(8);
        Round.Capture();
        Round.Advance(9);
        Round.Capture();
        Round.Advance(10);
        Round.Capture();
        Round.Advance(23);
        AssertTrue(Round.Phase == EBackstagePhase::Edit);
        Round.TogglePhoto(0);
        Round.Preview(true);
        AssertTrue(Round.Phase == EBackstagePhase::Edit);

        Round.Replay();
        AssertEquals(0, Round.Replays);
        AssertEquals(3, Round.Shutters);
        for (int Index = 0; Index < 4; ++Index)
            Round.Capture();
        AssertEquals(6, Round.Photos.Num());
        Round.Advance(50);
        Round.Replay();
        AssertTrue(Round.Phase == EBackstagePhase::Edit);
        Round.TogglePhoto(1);
        Round.TogglePhoto(2);
        Round.TogglePhoto(3);
        AssertEquals(3, Round.Slots.Num());
        Round.Preview(false);
        Round.Advance(6);
        AssertTrue(Round.Phase == EBackstagePhase::Edit);
        Round.Preview(true);
        Round.Advance(6);
        AssertTrue(Round.Phase == EBackstagePhase::Results);
        AssertTrue(Round.Result.Reading == EBackstageReading::Accountable);
        Round.Start();
        AssertEquals(0, Round.Photos.Num());
        AssertEquals(6, Round.Shutters);
    }

    UFUNCTION(meta=(AngelscriptTest))
    void PuppetPoseAndStageProjectionMatchTheBeats()
    {
        const auto Walk0 = BackstagePuppet::EvaluatePose(EBackstageRole::Artist, 2.00);
        const auto Walk1 = BackstagePuppet::EvaluatePose(EBackstageRole::Artist, 2.13);
        const auto Walk2 = BackstagePuppet::EvaluatePose(EBackstageRole::Artist, 2.26);
        const auto Walk3 = BackstagePuppet::EvaluatePose(EBackstageRole::Artist, 2.38);
        AssertTrue(Walk0.State == EBackstageAnimState::Walk);
        AssertTrue(Walk0.Expression == EBackstageExpression::Calm);
        AssertEquals(n"Artist_Walk", Walk0.SequenceKey);
        AssertTrue(!Walk0.bCrouching);
        AssertEquals(4, Walk0.FrameCount);
        AssertEquals(0, Walk0.FrameIndex);
        AssertEquals(1, Walk1.FrameIndex);
        AssertEquals(2, Walk2.FrameIndex);
        AssertEquals(3, Walk3.FrameIndex);

        const auto CollArtist = BackstagePuppet::EvaluatePose(EBackstageRole::Artist, 10.0);
        const auto CollCoord = BackstagePuppet::EvaluatePose(EBackstageRole::Coordinator, 10.0);
        AssertTrue(CollArtist.State == EBackstageAnimState::Shock);
        AssertTrue(CollArtist.Expression == EBackstageExpression::Shock);
        AssertEquals(n"Artist_Shock", CollArtist.SequenceKey);
        AssertEquals(n"Emotion_Shock", CollArtist.EmotionBadge);
        AssertTrue(CollCoord.State == EBackstageAnimState::Shock);
        AssertTrue(CollCoord.Expression == EBackstageExpression::Focus);
        AssertEquals(n"Coordinator_Shock", CollCoord.SequenceKey);

        const auto ArgArtist = BackstagePuppet::EvaluatePose(EBackstageRole::Artist, 18.0);
        const auto ArgVol = BackstagePuppet::EvaluatePose(EBackstageRole::Volunteer, 18.0);
        const auto ArgCoord = BackstagePuppet::EvaluatePose(EBackstageRole::Coordinator, 18.0);
        AssertTrue(ArgArtist.State == EBackstageAnimState::Argue);
        AssertTrue(ArgArtist.Expression == EBackstageExpression::Angry);
        AssertEquals(n"Artist_Argue", ArgArtist.SequenceKey);
        AssertEquals(n"Emotion_Anger", ArgArtist.EmotionBadge);
        AssertTrue(ArgArtist.FacingSign > 0.0);
        AssertTrue(ArgVol.State == EBackstageAnimState::Shock);
        AssertEquals(n"Volunteer_Shock", ArgVol.SequenceKey);
        AssertEquals(n"Emotion_Shock", ArgVol.EmotionBadge);
        AssertTrue(ArgVol.FacingSign < 0.0);
        AssertTrue(ArgCoord.State == EBackstageAnimState::Argue);
        AssertTrue(ArgCoord.Expression == EBackstageExpression::Focus);

        const auto PickArtist = BackstagePuppet::EvaluatePose(EBackstageRole::Artist, 28.0);
        const auto PickVol = BackstagePuppet::EvaluatePose(EBackstageRole::Volunteer, 28.0);
        AssertTrue(PickArtist.State == EBackstageAnimState::Pickup);
        AssertTrue(PickArtist.Expression == EBackstageExpression::Focus);
        AssertTrue(PickArtist.bCrouching);
        AssertEquals(n"Artist_Pickup", PickArtist.SequenceKey);
        AssertEquals(n"Emotion_Care", PickArtist.EmotionBadge);
        AssertTrue(PickVol.State == EBackstageAnimState::Care);
        AssertTrue(PickVol.Expression == EBackstageExpression::Calm);
        AssertTrue(!PickVol.bCrouching);
        AssertEquals(n"Volunteer_Care", PickVol.SequenceKey);

        const auto HandoffArtist = BackstagePuppet::EvaluatePose(EBackstageRole::Artist, 38.0);
        const auto HandoffVol = BackstagePuppet::EvaluatePose(EBackstageRole::Volunteer, 38.0);
        const auto HandoffCoord = BackstagePuppet::EvaluatePose(EBackstageRole::Coordinator, 38.0);
        AssertTrue(HandoffArtist.State == EBackstageAnimState::Handoff);
        AssertTrue(HandoffVol.State == EBackstageAnimState::Handoff);
        AssertTrue(HandoffCoord.State == EBackstageAnimState::Warm);
        AssertEquals(n"Artist_Handoff", HandoffArtist.SequenceKey);
        AssertEquals(n"Volunteer_Handoff", HandoffVol.SequenceKey);
        AssertEquals(n"Coordinator_Warm", HandoffCoord.SequenceKey);
        AssertEquals(n"Emotion_Warm", HandoffArtist.EmotionBadge);

        const auto TogetherArtist = BackstagePuppet::EvaluatePose(EBackstageRole::Artist, 46.0);
        const auto TogetherCoord = BackstagePuppet::EvaluatePose(EBackstageRole::Coordinator, 46.0);
        AssertTrue(TogetherArtist.State == EBackstageAnimState::Warm);
        AssertTrue(TogetherArtist.Expression == EBackstageExpression::Warm);
        AssertEquals(n"Emotion_Warm", TogetherArtist.EmotionBadge);
        AssertTrue(TogetherCoord.State == EBackstageAnimState::Warm);
        AssertEquals(NAME_None, TogetherCoord.EmotionBadge);
    }

    UFUNCTION(meta=(AngelscriptTest))
    void BraindanceTimelineAndSmoothCameraWithElasticityWork()
    {
        FBackstageRound Round;
        Round.Start();
        Round.EnableBraindance();

        // 1. Smooth curve damping on Pan, Tilt and Zoom
        Round.PushCamera(1.0, 0.5, 0.5);
        const float TargetPan = Round.TargetCamera.Pan;
        const float TargetTilt = Round.TargetCamera.Tilt;
        Round.Advance(0.04);
        AssertTrue(Round.Camera.Pan > 0.05 && Round.Camera.Pan < TargetPan);
        AssertTrue(Round.Camera.Tilt > 0.02 && Round.Camera.Tilt < TargetTilt);
        AssertTrue(Round.Camera.Zoom > Backstage::InitialZoom && Round.Camera.Zoom < Round.TargetCamera.Zoom);

        for (int Step = 0; Step < 25; ++Step)
            Round.Advance(0.05);
        AssertTrue(Math::Abs(Round.Camera.Pan - TargetPan) < 0.02);
        AssertTrue(Math::Abs(Round.Camera.Tilt - TargetTilt) < 0.02);

        // Subtle Limbus-style vertical tilt shifts viewfinder projection height slightly
        FBackstagePhoto Tilted = Round.Camera;
        FBackstagePhoto Flat = Round.Camera;
        Tilted.Tilt = 0.8;
        Flat.Tilt = 0.0;
        AssertTrue(Backstage::ProjectHeight(Tilted, 0.67) > Backstage::ProjectHeight(Flat, 0.67));

        // 2. Edge rubber-band overshoot & elastic rebound on Pan, Tilt, Zoom
        Round.PushCamera(6.0, 6.0, 6.0);
        Round.Advance(0.25);
        AssertTrue(Round.Camera.Pan > Backstage::PanLimit);
        AssertTrue(Round.Camera.Tilt > Backstage::TiltLimit);
        AssertTrue(Round.Camera.Zoom > Backstage::ZoomMax);

        for (int Step = 0; Step < 35; ++Step)
            Round.Advance(0.05);
        AssertTrue(Round.Camera.Pan <= Backstage::PanLimit + 0.02);
        AssertTrue(Round.Camera.Tilt <= Backstage::TiltLimit + 0.02);
        AssertTrue(Round.Camera.Zoom <= Backstage::ZoomMax + 0.02);

        // 3. Smooth Station 1 -> 2 arc transition
        Round.SetTargetStation(1);
        Round.Advance(0.06);
        AssertTrue(Round.Camera.SmoothAngle < -0.05 && Round.Camera.SmoothAngle > -0.58);
        for (int Step = 0; Step < 30; ++Step)
            Round.Advance(0.05);
        AssertTrue(Math::Abs(Round.Camera.SmoothAngle - Backstage::StationAngle(1)) < 0.02);

        // 4. Braindance timeline: play, rewind, pause, 50s hold, and Edit roundtrip
        for (int Step = 0; Step < 10; ++Step)
            Round.Advance(0.2);
        const float PeakTime = Round.Camera.Time;
        AssertTrue(PeakTime > 1.0);

        Round.SetScrubInput(-1.0);
        for (int Step = 0; Step < 8; ++Step)
            Round.Advance(0.05);
        const float RewoundTime = Round.Camera.Time;
        AssertTrue(RewoundTime < PeakTime);

        Round.SetScrubInput(0.0);
        Round.TogglePause();
        for (int Step = 0; Step < 15; ++Step)
            Round.Advance(0.05);
        const float HeldTime = Round.Camera.Time;
        Round.Advance(1.0);
        AssertTrue(Math::Abs(Round.Camera.Time - HeldTime) < 0.001);

        Round.TogglePause();
        for (int Step = 0; Step < 60; ++Step)
            Round.Advance(1.0);
        AssertTrue(Round.Phase == EBackstagePhase::Live);
        AssertTrue(Round.Camera.Time >= Backstage::Duration - 0.01);
        AssertTrue(Round.bPaused);

        // Scrubbing backward from the 50s end works cleanly
        Round.SetScrubInput(-1.0);
        for (int Step = 0; Step < 10; ++Step)
            Round.Advance(0.1);
        Round.SetScrubInput(0.0);
        AssertTrue(Round.Camera.Time < Backstage::Duration - 1.0);

        Round.Capture();
        Round.Capture();
        Round.Capture();
        Round.Phase = EBackstagePhase::Edit;
        const int PrevReplays = Round.Replays;
        Round.ReturnToBraindance();
        AssertTrue(Round.Phase == EBackstagePhase::Live);
        AssertEquals(3, Round.Photos.Num());
        AssertEquals(PrevReplays, Round.Replays);
    }

    UFUNCTION(meta=(AngelscriptTest))
    void LevelOneRequiresOnePhotoConfirmsAndEvaluatesNewspaper()
    {
        FBackstageRound Round;
        Round.StartLevel(0);
        AssertEquals(1, Round.RequiredFrameCount());
        AssertTrue(Math::Abs(Round.ActiveDuration() - 36.0) < 0.01);
        AssertFalse(Round.Config.bUsesEditBoard);

        Round.Advance(26.0);
        Round.Camera.Pan = 0.85;
        Round.Camera.Zoom = 1.65;
        Round.CapturePending();
        AssertTrue(Round.Phase == EBackstagePhase::Confirm);
        AssertTrue(Round.bHasPending);

        Round.RejectPending();
        AssertTrue(Round.Phase == EBackstagePhase::Live);
        AssertEquals(0, Round.Photos.Num());

        Round.CapturePending();
        AssertTrue(Round.AcceptPending());
        AssertTrue(Round.Phase == EBackstagePhase::Results);
        AssertEquals(1, Round.Cut.Num());
        AssertTrue(Round.Result.bTimeMatched);
        AssertTrue(Round.Result.bSubjectsMatched);
        AssertTrue(Round.Result.bPassed);
        AssertEquals("《雨中暖心举动!林晚把唯一的伞送给粉丝》", Round.Result.NewspaperHeadline);
    }

    UFUNCTION(meta=(AngelscriptTest))
    void LevelOneRejectsWrongTimeAndAssistantLeakInFrame()
    {
        const FBackstageLevelConfig Cfg = Backstage::GetLevelConfig(0);
        TArray<FBackstagePhoto> Seq;

        // Wrong time window (rain just started, XiaoBei still next to LinWan)
        Seq.Add(Photo(10.0, 0.85, 1.65));
        const FBackstageResult WrongTime = Backstage::EvaluateLevel(Cfg, Seq, EBackstageHeadline::Helpful);
        AssertFalse(WrongTime.bTimeMatched);
        AssertFalse(WrongTime.bPassed);

        // Right time window (26s) but wide shot leaks XiaoBei on the left
        Seq[0] = Photo(26.0, 0.0, 0.8);
        const FBackstageResult LeakedAssistant = Backstage::EvaluateLevel(Cfg, Seq, EBackstageHeadline::Helpful);
        AssertTrue(LeakedAssistant.bTimeMatched);
        AssertFalse(LeakedAssistant.bSubjectsMatched);
        AssertFalse(LeakedAssistant.bPassed);

        // Right time window (26s) but panned left misses Fan & FanSign
        Seq[0] = Photo(26.0, -1.2, 1.8);
        const FBackstageResult MissingFan = Backstage::EvaluateLevel(Cfg, Seq, EBackstageHeadline::Helpful);
        AssertTrue(MissingFan.bTimeMatched);
        AssertFalse(MissingFan.bSubjectsMatched);
        AssertFalse(MissingFan.bPassed);
    }
}
