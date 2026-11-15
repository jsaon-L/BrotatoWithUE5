// Fill out your copyright notice in the Description page of Project Settings.

#include "Animation/BRT_PaperDollAnimData.h"

#include "Animation/BRT_GodotTresAnimParser.h"

DEFINE_LOG_CATEGORY(LogBRTAnim);

float FBRT_PaperDollTrack::Ease(float X, float C)
{
	// Godot 3.5 Math::ease() 语义，由 FMath::InterpEaseIn/Out/InOut 等价实现
	X = FMath::Clamp(X, 0.f, 1.f);
	if (C > 0.f)
	{
		if (C < 1.f)
		{
			// Godot: 1 - pow(1 - X, 1/C) ≡ InterpEaseOut 归一化形式
			return FMath::InterpEaseOut(0.f, 1.f, X, 1.f / C);
		}
		// Godot: pow(X, C) ≡ InterpEaseIn 归一化形式
		return FMath::InterpEaseIn(0.f, 1.f, X, C);
	}
	if (C < 0.f)
	{
		// Godot 负指数对称 ease-in-out ≡ InterpEaseInOut（指数取 -C）
		return FMath::InterpEaseInOut(0.f, 1.f, X, -C);
	}
	return 0.f;
}

namespace
{
	/** Godot Animation::_find 语义：返回时间 <= Time 的最后一条 key 索引；Time 早于首 key 返回 -1 */
	int32 FindKeyIndex(const TArray<FBRT_ScalarKey>& Keys, float Time, int32& InOutHint)
	{
		const int32 NumKeys = Keys.Num();
		if (NumKeys == 0)
		{
			return INDEX_NONE;
		}

		// 顺序播放快路径：命中上次索引则免二分
		if (Keys.IsValidIndex(InOutHint) &&
			Keys[InOutHint].Time <= Time &&
			(InOutHint + 1 >= NumKeys || Time < Keys[InOutHint + 1].Time))
		{
			return InOutHint;
		}

		int32 Low = 0;
		int32 High = NumKeys - 1;
		int32 Middle = 0;
		while (Low <= High)
		{
			Middle = (Low + High) / 2;
			if (FMath::IsNearlyEqual(Time, Keys[Middle].Time))
			{
				break;
			}
			if (Time < Keys[Middle].Time)
			{
				High = Middle - 1;
			}
			else
			{
				Low = Middle + 1;
			}
		}
		if (Keys[Middle].Time > Time)
		{
			Middle--;
		}
		InOutHint = Middle;
		return Middle;
	}

	/** 三次贝塞尔单轴求值（Godot _bezier_interp 公式） */
	float BezierComponent(float U, float Start, float Control1, float Control2, float End)
	{
		const float OMT = 1.f - U;
		const float OMT2 = OMT * OMT;
		const float OMT3 = OMT2 * OMT;
		const float U2 = U * U;
		const float U3 = U2 * U;
		return Start * OMT3 + Control1 * OMT2 * U * 3.f + Control2 * OMT * U2 * 3.f + End * U3;
	}
}

float FBRT_PaperDollTrack::EvaluateAtTime(float Time, float TotalLength, bool bLooping, int32& InOutSearchHint) const
{
	const int32 NumKeys = Keys.Num();
	if (NumKeys == 0)
	{
		return 0.f;
	}
	if (NumKeys == 1)
	{
		return Keys[0].Value;
	}

	const int32 Idx = FindKeyIndex(Keys, Time, InOutSearchHint);
	if (Idx < 0)
	{
		return Keys[0].Value;
	}
	if (Update == EBRT_TrackUpdate::Discrete)
	{
		return Keys[Idx].Value;
	}

	int32 Next = INDEX_NONE;
	float Delta = 0.f;
	if (Idx + 1 < NumKeys)
	{
		Next = Idx + 1;
		Delta = Keys[Next].Time - Keys[Idx].Time;
	}
	else if (bLooping && bLoopWrap)
	{
		// Godot loop 分支：越过末 key 后向首 key 跨环绕插值
		Next = 0;
		Delta = (TotalLength - Keys[Idx].Time) + Keys[0].Time;
	}
	else
	{
		return Keys[Idx].Value;
	}

	if (Delta <= SMALL_NUMBER)
	{
		return Keys[Idx].Value;
	}

	if (Interp == EBRT_CurveInterp::Bezier)
	{
		// Godot bezier_track_interpolate：段内二分 10 次求参；无环绕插值
		if (Next <= Idx)
		{
			return Keys[Idx].Value;
		}

		const float T = Time - Keys[Idx].Time;
		const FVector2D Start(0.f, Keys[Idx].Value);
		const FVector2D StartOut = Start + Keys[Idx].OutHandle;
		const FVector2D End(Delta, Keys[Next].Value);
		const FVector2D EndIn = End + Keys[Next].InHandle;

		float Low = 0.f;
		float High = 1.f;
		for (int32 Iter = 0; Iter < 10; ++Iter)
		{
			const float Middle = (Low + High) * 0.5f;
			const float InterpX = BezierComponent(Middle, Start.X, StartOut.X, EndIn.X, End.X);
			if (InterpX < T)
			{
				Low = Middle;
			}
			else
			{
				High = Middle;
			}
		}

		const float LowX = BezierComponent(Low, Start.X, StartOut.X, EndIn.X, End.X);
		const float HighX = BezierComponent(High, Start.X, StartOut.X, EndIn.X, End.X);
		const float LowY = BezierComponent(Low, Start.Y, StartOut.Y, EndIn.Y, End.Y);
		const float HighY = BezierComponent(High, Start.Y, StartOut.Y, EndIn.Y, End.Y);
		const float Denom = HighX - LowX;
		const float C = FMath::IsNearlyZero(Denom) ? 0.f : (T - LowX) / Denom;
		return FMath::Lerp(LowY, HighY, C);
	}

	// LinearEase：取左 key 的 Transition 经 Math::ease 整形后线性插值
	float C = (Time - Keys[Idx].Time) / Delta;
	const float Tr = Keys[Idx].Transition;
	if (Tr == 0.f)
	{
		return Keys[Idx].Value;
	}
	if (Tr != 1.f)
	{
		C = Ease(C, Tr);
	}
	return FMath::Lerp(Keys[Idx].Value, Keys[Next].Value, C);
}

float FBRT_PaperDollTrack::EvaluateAtTime(float Time, float TotalLength, bool bLooping) const
{
	int32 Hint = INDEX_NONE;
	return EvaluateAtTime(Time, TotalLength, bLooping, Hint);
}

void UBRT_PaperDollAnimAsset::ImportFromTres()
{
	if (TresImportPath.IsEmpty())
	{
		UE_LOG(LogBRTAnim, Warning, TEXT("ImportFromTres: TresImportPath 为空"));
		return;
	}
	ImportFromTresFile(TresImportPath);
}

bool UBRT_PaperDollAnimAsset::ImportFromTresFile(const FString& TresFilePath)
{
	FBRT_GodotAnimImportParams Params;
	Params.PixelsPerUnrealUnit = ImportPixelsPerUnrealUnit;
	Params.RotationSign = ImportRotationSign;
	Params.OriginOffset = ImportOriginOffset;

	FBRT_GodotAnimParseResult Result;
	if (!FBRT_GodotTresAnimParser::ParseTresFile(TresFilePath, Params, Result))
	{
		return false;
	}

	AnimName = Result.AnimName;
	Length = Result.Length;
	bLoop = Result.bLoop;
	Tracks = MoveTemp(Result.Tracks);
	Events = MoveTemp(Result.Events);
	return true;
}
