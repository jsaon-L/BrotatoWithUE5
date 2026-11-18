// Fill out your copyright notice in the Description page of Project Settings.


#include "BrotatoGAS/AttributeSet/BRT_AttributeSet.h"


UBRT_AttributeSet::UBRT_AttributeSet()
{
	InitSpeed(1);
	InitSpeedCap(999);
	
	InitHealth(30);
	InitMaxHp(999);
}

void UBRT_AttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	
}

void UBRT_AttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	
}

