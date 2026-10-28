// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRAbilitySystemComponent.h"

FGameplayEffectSpecHandle UPRAbilitySystemComponent::MakeEffectSpec(TSubclassOf<UGameplayEffect> EffectClass, float Level) const
{
	if (!EffectClass)
	{
		return FGameplayEffectSpecHandle();
	}
	return MakeOutgoingSpec(EffectClass, Level, MakeEffectContext());
}

float UPRAbilitySystemComponent::GetAttributeValue(const FGameplayAttribute& Attribute) const
{
	return GetNumericAttributeBase(Attribute);
}

void UPRAbilitySystemComponent::SetAttributeValue(const FGameplayAttribute& Attribute, float NewValue)
{
	SetNumericAttributeBase(Attribute, NewValue);
}

bool UPRAbilitySystemComponent::HasGameplayTag(const FGameplayTag& Tag) const
{
	return GetOwnedGameplayTags().HasTag(Tag);
}
