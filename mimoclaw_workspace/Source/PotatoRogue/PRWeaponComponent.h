// Copyright (c) PotatoRogue. MIT Licensed.
// 武器组件 + 武器运行时实例：负责把武器技能授予角色 ASC

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "PRWeaponComponent.generated.h"

class UPRWeaponDefinition;
class UPRGameplayAbility;
class APRCharacterBase;

/** 武器运行时实例（作为技能的 SourceObject） */
UCLASS()
class POTATOROGUE_API UPRWeaponInstance : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UPRWeaponDefinition> Definition;

	FGameplayAbilitySpecHandle AbilityHandle;

	/** 同名武器叠加数（伤害按 1 + 0.35*(n-1) 放大） */
	int32 StackCount = 1;

	float GetStackedDamage() const;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class POTATOROGUE_API UPRWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPRWeaponComponent();

	/** 添加武器（重复武器自动叠加） */
	UFUNCTION(BlueprintCallable, Category = "PR|Weapon")
	UPRWeaponInstance* AddWeapon(UPRWeaponDefinition* Definition);

	/** 移除武器（出售用） */
	UFUNCTION(BlueprintCallable, Category = "PR|Weapon")
	bool RemoveWeapon(UPRWeaponInstance* Instance);

	UFUNCTION(BlueprintCallable, Category = "PR|Weapon")
	int32 GetWeaponCount() const { return Weapons.Num(); }

	UFUNCTION(BlueprintCallable, Category = "PR|Weapon")
	const TArray<UPRWeaponInstance*>& GetWeapons() const { return Weapons; }

	/** 当前武器 DPS 估算（商店展示用） */
	UFUNCTION(BlueprintCallable, Category = "PR|Weapon")
	float GetEstimatedDPS() const;

protected:
	UPROPERTY()
	TArray<TObjectPtr<UPRWeaponInstance>> Weapons;

	APRCharacterBase* GetOwnerCharacter() const;
};
