// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

/** 批量导入选项 */
struct FBRT_GodotAnimImportOptions
{
	/** Godot 工程根目录（含 project.godot 的目录），递归扫描其下所有 .tres / .tscn */
	FString GodotRoot = TEXT("E:/UE/UEProject/BrotatoProject-main");

	/** UE 内容根：Godot 相对目录原样镜像到其下，动画统一落在 <目录>/Animations/ */
	FString ContentRoot = TEXT("/Game/BrotatoProject");

	/** 像素/UE 单位，需与精灵导入的 Pixels Per Unreal Unit 一致 */
	float PixelsPerUnrealUnit = 1.f;

	/** rotation_degrees -> RotationPitch 的符号；Godot(x,y)->UE(x,-z) 推导为 -1 */
	float RotationSign = -1.f;

	/** 是否把资产保存到磁盘（批处理 / 自动化测试需要 true） */
	bool bSaveAssets = true;

	/** 是否覆盖已存在资产；false 时只导入新资产 */
	bool bOverwriteExisting = true;

	/**
	 * 是否保留"无变换轨道"的动画（只有贴图帧动画或空动画，如 nuclear_projectile 的 fire）。
	 * true 时仍产出资产（只含 Length / 事件），保证资产结构与 Godot 工程一致。
	 */
	bool bKeepEmptyAnimations = true;
};

/** 单条导入结果 */
struct FBRT_GodotAnimImportEntry
{
	/** Godot 源文件（.tres 或 .tscn）绝对路径 */
	FString SourceFile;

	/** Godot 动画名（.tres 取 resource_name / 文件名，.tscn 取 AnimationPlayer 的 anims/xxx） */
	FString AnimName;

	/** 生成的 UE 资产包路径 */
	FString PackagePath;

	bool bCreated = false;
	bool bImported = false;
	int32 NumTracks = 0;
	int32 NumEvents = 0;

	/** 解析期警告（被跳过的非变换轨道等） */
	TArray<FString> Warnings;
};

/** 批量导入汇总 */
struct FBRT_GodotAnimImportReport
{
	TArray<FBRT_GodotAnimImportEntry> Entries;

	/** 未产生资产的源文件（无动画数据 / 空动画 / 读取失败），附原因 */
	TArray<FString> SkippedFiles;

	int32 NumImported = 0;
	int32 NumCreated = 0;
	int32 NumFailed = 0;

	FString ToSummaryString() const;
};

/**
 * Godot 动画批量导入器：递归扫描 Godot 工程，把能找到的 Animation 资源
 * （独立 .tres + .tscn 内嵌的 [sub_resource type="Animation"]）导入为 UBRT_PaperDollAnimAsset。
 * 资产落在 <ContentRoot>/<Godot 相对目录>/Animations 下，与 Godot 工程目录结构保持一致；
 * 若同名资产已存在于 <ContentRoot>/<Godot 相对目录> 这一旧位置，则沿用旧位置以避免重复资产。
 */
class BROTATOWITHUE5_API FBRT_GodotAnimBatchImporter
{
public:
	/** 扫描并导入全部动画（可重复执行，幂等覆盖） */
	static FBRT_GodotAnimImportReport ImportAll(const FBRT_GodotAnimImportOptions& Options);

	/** snake_case / kebab-case / 点分 -> PascalCase */
	static FString ToPascalCase(const FString& InName);

	/** player_move.tres -> PA_PlayerMove；turret_shoot_animation.tres -> PA_TurretShoot */
	static FString MakeTresAssetName(const FString& InFileName);

	/** (entity.tscn, idle) -> PA_EntityIdle；(bullet_projectile.tscn, fire) -> PA_BulletProjectileFire */
	static FString MakeSceneAnimAssetName(const FString& InSceneFileName, const FString& InAnimName);

	/** 目标包路径：<ContentRoot>/<相对目录>/Animations/<资产名>，旧位置已存在同名资产时返回旧位置 */
	static FString MakeAssetPackagePath(const FString& ContentRoot, const FString& GodotRelativeDir, const FString& AssetName);
};

#endif // WITH_EDITOR
