// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/BRT_BaseCharacter.h"
#include "BRT_EnemyCharacter.generated.h"

class UBRT_EnemyMovementBehaviorComponent;

/**
 * 敌人角色。
 *
 * 移动相关的全部逻辑（6 种移动策略、速度/击退/冲撞锁定）都在 UBRT_EnemyMovementBehaviorComponent 里，
 * 这里只负责：配置 CharacterMovement 为 2D 平面飞行、挂载组件、把「被击退」转成接触伤害开关事件。
 */
UCLASS()
class BROTATOWITHUE5_API ABRT_EnemyCharacter : public ABRT_BaseCharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABRT_EnemyCharacter();

	/** 死亡时让移动组件解除冲撞锁定并停下 */
	virtual void HandleDeath() override;

	/** 敌人移动行为组件（移动策略 / 速度 / 击退 / 冲撞锁定都在这里配置和调用） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Move")
	TObjectPtr<UBRT_EnemyMovementBehaviorComponent> MovementBehavior;

	/** 接触伤害开关变化（对应 enemy.gd 里被击退时 _hitbox.disable()，由蓝图关掉攻击判定） */
	UFUNCTION(BlueprintImplementableEvent, Category = "BRT|Move")
	void OnContactDamageSuppressedChanged(bool bSuppressed);

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:
	/** 移动组件的被击退状态变化 -> 转成接触伤害开关事件 */
	UFUNCTION()
	void HandleKnockedBackChanged(bool bKnockedBack);
};
