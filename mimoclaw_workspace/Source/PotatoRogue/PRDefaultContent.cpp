// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRDefaultContent.h"
#include "PRDataAssets.h"
#include "PRWeaponAbilities.h"
#include "PRProjectile.h"
#include <initializer_list>

namespace
{
	UPRItemDefinition* MakeItem(UObject* Outer, const TCHAR* Name, const TCHAR* Desc,
		EPRRarity Rarity, int32 Price, std::initializer_list<FPRStatMod> Mods)
	{
		UPRItemDefinition* Item = NewObject<UPRItemDefinition>(Outer, NAME_None, RF_Transient);
		Item->ItemName = FText::FromString(Name);
		Item->Description = FText::FromString(Desc);
		Item->Rarity = Rarity;
		Item->BasePrice = Price;
		for (const FPRStatMod& Mod : Mods)
		{
			Item->StatMods.Add(Mod);
		}
		return Item;
	}
}

namespace PRDefaultContent
{
	TArray<UPRItemDefinition*> CreateItems(UObject* Outer)
	{
		TArray<UPRItemDefinition*> Items;
		Items.Add(MakeItem(Outer, TEXT("蛋白质粉"), TEXT("生命上限 +8"),
			EPRRarity::Common, 14, { FPRStatMod(EPRStat::MaxHealth, 8.f) }));

		Items.Add(MakeItem(Outer, TEXT("咖啡"), TEXT("攻击速度 +8%"),
			EPRRarity::Common, 16, { FPRStatMod(EPRStat::AttackSpeedPct, 8.f) }));

		Items.Add(MakeItem(Outer, TEXT("绷带"), TEXT("生命回复 +1.5"),
			EPRRarity::Uncommon, 22, { FPRStatMod(EPRStat::HealthRegen, 1.5f) }));

		Items.Add(MakeItem(Outer, TEXT("望远镜"), TEXT("射程 +60"),
			EPRRarity::Common, 15, { FPRStatMod(EPRStat::Range, 60.f) }));

		Items.Add(MakeItem(Outer, TEXT("板甲"), TEXT("护甲 +4"),
			EPRRarity::Uncommon, 26, { FPRStatMod(EPRStat::Armor, 4.f) }));

		Items.Add(MakeItem(Outer, TEXT("跑鞋"), TEXT("移动速度 +6%"),
			EPRRarity::Common, 15, { FPRStatMod(EPRStat::SpeedPct, 6.f) }));

		Items.Add(MakeItem(Outer, TEXT("四叶草"), TEXT("幸运 +10"),
			EPRRarity::Uncommon, 24, { FPRStatMod(EPRStat::Luck, 10.f) }));

		Items.Add(MakeItem(Outer, TEXT("磨刀石"), TEXT("暴击伤害 +15%"),
			EPRRarity::Uncommon, 25, { FPRStatMod(EPRStat::CritDamagePct, 15.f) }));

		Items.Add(MakeItem(Outer, TEXT("战术手套"), TEXT("暴击率 +5%"),
			EPRRarity::Rare, 38, { FPRStatMod(EPRStat::CritChance, 5.f) }));

		Items.Add(MakeItem(Outer, TEXT("狂战斧"), TEXT("伤害 +12%"),
			EPRRarity::Rare, 42, { FPRStatMod(EPRStat::DamagePct, 12.f) }));

		Items.Add(MakeItem(Outer, TEXT("磁铁"), TEXT("拾取范围 +150"),
			EPRRarity::Common, 12, { FPRStatMod(EPRStat::PickupRange, 150.f) }));

		Items.Add(MakeItem(Outer, TEXT("收割镰刀"), TEXT("收获 +8"),
			EPRRarity::Rare, 40, { FPRStatMod(EPRStat::Harvesting, 8.f) }));

		Items.Add(MakeItem(Outer, TEXT("工程图纸"), TEXT("工程 +10%"),
			EPRRarity::Rare, 36, { FPRStatMod(EPRStat::EngineeringPct, 10.f) }));

		Items.Add(MakeItem(Outer, TEXT("蛮王指环"), TEXT("近战伤害 +18%"),
			EPRRarity::Epic, 55, { FPRStatMod(EPRStat::MeleeDamagePct, 18.f) }));

		Items.Add(MakeItem(Outer, TEXT("狙击镜片"), TEXT("远程伤害 +18%"),
			EPRRarity::Epic, 55, { FPRStatMod(EPRStat::RangedDamagePct, 18.f) }));

		return Items;
	}

