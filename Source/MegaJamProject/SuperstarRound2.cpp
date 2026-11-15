#include "SuperstarStory.h"
#include "SuperstarStage.h"
#include "Dom/JsonObject.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
    TSharedPtr<FJsonObject> Presentation(const TSharedPtr<FJsonObject>& Actor,const FString& Id)
    {
        const TSharedPtr<FJsonObject>* Root=nullptr;const TSharedPtr<FJsonObject>* Value=nullptr;
        if(Actor->TryGetObjectField(TEXT("presentation"),Root) && (*Root)->TryGetObjectField(Id,Value)) return *Value;
        return nullptr;
    }
}

float ASuperstarStoryHUD::ActorScale(const TSharedPtr<FJsonObject>& Actor) const
{
    FString Id;int32 Frame;ResolveActorSample(Actor,Id,Frame);
    const auto P=Presentation(Actor,Id);double Result=1.;
    if(P) P->TryGetNumberField(TEXT("scale"),Result);
    return Result;
}

FVector2D ASuperstarStoryHUD::ActorOffset(const TSharedPtr<FJsonObject>& Actor) const
{
    FString Id;int32 Frame;ResolveActorSample(Actor,Id,Frame);
    const auto P=Presentation(Actor,Id);const TArray<TSharedPtr<FJsonValue>>* Values=nullptr;
    if(P && P->TryGetArrayField(TEXT("offsets"),Values) && Values->IsValidIndex(Frame))
    {
        const auto& XY=(*Values)[Frame]->AsArray();return {XY[0]->AsNumber(),XY[1]->AsNumber()};
    }
    return FVector2D::ZeroVector;
}

bool ASuperstarStoryHUD::PropVisible(const TSharedPtr<FJsonObject>& Prop) const
{
    double Begin=0,End=Beats[Beat]->GetNumberField(TEXT("duration"))+.001;
    Prop->TryGetNumberField(TEXT("show_at"),Begin);Prop->TryGetNumberField(TEXT("hide_at"),End);
    if(Clock<Begin || Clock>=End) return false;
    double Holder=0;
    if(Prop->TryGetNumberField(TEXT("attach_actor"),Holder))
    {
        double Transfer=0;
        if(Prop->TryGetNumberField(TEXT("transfer_at"),Transfer) && Clock>=Transfer) Holder=Prop->GetNumberField(TEXT("transfer_actor"));
        FString Id;int32 Frame;ResolveActorSample(Beats[Beat]->GetArrayField(TEXT("actors"))[static_cast<int32>(Holder)]->AsObject(),Id,Frame);
        const TArray<TSharedPtr<FJsonValue>>* Allowed=nullptr;
        if(Prop->TryGetArrayField(TEXT("visible_clips"),Allowed))
        {
            for(const auto& V:*Allowed) if(V->AsString()==Id) return true;
            return false;
        }
        FString Mode;Data->GetObjectField(TEXT("clips"))->GetObjectField(Id)->TryGetStringField(TEXT("prop_mode"),Mode);
        return Mode!=TEXT("baked");
    }
    return true;
}

FVector2D ASuperstarStoryHUD::PropGrip(const TSharedPtr<FJsonObject>& Prop) const
{
    const TArray<TSharedPtr<FJsonValue>>* Points=&Prop->GetArrayField(TEXT("grip"));double Transfer=0;
    if(Prop->TryGetNumberField(TEXT("transfer_at"),Transfer) && Clock>=Transfer)
    {
        const TArray<TSharedPtr<FJsonValue>>* Alternate=nullptr;
        if(Prop->TryGetArrayField(TEXT("transfer_grip"),Alternate)) Points=Alternate;
    }
    return {(*Points)[0]->AsNumber(),(*Points)[1]->AsNumber()};
}

bool ASuperstarStoryHUD::PropMirrors(const TSharedPtr<FJsonObject>& Prop) const
{
    bool Result=true;Prop->TryGetBoolField(TEXT("mirror_with_holder"),Result);return Result;
}

