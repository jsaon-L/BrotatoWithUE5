// Copyright (c) PotatoRogue. MIT Licensed.
// 武器技能：远程射击 / 近战挥击 / 冲刺

#pragma once

#include "CoreMinimal.h"
#include "PRGameplayAbility.h"
#include "GameplayEffect.h"
#include "PRWeaponAbilities.generated.h"

/** 远程：自动索敌发射弹丸（支持多发散射） */
UCLASS()
class POTATOROGUE_API UPRGameplayAbility_RangedAttack : public UPRGameplayAbility
{
	GENERATED_BODY()

public:
	UPRGameplayAbility_RangedAttack();

protected:
	virtual void PerformAttack() override;
};

/** 近战：扇形范围判定 */
UCLASS()
class POTATOROGUE_API UPRGameplayAbility_MeleeAttack : public UPRGameplayAbility
{
	GENERATED_BODY()

public:
	UPRGameplayAbility_MeleeAttack();

protected:
	virtual void PerformAttack() override;
};

/** 冲刺：位移 + 无敌帧 */
UCLASS()
class POTATOROGUE_API UPRGameplayAbility_Dash : public UPRGameplayAbility
{
	GENERATED_BODY()

public:
	UPRGameplayAbility_Dash();

	UPROPERTY(EditDefaultsOnly, Category = "PR|Dash")
	float DashSpeed = 1800.f;

	UPROPERTY(EditDefaultsOnly, Category = "PR|Dash")
	float DashDuration = 0.22f;

protected:
	virtual void PerformAttack() override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	void OnDashFinished();
};

/** 冲刺冷却 GE（纯 C++ 定义） */
UCLASS()
class POTATOROGUE_API UPRDashCooldownEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRDashCooldownEffect();
};
