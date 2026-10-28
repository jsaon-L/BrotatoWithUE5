// Copyright (c) PotatoRogue. MIT Licensed.
// 掉落物：金币 / 经验 / 治疗包（含拾取范围磁吸）

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PRGameTypes.h"
#include "PRPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class APRPlayerCharacter;

UCLASS()
class POTATOROGUE_API APRPickup : public AActor
{
	GENERATED_BODY()

public:
	APRPickup();

	/** 初始化奖励内容 */
	void InitAward(EPRAwardType InType, int32 InAmount);

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PR|Components")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PR|Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(BlueprintReadOnly, Category = "PR|Award")
	EPRAwardType AwardType = EPRAwardType::Coin;

	UPROPERTY(BlueprintReadOnly, Category = "PR|Award")
	int32 Amount = 1;

	/** 磁吸速度 */
	UPROPERTY(EditDefaultsOnly, Category = "PR|Award")
	float MagnetSpeed = 1100.f;

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void ApplyAward(APRPlayerCharacter* Player);
	FLinearColor GetAwardColor() const;
};
