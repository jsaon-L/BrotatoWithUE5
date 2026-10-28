// Copyright (c) PotatoRogue. MIT Licensed.

#include "PREnemyCharacter.h"
#include "PREnemyHealthBar.h"
#include "PRDataAssets.h"
#include "PRGameState.h"
#include "PRPlayerCharacter.h"
#include "PRAttributeSet.h"
#include "PRAbilitySystemComponent.h"
#include "PRGameplayEffectLib.h"
#include "Components/WidgetComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

APREnemyCharacter::APREnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -22.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(SphereMesh.Object);
	}

	HealthBarComp = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBarComp->SetupAttachment(RootComponent);
	HealthBarComp->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBarComp->SetDrawSize(FVector2D(80.f, 10.f));
	HealthBarComp->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	HealthBarComp->SetWidgetClass(UPREnemyHealthBar::StaticClass());
	HealthBarComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 敌人默认更慢
	GetCharacterMovement()->MaxFlySpeed = 260.f;
}

void APREnemyCharacter::InitFromDefinition(UPREnemyDefinition* InDefinition, int32 WaveNumber)
{
	Definition = InDefinition;
	if (!Definition)
	{
		return;
	}

	// 波次缩放：血量与伤害随波次成长（肉鸽难度曲线）
	const float HealthScale = 1.f + 0.35f * (WaveNumber - 1) + 0.02f * WaveNumber * WaveNumber;
	const float DamageScale = 1.f + 0.18f * (WaveNumber - 1);

	AttributeSet->SetMaxHealth(Definition->MaxHealth * HealthScale);
	AttributeSet->SetHealth(Definition->MaxHealth * HealthScale);
	AttributeSet->SetMoveSpeed(Definition->MoveSpeed);
	AttributeSet->SetArmor(0.f);

	GetCharacterMovement()->MaxFlySpeed = Definition->MoveSpeed;
	GetCapsuleComponent()->SetCapsuleRadius(Definition->CapsuleRadius);

	// 体型与颜色
	SetActorScale3D(FVector(Definition->Scale));
	if (UMaterialInstanceDynamic* MID = BodyMesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Definition->BodyColor);
	}

	// 接触伤害倍率缓存进属性（伤害执行仍走 GAS）
	AttributeSet->SetDamageMultiplier(100.f * DamageScale);
}

void APREnemyCharacter::InitAbilityActorInfo()
{
	// 敌人无 Controller：Owner = Avatar = 自身
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

void APREnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDead)
	{
		return;
	}

	// 锁定玩家
	if (!TargetPlayer.IsValid())
	{
		if (APRGameState* GameState = GetWorld()->GetGameState<APRGameState>())
		{
			TargetPlayer = GameState->GetPlayerCharacter();
		}
	}

	if (APRCharacterBase* Target = TargetPlayer.Get())
	{
		if (Target->IsDead())
		{
			return;
		}

		const FVector ToPlayer = Target->GetActorLocation() - GetActorLocation();
		const float Distance = ToPlayer.Size2D();

		// 追击
		if (Distance > 1.f)
		{
			AddMovementInput(ToPlayer.GetSafeNormal2D());
		}

		// 接触伤害
		AttackCooldownRemaining -= DeltaSeconds;
		const float AttackRange = GetCapsuleComponent()->GetScaledCapsuleRadius() + 34.f;
		if (Distance <= AttackRange && AttackCooldownRemaining <= 0.f)
		{
			AttackCooldownRemaining = Definition ? Definition->AttackInterval : 1.2f;

			if (UPRAbilitySystemComponent* TargetASC = Target->GetPRAbilitySystemComponent())
			{
				const float Damage = Definition ? Definition->ContactDamage : 8.f;
				PRGameplayEffects::ApplyDamage(AbilitySystemComponent, TargetASC, Damage);
			}
		}
	}

	// 同步头顶血条
	UpdateHealthBar();
}

int32 APREnemyCharacter::GetXPReward() const
{
	return Definition ? Definition->XPReward : 3;
}

int32 APREnemyCharacter::GetCoinReward() const
{
	return Definition ? Definition->CoinReward : 1;
}

void APREnemyCharacter::HandleDeath(AActor* Killer)
{
	if (bDead)
	{
		return;
	}

	Super::HandleDeath(Killer);

	// 奖励与掉落交给波次管理器
	if (APRGameState* GameState = GetWorld()->GetGameState<APRGameState>())
	{
		GameState->NotifyEnemyKilled(this);
	}

	HealthBarComp->SetVisibility(false);
	SetLifeSpan(0.2f);
}

void APREnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 确保从活跃列表移除
	if (APRGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APRGameState>() : nullptr)
	{
		GameState->UnregisterEnemy(this);
	}
	Super::EndPlay(EndPlayReason);
}

void APREnemyCharacter::UpdateHealthBar() const
{
	if (UPREnemyHealthBar* HealthBar = Cast<UPREnemyHealthBar>(HealthBarComp->GetWidget()))
	{
		const float Health = GetAttributeValue(UPRAttributeSet::GetHealthAttribute());
		const float MaxHealth = GetAttributeValue(UPRAttributeSet::GetMaxHealthAttribute());
		HealthBar->SetHealthPercent(MaxHealth > 0.f ? Health / MaxHealth : 0.f);
	}
}
