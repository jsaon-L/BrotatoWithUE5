// Fill out your copyright notice in the Description page of Project Settings.

#include "Animation/BRT_GodotTresAnimParser.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	// ---------------- 基础文本工具 ----------------

	int32 CountChars(const FString& Str, TCHAR Char)
	{
		int32 Count = 0;
		for (const TCHAR C : Str)
		{
			if (C == Char)
			{
				++Count;
			}
		}
		return Count;
	}

	FString TrimQuotes(const FString& Str)
	{
		FString Result = Str.TrimStartAndEnd();
		if (Result.Len() >= 2 && Result.StartsWith(TEXT("\"")) && Result.EndsWith(TEXT("\"")))
		{
			Result = Result.Mid(1, Result.Len() - 2);
		}
		return Result;
	}

	/** 按 ',' 切分并解析浮点列表（容忍空项与科学计数法） */
	bool ParseFloatList(const FString& Body, TArray<float>& OutValues)
	{
		TArray<FString> Parts;
		Body.ParseIntoArray(Parts, TEXT(","), /*bCullEmpty*/ true);
		OutValues.Reset(Parts.Num());
		for (const FString& Part : Parts)
		{
			const FString Trimmed = Part.TrimStartAndEnd();
			if (Trimmed.IsEmpty())
			{
				continue;
			}
			OutValues.Add(FCString::Atof(*Trimmed));
		}
		return OutValues.Num() > 0;
	}

	/** 提取 "PoolRealArray( ... )" 内的浮点数组 */
	bool ParsePoolRealArray(const FString& Src, TArray<float>& OutValues)
	{
		const int32 OpenIdx = Src.Find(TEXT("PoolRealArray("));
		if (OpenIdx == INDEX_NONE)
		{
			return false;
		}
		const int32 StartIdx = OpenIdx + FCString::Strlen(TEXT("PoolRealArray("));
		const int32 CloseIdx = Src.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, StartIdx);
		if (CloseIdx == INDEX_NONE)
		{
			return false;
		}
		return ParseFloatList(Src.Mid(StartIdx, CloseIdx - StartIdx), OutValues);
	}

	/** 提取 "NodePath(\"...\")" 内的路径串 */
	bool ParseNodePathValue(const FString& Src, FString& OutPath)
	{
		const int32 FirstQuote = Src.Find(TEXT("\""));
		const int32 LastQuote = Src.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (FirstQuote == INDEX_NONE || LastQuote <= FirstQuote)
		{
			return false;
		}
		OutPath = Src.Mid(FirstQuote + 1, LastQuote - FirstQuote - 1);
		return true;
	}

	/** 从 '[ ... ]' 提取列表体（括号深度匹配，兼容 method 轨道的嵌套 []） */
	bool ExtractBracketedBody(const FString& Src, FString& OutBody)
	{
		const int32 StartIdx = Src.Find(TEXT("["));
		if (StartIdx == INDEX_NONE)
		{
			return false;
		}
		int32 Depth = 0;
		for (int32 Idx = StartIdx; Idx < Src.Len(); ++Idx)
		{
			if (Src[Idx] == TEXT('['))
			{
				++Depth;
			}
			else if (Src[Idx] == TEXT(']'))
			{
				--Depth;
				if (Depth == 0)
				{
					OutBody = Src.Mid(StartIdx + 1, Idx - StartIdx - 1);
					return true;
				}
			}
		}
		return false;
	}

	/** 解析 Vector2 列表：[ Vector2( a, b ), Vector2( ... ) ] */
	bool ParseVector2List(const FString& Body, TArray<FVector2D>& OutValues)
	{
		OutValues.Reset();
		int32 SearchFrom = 0;
		while (true)
		{
			const int32 OpenIdx = Body.Find(TEXT("Vector2("), ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
			if (OpenIdx == INDEX_NONE)
			{
				break;
			}
			const int32 StartIdx = OpenIdx + FCString::Strlen(TEXT("Vector2("));
			const int32 CloseIdx = Body.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, StartIdx);
			if (CloseIdx == INDEX_NONE)
			{
				return false;
			}
			TArray<float> Comps;
			if (!ParseFloatList(Body.Mid(StartIdx, CloseIdx - StartIdx), Comps) || Comps.Num() != 2)
			{
				return false;
			}
			OutValues.Add(FVector2D(Comps[0], Comps[1]));
			SearchFrom = CloseIdx + 1;
		}
		return OutValues.Num() > 0;
	}

	/** 解析 method 轨道的 values 列表：[ {"args": [...], "method": "xxx"}, ... ] */
	bool ParseMethodValues(const FString& ValuesBody, TArray<FString>& OutMethods, TArray<TArray<FString>>& OutArgs)
	{
		OutMethods.Reset();
		OutArgs.Reset();

		int32 SearchFrom = 0;
		while (true)
		{
			// 查找每个 { ... } 字典块
			const int32 OpenIdx = ValuesBody.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
			if (OpenIdx == INDEX_NONE)
			{
				break;
			}

			// 括号深度匹配找到对应的 }
			int32 Depth = 0;
			int32 CloseIdx = INDEX_NONE;
			for (int32 Idx = OpenIdx; Idx < ValuesBody.Len(); ++Idx)
			{
				if (ValuesBody[Idx] == TEXT('{'))
				{
					++Depth;
				}
				else if (ValuesBody[Idx] == TEXT('}'))
				{
					--Depth;
					if (Depth == 0)
					{
						CloseIdx = Idx;
						break;
					}
				}
			}

			if (CloseIdx == INDEX_NONE)
			{
				break;
			}

			const FString DictBlock = ValuesBody.Mid(OpenIdx, CloseIdx - OpenIdx + 1);

			// 提取 method 名称
			FString MethodName;
			const int32 MethodIdx = DictBlock.Find(TEXT("\"method\""));
			if (MethodIdx != INDEX_NONE)
			{
				const int32 QuoteStart = DictBlock.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, MethodIdx + 8);
				const int32 QuoteEnd = DictBlock.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, QuoteStart + 1);
				if (QuoteStart != INDEX_NONE && QuoteEnd != INDEX_NONE)
				{
					MethodName = DictBlock.Mid(QuoteStart + 1, QuoteEnd - QuoteStart - 1);
				}
			}

			// 提取 args 数组
			TArray<FString> Args;
			const int32 ArgsIdx = DictBlock.Find(TEXT("\"args\""));
			if (ArgsIdx != INDEX_NONE)
			{
				FString ArgsBody;
				if (ExtractBracketedBody(DictBlock.Mid(ArgsIdx), ArgsBody))
				{
					// 解析字符串数组: ["str1", "str2"] 或 [1, 2]
					int32 ArgSearchFrom = 0;
					while (true)
					{
						const int32 ArgQuoteStart = ArgsBody.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, ArgSearchFrom);
						if (ArgQuoteStart == INDEX_NONE)
						{
							break;
						}
						const int32 ArgQuoteEnd = ArgsBody.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, ArgQuoteStart + 1);
						if (ArgQuoteEnd == INDEX_NONE)
						{
							break;
						}
						Args.Add(ArgsBody.Mid(ArgQuoteStart + 1, ArgQuoteEnd - ArgQuoteStart - 1));
						ArgSearchFrom = ArgQuoteEnd + 1;
					}
				}
			}

			if (!MethodName.IsEmpty())
			{
				OutMethods.Add(MethodName);
				OutArgs.Add(MoveTemp(Args));
			}

			SearchFrom = CloseIdx + 1;
		}

		return OutMethods.Num() > 0;
	}

	// ---------------- keys 字典 ----------------

	struct FParsedKeysDict
	{
		TArray<float> Times;
		TArray<float> Transitions;
		TArray<float> Points;        // bezier：每 key 5 个浮点
		TArray<float> FloatValues;   // value 标量值
		TArray<FVector2D> VectorValues; // value Vector2 值
		int32 Update = 0;
		bool bValuesAreVectors = false;
		bool bValuesAreMethods = false;
	};

	bool ParseKeysDict(const FString& DictText, FParsedKeysDict& OutDict)
	{
		// 各数组按自身标签定位提取（bezier 字典中 points 先于 times，不能取首个 PoolRealArray）
		{
			const int32 TimesIdx = DictText.Find(TEXT("\"times\""));
			if (TimesIdx != INDEX_NONE)
			{
				ParsePoolRealArray(DictText.Mid(TimesIdx), OutDict.Times);
			}
		}
		{
			const int32 TransIdx = DictText.Find(TEXT("\"transitions\""));
			if (TransIdx != INDEX_NONE)
			{
				ParsePoolRealArray(DictText.Mid(TransIdx), OutDict.Transitions);
			}
		}
		{
			const int32 PointsIdx = DictText.Find(TEXT("\"points\""));
			if (PointsIdx != INDEX_NONE)
			{
				ParsePoolRealArray(DictText.Mid(PointsIdx), OutDict.Points);
			}
		}

		// update = 0/1/2
		{
			int32 UpdateIdx = DictText.Find(TEXT("\"update\""));
			if (UpdateIdx != INDEX_NONE)
			{
				const int32 EqIdx = DictText.Find(TEXT(":"), ESearchCase::CaseSensitive, ESearchDir::FromStart, UpdateIdx);
				if (EqIdx != INDEX_NONE)
				{
					OutDict.Update = FCString::Atoi(*DictText.Mid(EqIdx + 1));
				}
			}
		}

		// values 列表
		const int32 ValuesIdx = DictText.Find(TEXT("\"values\""));
		if (ValuesIdx != INDEX_NONE)
		{
			FString ValuesBody;
			if (ExtractBracketedBody(DictText.Mid(ValuesIdx), ValuesBody))
			{
				if (ValuesBody.Contains(TEXT("Vector2(")))
				{
					OutDict.bValuesAreVectors = true;
					ParseVector2List(ValuesBody, OutDict.VectorValues);
				}
				else if (ValuesBody.Contains(TEXT("{")))
				{
					// method 轨道的 { ... } 字典列表，跳过
					OutDict.bValuesAreMethods = true;
				}
				else
				{
					ParseFloatList(ValuesBody, OutDict.FloatValues);
				}
			}
		}

		return OutDict.Times.Num() > 0;
	}

	// ---------------- 节点路径 / 属性 -> 通道 ----------------

	struct FTargetChannel
	{
		FName TargetName;
		EBRT_AnimChannel Channel;
		/** 值缩放：PPU 与旋转符号在此换算 */
		float ValueScale = 1.f;
		/** 加性偏移：仅 Root 位移通道使用 OriginOffset */
		bool bApplyOriginOffset = false;
	};

	FName ResolveTargetName(const FString& NodePathStr)
	{
		if (NodePathStr == TEXT("Animation"))
		{
			return FName(TEXT("Root"));
		}
		if (NodePathStr == TEXT("Animation/Legs/LegL"))
		{
			return FName(TEXT("LegL"));
		}
		if (NodePathStr == TEXT("Animation/Legs/LegR"))
		{
			return FName(TEXT("LegR"));
		}
		return NAME_None;
	}

	bool MapPropertyToChannels(const FString& NodePathStr, const FString& Property,
		const FBRT_GodotAnimImportParams& Params, TArray<FTargetChannel>& OutChannels)
	{
		const FName Target = ResolveTargetName(NodePathStr);
		if (Target.IsNone())
		{
			return false;
		}

		const float PPU = Params.PixelsPerUnrealUnit > SMALL_NUMBER ? Params.PixelsPerUnrealUnit : 1.f;
		const float InvPPU = 1.f / PPU;
		const bool bIsRoot = (Target == FName(TEXT("Root")));

		OutChannels.Reset();

		// Godot(x,y) -> UE(x,0,-y)：位置 x/PPU，位置 y 取负 /PPU；scale (sx,sy) -> (sx,1,sy)
		if (Property == TEXT("position"))
		{
			FTargetChannel ChX{ Target, EBRT_AnimChannel::LocationX, InvPPU, bIsRoot };
			FTargetChannel ChZ{ Target, EBRT_AnimChannel::LocationZ, -InvPPU, bIsRoot };
			OutChannels = { ChX, ChZ };
			return true;
		}
		if (Property == TEXT("position:x"))
		{
			OutChannels.Add({ Target, EBRT_AnimChannel::LocationX, InvPPU, bIsRoot });
			return true;
		}
		if (Property == TEXT("position:y"))
		{
			OutChannels.Add({ Target, EBRT_AnimChannel::LocationZ, -InvPPU, bIsRoot });
			return true;
		}
		if (Property == TEXT("scale"))
		{
			FTargetChannel ChX{ Target, EBRT_AnimChannel::ScaleX, 1.f, false };
			FTargetChannel ChZ{ Target, EBRT_AnimChannel::ScaleZ, 1.f, false };
			OutChannels = { ChX, ChZ };
			return true;
		}
		if (Property == TEXT("scale:x"))
		{
			OutChannels.Add({ Target, EBRT_AnimChannel::ScaleX, 1.f, false });
			return true;
		}
		if (Property == TEXT("scale:y"))
		{
			OutChannels.Add({ Target, EBRT_AnimChannel::ScaleZ, 1.f, false });
			return true;
		}
		if (Property == TEXT("rotation_degrees"))
		{
			OutChannels.Add({ Target, EBRT_AnimChannel::RotationPitch, Params.RotationSign, false });
			return true;
		}
		return false;
	}

	// ---------------- 轨道原始属性 ----------------

	struct FTrackRaw
	{
		FString Type;
		FString Path;
		int32 Interp = 1;
		bool bLoopWrap = true;
		bool bEnabled = true;
		FString KeysBlock;
	};
}

