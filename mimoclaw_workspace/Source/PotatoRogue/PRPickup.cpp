// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRPickup.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "PRPlayerCharacter.h"
#include "PRPlayerState.h"
#include "PRAttributeSet.h"
#include "PRAbilitySystemComponent.h"
#include "PRGameplayEffectLib.h"
#include "Engine/World.h"

APRPickup::APRPickup()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComp->InitSphereRadius(18.f);
	CollisionComp->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetRelativeScale3D(FVector(0.12f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		MeshComp->SetStaticMesh(SphereMesh.Object);
	}

	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &APRPickup::OnOverlap);
}

void APRPickup::InitAward(EPRAwardType InType, int32 InAmount)
{
	AwardType = InType;
	Amount = InAmount;

	if (UMaterialInstanceDynamic* MID = MeshComp->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), GetAwardColor());
	}
}

FLinearColor APRPickup::GetAwardColor() const
{
	switch (AwardType)
	{
	case EPRAwardType::Coin:		return FLinearColor(1.f, 0.8f, 0.1f);
	case EPRAwardType::Experience:	return FLinearColor(0.2f, 0.9f, 1.f);
	case EPRAwardType::Health:		return FLinearColor(0.2f, 1.f, 0.35f);
	default:						return FLinearColor::White;
	}
}

void APRPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APRPlayerCharacter* Player = Cast<APRPlayerCharacter>(GetWorld()->GetFirstPlayerController() ?
		GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr);
	if (!Player)
	{
		return;
	}

	const float Distance = FVector::Dist(Player->GetActorLocation(), GetActorLocation());
	const float PickupRange = Player->GetAttributeValue(UPRAttributeSet::GetPickupRangeAttribute());

	// 拾取范围内磁吸
	if (Distance <= PickupRange)
	{
		const FVector Dir = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		AddActorWorldOffset(Dir * MagnetSpeed * DeltaSeconds, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void APRPickup::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APRPlayerCharacter* Player = Cast<APRPlayerCharacter>(OtherActor);
	if (Player && !Player->IsDead())
	{
		ApplyAward(Player);
		Destroy();
	}
}

void APRPickup::ApplyAward(APRPlayerCharacter* Player)
{
	switch (AwardType)
	{
	case EPRAwardType::Coin:
	{
		if (APRPlayerState* PS = Player->GetPlayerState<APRPlayerState>())
		{
			PS->AddGold(Amount);
		}
		break;
	}
	case EPRAwardType::Experience:
	{
		if (APRPlayerState* PS = Player->GetPlayerState<APRPlayerState>())
		{
			PS->AddExperience(Amount);
		}
		break;
	}
	case EPRAwardType::Health:
	{
		PRGameplayEffects::ApplyHeal(Player->GetPRAbilitySystemComponent(), (float)Amount);
		break;
	}
	default:
		break;
	}
}
