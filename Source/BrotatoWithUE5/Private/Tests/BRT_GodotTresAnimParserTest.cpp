// Fill out your copyright notice in the Description page of Project Settings.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#include "Animation/BRT_GodotTresAnimParser.h"
#include "Animation/BRT_PaperDollAnimData.h"

namespace BRT_GodotTresAnimParserTest
{
	// 真实 Godot 示例动画文件（存在时执行 1:1 数值断言，不存在则跳过）
	const TCHAR* MoveTresPath = TEXT("h:/Engine/Godot/Projects/BrotatoProject-main/entities/units/player/player_move.tres");
	const TCHAR* IdleTresPath = TEXT("h:/Engine/Godot/Projects/BrotatoProject-main/entities/units/player/player_idle.tres");

	FBRT_GodotAnimImportParams MakeParams()
	{
		FBRT_GodotAnimImportParams Params;
		Params.PixelsPerUnrealUnit = 1.f; // 与已导入腿部精灵 PixelsPerUnrealUnit 一致（实测 1）
		Params.RotationSign = -1.f;
		Params.OriginOffset = FVector::ZeroVector;
		return Params;
	}

	const FBRT_PaperDollTrack* FindTrack(const TArray<FBRT_PaperDollTrack>& Tracks, FName Target, EBRT_AnimChannel Channel)
	{
		for (const FBRT_PaperDollTrack& Track : Tracks)
		{
			if (Track.TargetName == Target && Track.Channel == Channel)
			{
				return &Track;
			}
		}
		return nullptr;
	}
}

