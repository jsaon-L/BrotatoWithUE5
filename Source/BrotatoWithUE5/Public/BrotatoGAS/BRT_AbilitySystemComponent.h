// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "BRT_AbilitySystemComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class BROTATOWITHUE5_API UBRT_AbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UBRT_AbilitySystemComponent();

	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	
	void HandleAutoActivatedAbility(const FGameplayAbilitySpec& AbilitySpec);
	
};
