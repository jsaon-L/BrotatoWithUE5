// Copyright (c) PotatoRogue. MIT Licensed.
// 敌人：追击 AI + 接触伤害 + 波次属性缩放

#pragma once

#include "CoreMinimal.h"
#include "PRCharacterBase.h"
#include "PREnemyCharacter.generated.h"

class UPREnemyDefinition;
class UWidgetComponent;
class UStaticMeshComponent;

UCLASS()
class POTATOROGUE_API APREnemyCharacter : public APRCharacterBase
{
	GENERATED_BODY()

public:
	APREnemyCharacter();

	/** 生成后立即调用：按定义 + 波次初始化属性 */
	void InitFromDefinition(UPREnemyDefinition* InDefinition, int32 WaveNumber);

	virtual void Tick(float DeltaSeconds) override;
	virtual void HandleDeath(AActor* Killer) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(BlueprintReadOnly, Category = "PR|Enemy")
	TObjectPtr<UPREnemyDefinition> Definition;

	int32 GetXPReward() const;
	int32 GetCoinReward() const;

protected:
	virtual void InitAbilityActorInfo() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PR|Visual")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PR|UI")
	TObjectPtr<UWidgetComponent> HealthBarComp;

	/** 追击目标（玩家） */
	TWeakObjectPtr<APRCharacterBase> TargetPlayer;

	/** 接触伤害冷却 */
	float AttackCooldownRemaining = 0.f;

	void UpdateHealthBar() const;
};
