// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BRT_BaseCharacter.h"

#include "BrotatoGAS/AttributeSet/BRT_AttributeSet.h"


// Sets default values
ABRT_BaseCharacter::ABRT_BaseCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ABRT_BaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	//直接初始化
	if (!IsValid(AbilitySystemComponent))return;

	//初始化ASC
	AbilitySystemComponent->InitAbilityActorInfo(this,this);
	BindHealthChangedDelegate();
}


UAbilitySystemComponent* ABRT_BaseCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UAttributeSet* ABRT_BaseCharacter::GetAttributeSet() const
{
	return  AttributeSet;
}


void ABRT_BaseCharacter::BindHealthChangedDelegate()
{
	//监听属性变化
	UBRT_AttributeSet* BRT_Attributes = Cast<UBRT_AttributeSet>(GetAttributeSet());
	if (!IsValid(BRT_Attributes)) return;
	
	GetAbilitySystemComponent()->GetGameplayAttributeValueChangeDelegate(BRT_Attributes->GetHealthAttribute())
	.AddUObject(this,&ThisClass::OnHealthChanged);
}

void ABRT_BaseCharacter::OnHealthChanged(const FOnAttributeChangeData& Data)
{
	if (Data.NewValue <= 0)
	{
		// HandleDeath();
	}
}