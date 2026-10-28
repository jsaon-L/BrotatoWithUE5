// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRGameTypes.h"
#include "PRDataAssets.h"

FText FPRShopOffer::GetDisplayName() const
{
	if (Item)
	{
		return Item->ItemName;
	}
	if (Weapon)
	{
		return Weapon->WeaponName;
	}
	return FText::GetEmpty();
}

FText FPRShopOffer::GetDescription() const
{
	if (Item)
	{
		return Item->Description;
	}
	if (Weapon)
	{
		return Weapon->Description;
	}
	return FText::GetEmpty();
}

namespace PRGameTypes
{
	FText StatToText(EPRStat Stat)
	{
		switch (Stat)
		{
		case EPRStat::MaxHealth:			return NSLOCTEXT("PR", "Stat_MaxHealth", "生命上限");
		case EPRStat::HealthRegen:			return NSLOCTEXT("PR", "Stat_HealthRegen", "生命回复");
		case EPRStat::DamagePct:			return NSLOCTEXT("PR", "Stat_Damage", "伤害");
		case EPRStat::MeleeDamagePct:		return NSLOCTEXT("PR", "Stat_MeleeDamage", "近战伤害");
		case EPRStat::RangedDamagePct:		return NSLOCTEXT("PR", "Stat_RangedDamage", "远程伤害");
		case EPRStat::AttackSpeedPct:		return NSLOCTEXT("PR", "Stat_AttackSpeed", "攻击速度");
		case EPRStat::CritChance:			return NSLOCTEXT("PR", "Stat_CritChance", "暴击率");
		case EPRStat::CritDamagePct:		return NSLOCTEXT("PR", "Stat_CritDamage", "暴击伤害");
		case EPRStat::Range:				return NSLOCTEXT("PR", "Stat_Range", "射程");
		case EPRStat::Armor:				return NSLOCTEXT("PR", "Stat_Armor", "护甲");
		case EPRStat::Dodge:				return NSLOCTEXT("PR", "Stat_Dodge", "闪避");
		case EPRStat::SpeedPct:				return NSLOCTEXT("PR", "Stat_Speed", "移动速度");
		case EPRStat::Luck:					return NSLOCTEXT("PR", "Stat_Luck", "幸运");
		case EPRStat::Harvesting:			return NSLOCTEXT("PR", "Stat_Harvesting", "收获");
		case EPRStat::EngineeringPct:		return NSLOCTEXT("PR", "Stat_Engineering", "工程");
		case EPRStat::PickupRange:			return NSLOCTEXT("PR", "Stat_PickupRange", "拾取范围");
		default:							return FText::GetEmpty();
		}
	}

	FText StatModToText(const FPRStatMod& Mod)
	{
		const bool bPercent =
			Mod.Stat == EPRStat::DamagePct || Mod.Stat == EPRStat::MeleeDamagePct ||
			Mod.Stat == EPRStat::RangedDamagePct || Mod.Stat == EPRStat::AttackSpeedPct ||
			Mod.Stat == EPRStat::CritChance || Mod.Stat == EPRStat::CritDamagePct ||
			Mod.Stat == EPRStat::Dodge || Mod.Stat == EPRStat::SpeedPct ||
			Mod.Stat == EPRStat::EngineeringPct;

		const FString Sign = Mod.Value >= 0.f ? TEXT("+") : TEXT("");
		return FText::FromString(FString::Printf(TEXT("%s%s%.0f%s"),
			*StatToText(Mod.Stat).ToString(), *Sign, Mod.Value, bPercent ? TEXT("%") : TEXT("")));
	}

	FLinearColor RarityToColor(EPRRarity Rarity)
	{
		switch (Rarity)
		{
		case EPRRarity::Common:		return FLinearColor(0.8f, 0.8f, 0.8f);
		case EPRRarity::Uncommon:	return FLinearColor(0.2f, 0.9f, 0.3f);
		case EPRRarity::Rare:		return FLinearColor(0.2f, 0.5f, 1.0f);
		case EPRRarity::Epic:		return FLinearColor(0.7f, 0.3f, 1.0f);
		case EPRRarity::Legendary:	return FLinearColor(1.0f, 0.65f, 0.1f);
		default:					return FLinearColor::White;
		}
	}

	float RarityToPriceScale(EPRRarity Rarity)
	{
		switch (Rarity)
		{
		case EPRRarity::Common:		return 1.0f;
		case EPRRarity::Uncommon:	return 1.4f;
		case EPRRarity::Rare:		return 2.0f;
		case EPRRarity::Epic:		return 3.0f;
		case EPRRarity::Legendary:	return 4.5f;
		default:					return 1.0f;
		}
	}
}
