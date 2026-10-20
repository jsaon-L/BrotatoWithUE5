// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BRT_PlayerCharacter.h"


// Sets default values
ABRT_PlayerCharacter::ABRT_PlayerCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
	
	
}

// Called when the game starts or when spawned
void ABRT_PlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABRT_PlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
}

// Called to bind functionality to input
void ABRT_PlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

