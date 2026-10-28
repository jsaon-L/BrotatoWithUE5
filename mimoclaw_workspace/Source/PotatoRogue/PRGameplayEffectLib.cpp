// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRGameplayEffectLib.h"
#include "PRAbilitySystemComponent.h"
#include "PRAttributeSet.h"
#include "PRDamageExecution.h"
#include "PRGameplayTags.h"
#include "GameplayEffect.h"

namespace
{
	/** 百分比类属性 -> 属性值换算（百分点 -> 小数增量等） */
	FGameplayAttribute StatToAttribute(EPRStat Stat, float& OutValue)
	{
		switch (Stat)
		{
		case EPRStat::MaxHealth:
			return UPRAttributeSet::GetMaxHealthAttribute();
		case EPRStat::HealthRegen:
			return UPRAttributeSet::GetHealthRegenAttribute();
		case EPRStat::DamagePct:
			OutValue *= 0.01f; // 10% -> +0.10
			return UPRAttributeSet::GetDamageMultiplierAttribute();
		case EPRStat::MeleeDamagePct:
			OutValue *= 0.01f;
			return UPRAttributeSet::GetMeleeDamageMultiplierAttribute();
		case EPRStat::RangedDamagePct:
			OutValue *= 0.01f;
			return UPRAttributeSet::GetRangedDamageMultiplierAttribute();
		case EPRStat::AttackSpeedPct:
			OutValue *= 0.01f;
			return UPRAttributeSet::GetAttackSpeedMultiplierAttribute();
		case EPRStat::CritChance:
			return UPRAttributeSet::GetCritChanceAttribute();
		case EPRStat::CritDamagePct:
			OutValue *= 0.01f;
			return UPRAttributeSet::GetCritMultiplierAttribute();
		case EPRStat::Range:
			return UPRAttributeSet::GetRangeAttribute();
		case EPRStat::Armor:
			return UPRAttributeSet::GetArmorAttribute();
		case EPRStat::Dodge:
			return UPRAttributeSet::GetDodgeAttribute();
		case EPRStat::SpeedPct:
			OutValue = UPRAttributeSet::BaseMoveSpeed * OutValue * 0.01f; // 百分比换算为速度增量
			return UPRAttributeSet::GetMoveSpeedAttribute();
		case EPRStat::Luck:
			return UPRAttributeSet::GetLuckAttribute();
		case EPRStat::Harvesting:
			return UPRAttributeSet::GetHarvestingAttribute();
		case EPRStat::EngineeringPct:
			OutValue *= 0.01f;
			return UPRAttributeSet::GetEngineeringAttribute();
		case EPRStat::PickupRange:
			return UPRAttributeSet::GetPickupRangeAttribute();
		default:
			return UPRAttributeSet::GetMaxHealthAttribute();
		}
	}
}

namespace PRGameplayEffects
{
	void ApplyDamage(UAbilitySystemComponent* Source, UAbilitySystemComponent* Target, float BaseDamage)
	{
		if (!Target || BaseDamage <= 0.f)
		{
			return;
		}

		// 即使没有来源（陷阱等）也要能结算，退化为以目标为源
		UAbilitySystemComponent* InstigatorASC = Source ? Source : Target;

		FGameplayEffectSpecHandle SpecHandle = InstigatorASC->MakeOutgoingSpec(
			UPRDamageEffect::StaticClass(), 1.f, InstigatorASC->MakeEffectContext());

		if (!SpecHandle.IsValid())
		{
			return;
		}

		SpecHandle.Data->SetSetByCallerMagnitude(PRGameplayTags::Data_Damage, BaseDamage);
		Target->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}

	FGameplayEffectSpecHandle MakeDamageSpec(UAbilitySystemComponent* Source, float BaseDamage)
	{
		if (!Source)
		{
			return FGameplayEffectSpecHandle();
		}
		FGameplayEffectSpecHandle SpecHandle = Source->MakeOutgoingSpec(
			UPRDamageEffect::StaticClass(), 1.f, Source->MakeEffectContext());
		if (SpecHandle.IsValid())
		{
			SpecHandle.Data->SetSetByCallerMagnitude(PRGameplayTags::Data_Damage, BaseDamage);
		}
		return SpecHandle;
	}

	void ApplyHeal(UAbilitySystemComponent* Target, float HealAmount)
	{
		if (!Target || HealAmount <= 0.f)
		{
			return;
		}

		FGameplayEffectSpecHandle SpecHandle = Target->MakeOutgoingSpec(
			UPRHealEffect::StaticClass(), 1.f, Target->MakeEffectContext());
		if (!SpecHandle.IsValid())
		{
			return;
		}

		SpecHandle.Data->SetSetByCallerMagnitude(PRGameplayTags::Data_Heal, HealAmount);
		Target->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}

	void ApplyStatMod(UAbilitySystemComponent* Target, EPRStat Stat, float Value)
	{
		if (!Target)
		{
			return;
		}

		UGameplayEffect* GE = NewObject<UGameplayEffect>(GetTransientPackage(), NAME_None, RF_Transient);
		GE->DurationPolicy = EGameplayEffectDurationType::Instant;

		float Converted = Value;
		const FGameplayAttribute Attribute = StatToAttribute(Stat, Converted);

		FGameplayModifierInfo Mod;
		Mod.Attribute = Attribute;
		Mod.ModifierOp = EGameplayModOp::Additive;
		Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Converted));
		GE->Modifiers.Add(Mod);

		// 生命上限提升时同步抬升当前生命（土豆兄弟行为）
		if (Stat == EPRStat::MaxHealth && Value > 0.f)
		{
			FGameplayModifierInfo HealthMod;
			HealthMod.Attribute = UPRAttributeSet::GetHealthAttribute();
			HealthMod.ModifierOp = EGameplayModOp::Additive;
			HealthMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Value));
			GE->Modifiers.Add(HealthMod);
		}

		Target->ApplyGameplayEffectToSelf(GE, 1.f, Target->MakeEffectContext());
	}

	void ApplyStatMods(UAbilitySystemComponent* Target, const TArray<FPRStatMod>& Mods)
	{
		for (const FPRStatMod& Mod : Mods)
		{
			ApplyStatMod(Target, Mod.Stat, Mod.Value);
		}
	}
}
