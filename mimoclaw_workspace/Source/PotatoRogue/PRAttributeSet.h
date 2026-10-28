// Copyright (c) PotatoRogue. MIT Licensed.
// 属性集：所有战斗数值的唯一真相来源

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "PRAttributeSet.generated.h"

// 标准属性访问宏
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

class APRCharacterBase;

UCLASS()
class POTATOROGUE_API UPRAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UPRAttributeSet();

	// ---------------- 生存 ----------------
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData HealthRegen;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, HealthRegen)

	// ---------------- 伤害（百分比倍率，基准 100） ----------------
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData DamageMultiplier;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, DamageMultiplier)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData MeleeDamageMultiplier;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, MeleeDamageMultiplier)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData RangedDamageMultiplier;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, RangedDamageMultiplier)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData AttackSpeedMultiplier;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, AttackSpeedMultiplier)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData CritChance;          // 0-100
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, CritChance)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData CritMultiplier;      // 百分比，基准 150
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, CritMultiplier)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Range;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, Range)

	// ---------------- 防御 / 机动 ----------------
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Armor;               // 可为负
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, Armor)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Dodge;               // 0-60
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, Dodge)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, MoveSpeed)

	// ---------------- 资源类 ----------------
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Luck;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, Luck)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Harvesting;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, Harvesting)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Engineering;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, Engineering)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData PickupRange;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, PickupRange)

	// ---------------- Meta 属性（仅瞬时传递用） ----------------
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData IncomingDamage;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, IncomingDamage)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData IncomingHeal;
	ATTRIBUTE_ACCESSORS(UPRAttributeSet, IncomingHeal)

	// ---------------- UAttributeSet 接口 ----------------
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	/** 核心减伤公式：护甲 -> 减伤系数 */
	static float ArmorToMultiplier(float Armor);

	/** 基础移动速度（SpeedPct 属性换算用） */
	static constexpr float BaseMoveSpeed = 600.f;
};
