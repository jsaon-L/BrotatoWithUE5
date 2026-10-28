// Copyright (c) PotatoRogue. MIT Licensed.
// 弹丸：GAS 伤害结算的投射物载体

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PRProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;
class UAbilitySystemComponent;

UCLASS()
class POTATOROGUE_API APRProjectile : public AActor
{
	GENERATED_BODY()

public:
	APRProjectile();

	/** 生成后由技能调用初始化 */
	void InitProjectile(UAbilitySystemComponent* InSourceASC, float InDamage,
		const FVector& Direction, float Speed, const FLinearColor& Color);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PR|Components")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PR|Components")
	TObjectPtr<UProjectileMovementComponent> MovementComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PR|Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	/** 伤害来源 ASC（技能持有者） */
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> SourceASC;

	float Damage = 10.f;
	float LifeSpanSeconds = 4.f;

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
