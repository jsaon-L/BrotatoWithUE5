// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "PaperCharacter.h"
#include "BRT_BaseCharacter.generated.h"


struct FOnAttributeChangeData;
class UAttributeSet;
//TODO:目前玩家和敌人都从此类扩展,可是敌人其实不需要这么重的Character,可以从Actor或者其他渲染方式驱动,后续优化
UCLASS()
class BROTATOWITHUE5_API ABRT_BaseCharacter : public APaperCharacter,public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABRT_BaseCharacter();

private:

	//TODO:单机游戏ASC放到Character中,多人游戏player的ASC放到PlayerState中最好
	UPROPERTY(VisibleAnywhere,Category = "BRT")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UAttributeSet> AttributeSet;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:


	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual UAttributeSet* GetAttributeSet() const;


protected:
	
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void BindHealthChangedDelegate();

	
};
