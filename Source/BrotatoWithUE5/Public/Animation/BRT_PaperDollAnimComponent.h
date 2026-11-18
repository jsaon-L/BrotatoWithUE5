// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "BRT_PaperDollAnimComponent.generated.h"

class UBRT_PaperDollAnimAsset;
class USceneComponent;

/** 纸娃娃动画事件（对应 Godot method 轨道） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnPaperDollAnimEvent, FName, EventName, float, EventTime, const TArray<FString>&, EventArgs);

/** 细节面板中配置的一条目标绑定 */
USTRUCT()
struct FBRT_PaperDollTargetBinding
{
	GENERATED_BODY()

	/** 对应动画资产轨道的 TargetName（Root / LegL / LegR） */
	UPROPERTY(EditAnywhere, Category = "Binding")
	FName TargetName;

	/**
	 * 要驱动的组件。用 FComponentReference + UseComponentPicker：
	 * 细节面板的下拉框只列出「本 Actor 自己」的组件（不含 AllowAnyActor 就不能跨 Actor 选择），
	 * AllowedClasses 再把非 SceneComponent 的组件（如移动/音频组件）过滤掉。
	 */
	UPROPERTY(EditAnywhere, Category = "Binding", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.SceneComponent"))
	FComponentReference Component;
};

/**
 * 纸娃娃变换动画播放器：逐帧求值标量通道曲线并写入已注册组件的相对变换。
 * 只做"切换即播"，无动画融合/过渡（用户需求）。
 */
UCLASS(ClassGroup = (BRT), meta = (BlueprintSpawnableComponent))
class BROTATOWITHUE5_API UBRT_PaperDollAnimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBRT_PaperDollAnimComponent();

	/**
	 * 注册动画目标。TargetName 对应资产轨道的 TargetName（Root / LegL / LegR），
	 * 注册时快照组件当前相对变换作为 Rest 值（未被轨道驱动的通道保持 Rest）。
	 * 也可直接在细节面板的 ConfiguredTargets 中配置，BeginPlay 时自动注册。
	 */
	void RegisterTarget(FName TargetName, USceneComponent* Component);

	/** 切换即播（无融合）：不同动画立即整体替换并从头播放；同一动画重复调用不重播 */
	UFUNCTION(BlueprintCallable, Category = "BRT|Anim")
	void PlayAnim(UBRT_PaperDollAnimAsset* NewAnim);

	/** 细节面板中配置的动画资源库，PlayAnimByName 从中按名称查找 */
	UPROPERTY(EditAnywhere, Category = "BRT|Anim")
	TArray<TObjectPtr<UBRT_PaperDollAnimAsset>> AnimAssets;

	/** 按动画资产 AnimName 在 AnimAssets 中查找并播放；找到返回 true，否则返回 false */
	UFUNCTION(BlueprintCallable, Category = "BRT|Anim")
	bool PlayAnimByName(FName AnimName);

	UFUNCTION(BlueprintCallable, Category = "BRT|Anim")
	void SetPaused(bool bPaused);

	UFUNCTION(BlueprintCallable, Category = "BRT|Anim")
	void SetPlayRate(float NewPlayRate);

	UFUNCTION(BlueprintPure, Category = "BRT|Anim")
	float GetPlayTime() const { return TimeInAnim; }

	UFUNCTION(BlueprintPure, Category = "BRT|Anim")
	UBRT_PaperDollAnimAsset* GetCurrentAnim() const { return CurrentAnim; }

	/** 动画事件广播（对应 Godot method 轨道触发） */
	UPROPERTY(BlueprintAssignable, Category = "BRT|Anim")
	FOnPaperDollAnimEvent OnAnimEvent;

	/** 在细节面板配置目标绑定，BeginPlay 时自动逐个调用 RegisterTarget */
	UPROPERTY(EditAnywhere, Category = "BRT|Anim|Bindings")
	TArray<FBRT_PaperDollTargetBinding> ConfiguredTargets;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FBoundTarget
	{
		TWeakObjectPtr<USceneComponent> Component;
		FVector RestLocation = FVector::ZeroVector;
		FRotator RestRotation = FRotator::ZeroRotator;
		FVector RestScale = FVector::OneVector;
	};

	struct FTrackRuntime
	{
		int32 AnimTrackIndex = INDEX_NONE;
		int32 TargetIndex = INDEX_NONE;
		/** 顺序求值加速：上次命中的关键帧索引 */
		int32 SearchHint = INDEX_NONE;
	};

	UPROPERTY()
	TObjectPtr<UBRT_PaperDollAnimAsset> CurrentAnim;

	TArray<FBoundTarget> BoundTargets;
	TMap<FName, int32> TargetNameToIndex;
	TArray<FTrackRuntime> RuntimeTracks;

	/**
	 * 触发 (RangeStart, RangeEnd] 区间内的动画事件（bIncludeStart 为 true 时区间为 [RangeStart, RangeEnd]）。
	 * 每次整表扫描、不依赖 Events 的排列顺序；事件回调里切换了动画则立即中止本轮剩余事件。
	 */
	void FireEventsInRange(float RangeStart, float RangeEnd, bool bIncludeStart);

	float TimeInAnim = 0.f;
	float PlayRate = 1.f;
	bool bPaused = false;

	/** 刚切换动画、尚未 Tick 过：下一次 Tick 的事件区间需要包含起点，保证 Time = 0 的事件能触发 */
	bool bFireEventsAtStart = false;
};
