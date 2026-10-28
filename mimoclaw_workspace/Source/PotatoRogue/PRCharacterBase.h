// Copyright (c) PotatoRogue. MIT Licensed.
// 角色基类：GAS 装配、属性访问、自动索敌、死亡处理

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffectTypes.h"
#include "PRCharacterBase.generated.h"

class UPRAbilitySystemComponent;
class UPRAttributeSet;
class APRGameState;

UCLASS(Abstract)
class POTATOROGUE_API APRCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APRCharacterBase();

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintCallable, Category = "PR|GAS")
	UPRAbilitySystemComponent* GetPRAbilitySystemComponent() const { return AbilitySystemComponent; }

	// 属性快捷访问
	float GetAttributeValue(const FGameplayAttribute& Attribute) const;

	/** 自动索敌方向：最近敌人 -> 否则移动/朝向 */
	virtual FVector GetAimDirection() const;

	/** 找到射程内最近的敌人（无则返回空） */
	AActor* FindNearestEnemy(float MaxRange) const;

	// 生死
	UFUNCTION(BlueprintCallable, Category = "PR|Combat")
	bool IsDead() const { return bDead; }

	virtual void HandleDeath(AActor* Killer);

	/** 被打时回调（飘字/音效钩子） */
	virtual void NotifyDamageTaken(float DamageAmount);

	/** 冲刺无敌帧计时器句柄 */
	FTimerHandle DashTimerHandle;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** 初始化 GAS ActorInfo（子类在 PossessedBy 中调用） */
	virtual void InitAbilityActorInfo();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PR|GAS")
	TObjectPtr<UPRAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UPRAttributeSet> AttributeSet;

	bool bDead = false;
};
