// Fill out your copyright notice in the Description page of Project Settings.


#include "BrotatoGAS/AttributeSet/BRT_AttributeSet.h"


void UBRT_AttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	
}

void UBRT_AttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	
}

