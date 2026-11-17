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

	/**
	 * 是否保留"无变换轨道"的动画（如只含贴图帧动画 Sprite:texture 的 fire，或空动画）。
	 * false：解析失败/跳过（原行为）；true：仍产出资产（只有 Length / 事件），批量导入用它保证结构完整。
	 */
	bool bKeepEmptyAnimations = false;
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
 * .tscn 场景内多个 [sub_resource type="Animation" id=N] 的解析输出。
 * 每个场景可能内嵌多套动画（如 entity.tscn 的 RESET/death/idle），按 AnimationPlayer 的
 * anims/xxx = SubResource(N) 反查动画名；RESET（编辑器复位动画，运行时不播放）自动跳过。
 */
struct FBRT_GodotSceneAnimParseResult
{
	/** 已排除 RESET 的各动画解析结果 */
	TArray<FBRT_GodotAnimParseResult> Animations;
	TArray<FString> Warnings;
	bool bSuccess = false;
};

/**
 * Godot 3.x Animation 文本（.tres / gd_resource format=2）解析器。
 * value / bezier 轨道 -> 标量通道轨道；裁剪 time > length 的越界 key；method 轨道 -> 动画事件；
 * 导入期完成 PPU / 轴向 / 旋转符号换算（映射约定：Godot(x,y) -> UE(x,0,-y)，Pitch = -rotation_degrees）。
 *
 * 节点路径映射（TargetName）："." 与 "Animation" -> Root，其余取路径最后一段节点名
 * （"Animation/Sprite" -> Sprite、"Animation/Legs/LegL" -> LegL、"Hitbox/Collision" -> Collision）。
 * 支持属性：position / scale / rotation_degrees 与 UI Control 的 rect_position / rect_scale / rect_rotation；
 * texture / modulate / shape 等非变换属性无法用标量通道表达，导入时跳过并计入 Warnings。
 */
class BROTATOWITHUE5_API FBRT_GodotTresAnimParser
{
public:
	/** 解析 .tres 文本（单个 [resource] 块）；失败原因记入 OutResult.Warnings 并打日志 */
	static bool ParseTresText(const FString& TresText, const FBRT_GodotAnimImportParams& Params, FBRT_GodotAnimParseResult& OutResult);

	/** 从文件读取并解析（单资源 .tres） */
	static bool ParseTresFile(const FString& TresFilePath, const FBRT_GodotAnimImportParams& Params, FBRT_GodotAnimParseResult& OutResult);

	/**
	 * 解析 .tscn 场景文本中所有 [sub_resource type="Animation" id=N] 块（可多个），
	 * 每块复用 ParseTresText 的单资源解析逻辑，并按 anims/xxx = SubResource(N) 反查动画名。
	 */
	static bool ParseScnText(const FString& ScnText, const FBRT_GodotAnimImportParams& Params, FBRT_GodotSceneAnimParseResult& OutResult);

	/** 从文件读取并解析（.tscn 场景，含多个内嵌 Animation sub_resource） */
	static bool ParseScnFile(const FString& ScnFilePath, const FBRT_GodotAnimImportParams& Params, FBRT_GodotSceneAnimParseResult& OutResult);
};
