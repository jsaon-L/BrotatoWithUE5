// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BRT_EnemyCharacter.h"


// Sets default values
ABRT_EnemyCharacter::ABRT_EnemyCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ABRT_EnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABRT_EnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void ABRT_EnemyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

