// Copyright (c) PotatoRogue. MIT Licensed.
// 技能基类：支持"自动循环攻击"模式（土豆兄弟式自动开火）

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "PRGameplayAbility.generated.h"

class UPRWeaponInstance;
class APRCharacterBase;

UCLASS(Abstract)
class POTATOROGUE_API UPRGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPRGameplayAbility();

	/** 基础攻击间隔（秒），实际间隔 = BaseCooldown / 攻速倍率 */
	UPROPERTY(EditDefaultsOnly, Category = "PR|Attack")
	float BaseCooldown = 1.f;

	/** true = 授予后自动按攻速循环施放（武器技能） */
	UPROPERTY(EditDefaultsOnly, Category = "PR|Attack")
	bool bAutoLoop = true;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	virtual void PerformAttack() PURE_VIRTUAL(UPRGameplayAbility::PerformAttack, );

	/** 当前武器实例（由 WeaponComponent 授予时作为 SourceObject 传入） */
	UPROPERTY()
	TObjectPtr<UPRWeaponInstance> WeaponInstance;

	/** 计算当前攻击间隔（吃攻速属性） */
	float GetAttackInterval() const;

	/** 宿主角色 */
	APRCharacterBase* GetPRCharacter() const;

	/** 期望攻击方向（自动索敌；无目标则用角色朝向） */
	FVector GetAimDirection() const;

	UFUNCTION()
	void OnLoopDelayFinished();

private:
	void ScheduleNextAttack();
};
