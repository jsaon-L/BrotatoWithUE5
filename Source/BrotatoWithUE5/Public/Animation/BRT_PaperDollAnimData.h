// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BRT_PaperDollAnimData.generated.h"

BROTATOWITHUE5_API DECLARE_LOG_CATEGORY_EXTERN(LogBRTAnim, Log, All);

struct FBRT_GodotAnimParseResult;

/**
 * 纸娃娃动画标量通道。
 * Godot 2D 变换（position/scale Vector2 + rotation_degrees）拆解为 UE 变换分量，
 * 映射约定见 Docs/AI/玩法设计/07（Z 轴 = -Y）：Godot(x,y) -> UE(x,0,-y)。
 */
UENUM(BlueprintType)
enum class EBRT_AnimChannel : uint8
{
	LocationX,
	LocationY,
	LocationZ,
	RotationPitch,
	ScaleX,
	ScaleY,
	ScaleZ,
};

/** 关键帧插值方式 */
UENUM(BlueprintType)
enum class EBRT_CurveInterp : uint8
{
	/** Godot value 轨道：段内 ratio 经 Math::ease(左 key Transition) 整形后线性插值 */
	LinearEase,
	/** Godot bezier 轨道：时间-数值平面三次贝塞尔（二分求参） */
	Bezier,
};

/** 轨道更新模式。Discrete 表示保持左 key 值，不插值（对应 Godot update!=0 或 interp=nearest） */
UENUM(BlueprintType)
enum class EBRT_TrackUpdate : uint8
{
	Continuous,
	Discrete,
};

/** 标量关键帧 */
USTRUCT(BlueprintType)
struct FBRT_ScalarKey
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	float Time = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	float Value = 0.f;

	/** LinearEase 用：Godot transitions，1=线性，0=保持左 key 值；段内取左 key 的该值 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	float Transition = 1.f;

	/** Bezier 用：入控制柄相对 (Time,Value) 的偏移 (dt,dv) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	FVector2D InHandle = FVector2D::ZeroVector;

	/** Bezier 用：出控制柄相对 (Time,Value) 的偏移 (dt,dv) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	FVector2D OutHandle = FVector2D::ZeroVector;
};

/** 单条标量通道轨道（按 Time 升序的关键帧序列） */
USTRUCT(BlueprintType)
struct FBRT_PaperDollTrack
{
	GENERATED_BODY()

	/** 绑定目标名：由解析器从 Godot 节点路径映射（"."/"Animation" -> Root，其余取末段节点名，如 Sprite / LegL / Collision） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	FName TargetName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	EBRT_AnimChannel Channel = EBRT_AnimChannel::LocationX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	EBRT_CurveInterp Interp = EBRT_CurveInterp::LinearEase;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	EBRT_TrackUpdate Update = EBRT_TrackUpdate::Continuous;

	/** Godot loop_wrap：为 false 时不跨越循环点插值（保持末 key 值） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	bool bLoopWrap = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	TArray<FBRT_ScalarKey> Keys;

	/**
	 * 按 Godot 3.5 语义求值（已对照 scene/resources/animation.cpp 与 core/math/math_funcs.cpp）。
	 * @param Time           动画内时间（通常已折算进 [0, TotalLength)）
	 * @param TotalLength    动画总长
	 * @param bLooping       是否循环（跨越末 key 时按 Godot loop 分支向首 key 插值）
	 * @param InOutSearchHint 顺序求值加速提示（上次命中 key 索引，可为 -1）
	 */
	float EvaluateAtTime(float Time, float TotalLength, bool bLooping, int32& InOutSearchHint) const;

	/** 无缓存提示的便捷重载 */
	float EvaluateAtTime(float Time, float TotalLength, bool bLooping) const;

	/** Godot Math::ease(x, c) 精确移植（x 钳制到 [0,1]，c==0 返回 0） */
	static float Ease(float X, float C);
};

/** 动画事件（对应 Godot method 轨道） */
USTRUCT(BlueprintType)
struct FBRT_PaperDollAnimEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	float Time = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	FName EventName;

	/** method 轨道的参数列表 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	TArray<FString> Args;
};

/**
 * 纸娃娃变换动画资产：标量通道曲线 + 循环设置。
 * 资产中的值已是 UE 空间（导入期完成 PPU / 轴向 / 旋转符号换算），播放器零换算直写相对变换。
 */
UCLASS(BlueprintType)
class BROTATOWITHUE5_API UBRT_PaperDollAnimAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	FName AnimName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	float Length = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	bool bLoop = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	TArray<FBRT_PaperDollTrack> Tracks;

	/** method 轨道导入结果：动画事件（含时间、方法名、参数） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	TArray<FBRT_PaperDollAnimEvent> Events;

	// ---- 导入设置（仅 ImportFromTres 使用，不影响运行时） ----

	/** Godot .tres 文件路径（配合 ImportFromTres 按钮） */
	UPROPERTY(EditAnywhere, Category = "BRT|Import")
	FString TresImportPath;

	/** 像素/UE 单位，需与精灵导入的 Pixels Per Unreal Unit 一致 */
	UPROPERTY(EditAnywhere, Category = "BRT|Import")
	float ImportPixelsPerUnrealUnit = 1.f;

	/** 旋转映射符号：RotationPitch = 该值 × rotation_degrees（Godot(x,y)->UE(x,-z) 推导为 -1） */
	UPROPERTY(EditAnywhere, Category = "BRT|Import")
	float ImportRotationSign = -1.f;

	/** 原点补偿（UE 空间），加在 Root 位移通道，用于胶囊体原点与 Godot 单位原点的差异 */
	UPROPERTY(EditAnywhere, Category = "BRT|Import")
	FVector ImportOriginOffset = FVector::ZeroVector;

	/** 编辑器按钮：从 TresImportPath 导入并覆盖本资产（CallInEditor 不允许带参，故路径存于属性） */
	UFUNCTION(CallInEditor, Category = "BRT|Import")
	void ImportFromTres();

	/** 程序化导入（测试 / 批量工具）；成功时覆盖本资产并返回 true */
	bool ImportFromTresFile(const FString& TresFilePath);

	/**
	 * 从 .tscn 场景中按动画名导入（场景可能内嵌多个 Animation，如 entity.tscn 的 idle/death）。
	 * 按 WantedAnimName 在场景解析结果中查找匹配项；找到则覆盖本资产并返回 true。
	 */
	bool ImportFromScnFile(const FString& ScnFilePath, FName WantedAnimName);

	/**
	 * 直接用解析结果覆盖本资产。
	 * 批量导入同一 .tscn 的多套动画时复用一次解析结果，避免重复读盘/解析。
	 */
	void ApplyParseResult(const FBRT_GodotAnimParseResult& Result);
};
