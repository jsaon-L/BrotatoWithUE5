// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRGameplayAbility.h"
#include "PRCharacterBase.h"
#include "PRAttributeSet.h"
#include "PRWeaponComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"

UPRGameplayAbility::UPRGameplayAbility()
{
	// 武器技能：授予即激活，自动循环
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 不走技能输入/事件触发，由武器组件/角色代码直接激活
	Triggers.Reset();
}

void UPRGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 记录来源武器实例
	WeaponInstance = Cast<UPRWeaponInstance>(GetCurrentSourceObject());

	PerformAttack();

	if (bAutoLoop && IsActive())
	{
		ScheduleNextAttack();
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	}
}

void UPRGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

float UPRGameplayAbility::GetAttackInterval() const
{
	float SpeedMultiplier = 100.f;
	if (const APRCharacterBase* Character = GetPRCharacter())
	{
		SpeedMultiplier = Character->GetAttributeValue(UPRAttributeSet::GetAttackSpeedMultiplierAttribute());
	}
	const float Interval = BaseCooldown / FMath::Max(SpeedMultiplier / 100.f, 0.1f);
	return FMath::Max(Interval, 0.05f);
}

APRCharacterBase* UPRGameplayAbility::GetPRCharacter() const
{
	return Cast<APRCharacterBase>(GetAvatarActorFromActorInfo());
}

FVector UPRGameplayAbility::GetAimDirection() const
{
	if (const APRCharacterBase* Character = GetPRCharacter())
	{
		return Character->GetAimDirection();
	}
	return FVector::ForwardVector;
}

void UPRGameplayAbility::ScheduleNextAttack()
{
	if (UAbilityTask_WaitDelay* Task = UAbilityTask_WaitDelay::WaitDelay(this, GetAttackInterval()))
	{
		Task->OnFinish.AddDynamic(this, &UPRGameplayAbility::OnLoopDelayFinished);
		Task->ReadyForActivation();
	}
}

void UPRGameplayAbility::OnLoopDelayFinished()
{
	if (!IsActive())
	{
		return;
	}

	PerformAttack();
	ScheduleNextAttack();
}
