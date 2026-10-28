// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRDamageExecution.h"
#include "PRAttributeSet.h"
#include "PRGameplayTags.h"
#include "AbilitySystemComponent.h"

// ---------------- 捕获定义 ----------------
namespace
{
	struct FPRDamageStatics
	{
		DECLARE_ATTRIBUTE_CAPTUREDEF(Armor);
		DECLARE_ATTRIBUTE_CAPTUREDEF(Dodge);
		DECLARE_ATTRIBUTE_CAPTUREDEF(DamageMultiplier);
		DECLARE_ATTRIBUTE_CAPTUREDEF(CritChance);
		DECLARE_ATTRIBUTE_CAPTUREDEF(CritMultiplier);

		FPRDamageStatics()
		{
			DEFINE_ATTRIBUTE_CAPTUREDEF(UPRAttributeSet, Armor, Target, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UPRAttributeSet, Dodge, Target, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UPRAttributeSet, DamageMultiplier, Source, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UPRAttributeSet, CritChance, Source, false);
			DEFINE_ATTRIBUTE_CAPTUREDEF(UPRAttributeSet, CritMultiplier, Source, false);
		}
	};

	static const FPRDamageStatics& DamageStatics()
	{
		static FPRDamageStatics Statics;
		return Statics;
	}
}

UPRDamageExecution::UPRDamageExecution()
{
	RelevantAttributesToCapture.Add(DamageStatics().ArmorDef);
	RelevantAttributesToCapture.Add(DamageStatics().DodgeDef);
	RelevantAttributesToCapture.Add(DamageStatics().DamageMultiplierDef);
	RelevantAttributesToCapture.Add(DamageStatics().CritChanceDef);
	RelevantAttributesToCapture.Add(DamageStatics().CritMultiplierDef);
}

void UPRDamageExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	// 冲刺 / 无敌帧免疫伤害
	if (TargetTags && (TargetTags->HasTag(PRGameplayTags::State_Dashing) ||
					   TargetTags->HasTag(PRGameplayTags::State_Invulnerable)))
	{
		return;
	}

	FAggregatorEvaluateParameters EvalParams;
	EvalParams.SourceTags = SourceTags;
	EvalParams.TargetTags = TargetTags;

	// 基础伤害来自 SetByCaller（武器面板 * 近战/远程倍率）
	const float BaseDamage = Spec.GetSetByCallerMagnitude(PRGameplayTags::Data_Damage, false, 0.f);
	if (BaseDamage <= 0.f)
	{
		return;
	}

	float Armor = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ArmorDef, EvalParams, Armor);

	float Dodge = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().DodgeDef, EvalParams, Dodge);

	float DamageMultiplier = 100.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().DamageMultiplierDef, EvalParams, DamageMultiplier);

	float CritChance = 0.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CritChanceDef, EvalParams, CritChance);

	float CritMultiplier = 150.f;
	ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CritMultiplierDef, EvalParams, CritMultiplier);

	// 1) 闪避
	if (FMath::FRandRange(0.f, 100.f) < Dodge)
	{
		return; // 完全闪避
	}

	// 2) 全局伤害倍率
	float FinalDamage = BaseDamage * (DamageMultiplier / 100.f);

	// 3) 暴击
	const bool bCrit = FMath::FRandRange(0.f, 100.f) < CritChance;
	if (bCrit)
	{
		FinalDamage *= (CritMultiplier / 100.f);
	}

	// 4) 护甲减伤
	FinalDamage *= UPRAttributeSet::ArmorToMultiplier(Armor);

	FinalDamage = FMath::Max(FinalDamage, 0.f);

	OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
		UPRAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, FinalDamage));
}

UPRHealExecution::UPRHealExecution()
{
}

void UPRHealExecution::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
	FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
	const float HealAmount = Spec.GetSetByCallerMagnitude(PRGameplayTags::Data_Heal, false, 0.f);

	if (HealAmount > 0.f)
	{
		OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
			UPRAttributeSet::GetIncomingHealAttribute(), EGameplayModOp::Additive, HealAmount));
	}
}

// ---------------- GE 定义 ----------------

UPRDamageEffect::UPRDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition ExecDef;
	ExecDef.CalculationClass = UPRDamageExecution::StaticClass();
	Executions.Add(ExecDef);
}

UPRHealEffect::UPRHealEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayEffectExecutionDefinition ExecDef;
	ExecDef.CalculationClass = UPRHealExecution::StaticClass();
	Executions.Add(ExecDef);
}