	TArray<UPRWeaponDefinition*> CreateWeapons(UObject* Outer)
	{
		TArray<UPRWeaponDefinition*> Weapons;

		auto MakeWeapon = [Outer](const TCHAR* Name, const TCHAR* Desc, EPRWeaponSlot Slot,
			EPRRarity Rarity, float Damage, float Cooldown, float Range, int32 Price,
			int32 ProjectileCount = 1, float Spread = 6.f, float MeleeArc = 70.f) -> UPRWeaponDefinition*
		{
			UPRWeaponDefinition* Def = NewObject<UPRWeaponDefinition>(Outer, NAME_None, RF_Transient);
			Def->WeaponName = FText::FromString(Name);
			Def->Description = FText::FromString(Desc);
			Def->SlotType = Slot;
			Def->Rarity = Rarity;
			Def->Damage = Damage;
			Def->Cooldown = Cooldown;
			Def->Range = Range;
			Def->BasePrice = Price;
			Def->ProjectileCount = ProjectileCount;
			Def->SpreadAngle = Spread;
			Def->MeleeArcDegrees = MeleeArc;
			Def->ProjectileClass = APRProjectile::StaticClass();
			Def->AbilityClass = (Slot == EPRWeaponSlot::Ranged)
				? UPRGameplayAbility_RangedAttack::StaticClass()
				: UPRGameplayAbility_MeleeAttack::StaticClass();
			return Def;
		};

		// 远程
		UPRWeaponDefinition* Pistol = MakeWeapon(TEXT("手枪"), TEXT("可靠的单发手枪"),
			EPRWeaponSlot::Ranged, EPRRarity::Common, 12.f, 1.1f, 420.f, 18);
		Pistol->Color = FLinearColor(1.f, 0.85f, 0.2f);
		Weapons.Add(Pistol);

		UPRWeaponDefinition* SMG = MakeWeapon(TEXT("冲锋枪"), TEXT("射速极快，单发较弱"),
			EPRWeaponSlot::Ranged, EPRRarity::Uncommon, 6.f, 0.42f, 360.f, 30);
		SMG->Color = FLinearColor(0.3f, 1.f, 0.5f);
		Weapons.Add(SMG);

		UPRWeaponDefinition* Shotgun = MakeWeapon(TEXT("霰弹枪"), TEXT("一次发射 3 颗弹丸"),
			EPRWeaponSlot::Ranged, EPRRarity::Rare, 9.f, 1.35f, 330.f, 45, 3, 16.f);
		Shotgun->Color = FLinearColor(1.f, 0.45f, 0.15f);
		Weapons.Add(Shotgun);

		UPRWeaponDefinition* Rifle = MakeWeapon(TEXT("狙击步枪"), TEXT("高伤害远射程"),
			EPRWeaponSlot::Ranged, EPRRarity::Epic, 30.f, 1.9f, 720.f, 62);
		Rifle->Color = FLinearColor(0.4f, 0.7f, 1.f);
		Weapons.Add(Rifle);

		// 近战
		UPRWeaponDefinition* Sword = MakeWeapon(TEXT("短剑"), TEXT("快速挥砍"),
			EPRWeaponSlot::Melee, EPRRarity::Common, 14.f, 0.95f, 190.f, 20, 1, 0.f, 80.f);
		Sword->Color = FLinearColor(0.9f, 0.9f, 1.f);
		Weapons.Add(Sword);

		UPRWeaponDefinition* Spear = MakeWeapon(TEXT("长枪"), TEXT("攻击距离远，扇形窄"),
			EPRWeaponSlot::Melee, EPRRarity::Uncommon, 18.f, 1.2f, 280.f, 32, 1, 0.f, 45.f);
		Spear->Color = FLinearColor(0.85f, 0.7f, 0.4f);
		Weapons.Add(Spear);

		UPRWeaponDefinition* Hammer = MakeWeapon(TEXT("战锤"), TEXT("慢速大范围横扫"),
			EPRWeaponSlot::Melee, EPRRarity::Rare, 32.f, 1.7f, 230.f, 48, 1, 0.f, 120.f);
		Hammer->Color = FLinearColor(1.f, 0.5f, 0.7f);
		Weapons.Add(Hammer);

		return Weapons;
	}

	TArray<UPREnemyDefinition*> CreateEnemies(UObject* Outer)
	{
		TArray<UPREnemyDefinition*> Enemies;

		auto MakeEnemy = [Outer](const TCHAR* Name, float Health, float Speed, float Damage,
			int32 XP, int32 Coins, int32 MinWave, float Weight, float Scale, FLinearColor Color) -> UPREnemyDefinition*
		{
			UPREnemyDefinition* Def = NewObject<UPREnemyDefinition>(Outer, NAME_None, RF_Transient);
			Def->EnemyName = FText::FromString(Name);
			Def->MaxHealth = Health;
			Def->MoveSpeed = Speed;
			Def->ContactDamage = Damage;
			Def->XPReward = XP;
			Def->CoinReward = Coins;
			Def->MinWave = MinWave;
			Def->SpawnWeight = Weight;
			Def->Scale = Scale;
			Def->BodyColor = Color;
			return Def;
		};

		Enemies.Add(MakeEnemy(TEXT("小恶魔"), 22.f, 300.f, 6.f, 3, 1, 1, 5.f, 0.6f,
			FLinearColor(0.85f, 0.25f, 0.25f)));
		Enemies.Add(MakeEnemy(TEXT("爬行者"), 34.f, 220.f, 9.f, 4, 2, 2, 3.5f, 0.8f,
			FLinearColor(0.6f, 0.8f, 0.3f)));
		Enemies.Add(MakeEnemy(TEXT("重装兽"), 95.f, 150.f, 16.f, 8, 4, 4, 2.f, 1.25f,
			FLinearColor(0.5f, 0.35f, 0.75f)));
		Enemies.Add(MakeEnemy(TEXT("狂暴者"), 55.f, 380.f, 12.f, 7, 3, 6, 1.6f, 1.0f,
			FLinearColor(1.f, 0.55f, 0.15f)));
		Enemies.Add(MakeEnemy(TEXT("首领·石巨人"), 420.f, 130.f, 28.f, 30, 25, 5, 0.35f, 1.9f,
			FLinearColor(0.35f, 0.35f, 0.45f)));

		return Enemies;
	}
}
