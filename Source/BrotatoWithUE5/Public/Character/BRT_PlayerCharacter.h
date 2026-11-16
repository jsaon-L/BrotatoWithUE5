// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/BRT_BaseCharacter.h"
#include "BRT_PlayerCharacter.generated.h"

class UBRT_PaperDollAnimAsset;
class UBRT_PaperDollAnimComponent;
class UPaperSprite;
class UPaperSpriteComponent;
class USceneComponent;

UCLASS()
class BROTATOWITHUE5_API ABRT_PlayerCharacter : public ABRT_BaseCharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABRT_PlayerCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	// ---- 纸娃娃层级（对齐 Godot player.tscn：Animation 根 + Legs/LegL + Legs/LegR） ----

	UPROPERTY(VisibleAnywhere, Category = "BRT|Anim")
	TObjectPtr<USceneComponent> SpriteRoot;
	
	/** 身体精灵（动画 Root 目标），腿部关节挂其下并跟随根缩放 */
	UPROPERTY(VisibleAnywhere, Category = "BRT|Anim")
	TObjectPtr<UPaperSpriteComponent> PaperDollBody;

	/** 左腿关节（动画 LegL 目标，对应 Godot Animation/Legs/LegL） */
	UPROPERTY(VisibleAnywhere, Category = "BRT|Anim")
	TObjectPtr<USceneComponent> LegLJoint;

	/** 右腿关节（动画 LegR 目标，对应 Godot Animation/Legs/LegR） */
	UPROPERTY(VisibleAnywhere, Category = "BRT|Anim")
	TObjectPtr<USceneComponent> LegRJoint;

	UPROPERTY(VisibleAnywhere, Category = "BRT|Anim")
	TObjectPtr<UPaperSpriteComponent> LegLSprite;

	UPROPERTY(VisibleAnywhere, Category = "BRT|Anim")
	TObjectPtr<UPaperSpriteComponent> LegRSprite;


	// ---- 动画与精灵资产（为空时按默认路径加载） ----
	
	/** 移动切换阈值（UE 单位/秒，PPU=1 时即像素/秒） */
	UPROPERTY(EditDefaultsOnly, Category = "BRT|Anim")
	float MoveSpeedThreshold = 10.f;

	/** 回到 idle 的速度阈值 = MoveSpeedThreshold × 该比例（滞回，避免阈值附近抖动） */
	UPROPERTY(EditDefaultsOnly, Category = "BRT|Anim")
	float IdleSpeedRatio = 0.4f;

	/** 动画最小停留时间（秒），抑制抖动频繁重播（对应 Godot MovingTimer/NotMovingTimer） */
	UPROPERTY(EditDefaultsOnly, Category = "BRT|Anim")
	float MinAnimDwellSeconds = 0.25f;



private:

	/** 当前是否处于 move 动画（滞回状态） */
	bool bMoveAnimActive = false;

	/** 动画切换冷却计时 */
	float AnimDwellCountdown = 0.f;
};
