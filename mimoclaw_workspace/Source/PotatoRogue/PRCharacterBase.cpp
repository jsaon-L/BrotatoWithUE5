// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRCharacterBase.h"
#include "PRAbilitySystemComponent.h"
#include "PRAttributeSet.h"
#include "PRGameState.h"
#include "PREnemyCharacter.h"
#include "PRGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

APRCharacterBase::APRCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;

	AbilitySystemComponent = CreateDefaultSubobject<UPRAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(false); // 单机肉鸽，无需复制

	AttributeSet = CreateDefaultSubobject<UPRAttributeSet>(TEXT("AttributeSet"));

	// 俯视角 2D：飞行移动模式 + 零重力，锁定在同一平面
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCharacterMovement()->GravityScale = 0.f;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 720.f, 0.f);
	GetCharacterMovement()->BrakingDecelerationFlying = 1600.f;
	GetCharacterMovement()->MaxFlySpeed = 600.f;

	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));

	// 不使用 Mesh，俯视角下由静态网格/精灵表现（见子类）
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetVisibility(false);
	}
}

void APRCharacterBase::BeginPlay()
{
	Super::BeginPlay();
	InitAbilityActorInfo();
}

void APRCharacterBase::InitAbilityActorInfo()
{
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

void APRCharacterBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDead)
	{
		return;
	}

	// 生命回复（每秒结算）
	const float Regen = GetAttributeValue(UPRAttributeSet::GetHealthRegenAttribute());
	if (Regen > 0.f)
	{
		const float Health = GetAttributeValue(UPRAttributeSet::GetHealthAttribute());
		const float MaxHealth = GetAttributeValue(UPRAttributeSet::GetMaxHealthAttribute());
		if (Health < MaxHealth)
		{
			AbilitySystemComponent->SetAttributeValue(UPRAttributeSet::GetHealthAttribute(),
				FMath::Min(Health + Regen * DeltaSeconds, MaxHealth));
		}
	}
}

UAbilitySystemComponent* APRCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

float APRCharacterBase::GetAttributeValue(const FGameplayAttribute& Attribute) const
{
	return AbilitySystemComponent ? AbilitySystemComponent->GetAttributeValue(Attribute) : 0.f;
}

FVector APRCharacterBase::GetAimDirection() const
{
	if (AActor* Target = FindNearestEnemy(2000.f))
	{
		return (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	}

	const FVector MoveDir = GetLastMovementInputVector().GetSafeNormal2D();
	return MoveDir.IsNearlyZero() ? GetActorForwardVector().GetSafeNormal2D() : MoveDir;
}

AActor* APRCharacterBase::FindNearestEnemy(float MaxRange) const
{
	APRGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APRGameState>() : nullptr;
	if (!GameState)
	{
		return nullptr;
	}

	const FVector Origin = GetActorLocation();
	AActor* Best = nullptr;
	float BestDistSq = FMath::Square(MaxRange);

	for (APREnemyCharacter* Enemy : GameState->GetAliveEnemies())
	{
		if (!IsValid(Enemy) || Enemy->IsDead())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Origin, Enemy->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Enemy;
		}
	}
	return Best;
}

void APRCharacterBase::HandleDeath(AActor* Killer)
{
	if (bDead)
	{
		return;
	}
	bDead = true;

	AbilitySystemComponent->AddLooseGameplayTag(PRGameplayTags::State_Dead);
	AbilitySystemComponent->CancelAllAbilities();

	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APRCharacterBase::NotifyDamageTaken(float DamageAmount)
{
	// 钩子：飘字 / 受击特效可在此实现
}
