// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRPlayerCharacter.h"
#include "PRWeaponComponent.h"
#include "PRWeaponAbilities.h"
#include "PRAttributeSet.h"
#include "PRAbilitySystemComponent.h"
#include "PRGameplayTags.h"
#include "PRGameMode.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

APRPlayerCharacter::APRPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 相机臂：垂直向下俯视
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 1400.f;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f)); // 垂直俯视

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->ProjectionMode = ECameraProjectionMode::Orthographic;
	TopDownCamera->OrthoWidth = 2200.f;
	TopDownCamera->bUsePawnControlRotation = false;

	WeaponComponent = CreateDefaultSubobject<UPRWeaponComponent>(TEXT("WeaponComponent"));

	// 免美术资源：用引擎自带球体作为玩家表现
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetRelativeScale3D(FVector(0.55f, 0.55f, 0.55f));
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -22.f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(SphereMesh.Object);
	}

	GetCharacterMovement()->MaxFlySpeed = 520.f;
	GetCapsuleComponent()->InitCapsuleSize(30.f, 44.f);
}

void APRPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 玩家主体色（蓝色）
	if (UMaterialInstanceDynamic* MID = BodyMesh->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.25f, 0.55f, 1.f));
	}
}

void APRPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitAbilityActorInfo();
}

void APRPlayerCharacter::InitAbilityActorInfo()
{
	// 玩家：Owner = Actor，Avatar = Actor，Controller 单独处理
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

void APRPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 按输入向量移动（屏幕空间 -> 世界空间）
	FVector MoveDir = FVector::ZeroVector;
	if (!MoveInput.IsNearlyZero())
	{
		const FVector CamForward = GetCameraForward2D();
		const FVector CamRight = FVector::CrossProduct(FVector::UpVector, CamForward).GetSafeNormal2D();
		MoveDir = (CamForward * MoveInput.Y + CamRight * MoveInput.X).GetSafeNormal();
	}

	if (!MoveDir.IsNearlyZero())
	{
		AddMovementInput(MoveDir);
	}

	// 同步移动速度属性
	if (GetCharacterMovement()->MaxFlySpeed != GetAttributeValue(UPRAttributeSet::GetMoveSpeedAttribute()))
	{
		GetCharacterMovement()->MaxFlySpeed = GetAttributeValue(UPRAttributeSet::GetMoveSpeedAttribute());
	}
}

FVector APRPlayerCharacter::GetCameraForward2D() const
{
	if (TopDownCamera)
	{
		const FVector Forward = TopDownCamera->GetForwardVector();
		const FVector Flat(Forward.X, Forward.Y, 0.f);
		if (!Flat.IsNearlyZero())
		{
			return Flat.GetSafeNormal();
		}
	}
	return FVector::ForwardVector;
}

void APRPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &APRPlayerCharacter::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &APRPlayerCharacter::MoveRight);
	PlayerInputComponent->BindAction(TEXT("Dash"), IE_Pressed, this, &APRPlayerCharacter::TryDash);
}

void APRPlayerCharacter::MoveForward(float Value)
{
	MoveInput.Y = Value;
}

void APRPlayerCharacter::MoveRight(float Value)
{
	MoveInput.X = Value;
}

void APRPlayerCharacter::TryDash()
{
	if (IsDead())
	{
		return;
	}

	// 冷却标签存在时不能再次冲刺
	if (AbilitySystemComponent->HasGameplayTag(PRGameplayTags::Cooldown_Dash))
	{
		return;
	}

	AbilitySystemComponent->TryActivateAbilityByClass(UPRGameplayAbility_Dash::StaticClass());
}

void APRPlayerCharacter::HandleDeath(AActor* Killer)
{
	Super::HandleDeath(Killer);

	if (APRGameMode* GameMode = GetWorld() ? Cast<APRGameMode>(GetWorld()->GetAuthGameMode()) : nullptr)
	{
		GameMode->HandlePlayerDeath();
	}
}
