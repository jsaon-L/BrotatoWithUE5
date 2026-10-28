// Fill out your copyright notice in the Description page of Project Settings.
#include "Player/BRT_PlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

void ABRT_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!IsValid(InputSubsystem)) return;
	
	for (UInputMappingContext* Content:InputMappingContexts)
	{
		InputSubsystem->AddMappingContext(Content,0);
	}
	
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!IsValid(EnhancedInputComponent)) return;

	if (!IsValid(MoveAction)) return;

	EnhancedInputComponent->BindAction(MoveAction,ETriggerEvent::Triggered,this,&ThisClass::Move);
}


void ABRT_PlayerController::Move(const FInputActionValue& Value)
{
	if (!IsValid(GetPawn())) return;
	// if (!IsAlive()) return;

	const FVector2d MovementVector = Value.Get<FVector2d>();

	const FRotator YawRotation(0.f,GetControlRotation().Yaw,0.f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// IMC 中 WASD 常见约定：X=左右(A/D), Y=前后(W/S)
	// 具体对应关系取决于 IMC 里 Swizzle/Negate Modifier 的配置
	GetPawn()->AddMovementInput(ForwardDirection,MovementVector.Y);
	GetPawn()->AddMovementInput(RightDirection,MovementVector.X);
}