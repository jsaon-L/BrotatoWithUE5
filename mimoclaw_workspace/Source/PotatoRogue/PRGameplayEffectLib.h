// Copyright (c) PotatoRogue. MIT Licensed.
// GameplayEffect 工具库：伤害 / 治疗 / 属性修改 的统一入口

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "PRGameTypes.h"

class UAbilitySystemComponent;

namespace PRGameplayEffects
{
	/** 造成一次伤害（走 UPRDamageExecution：闪避/暴击/护甲全在执行里判定） */
	void ApplyDamage(UAbilitySystemComponent* Source, UAbilitySystemComponent* Target, float BaseDamage);

	/** 构造伤害 Spec（需要自定义 Context 时使用） */
	FGameplayEffectSpecHandle MakeDamageSpec(UAbilitySystemComponent* Source, float BaseDamage);

	/** 治疗 */
	void ApplyHeal(UAbilitySystemComponent* Target, float HealAmount);

	/**
	 * 应用一批属性修改（瞬时、加算）。
	 * 例如物品 +10% 攻速 -> AttackSpeedMultiplier += 10
	 */
	void ApplyStatMods(UAbilitySystemComponent* Target, const TArray<FPRStatMod>& Mods);

	/** 单条属性修改 */
	void ApplyStatMod(UAbilitySystemComponent* Target, EPRStat Stat, float Value);
}
