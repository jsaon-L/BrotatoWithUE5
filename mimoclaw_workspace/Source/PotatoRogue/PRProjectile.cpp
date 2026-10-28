// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "PRGameplayEffectLib.h"
#include "PRCharacterBase.h"
#include "Engine/World.h"

APRProjectile::APRProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComp->InitSphereRadius(14.f);
	CollisionComp->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	CollisionComp->SetGenerateOverlapEvents(true);
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetRelativeScale3D(FVector(0.16f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		MeshComp->SetStaticMesh(SphereMesh.Object);
	}

	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	MovementComp->ProjectileGravityScale = 0.f;      // 2D 平面，无重力
	MovementComp->bRotationFollowsVelocity = true;
	MovementComp->bShouldBounce = false;
	MovementComp->InitialSpeed = 1800.f;
	MovementComp->MaxSpeed = 1800.f;

	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &APRProjectile::OnOverlap);
}

void APRProjectile::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(LifeSpanSeconds);
}

void APRProjectile::InitProjectile(UAbilitySystemComponent* InSourceASC, float InDamage,
	const FVector& Direction, float Speed, const FLinearColor& Color)
{
	SourceASC = InSourceASC;
	Damage = InDamage;

	MovementComp->Velocity = Direction.GetSafeNormal() * Speed;

	if (UMaterialInstanceDynamic* MID = MeshComp->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), Color);
	}

	// 生命周期略大于射程/速度，保证子弹自然消失
	LifeSpanSeconds = 4.f;
}

void APRProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this || OtherActor == GetOwner())
	{
		return;
	}

	// 不能命中发射者自己
	APRCharacterBase* InstigatorCharacter = Cast<APRCharacterBase>(GetOwner());
	if (InstigatorCharacter && OtherActor == InstigatorCharacter)
	{
		return;
	}

	IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(OtherActor);
	UAbilitySystemComponent* TargetASC = ASI ? ASI->GetAbilitySystemComponent() : nullptr;
	if (!TargetASC)
	{
		return; // 只对具备 GAS 的角色造成伤害
	}

	APRCharacterBase* TargetCharacter = Cast<APRCharacterBase>(OtherActor);
	if (TargetCharacter && TargetCharacter->IsDead())
	{
		return;
	}

	PRGameplayEffects::ApplyDamage(SourceASC, TargetASC, Damage);
	Destroy();
}
