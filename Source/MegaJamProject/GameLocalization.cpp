#include "GameLocalization.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Dom/JsonObject.h"
#include "Internationalization/Regex.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace GameLocalization
{
namespace
{
    struct FTranslationPattern
    {
        FRegexPattern Regex;
        FString English;
        int32 Args;
        FTranslationPattern(const FString& Pattern, const FString& InEnglish, int32 InArgs)
            : Regex(Pattern), English(InEnglish), Args(InArgs) {}
    };
    bool bInitialized = false;
    bool bEnglish = false;
    TMap<FString,FString> Dictionary;
    TArray<FTranslationPattern> Patterns;
    TArray<FString> Fragments;
    TMap<FString,FString> Cache;
    TSet<FString> Missing;

    bool HasChinese(const FString& Text)
    {
        for (TCHAR C : Text) if (C >= 0x3400 && C <= 0x9fff) return true;
        return false;
    }

    FString Resolve(const FString& Source, int32 Depth);

    void Initialize()
    {
        if (bInitialized) return;
        bInitialized = true;
        FString Language;
        GConfig->GetString(TEXT("GameLocalization"), TEXT("Language"), Language, GGameUserSettingsIni);
        FParse::Value(FCommandLine::Get(), TEXT("GameLanguage="), Language);
        bEnglish = Language.Equals(TEXT("en"), ESearchCase::IgnoreCase);
        FString Json;
        TSharedPtr<FJsonObject> Root;
        if (!FFileHelper::LoadFileToString(Json, *(FPaths::ProjectContentDir()/TEXT("Localization/en.json")))
            || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("GameLocalization: cannot load Content/Localization/en.json"));
            return;
        }
        for (const auto& Value : Root->GetArrayField(TEXT("entries")))
        {
            const auto Entry = Value->AsObject();
            const FString Source = Entry->GetStringField(TEXT("source"));
            const FString English = Entry->GetStringField(TEXT("en"));
            FString Pattern;
            if (Entry->TryGetStringField(TEXT("pattern"), Pattern))
                Patterns.Emplace(Pattern, English, Entry->GetIntegerField(TEXT("args")));
            else
            {
                Dictionary.Add(Source, English);
                Fragments.Add(Source);
            }
        }
        Fragments.Sort([](const FString& A, const FString& B){ return A.Len() > B.Len(); });
        if(FParse::Param(FCommandLine::Get(),TEXT("GameLocalizationAudit")))
        {
            FString Cases;TArray<TSharedPtr<FJsonValue>> Probes;
            if(!FFileHelper::LoadFileToString(Cases,*(FPaths::ProjectSavedDir()/TEXT("Localization/probes.json"))) ||
                !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Cases),Probes) || Probes.IsEmpty())
                UE_LOG(LogTemp,Fatal,TEXT("GameLocalization audit missing probes: run Tools/Validate-GameLocalization.py --probes"));
            int32 Failures=0;
            for(const auto& Probe:Probes)
            {
                const auto Test=Probe->AsObject();
                const FString Actual=Resolve(Test->GetStringField(TEXT("source")),0);
                if(Actual!=Test->GetStringField(TEXT("en")))
                {
                    ++Failures;
                    UE_LOG(LogTemp,Error,TEXT("GameLocalization audit mismatch: %s => %s"),*Test->GetStringField(TEXT("source")),*Actual);
                }
            }
            if(Failures) UE_LOG(LogTemp,Fatal,TEXT("GameLocalization audit failed: %d"),Failures);
            UE_LOG(LogTemp,Display,TEXT("GameLocalization audit PASS: %d runtime translation cases"),Probes.Num());
        }
        UE_LOG(LogTemp, Display, TEXT("GameLocalization: language=%s, %d phrases, %d format patterns"),
            bEnglish ? TEXT("en") : TEXT("zh"), Dictionary.Num(), Patterns.Num());
    }

    FString Resolve(const FString& Source, int32 Depth)
    {
        if (!HasChinese(Source) || Depth > 5) return Source;
        if (const FString* Exact = Dictionary.Find(Source)) return *Exact;
        for (const auto& Pattern : Patterns)
        {
            FRegexMatcher Match(Pattern.Regex, Source);
            if (!Match.FindNext()) continue;
            FString Result = Pattern.English;
            // Substitute backwards so {1} and {10} remain independent.
            for (int32 I = Pattern.Args - 1; I >= 0; --I)
                Result.ReplaceInline(*FString::Printf(TEXT("{%d}"), I), *Resolve(Match.GetCaptureGroup(I+1), Depth+1), ESearchCase::CaseSensitive);
            return Result;
        }
        // Compound labels such as "Objective: " + story text are assembled at runtime.
        // Single pass over the source prevents newly translated text from being translated again.
        FString Result;
        for (int32 Pos=0; Pos<Source.Len();)
        {
            bool bFound=false;
            for (const FString& Fragment : Fragments)
            {
                if (Source.Mid(Pos, Fragment.Len()).Equals(Fragment, ESearchCase::CaseSensitive))
                {
                    Result += Dictionary.FindChecked(Fragment);
                    Pos += Fragment.Len(); bFound=true; break;
                }
            }
            if (!bFound) Result.AppendChar(Source[Pos++]);
        }
        return Result;
    }
}

FString WrapForWidth(const FString& Text, const FSlateFontInfo& Font, float Width)
{
    if (!FSlateApplication::IsInitialized()) return Text;
    const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
    TArray<FString> Words; Text.ParseIntoArrayWS(Words);
    FString Result, Line;
    for (const FString& Word : Words)
    {
        const FString Next=Line.IsEmpty()?Word:Line+TEXT(" ")+Word;
        if (!Line.IsEmpty() && Measure->Measure(Next,Font).X>Width)
        {
            Result+=Line+TEXT("\n"); Line=Word;
        }
        else Line=Next;
    }
    return Result+Line;
}

bool IsEnglish() { Initialize(); return bEnglish; }

void Toggle()
{
    Initialize();
    bEnglish = !bEnglish;
    Cache.Reset();
    GConfig->SetString(TEXT("GameLocalization"), TEXT("Language"), bEnglish ? TEXT("en") : TEXT("zh"), GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
    UE_LOG(LogTemp, Display, TEXT("GameLocalization: switched and saved language=%s"), bEnglish ? TEXT("en") : TEXT("zh"));
}

FString Translate(const FString& Source)
{
    Initialize();
    if (!bEnglish || !HasChinese(Source)) return Source;
    if (const FString* Found = Cache.Find(Source)) return *Found;
    const FString Result = Resolve(Source, 0);
    if (HasChinese(Result) && !Missing.Contains(Source))
    {
        Missing.Add(Source);
        UE_LOG(LogTemp, Warning, TEXT("GameLocalization: untranslated: %s"), *Source);
    }
    if (Cache.Num() >= 4096) Cache.Reset();
    Cache.Add(Source,Result);
    return Result;
}
}
