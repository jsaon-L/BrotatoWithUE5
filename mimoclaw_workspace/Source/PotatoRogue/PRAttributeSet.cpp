// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRAttributeSet.h"
#include "PRCharacterBase.h"
#include "GameplayEffectExtension.h"
#include "GameplayEffectTypes.h"

UPRAttributeSet::UPRAttributeSet()
{
	InitMaxHealth(100.f);
	InitHealth(100.f);
	InitHealthRegen(0.f);

	InitDamageMultiplier(100.f);
	InitMeleeDamageMultiplier(100.f);
	InitRangedDamageMultiplier(100.f);
	InitAttackSpeedMultiplier(100.f);
	InitCritChance(5.f);
	InitCritMultiplier(150.f);
	InitRange(350.f);

	InitArmor(0.f);
	InitDodge(0.f);
	InitMoveSpeed(BaseMoveSpeed);

	InitLuck(0.f);
	InitHarvesting(0.f);
	InitEngineering(0.f);
	InitPickupRange(250.f);

	InitIncomingDamage(0.f);
	InitIncomingHeal(0.f);
}

void UPRAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// 只做数值钳制，不改游戏逻辑
	if (Attribute == GetMaxHealthAttribute())
	{
		NewValue = FMath::Max(NewValue, 1.f);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetCritChanceAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	}
	else if (Attribute == GetDodgeAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, 60.f);   // 与土豆兄弟一致：闪避上限 60%
	}
	else if (Attribute == GetMoveSpeedAttribute())
	{
		NewValue = FMath::Max(NewValue, 50.f);
	}
	else if (Attribute == GetAttackSpeedMultiplierAttribute() ||
			 Attribute == GetDamageMultiplierAttribute() ||
			 Attribute == GetMeleeDamageMultiplierAttribute() ||
			 Attribute == GetRangedDamageMultiplierAttribute())
	{
		NewValue = FMath::Max(NewValue, 10.f);
	}
}

void UPRAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	APRCharacterBase* Owner = Cast<APRCharacterBase>(GetOwningActor());
	const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetContext();

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float DamageDone = GetIncomingDamage();
		SetIncomingDamage(0.f);

		if (DamageDone > 0.f)
		{
			const float NewHealth = FMath::Clamp(GetHealth() - DamageDone, 0.f, GetMaxHealth());
			SetHealth(NewHealth);

			if (Owner)
			{
				Owner->NotifyDamageTaken(DamageDone);
			}

			if (NewHealth <= 0.f && Owner && !Owner->IsDead())
			{
				Owner->HandleDeath(Context.GetInstigator());
			}
		}
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingHealAttribute())
	{
		const float HealAmount = GetIncomingHeal();
		SetIncomingHeal(0.f);
		SetHealth(FMath::Clamp(GetHealth() + HealAmount, 0.f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		// 上限提高时同步抬高当前血量（土豆兄弟行为）
		const float Delta = GetMaxHealth();
		SetHealth(FMath::Min(GetHealth(), Delta));
	}
}

float UPRAttributeSet::ArmorToMultiplier(float Armor)
{
	// 正护甲：100/(100+护甲)；负护甲：伤害放大（对称公式，避免除零）
	if (Armor >= 0.f)
	{
		return 100.f / (100.f + Armor);
	}
	return 2.f - (100.f / (100.f - Armor));
}
