// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRWeaponAbilities.h"
#include "PRCharacterBase.h"
#include "PRPlayerCharacter.h"
#include "PREnemyCharacter.h"
#include "PRGameState.h"
#include "PRProjectile.h"
#include "PRDataAssets.h"
#include "PRWeaponComponent.h"
#include "PRAttributeSet.h"
#include "PRGameplayEffectLib.h"
#include "PRGameplayTags.h"
#include "PRAbilitySystemComponent.h"
#include "Engine/World.h"

// ---------------- 远程 ----------------

UPRGameplayAbility_RangedAttack::UPRGameplayAbility_RangedAttack()
{
	BaseCooldown = 1.0f;
	bAutoLoop = true;
}

void UPRGameplayAbility_RangedAttack::PerformAttack()
{
	APRCharacterBase* Character = GetPRCharacter();
	APRGameState* GameState = Character ? Character->GetWorld()->GetGameState<APRGameState>() : nullptr;
	if (!Character || !GameState || !WeaponInstance || !WeaponInstance->Definition)
	{
		return;
	}

	const UPRWeaponDefinition* Def = WeaponInstance->Definition;
	const float RangedMultiplier = Character->GetAttributeValue(UPRAttributeSet::GetRangedDamageMultiplierAttribute()) / 100.f;
	const float FinalDamage = WeaponInstance->GetStackedDamage() * RangedMultiplier;

	const FVector Origin = Character->GetActorLocation();
	const FVector AimDir = GetAimDirection().GetSafeNormal2D();

	const int32 Count = FMath::Max(Def->ProjectileCount, 1);
	const float Spread = Def->SpreadAngle;

	for (int32 i = 0; i < Count; ++i)
	{
		// 均匀散射：多发弹时居中分布
		const float AngleOffset = (Count > 1)
			? FMath::DegreesToRadians(Spread) * ((float)i / (Count - 1) - 0.5f)
			: 0.f;

		const FVector Dir = AimDir.RotateAngleAxis(FMath::RadiansToDegrees(AngleOffset), FVector::UpVector);

		FTransform SpawnTM(Dir.Rotation(), Origin);
		APRProjectile* Projectile = Character->GetWorld()->SpawnActorDeferred<APRProjectile>(
			Def->ProjectileClass ? *Def->ProjectileClass : APRProjectile::StaticClass(),
			SpawnTM, Character, Character, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if (Projectile)
		{
			Projectile->InitProjectile(Character->GetPRAbilitySystemComponent(), FinalDamage, Dir, Def->ProjectileSpeed, Def->Color);
			Projectile->FinishSpawning(SpawnTM);
		}
	}

	// TODO: 在这里触发开火特效/音效（GameplayCue）
}

// ---------------- 近战 ----------------

UPRGameplayAbility_MeleeAttack::UPRGameplayAbility_MeleeAttack()
{
	BaseCooldown = 1.0f;
	bAutoLoop = true;
}

void UPRGameplayAbility_MeleeAttack::PerformAttack()
{
	APRCharacterBase* Character = GetPRCharacter();
	APRGameState* GameState = Character ? Character->GetWorld()->GetGameState<APRGameState>() : nullptr;
	if (!Character || !GameState || !WeaponInstance || !WeaponInstance->Definition)
	{
		return;
	}

	const UPRWeaponDefinition* Def = WeaponInstance->Definition;
	const float MeleeMultiplier = Character->GetAttributeValue(UPRAttributeSet::GetMeleeDamageMultiplierAttribute()) / 100.f;
	const float FinalDamage = WeaponInstance->GetStackedDamage() * MeleeMultiplier;

	const FVector Origin = Character->GetActorLocation();
	const FVector AimDir = GetAimDirection().GetSafeNormal2D();
	const float HalfArc = Def->MeleeArcDegrees * 0.5f;

	// 扇形判定：直接查询波次管理器维护的敌人列表（俯视角无需物理 trace）
	for (APREnemyCharacter* Enemy : GameState->GetAliveEnemies())
	{
		if (!IsValid(Enemy) || Enemy->IsDead())
		{
			continue;
		}

		const FVector ToEnemy = Enemy->GetActorLocation() - Origin;
		if (ToEnemy.Size2D() > Def->Range)
		{
			continue;
		}

		const float Angle = FMath::RadiansToDegrees(FMath::Acos(
			FVector::DotProduct(ToEnemy.GetSafeNormal2D(), AimDir)));
		if (Angle > HalfArc)
		{
			continue;
		}

		if (UPRAbilitySystemComponent* EnemyASC = Enemy->GetPRAbilitySystemComponent())
		{
			PRGameplayEffects::ApplyDamage(Character->GetPRAbilitySystemComponent(), EnemyASC, FinalDamage);
		}
	}

	// TODO: 挥砍特效（GameplayCue）
}

// ---------------- 冲刺 ----------------

UPRGameplayAbility_Dash::UPRGameplayAbility_Dash()
{
	BaseCooldown = 2.5f;
	bAutoLoop = false;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 冲刺冷却：基于 GE 的标准冷却流程
	CooldownGameplayEffectClass = UPRDashCooldownEffect::StaticClass();
}

void UPRGameplayAbility_Dash::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UPRGameplayAbility_Dash::PerformAttack()
{
	APRCharacterBase* Character = GetPRCharacter();
	if (!Character)
	{
		return;
	}

	// 冲刺方向 = 当前移动方向，无移动则用瞄准方向
	FVector Dir = Character->GetLastMovementInputVector().GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		Dir = GetAimDirection().GetSafeNormal2D();
	}
	if (Dir.IsNearlyZero())
	{
		Dir = FVector::ForwardVector;
	}

	if (UPRAbilitySystemComponent* ASC = Character->GetPRAbilitySystemComponent())
	{
		// 无敌帧
		ASC->AddLooseGameplayTag(PRGameplayTags::State_Dashing);
	}

	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->Velocity = Dir * DashSpeed;
	}

	// 冲刺结束后移除无敌帧
	TWeakObjectPtr<UPRGameplayAbility_Dash> WeakThis(this);
	Character->GetWorld()->GetTimerManager().SetTimer(
		Character->DashTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
		{
			if (UPRGameplayAbility_Dash* Self = WeakThis.Get())
			{
				Self->OnDashFinished();
			}
		}),
		DashDuration, false);
}

void UPRGameplayAbility_Dash::OnDashFinished()
{
	if (APRCharacterBase* Character = GetPRCharacter())
	{
		if (UPRAbilitySystemComponent* ASC = Character->GetPRAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(PRGameplayTags::State_Dashing);
		}
	}
}

// ---------------- 冲刺冷却 ----------------

UPRDashCooldownEffect::UPRDashCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(2.5f));
	InheritableOwnedTagsContainer.AddTag(PRGameplayTags::Cooldown_Dash);
}
