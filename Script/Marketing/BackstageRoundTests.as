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
}
