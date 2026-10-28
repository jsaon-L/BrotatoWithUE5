// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BRT_BaseCharacter.h"

#include "BrotatoGAS/BRT_AbilitySystemComponent.h"
#include "BrotatoGAS/AttributeSet/BRT_AttributeSet.h"


// Sets default values
ABRT_BaseCharacter::ABRT_BaseCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	AbilitySystemComponent = CreateDefaultSubobject<UBRT_AbilitySystemComponent>("AbilitySystemComponent");
	//单击游戏不同步
	AbilitySystemComponent->SetIsReplicated(false);
	//来自Gemini
	//对于 拥有该角色的客户端 (Owner)：采用 Full 模式，同步所有 GE 详情（以便 UI 显示 Buff 图标、剩余时间等）。
	//对于 其他观察者 (Simulated Proxies)：采用 Minimal 模式，只同步 Tags 和 Attributes。
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UBRT_AttributeSet>("AttributeSet");
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