bool FBRT_GodotTresAnimParser::ParseTresText(const FString& TresText, const FBRT_GodotAnimImportParams& Params, FBRT_GodotAnimParseResult& OutResult)
{
	OutResult = FBRT_GodotAnimParseResult();

	TArray<FString> Lines;
	TresText.ParseIntoArrayLines(Lines);

	TMap<int32, FTrackRaw> TrackRaws;

	// 轨道 keys 字典多行累积状态
	FString PendingDict;
	int32 PendingDictTrack = INDEX_NONE;
	int32 PendingDictDepth = 0;

	// 计数器（最终汇总为 Warning）
	int32 TrimmedKeyCount = 0;
	int32 MethodTrackCount = 0;
	int32 UnknownTypeCount = 0;
	int32 UnknownPathCount = 0;
	int32 CubicTrackCount = 0;
	int32 DiscreteTrackCount = 0;
	int32 MalformedCount = 0;

	auto FlushPendingDict = [&]()
	{
		if (PendingDictTrack != INDEX_NONE)
		{
			TrackRaws.FindOrAdd(PendingDictTrack).KeysBlock = PendingDict;
			PendingDict.Reset();
			PendingDictTrack = INDEX_NONE;
			PendingDictDepth = 0;
		}
	};

	for (const FString& RawLine : Lines)
	{
		const FString Line = RawLine.TrimStartAndEnd();
		if (Line.IsEmpty())
		{
			continue;
		}

		// keys 字典多行累积
		if (PendingDictTrack != INDEX_NONE)
		{
			PendingDict += Line + LINE_TERMINATOR;
			PendingDictDepth += CountChars(Line, TEXT('{')) - CountChars(Line, TEXT('}'));
			if (PendingDictDepth <= 0)
			{
				FlushPendingDict();
			}
			continue;
		}

		if (Line.StartsWith(TEXT("tracks/")))
		{
			int32 EqIdx = INDEX_NONE;
			if (!Line.FindChar(TEXT('='), EqIdx))
			{
				continue;
			}
			const FString Key = Line.Left(EqIdx).TrimStartAndEnd();
			const FString Value = Line.Mid(EqIdx + 1).TrimStartAndEnd();

			// tracks/N/prop
			const FString Tail = Key.Mid(FCString::Strlen(TEXT("tracks/")));
			int32 SlashIdx = INDEX_NONE;
			if (!Tail.FindChar(TEXT('/'), SlashIdx))
			{
				continue;
			}
			const int32 TrackIndex = FCString::Atoi(*Tail.Left(SlashIdx));
			const FString Prop = Tail.Mid(SlashIdx + 1).TrimStartAndEnd();

			FTrackRaw& Track = TrackRaws.FindOrAdd(TrackIndex);
			if (Prop == TEXT("type"))
			{
				Track.Type = TrimQuotes(Value);
			}
			else if (Prop == TEXT("path"))
			{
				ParseNodePathValue(Value, Track.Path);
			}
			else if (Prop == TEXT("interp"))
			{
				Track.Interp = FCString::Atoi(*Value);
			}
			else if (Prop == TEXT("loop_wrap"))
			{
				Track.bLoopWrap = (Value == TEXT("true"));
			}
			else if (Prop == TEXT("enabled"))
			{
				Track.bEnabled = (Value == TEXT("true"));
			}
			else if (Prop == TEXT("keys"))
			{
				if (Value.StartsWith(TEXT("{")))
				{
					PendingDict = Value + LINE_TERMINATOR;
					PendingDictTrack = TrackIndex;
					PendingDictDepth = CountChars(Value, TEXT('{')) - CountChars(Value, TEXT('}'));
					if (PendingDictDepth <= 0)
					{
						FlushPendingDict();
					}
				}
			}
			continue;
		}

		// 顶层属性
		int32 EqIdx = INDEX_NONE;
		if (!Line.FindChar(TEXT('='), EqIdx))
		{
			continue; // [gd_resource ...] / [resource] 等区段头
		}
		const FString Key = Line.Left(EqIdx).TrimStartAndEnd();
		const FString Value = Line.Mid(EqIdx + 1).TrimStartAndEnd();

		if (Key == TEXT("resource_name"))
		{
			OutResult.AnimName = FName(*TrimQuotes(Value));
		}
		else if (Key == TEXT("length"))
		{
			OutResult.Length = FCString::Atof(*Value);
		}
		else if (Key == TEXT("loop"))
		{
			OutResult.bLoop = (Value == TEXT("true"));
		}
		// step / 其余编辑器属性忽略
	}
	FlushPendingDict();

	// 默认值：length 缺省 1.0（Godot 默认）；名称缺省用调用方文件名补
	if (OutResult.Length <= SMALL_NUMBER)
	{
		OutResult.Length = 1.f;
	}

	// 按轨道序号处理
	TArray<int32> TrackIndices;
	TrackRaws.GenerateKeyArray(TrackIndices);
	TrackIndices.Sort();

	for (const int32 TrackIndex : TrackIndices)
	{
		const FTrackRaw& Raw = TrackRaws[TrackIndex];
		const FString TrackLabel = FString::Printf(TEXT("tracks/%d (%s)"), TrackIndex, *Raw.Path);

		if (!Raw.bEnabled)
		{
			OutResult.Warnings.Add(FString::Printf(TEXT("%s：轨道未启用，跳过"), *TrackLabel));
			continue;
		}
		if (Raw.Type == TEXT("method"))
		{
			++MethodTrackCount;

			// 解析 method 轨道的 keys 字典
			FParsedKeysDict Dict;
			if (!ParseKeysDict(Raw.KeysBlock, Dict))
			{
				++MalformedCount;
				continue;
			}

			// 提取 values 体中的 method 名称和参数
			const int32 ValuesIdx = Raw.KeysBlock.Find(TEXT("\"values\""));
			if (ValuesIdx == INDEX_NONE)
			{
				++MalformedCount;
				continue;
			}

			FString ValuesBody;
			if (!ExtractBracketedBody(Raw.KeysBlock.Mid(ValuesIdx), ValuesBody))
			{
				++MalformedCount;
				continue;
			}

			TArray<FString> Methods;
			TArray<TArray<FString>> ArgsList;
			if (!ParseMethodValues(ValuesBody, Methods, ArgsList))
			{
				++MalformedCount;
				continue;
			}

			// 生成事件，按时间裁剪越界 key
			for (int32 KeyIdx = 0; KeyIdx < Dict.Times.Num() && KeyIdx < Methods.Num(); ++KeyIdx)
			{
				const float Time = Dict.Times[KeyIdx];
				if (Time < 0.f || Time > OutResult.Length)
				{
					++TrimmedKeyCount;
					continue;
				}

				FBRT_PaperDollAnimEvent Event;
				Event.Time = Time;
				Event.EventName = FName(*Methods[KeyIdx]);
				Event.Args = ArgsList[KeyIdx];
				OutResult.Events.Add(MoveTemp(Event));
			}
			continue;
		}
		if (Raw.Type != TEXT("value") && Raw.Type != TEXT("bezier"))
		{
			++UnknownTypeCount;
			continue;
		}

		// path -> node + property
		int32 ColonIdx = INDEX_NONE;
		if (!Raw.Path.FindChar(TEXT(':'), ColonIdx))
		{
			++UnknownPathCount;
			continue;
		}
		const FString NodePathStr = Raw.Path.Left(ColonIdx);
		const FString Property = Raw.Path.Mid(ColonIdx + 1);

		TArray<FTargetChannel> Channels;
		if (!MapPropertyToChannels(NodePathStr, Property, Params, Channels))
		{
			++UnknownPathCount;
			continue;
		}

		FParsedKeysDict Dict;
		if (!ParseKeysDict(Raw.KeysBlock, Dict))
		{
			++MalformedCount;
			continue;
		}

		const bool bBezier = (Raw.Type == TEXT("bezier"));
		const bool bVectorValues = Dict.bValuesAreVectors;

		// 类型匹配校验
		if (Dict.bValuesAreMethods)
		{
			++MalformedCount;
			continue;
		}
		if (bBezier && bVectorValues)
		{
			++MalformedCount;
			continue;
		}
		if (bBezier && Channels.Num() != 1)
		{
			++MalformedCount;
			continue;
		}
		if (!bBezier && (Channels.Num() == 2) != bVectorValues)
		{
			++MalformedCount;
			continue;
		}

		// update / interp 降级计数
		if (!bBezier)
		{
			if (Raw.Interp == 0 || Dict.Update != 0)
			{
				++DiscreteTrackCount;
			}
			else if (Raw.Interp == 2)
			{
				++CubicTrackCount;
			}
		}

		const int32 NumKeys = Dict.Times.Num();
		if (bBezier)
		{
			if (Dict.Points.Num() != NumKeys * 5)
			{
				++MalformedCount;
				continue;
			}
		}
		else
		{
			const int32 ValueCount = bVectorValues ? Dict.VectorValues.Num() : Dict.FloatValues.Num();
			if (ValueCount != NumKeys)
			{
				++MalformedCount;
				continue;
			}
		}

		// 每个通道生成一条轨道
		for (int32 ChanIdx = 0; ChanIdx < Channels.Num(); ++ChanIdx)
		{
			const FTargetChannel& Chan = Channels[ChanIdx];
			FBRT_PaperDollTrack Track;
			Track.TargetName = Chan.TargetName;
			Track.Channel = Chan.Channel;
			Track.Interp = bBezier ? EBRT_CurveInterp::Bezier : EBRT_CurveInterp::LinearEase;
			Track.bLoopWrap = Raw.bLoopWrap;
			if (!bBezier && (Raw.Interp == 0 || Dict.Update != 0))
			{
				// nearest / 非连续更新：保持左 key 值（Godot 同语义）；interp=cubic 降级为线性
				Track.Update = EBRT_TrackUpdate::Discrete;
			}

			// 原点补偿仅加在 Root 位移通道
			float ChannelOffset = 0.f;
			if (Chan.bApplyOriginOffset)
			{
				ChannelOffset =
					(Chan.Channel == EBRT_AnimChannel::LocationX) ? Params.OriginOffset.X
					: (Chan.Channel == EBRT_AnimChannel::LocationY) ? Params.OriginOffset.Y
					: Params.OriginOffset.Z;
			}

			for (int32 KeyIdx = 0; KeyIdx < NumKeys; ++KeyIdx)
			{
				const float Time = Dict.Times[KeyIdx];
				if (Time < 0.f || Time > OutResult.Length)
				{
					// Godot 播放同样忽略 length 之外的 key（time == length 保留）
					++TrimmedKeyCount;
					continue;
				}

				FBRT_ScalarKey Key;
				Key.Time = Time;

				if (bBezier)
				{
					const float* P = &Dict.Points[KeyIdx * 5];
					Key.Value = P[0] * Chan.ValueScale + ChannelOffset;
					Key.InHandle = FVector2D(P[1], P[2] * Chan.ValueScale);
					Key.OutHandle = FVector2D(P[3], P[4] * Chan.ValueScale);
				}
				else
				{
					float RawValue = 0.f;
					if (bVectorValues)
					{
						RawValue = (ChanIdx == 0) ? Dict.VectorValues[KeyIdx].X : Dict.VectorValues[KeyIdx].Y;
					}
					else
					{
						RawValue = Dict.FloatValues[KeyIdx];
					}
					Key.Value = RawValue * Chan.ValueScale + ChannelOffset;
					Key.Transition = Dict.Transitions.IsValidIndex(KeyIdx) ? Dict.Transitions[KeyIdx] : 1.f;
				}

				Track.Keys.Add(MoveTemp(Key));
			}

			if (Track.Keys.Num() == 0)
			{
				++MalformedCount;
				continue;
			}
			OutResult.Tracks.Add(MoveTemp(Track));
		}
	}

	// 汇总 Warning（带计数）
	if (TrimmedKeyCount > 0)
	{
		OutResult.Warnings.Add(FString::Printf(TEXT("裁剪越界关键帧 %d 个（time > length 或 time < 0）"), TrimmedKeyCount));
	}
	if (MethodTrackCount > 0)
	{
		OutResult.Warnings.Add(FString::Printf(TEXT("解析 method 轨道 %d 条，导入 %d 个动画事件"), MethodTrackCount, OutResult.Events.Num()));
	}
	if (UnknownTypeCount > 0)
	{
		OutResult.Warnings.Add(FString::Printf(TEXT("跳过未知类型轨道 %d 条"), UnknownTypeCount));
	}
	if (UnknownPathCount > 0)
	{
		OutResult.Warnings.Add(FString::Printf(TEXT("跳过未知节点路径/属性轨道 %d 条"), UnknownPathCount));
	}
	if (CubicTrackCount > 0)
	{
		OutResult.Warnings.Add(FString::Printf(TEXT("interp=cubic 轨道 %d 条已按线性处理（未实现三次插值）"), CubicTrackCount));
	}
	if (DiscreteTrackCount > 0)
	{
		OutResult.Warnings.Add(FString::Printf(TEXT("离散/非连续更新轨道 %d 条按保持左 key 值处理"), DiscreteTrackCount));
	}
	if (MalformedCount > 0)
	{
		OutResult.Warnings.Add(FString::Printf(TEXT("数据格式不匹配轨道 %d 条被跳过"), MalformedCount));
	}

	for (const FString& Warning : OutResult.Warnings)
	{
		UE_LOG(LogBRTAnim, Warning, TEXT("ParseTres: %s"), *Warning);
	}

	OutResult.bSuccess = OutResult.Tracks.Num() > 0;
	if (!OutResult.bSuccess)
	{
		UE_LOG(LogBRTAnim, Warning, TEXT("ParseTres: 未解析出任何轨道"));
	}
	return OutResult.bSuccess;
}

bool FBRT_GodotTresAnimParser::ParseTresFile(const FString& TresFilePath, const FBRT_GodotAnimImportParams& Params, FBRT_GodotAnimParseResult& OutResult)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *TresFilePath))
	{
		UE_LOG(LogBRTAnim, Error, TEXT("ParseTresFile: 无法读取文件 %s"), *TresFilePath);
		OutResult = FBRT_GodotAnimParseResult();
		return false;
	}

	const bool bOk = ParseTresText(Text, Params, OutResult);
	if (bOk && OutResult.AnimName.IsNone())
	{
		OutResult.AnimName = FName(*FPaths::GetBaseFilename(TresFilePath));
	}
	return bOk;
}
