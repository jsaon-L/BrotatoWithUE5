// Copyright (c) PotatoRogue. MIT Licensed.
// 伤害执行计算：闪避判定 -> 暴击判定 -> 全局伤害倍率 -> 护甲减伤
// 同时定义 UPRDamageEffect / UPRHealEffect 两个纯 C++ 的 GameplayEffect

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayEffectExecutionCalculation.h"
#include "PRDamageExecution.generated.h"

// ---------------- 伤害执行 ----------------
UCLASS()
class POTATOROGUE_API UPRDamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UPRDamageExecution();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};

// ---------------- 治疗执行 ----------------
UCLASS()
class POTATOROGUE_API UPRHealExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UPRHealExecution();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};

// ---------------- 瞬时伤害 GE ----------------
UCLASS()
class POTATOROGUE_API UPRDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRDamageEffect();
};

// ---------------- 瞬时治疗 GE ----------------
UCLASS()
class POTATOROGUE_API UPRHealEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UPRHealEffect();
};
