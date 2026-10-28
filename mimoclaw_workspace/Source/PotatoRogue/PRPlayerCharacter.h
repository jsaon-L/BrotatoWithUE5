// Copyright (c) PotatoRogue. MIT Licensed.
// 玩家角色：正交俯视相机、移动输入、冲刺、武器组件

#pragma once

#include "CoreMinimal.h"
#include "PRCharacterBase.h"
#include "PRPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UPRWeaponComponent;
class UStaticMeshComponent;

UCLASS()
class POTATOROGUE_API APRPlayerCharacter : public APRCharacterBase
{
	GENERATED_BODY()

public:
	APRPlayerCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UFUNCTION(BlueprintCallable, Category = "PR|Weapon")
	UPRWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }

	/** 尝试冲刺 */
	void TryDash();

	/** 死亡：通知游戏模式进入结算 */
	virtual void HandleDeath(AActor* Killer) override;

	/** 视口屏幕中心的世界朝向（俯视相机映射） */
	FVector GetCameraForward2D() const;

protected:
	virtual void InitAbilityActorInfo() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PR|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PR|Camera")
	TObjectPtr<UCameraComponent> TopDownCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PR|Weapon")
	TObjectPtr<UPRWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PR|Visual")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	void MoveForward(float Value);
	void MoveRight(float Value);

	/** 缓存的输入向量（世界空间） */
	FVector2D MoveInput = FVector2D::ZeroVector;
};
