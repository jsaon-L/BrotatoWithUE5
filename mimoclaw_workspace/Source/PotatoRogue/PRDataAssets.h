// Copyright (c) PotatoRogue. MIT Licensed.
// 数据资产定义：物品 / 武器 / 敌人（全部可用纯 C++ 构造，无需美术资源）

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PRGameTypes.h"
#include "PRDataAssets.generated.h"

class UPRGameplayAbility;
class APRProjectile;

// ---------------- 物品（被动道具） ----------------
UCLASS(BlueprintType)
class POTATOROGUE_API UPRItemDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	FText ItemName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	TArray<FPRStatMod> StatMods;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	EPRRarity Rarity = EPRRarity::Common;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	int32 BasePrice = 15;

	/** 可叠加数量上限 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	int32 MaxStacks = 1;
};

// ---------------- 武器 ----------------
UCLASS(BlueprintType)
class POTATOROGUE_API UPRWeaponDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	FText WeaponName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	EPRWeaponSlot SlotType = EPRWeaponSlot::Ranged;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	EPRRarity Rarity = EPRRarity::Common;

	/** 单发面板伤害 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR|Attack")
	float Damage = 10.f;

	/** 基础攻击间隔（秒），实际 = Cooldown / 攻速倍率 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR|Attack")
	float Cooldown = 1.f;

	/** 攻击距离（自动索敌半径） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR|Attack")
	float Range = 400.f;

	/** 远程：弹丸数量（霰弹枪 = 多发） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR|Attack")
	int32 ProjectileCount = 1;

	/** 远程：散射角度（度） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR|Attack")
	float SpreadAngle = 6.f;

	/** 近战：扇形攻击半角（度） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR|Attack")
	float MeleeArcDegrees = 70.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR|Attack")
	float ProjectileSpeed = 1800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	int32 BasePrice = 20;

	/** 技能类（默认：远程/近战自动攻击技能） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR|Ability")
	TSubclassOf<UPRGameplayAbility> AbilityClass;

	/** 弹丸类（远程武器） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR|Ability")
	TSubclassOf<APRProjectile> ProjectileClass;

	/** 弹丸/特效颜色（免美术资源时用作顶点色） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR|Visual")
	FLinearColor Color = FLinearColor(1.f, 0.85f, 0.2f);
};

// ---------------- 敌人 ----------------
UCLASS(BlueprintType)
class POTATOROGUE_API UPREnemyDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	FText EnemyName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	float MaxHealth = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	float MoveSpeed = 260.f;

	/** 接触伤害 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	float ContactDamage = 8.f;

	/** 接触攻击间隔（秒） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	float AttackInterval = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	float CapsuleRadius = 30.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	int32 XPReward = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	int32 CoinReward = 2;

	/** 最早出现的波次 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	int32 MinWave = 1;

	/** 刷新权重 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	float SpawnWeight = 1.f;

	/** 体型缩放 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	float Scale = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PR")
	FLinearColor BodyColor = FLinearColor(0.75f, 0.2f, 0.2f);
};
