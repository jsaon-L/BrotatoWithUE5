// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/BRT_PaperDollAnimData.h"

/** Godot .tres 动画导入参数（导入期空间换算，资产内即 UE 原生值） */
struct FBRT_GodotAnimImportParams
{
	/** 像素/UE 单位，需与精灵导入的 Pixels Per Unreal Unit 一致 */
	float PixelsPerUnrealUnit = 1.f;

	/** rotation_degrees -> RotationPitch 的符号；Godot(x,y)->UE(x,-z) 推导为 -1 */
	float RotationSign = -1.f;

	/** 原点补偿（UE 空间），加在 Root 位移通道（胶囊体原点与 Godot 单位原点差异） */
	FVector OriginOffset = FVector::ZeroVector;
};

/** .tres 解析输出 */
struct FBRT_GodotAnimParseResult
{
	FName AnimName;
	float Length = 1.f;
	/** Godot loop 属性默认 false */
	bool bLoop = false;
	TArray<FBRT_PaperDollTrack> Tracks;
	/** method 轨道解析结果：动画事件（含时间、方法名、参数） */
	TArray<FBRT_PaperDollAnimEvent> Events;
	TArray<FString> Warnings;
	bool bSuccess = false;
};

/**
 * Godot 3.x Animation 文本（.tres / gd_resource format=2）解析器。
 * value / bezier 轨道 -> 标量通道轨道；裁剪 time > length 的越界 key；跳过 method 轨道；
 * 导入期完成 PPU / 轴向 / 旋转符号换算（映射约定：Godot(x,y) -> UE(x,0,-y)，Pitch = -rotation_degrees）。
 */
class BROTATOWITHUE5_API FBRT_GodotTresAnimParser
{
public:
	/** 解析 .tres 文本；失败原因记入 OutResult.Warnings 并打日志 */
	static bool ParseTresText(const FString& TresText, const FBRT_GodotAnimImportParams& Params, FBRT_GodotAnimParseResult& OutResult);

	/** 从文件读取并解析 */
	static bool ParseTresFile(const FString& TresFilePath, const FBRT_GodotAnimImportParams& Params, FBRT_GodotAnimParseResult& OutResult);
};