BEGIN_DEFINE_SPEC(FBRT_GodotTresAnimParserSpec, "Brotato.GodotTresAnimParser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FBRT_GodotTresAnimParserSpec)

void FBRT_GodotTresAnimParserSpec::Define()
{
	using namespace BRT_GodotTresAnimParserTest;

	Describe("Ease", [this]()
	{
		It("should match Godot Math::ease for all branches", [this]()
		{
			TestTrue(TEXT("ease(0.5,1)=0.5"), FMath::IsNearlyEqual(FBRT_PaperDollTrack::Ease(0.5f, 1.f), 0.5f, 1.e-4f));
			TestTrue(TEXT("ease(0.5,2)=0.25"), FMath::IsNearlyEqual(FBRT_PaperDollTrack::Ease(0.5f, 2.f), 0.25f, 1.e-4f));
			TestTrue(TEXT("ease(0.5,0.5)=0.75"), FMath::IsNearlyEqual(FBRT_PaperDollTrack::Ease(0.5f, 0.5f), 0.75f, 1.e-4f));
			TestTrue(TEXT("ease clamps x<0"), FMath::IsNearlyEqual(FBRT_PaperDollTrack::Ease(-1.f, 2.f), 0.f, 1.e-4f));
			TestTrue(TEXT("ease clamps x>1"), FMath::IsNearlyEqual(FBRT_PaperDollTrack::Ease(2.f, 2.f), 1.f, 1.e-4f));
			TestTrue(TEXT("ease(0.5,-1) inout"), FMath::IsNearlyEqual(FBRT_PaperDollTrack::Ease(0.5f, -1.f), 0.5f, 1.e-4f));
			TestTrue(TEXT("ease(c=0)=0"), FMath::IsNearlyEqual(FBRT_PaperDollTrack::Ease(0.5f, 0.f), 0.f, 1.e-4f));
		});
	});

	Describe("EvaluateAtTime", [this]()
	{
		It("should apply left-key transition via ease on linear tracks", [this]()
		{
			FBRT_PaperDollTrack Track;
			Track.Interp = EBRT_CurveInterp::LinearEase;
			FBRT_ScalarKey Key0;
			Key0.Time = 0.f;
			Key0.Value = 0.f;
			Key0.Transition = 2.f;
			FBRT_ScalarKey Key1;
			Key1.Time = 1.f;
			Key1.Value = 10.f;
			Track.Keys = { Key0, Key1 };

			TestTrue(TEXT("mid eased to 2.5"), FMath::IsNearlyEqual(Track.EvaluateAtTime(0.5f, 1.f, false), 2.5f, 1.e-4f));
		});

		It("should hold left key value on discrete tracks", [this]()
		{
			FBRT_PaperDollTrack Track;
			Track.Update = EBRT_TrackUpdate::Discrete;
			FBRT_ScalarKey Key0;
			Key0.Time = 0.f;
			Key0.Value = 3.f;
			FBRT_ScalarKey Key1;
			Key1.Time = 1.f;
			Key1.Value = 9.f;
			Track.Keys = { Key0, Key1 };

			TestTrue(TEXT("held at 3"), FMath::IsNearlyEqual(Track.EvaluateAtTime(0.75f, 1.f, false), 3.f, 1.e-4f));
		});

		It("should wrap across loop point like Godot loop branch", [this]()
		{
			FBRT_PaperDollTrack Track;
			FBRT_ScalarKey Key0;
			Key0.Time = 0.f;
			Key0.Value = 0.f;
			FBRT_ScalarKey Key1;
			Key1.Time = 0.3f;
			Key1.Value = 10.f;
			Track.Keys = { Key0, Key1 };

			// 段 [0.3 -> 0] 跨环绕：Delta=(0.6-0.3)+0=0.3，t=0.45 -> c=0.5 -> lerp(10,0)=5
			TestTrue(TEXT("wrap midpoint 5"), FMath::IsNearlyEqual(Track.EvaluateAtTime(0.45f, 0.6f, true), 5.f, 1.e-4f));
		});

		It("should evaluate bezier midpoint exactly on symmetric handles", [this]()
		{
			FBRT_PaperDollTrack Track;
			Track.Interp = EBRT_CurveInterp::Bezier;
			FBRT_ScalarKey Key0;
			Key0.Time = 0.f;
			Key0.Value = 24.f;
			Key0.OutHandle = FVector2D(0.25f, 0.f);
			FBRT_ScalarKey Key1;
			Key1.Time = 0.5f;
			Key1.Value = 19.f;
			Key1.InHandle = FVector2D(-0.25f, 0.f);
			Key1.OutHandle = FVector2D(0.25f, 0.f);
			FBRT_ScalarKey Key2;
			Key2.Time = 1.f;
			Key2.Value = 24.f;
			Key2.InHandle = FVector2D(-0.25f, 0.f);
			Track.Keys = { Key0, Key1, Key2 };

			TestTrue(TEXT("key0 exact"), FMath::IsNearlyEqual(Track.EvaluateAtTime(0.f, 1.f, true), 24.f, 1.e-3f));
			TestTrue(TEXT("mid key exact"), FMath::IsNearlyEqual(Track.EvaluateAtTime(0.5f, 1.f, true), 19.f, 1.e-3f));
			// 对称控制柄下 u=0.5 处 x=0.25、y=21.5
			TestTrue(TEXT("bezier quarter 21.5"), FMath::IsNearlyEqual(Track.EvaluateAtTime(0.25f, 1.f, true), 21.5f, 1.e-2f));
			TestTrue(TEXT("monotonic down"), Track.EvaluateAtTime(0.125f, 1.f, true) > Track.EvaluateAtTime(0.375f, 1.f, true));
		});
	});

	Describe("ParseTresText", [this]()
	{
		It("should trim out-of-range keys and skip method/unknown tracks", [this]()
		{
			const FString Sample =
				TEXT("[gd_resource type=\"Animation\" format=2]\n")
				TEXT("\n")
				TEXT("[resource]\n")
				TEXT("resource_name = \"sample\"\n")
				TEXT("length = 0.6\n")
				TEXT("loop = true\n")
				TEXT("tracks/0/type = \"value\"\n")
				TEXT("tracks/0/path = NodePath(\"Animation:position\")\n")
				TEXT("tracks/0/interp = 1\n")
				TEXT("tracks/0/loop_wrap = true\n")
				TEXT("tracks/0/imported = false\n")
				TEXT("tracks/0/enabled = true\n")
				TEXT("tracks/0/keys = {\n")
				TEXT("\"times\": PoolRealArray( 0, 0.3, 1.4 ),\n")
				TEXT("\"transitions\": PoolRealArray( 1, 1, 1 ),\n")
				TEXT("\"update\": 0,\n")
				TEXT("\"values\": [ Vector2( 0, -24 ), Vector2( 0, -14 ), Vector2( 0, -29 ) ]\n")
				TEXT("}\n")
				TEXT("tracks/1/type = \"method\"\n")
				TEXT("tracks/1/path = NodePath(\".\")\n")
				TEXT("tracks/1/interp = 1\n")
				TEXT("tracks/1/loop_wrap = true\n")
				TEXT("tracks/1/imported = false\n")
				TEXT("tracks/1/enabled = true\n")
				TEXT("tracks/1/keys = {\n")
				TEXT("\"times\": PoolRealArray( 0.1 ),\n")
				TEXT("\"transitions\": PoolRealArray( 1 ),\n")
				TEXT("\"values\": [ {\n")
				TEXT("\"args\": [  ],\n")
				TEXT("\"method\": \"play_step_sound\"\n")
				TEXT("} ]\n")
				TEXT("}\n")
				TEXT("tracks/2/type = \"value\"\n")
				TEXT("tracks/2/path = NodePath(\"Animation/Shadow:modulate\")\n")
				TEXT("tracks/2/interp = 1\n")
				TEXT("tracks/2/loop_wrap = true\n")
				TEXT("tracks/2/imported = false\n")
				TEXT("tracks/2/enabled = true\n")
				TEXT("tracks/2/keys = {\n")
				TEXT("\"times\": PoolRealArray( 0 ),\n")
				TEXT("\"transitions\": PoolRealArray( 1 ),\n")
				TEXT("\"update\": 0,\n")
				TEXT("\"values\": [ Color( 1, 1, 1, 1 ) ]\n")
				TEXT("}\n");

			FBRT_GodotAnimParseResult Result;
			const bool bOk = FBRT_GodotTresAnimParser::ParseTresText(Sample, MakeParams(), Result);
			TestTrue(TEXT("parse ok"), bOk);
			TestTrue(TEXT("anim name"), Result.AnimName == FName(TEXT("sample")));
			TestTrue(TEXT("length 0.6"), FMath::IsNearlyEqual(Result.Length, 0.6f, 1.e-4f));
			TestTrue(TEXT("loop"), Result.bLoop);
			TestTrue(TEXT("2 scalar tracks"), Result.Tracks.Num() == 2);

			const FBRT_PaperDollTrack* LocX = FindTrack(Result.Tracks, TEXT("Root"), EBRT_AnimChannel::LocationX);
			const FBRT_PaperDollTrack* LocZ = FindTrack(Result.Tracks, TEXT("Root"), EBRT_AnimChannel::LocationZ);
			TestTrue(TEXT("has LocationX"), LocX != nullptr);
			TestTrue(TEXT("has LocationZ"), LocZ != nullptr);
			if (LocX && LocZ)
			{
				TestTrue(TEXT("trimmed to 2 keys"), LocX->Keys.Num() == 2 && LocZ->Keys.Num() == 2);
				TestTrue(TEXT("x scaled"), FMath::IsNearlyEqual(LocX->Keys[1].Value, 0.f, 1.e-4f));
				TestTrue(TEXT("y negated"), FMath::IsNearlyEqual(LocZ->Keys[0].Value, 24.f, 1.e-4f));
				TestTrue(TEXT("y negated key1"), FMath::IsNearlyEqual(LocZ->Keys[1].Value, 14.f, 1.e-4f));
			}

			// 警告包含 method 与裁剪计数
			bool bHasMethodWarn = false;
			bool bHasTrimWarn = false;
			for (const FString& Warning : Result.Warnings)
			{
				bHasMethodWarn |= Warning.Contains(TEXT("method"));
				bHasTrimWarn |= Warning.Contains(TEXT("裁剪"));
			}
			TestTrue(TEXT("method warning"), bHasMethodWarn);
			TestTrue(TEXT("trim warning"), bHasTrimWarn);
		});
	});

	Describe("Real Godot files", [this]()
	{
		It("should parse player_move.tres 1:1", [this]()
		{
			if (!FPaths::FileExists(MoveTresPath))
			{
				AddInfo(TEXT("player_move.tres 不存在，跳过"));
				return;
			}

			FBRT_GodotAnimParseResult Result;
			const bool bOk = FBRT_GodotTresAnimParser::ParseTresFile(MoveTresPath, MakeParams(), Result);
			TestTrue(TEXT("parse ok"), bOk);
			TestTrue(TEXT("name=move"), Result.AnimName == FName(TEXT("move")));
			TestTrue(TEXT("length=0.6"), FMath::IsNearlyEqual(Result.Length, 0.6f, 1.e-4f));
			TestTrue(TEXT("loop=true"), Result.bLoop);
			// 8 条 value 轨道拆为 14 条标量轨道（position/scale 各 2 通道，rotation 1 通道）
			TestTrue(TEXT("14 scalar tracks"), Result.Tracks.Num() == 14);

			const FBRT_PaperDollTrack* RootZ = FindTrack(Result.Tracks, TEXT("Root"), EBRT_AnimChannel::LocationZ);
			TestTrue(TEXT("root Z track"), RootZ != nullptr);
			if (RootZ)
			{
				// times 1.4/2.2/2.6 超出 length=0.6 被裁剪：10 -> 7
				TestTrue(TEXT("7 keys after trim"), RootZ->Keys.Num() == 7);
				TestTrue(TEXT("key0 = 24"), FMath::IsNearlyEqual(RootZ->Keys[0].Value, 24.f, 1.e-4f));
				TestTrue(TEXT("key1 = 14"), FMath::IsNearlyEqual(RootZ->Keys[1].Value, 14.f, 1.e-4f));
				TestTrue(TEXT("key2 = 29"), FMath::IsNearlyEqual(RootZ->Keys[2].Value, 29.f, 1.e-4f));
				TestTrue(TEXT("transition@0.1=0.5"), FMath::IsNearlyEqual(RootZ->Keys[1].Transition, 0.5f, 1.e-4f));
				TestTrue(TEXT("transition@0.5=2"), FMath::IsNearlyEqual(RootZ->Keys[5].Transition, 2.f, 1.e-4f));
			}

			const FBRT_PaperDollTrack* LegRRot = FindTrack(Result.Tracks, TEXT("LegR"), EBRT_AnimChannel::RotationPitch);
			TestTrue(TEXT("LegR rot track"), LegRRot != nullptr);
			if (LegRRot)
			{
				// rotation_degrees 61.3 -> Pitch = -61.3
				TestTrue(TEXT("Pitch = -61.3"), FMath::IsNearlyEqual(LegRRot->Keys[0].Value, -61.3f, 1.e-3f));
			}

			const FBRT_PaperDollTrack* LegRScaleX = FindTrack(Result.Tracks, TEXT("LegR"), EBRT_AnimChannel::ScaleX);
			TestTrue(TEXT("LegR scale track"), LegRScaleX != nullptr);
			if (LegRScaleX)
			{
				bool bAllMinusOne = true;
				for (const FBRT_ScalarKey& Key : LegRScaleX->Keys)
				{
					bAllMinusOne &= FMath::IsNearlyEqual(Key.Value, -1.f, 1.e-4f);
				}
				TestTrue(TEXT("LegR mirrored -1"), bAllMinusOne);
			}
		});

		It("should parse player_idle.tres 1:1", [this]()
		{
			if (!FPaths::FileExists(IdleTresPath))
			{
				AddInfo(TEXT("player_idle.tres 不存在，跳过"));
				return;
			}

			FBRT_GodotAnimParseResult Result;
			const bool bOk = FBRT_GodotTresAnimParser::ParseTresFile(IdleTresPath, MakeParams(), Result);
			TestTrue(TEXT("parse ok"), bOk);
			TestTrue(TEXT("name=idle"), Result.AnimName == FName(TEXT("idle")));
			TestTrue(TEXT("length default 1"), FMath::IsNearlyEqual(Result.Length, 1.f, 1.e-4f));
			TestTrue(TEXT("loop=true"), Result.bLoop);
			// 4 bezier + 6 value -> 4 + 10 = 14 条标量轨道
			TestTrue(TEXT("14 scalar tracks"), Result.Tracks.Num() == 14);

			const FBRT_PaperDollTrack* RootZ = FindTrack(Result.Tracks, TEXT("Root"), EBRT_AnimChannel::LocationZ);
			TestTrue(TEXT("root Z track"), RootZ != nullptr);
			if (RootZ)
			{
				TestTrue(TEXT("bezier interp"), RootZ->Interp == EBRT_CurveInterp::Bezier);
				TestTrue(TEXT("3 keys"), RootZ->Keys.Num() == 3);
				TestTrue(TEXT("values 24/19/24"),
					FMath::IsNearlyEqual(RootZ->Keys[0].Value, 24.f, 1.e-4f) &&
					FMath::IsNearlyEqual(RootZ->Keys[1].Value, 19.f, 1.e-4f) &&
					FMath::IsNearlyEqual(RootZ->Keys[2].Value, 24.f, 1.e-4f));
				// 对称控制柄：t=0.25 处应为 21.5
				TestTrue(TEXT("bezier quarter 21.5"), FMath::IsNearlyEqual(RootZ->EvaluateAtTime(0.25f, 1.f, true), 21.5f, 1.e-2f));
			}

			const FBRT_PaperDollTrack* LegLX = FindTrack(Result.Tracks, TEXT("LegL"), EBRT_AnimChannel::LocationX);
			const FBRT_PaperDollTrack* LegRX = FindTrack(Result.Tracks, TEXT("LegR"), EBRT_AnimChannel::LocationX);
			TestTrue(TEXT("leg tracks"), LegLX != nullptr && LegRX != nullptr);
			if (LegLX && LegRX)
			{
				TestTrue(TEXT("LegL x=15"), FMath::IsNearlyEqual(LegLX->Keys[0].Value, 15.f, 1.e-4f));
				TestTrue(TEXT("LegR x=-16"), FMath::IsNearlyEqual(LegRX->Keys[0].Value, -16.f, 1.e-4f));
			}
		});
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
