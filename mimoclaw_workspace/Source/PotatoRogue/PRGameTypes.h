// Copyright (c) PotatoRogue. MIT Licensed.
// 游戏通用类型：属性枚举、商店报价、稀有度等

#pragma once

#include "CoreMinimal.h"
#include "PRGameTypes.generated.h"

class UPRItemDefinition;
class UPRWeaponDefinition;

/** 肉鸽角色属性（对应土豆兄弟的面板属性） */
UENUM(BlueprintType)
enum class EPRStat : uint8
{
	MaxHealth			UMETA(DisplayName = "生命上限"),
	HealthRegen			UMETA(DisplayName = "生命回复"),
	DamagePct			UMETA(DisplayName = "伤害 %"),
	MeleeDamagePct		UMETA(DisplayName = "近战伤害 %"),
	RangedDamagePct		UMETA(DisplayName = "远程伤害 %"),
	AttackSpeedPct		UMETA(DisplayName = "攻击速度 %"),
	CritChance			UMETA(DisplayName = "暴击率 %"),
	CritDamagePct		UMETA(DisplayName = "暴击伤害 %"),
	Range				UMETA(DisplayName = "射程"),
	Armor				UMETA(DisplayName = "护甲"),
	Dodge				UMETA(DisplayName = "闪避 %"),
	SpeedPct			UMETA(DisplayName = "移动速度 %"),
	Luck				UMETA(DisplayName = "幸运"),
	Harvesting			UMETA(DisplayName = "收获"),
	EngineeringPct		UMETA(DisplayName = "工程 %"),
	PickupRange			UMETA(DisplayName = "拾取范围")
};

/** 单条属性修改 */
USTRUCT(BlueprintType)
struct FPRStatMod
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR")
	EPRStat Stat = EPRStat::MaxHealth;

	/** 百分比属性以百分点计（10 表示 +10%），绝对值属性直接相加 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PR")
	float Value = 0.f;

	FPRStatMod() = default;
	FPRStatMod(EPRStat InStat, float InValue) : Stat(InStat), Value(InValue) {}
};

/** 武器槽位类型 */
UENUM(BlueprintType)
enum class EPRWeaponSlot : uint8
{
	Melee	UMETA(DisplayName = "近战"),
	Ranged	UMETA(DisplayName = "远程")
};

/** 稀有度（影响商店权重与价格） */
UENUM(BlueprintType)
enum class EPRRarity : uint8
{
	Common		UMETA(DisplayName = "普通"),
	Uncommon	UMETA(DisplayName = "优秀"),
	Rare		UMETA(DisplayName = "稀有"),
	Epic		UMETA(DisplayName = "史诗"),
	Legendary	UMETA(DisplayName = "传说")
};

/** 游戏阶段（波次 / 商店 / 升级 / 结束） */
UENUM(BlueprintType)
enum class EPRGamePhase : uint8
{
	Preparation,
	Wave,
	Shop,
	LevelUp,
	GameOver
};

/** 掉落物奖励类型 */
UENUM(BlueprintType)
enum class EPRAwardType : uint8
{
	Coin,
	Experience,
	Health
};

/** 商店单个格子的报价（物品或武器二选一） */
USTRUCT(BlueprintType)
struct FPRShopOffer
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PR")
	TObjectPtr<UPRItemDefinition> Item = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "PR")
	TObjectPtr<UPRWeaponDefinition> Weapon = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "PR")
	int32 Price = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PR")
	bool bSold = false;

	bool IsEmpty() const { return !Item && !Weapon; }
	FText GetDisplayName() const;
	FText GetDescription() const;
};

/** 常用文本工具 */
namespace PRGameTypes
{
	FText StatToText(EPRStat Stat);
	FText StatModToText(const FPRStatMod& Mod);
	FLinearColor RarityToColor(EPRRarity Rarity);
	float RarityToPriceScale(EPRRarity Rarity);
}