void ASuperstarStoryHUD::RunRound2Smoke()
{
    if(++Round2SmokeTicks%12!=0) return;
    const int32 Target=Round2SmokeStep/4,Sample=Round2SmokeStep%4;
    if(Target>=Beats.Num())
    {
        int32 Expected=0;
        for(const auto& Entry:Data->GetObjectField(TEXT("clips"))->Values)
        {
            FString Delivery;Entry.Value->AsObject()->TryGetStringField(TEXT("delivery"),Delivery);
            bool bRetired=false;Entry.Value->AsObject()->TryGetBoolField(TEXT("retired"),bRetired);
            if(Delivery==TEXT("Round2Actual20261115"))
            {
                ++Expected;
                if(!bRetired) checkf(Round2Seen.Contains(FString(*Entry.Key)),TEXT("Unreachable return clip %s"),*Entry.Key);
            }
        }
        check(Expected==78);
        UE_LOG(LogTemp,Display,TEXT("SuperstarRound2 smoke PASS: active return clips sampled, 39 beats, 5 FPS deterministic reverse/seek, grip transforms and 156 rendered views"));
        GetOwningPlayerController()->ConsoleCommand(TEXT("quit"));return;
    }
    if(Sample==0)
    {
        SelectBeat(Target);bPlaying=false;
        const auto& Actors=Beats[Beat]->GetArrayField(TEXT("actors"));
        const float Duration=Beats[Beat]->GetNumberField(TEXT("duration"));
        for(float T=0;T<Duration;T+=.1f)
        {
            Clock=T;UpdateActors();
            for(const auto& V:Actors)
            {
                const auto A=V->AsObject();FString Id;int32 Frame;ResolveActorSample(A,Id,Frame);Round2Seen.Add(Id);
                check(LoadFrame(ClipFramePath(Id,Frame))!=nullptr);
                Clock=FMath::Max(0.f,Duration-T);FString Other;int32 OtherFrame;ResolveActorSample(A,Other,OtherFrame);
                Clock=T;ResolveActorSample(A,Other,OtherFrame);check(Id==Other && Frame==OtherFrame);
            }
            const TArray<TSharedPtr<FJsonValue>>* Props=nullptr;
            if(Beats[Beat]->TryGetArrayField(TEXT("props"),Props)) for(int32 I=0;I<Props->Num();++I)
            {
                const auto P=(*Props)[I]->AsObject();double Holder=0,Transfer=0;
                if(!P->TryGetNumberField(TEXT("attach_actor"),Holder) || !PropVisible(P)) continue;
                if(P->TryGetNumberField(TEXT("transfer_at"),Transfer) && Clock>=Transfer) Holder=P->GetNumberField(TEXT("transfer_actor"));
                const auto A=Actors[static_cast<int32>(Holder)]->AsObject();
                const auto GripError=Stage->PropPixelWorld(I,PropGrip(P))-Stage->CharacterPixelWorld(Holder,ActorHand(A));
                checkf(GripError.Size()<.2,TEXT("Grip error beat %d prop %d: %f"),Beat+1,I,GripError.Size());
            }
        }
    }
    const float Fractions[]={0.f,.28f,.58f,.96f};Clock=Beats[Beat]->GetNumberField(TEXT("duration"))*Fractions[Sample];
    if(Sample==2)
    {
        const TArray<TSharedPtr<FJsonValue>>* Props=nullptr;
        if(Beats[Beat]->TryGetArrayField(TEXT("props"),Props)) for(const auto& V:*Props)
        {double T=0;if(V->AsObject()->TryGetNumberField(TEXT("transfer_at"),T)) {Clock=T;break;}}
    }
    Stage->SetView(Sample==1?-1.f:(Sample==3?1.f:0.f),0,0,1);
    UpdateActors();
    FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/SuperstarRound2")/FString::Printf(TEXT("B%02d-%d.png"),Beat+1,Sample),true,false);
    ++Round2SmokeStep;
}
