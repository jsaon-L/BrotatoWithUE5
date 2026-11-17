// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BRT_BaseCharacter.h"

#include "PaperSpriteComponent.h"
#include "Animation/BRT_PaperDollAnimComponent.h"
#include "BrotatoGAS/BRT_AbilitySystemComponent.h"
#include "BrotatoGAS/AttributeSet/BRT_AttributeSet.h"
#include "Components/CapsuleComponent.h"


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
	
	PaperDollAnim = CreateDefaultSubobject<UBRT_PaperDollAnimComponent>(TEXT("PaperDollAnim"));

	//用来显示的组件的根
	SpriteRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SpriteRoot"));
	SpriteRoot->SetupAttachment(GetCapsuleComponent());
	
	// 纸娃娃层级：身体精灵（Root）→ 腿部关节（LegL/LegR）→ 腿部精灵。
	// 位置按 Godot player.tscn 换算：Godot(x,y) -> UE(x,0,-y)，PPU=1（与腿部精灵 PixelsPerUnrealUnit 一致）。
	PaperDollBody = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("PaperDollBody"));
	PaperDollBody->SetupAttachment(SpriteRoot);
	PaperDollBody->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
	PaperDollBody->SetTranslucentSortPriority(1); // 身体渲染在腿之前（Godot show_behind_parent）

}

void ABRT_BaseCharacter::HandleDeath()
{
	bAlive = false;
}

void ABRT_BaseCharacter::HandleRespawn()
{
	bAlive = true;
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

void ABRT_BaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	
	//反转朝向移动的方向
	const double velocityY = GetVelocity().Y;
	// if (bAlive && velocityY != 0)
	{
		if (velocityY > 0)
		{
			SpriteRoot->SetRelativeScale3D(FVector::One());
		}
		else if (velocityY < 0)
		{
			SpriteRoot->SetRelativeScale3D(FVector(-1,1,1));
		}
	}

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
		HandleDeath();
	}
}