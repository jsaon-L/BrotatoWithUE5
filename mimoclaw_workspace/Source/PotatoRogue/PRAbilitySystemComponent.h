// Copyright (c) PotatoRogue. MIT Licensed.
// AbilitySystemComponent 子类：封装常用的 Spec 构造与属性读写

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "PRAbilitySystemComponent.generated.h"

class UGameplayEffect;

UCLASS()
class POTATOROGUE_API UPRAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	/** 便捷构造 GameplayEffectSpec */
	FGameplayEffectSpecHandle MakeEffectSpec(TSubclassOf<UGameplayEffect> EffectClass, float Level = 1.f) const;

	/** 读取数值属性 */
	float GetAttributeValue(const FGameplayAttribute& Attribute) const;

	/** 直接设置数值属性（用于回复、面板同步等） */
	void SetAttributeValue(const FGameplayAttribute& Attribute, float NewValue);

	/** 是否拥有某 Tag */
	bool HasGameplayTag(const FGameplayTag& Tag) const;
};
