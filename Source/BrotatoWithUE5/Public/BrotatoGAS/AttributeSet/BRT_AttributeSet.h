// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "BRT_AttributeSet.generated.h"


#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class UBRT_AttributeSet : public UAttributeSet
{
	GENERATED_BODY()
public:
	
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	
	
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(ThisClass, Health);
	
	//主属性
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData MaxHp;             // 最大生命 | 单位: 点 | 唯一非 0 初始值；每次升级 +1
	ATTRIBUTE_ACCESSORS(ThisClass, MaxHp);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData HpRegeneration;    // 生命回复 | 单位: 点 | 决定回血周期，每次回 1
	ATTRIBUTE_ACCESSORS(ThisClass, HpRegeneration);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Lifesteal;         // 吸血 | 单位: % | 命中触发概率，回固定 1 HP
	ATTRIBUTE_ACCESSORS(ThisClass, Lifesteal);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData PercentDamage;     // 伤害% | 单位: % | 唯一全局伤害乘区
	ATTRIBUTE_ACCESSORS(ThisClass, PercentDamage);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData MeleeDamage;       // 近战伤害 | 单位: 点 | 仅对声明了 scaling 的武器生效
	ATTRIBUTE_ACCESSORS(ThisClass, MeleeDamage);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData RangedDamage;      // 远程伤害 | 单位: 点 | 同上
	ATTRIBUTE_ACCESSORS(ThisClass, RangedDamage);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData ElementalDamage;   // 元素伤害 | 单位: 点 | 同上 + 燃烧伤害加成
	ATTRIBUTE_ACCESSORS(ThisClass, ElementalDamage);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData AttackSpeed;       // 攻击速度 | 单位: % | 缩短武器冷却
	ATTRIBUTE_ACCESSORS(ThisClass, AttackSpeed);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData CritChance;        // 暴击率 | 单位: % | 加到武器基础暴击率
	ATTRIBUTE_ACCESSORS(ThisClass, CritChance);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Engineering;       // 工程 | 单位: 点 | 炮塔/工具类武器的伤害成长源
	ATTRIBUTE_ACCESSORS(ThisClass, Engineering);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Range;             // 射程 | 单位: px | 近战吃一半，远程吃全额
	ATTRIBUTE_ACCESSORS(ThisClass, Range);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Armor;             // 护甲 | 单位: 点 | 玩家侧乘法减伤（无上限，渐近 0）
	ATTRIBUTE_ACCESSORS(ThisClass, Armor);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Dodge;             // 闪避 | 单位: % | 硬上限 60%（dodge_cap）
	ATTRIBUTE_ACCESSORS(ThisClass, Dodge);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Speed;             // 移动速度 | 单位: % | 基础 450 px/s × (1+%)
	ATTRIBUTE_ACCESSORS(ThisClass, Speed);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Luck;              // 幸运 | 单位: % | 提升稀有度与掉落率
	ATTRIBUTE_ACCESSORS(ThisClass, Luck);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Harvesting;        // 收获 | 单位: 点 | 波末固定给材料+经验，每波自增 5%
	ATTRIBUTE_ACCESSORS(ThisClass, Harvesting);
	
	
	//次级属性
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData XpGain;                     // 经验获取 | 单位: % | 经验获取 %
	ATTRIBUTE_ACCESSORS(ThisClass, XpGain);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData ConsumableHeal;             // 消耗品额外回血 | 单位: 点 | 消耗品额外回血
	ATTRIBUTE_ACCESSORS(ThisClass, ConsumableHeal);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData BurningCooldownReduction;   // 燃烧冷却缩减 | 单位: % | 燃烧 tick 间隔缩减 %
	ATTRIBUTE_ACCESSORS(ThisClass, BurningCooldownReduction);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData BurningSpread;              // 燃烧扩散 | 单位: - | 燃烧扩散
	ATTRIBUTE_ACCESSORS(ThisClass, BurningSpread);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Piercing;                   // 穿透次数 | 单位: 次 | 穿透次数
	ATTRIBUTE_ACCESSORS(ThisClass, Piercing);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData PiercingDamage;             // 穿透伤害保留 | 单位: % | 穿透伤害保留 %
	ATTRIBUTE_ACCESSORS(ThisClass, PiercingDamage);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Bounce;                     // 弹射次数 | 单位: 次 | 弹射次数
	ATTRIBUTE_ACCESSORS(ThisClass, Bounce);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData BounceDamage;               // 弹射伤害保留 | 单位: % | 弹射伤害保留 %
	ATTRIBUTE_ACCESSORS(ThisClass, BounceDamage);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData PickupRange;                // 拾取半径 | 单位: % | 拾取半径 %
	ATTRIBUTE_ACCESSORS(ThisClass, PickupRange);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData ChanceDoubleGold;           // 材料翻倍概率 | 单位: % | 材料翻倍概率 %
	ATTRIBUTE_ACCESSORS(ThisClass, ChanceDoubleGold);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData HealWhenPickupGold;         // 拾取回血概率 | 单位: % | 拾取回血概率 %
	ATTRIBUTE_ACCESSORS(ThisClass, HealWhenPickupGold);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData ItemBoxGold;                // 开宝箱额外材料 | 单位: 点 | 开宝箱额外材料
	ATTRIBUTE_ACCESSORS(ThisClass, ItemBoxGold);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Knockback;                  // 击退加值 | 单位: 点 | 击退加值
	ATTRIBUTE_ACCESSORS(ThisClass, Knockback);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData HpCap;                      // 最大生命硬上限 | 单位: 点 | 默认 999999，最大生命硬上限
	ATTRIBUTE_ACCESSORS(ThisClass, HpCap);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData SpeedCap;                   // 速度属性硬上限 | 单位: % | 默认 999999，速度属性硬上限
	ATTRIBUTE_ACCESSORS(ThisClass, SpeedCap);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData LoseHpPerSecond;            // 每秒扣血 | 单位: 点 | 每秒扣血
	ATTRIBUTE_ACCESSORS(ThisClass, LoseHpPerSecond);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData MapSize;                    // 地图尺寸 | 单位: % | 地图尺寸 %
	ATTRIBUTE_ACCESSORS(ThisClass, MapSize);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData DodgeCap;                   // 闪避上限 | 单位: % | 默认 60，闪避上限 %
	ATTRIBUTE_ACCESSORS(ThisClass, DodgeCap);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData GoldDrops;                  // 材料掉落率 | 单位: % | 默认 100，材料掉落率基准（100 = 1.0）
	ATTRIBUTE_ACCESSORS(ThisClass, GoldDrops);


	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData ExplosionSize;              // 爆炸范围 | 单位: % | 爆炸范围 %
	ATTRIBUTE_ACCESSORS(ThisClass, ExplosionSize);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData ExplosionDamage;            // 爆炸伤害 | 单位: % | 爆炸伤害 %
	ATTRIBUTE_ACCESSORS(ThisClass, ExplosionDamage);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData DamageAgainstBosses;        // 对 Boss 伤害 | 单位: % | 对 Boss 伤害 %
	ATTRIBUTE_ACCESSORS(ThisClass, DamageAgainstBosses);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData GiantCritDamage;            // 巨人腰带暴击追伤 | 单位: % | 巨人腰带：暴击追伤（按目标当前血量 %）
	ATTRIBUTE_ACCESSORS(ThisClass, GiantCritDamage);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData WeaponSlot;                 // 武器槽位 | 单位: 个 | 默认 6，商店槽位判断与 UI 显示 x/6 读此值
	ATTRIBUTE_ACCESSORS(ThisClass, WeaponSlot);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData ItemsPrice;                 // 物价（物品） | 单位: % | 物价 %
	ATTRIBUTE_ACCESSORS(ThisClass, ItemsPrice);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData WeaponsPrice;               // 物价（武器） | 单位: % | 物价 %
	ATTRIBUTE_ACCESSORS(ThisClass, WeaponsPrice);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData HarvestingGrowth;           // 收获每波成长 | 单位: % | 默认 5，收获每波成长 %
	ATTRIBUTE_ACCESSORS(ThisClass, HarvestingGrowth);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData HitProtection;              // 免伤次数 | 单位: 次 | 免伤次数
	ATTRIBUTE_ACCESSORS(ThisClass, HitProtection);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Inflation;                  // 难度通胀 | 单位: - | 难度通胀（进入定价公式）
	ATTRIBUTE_ACCESSORS(ThisClass, Inflation);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Accuracy;                   // 精度加值 | 单位: % | 精度加值 %
	ATTRIBUTE_ACCESSORS(ThisClass, Accuracy);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Projectiles;                // 弹数加值 | 单位: 发 | 弹数加值
	ATTRIBUTE_ACCESSORS(ThisClass, Projectiles);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData FreeRerolls;                // 免费重掷次数 | 单位: 次 | 免费重掷次数
	ATTRIBUTE_ACCESSORS(ThisClass, FreeRerolls);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData InstantGoldAttracting;      // 材料立即吸附概率 | 单位: % | 材料立即吸附概率 %
	ATTRIBUTE_ACCESSORS(ThisClass, InstantGoldAttracting);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData RecyclingGains;             // 回收率加成 | 单位: % | 回收率加成 %
	ATTRIBUTE_ACCESSORS(ThisClass, RecyclingGains);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData DiffGoldDrops;              // 难度掉落修正 | 单位: % | 难度掉落修正
	ATTRIBUTE_ACCESSORS(ThisClass, DiffGoldDrops);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData NeutralGoldDrops;           // 中立掉落量 | 单位: % | 中立/敌人掉落量 %
	ATTRIBUTE_ACCESSORS(ThisClass, NeutralGoldDrops);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData EnemyGoldDrops;             // 敌人掉落量 | 单位: % | 中立/敌人掉落量 %
	ATTRIBUTE_ACCESSORS(ThisClass, EnemyGoldDrops);
	
	//敌人属性
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData NumberOfEnemies;            // 敌人数量 | 单位: % | 敌人数量 %
	ATTRIBUTE_ACCESSORS(ThisClass, NumberOfEnemies);
	
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData BossStrength;               // Boss 强度 | 单位: % | Boss 强度
	ATTRIBUTE_ACCESSORS(ThisClass, BossStrength);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData EnemyHealth;                // 敌人生命 | 单位: % | 敌人属性 %
	ATTRIBUTE_ACCESSORS(ThisClass, EnemyHealth);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData EnemyDamage;                // 敌人伤害 | 单位: % | 敌人属性 %
	ATTRIBUTE_ACCESSORS(ThisClass, EnemyDamage);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData EnemySpeed;                 // 敌人速度 | 单位: % | 敌人属性 %
	ATTRIBUTE_ACCESSORS(ThisClass, EnemySpeed);
	
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Pacifist;                   // 角色专属 | 单位: - | 角色专属（pacifist）
	ATTRIBUTE_ACCESSORS(ThisClass, Pacifist);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Cryptid;                    // 角色专属 | 单位: - | 角色专属（cryptid）
	ATTRIBUTE_ACCESSORS(ThisClass, Cryptid);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Torture;                    // 角色专属 | 单位: - | 角色专属（torture）
	ATTRIBUTE_ACCESSORS(ThisClass, Torture);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Trees;                      // 树的数量 | 单位: 棵 | 树的数量
	ATTRIBUTE_ACCESSORS(ThisClass, Trees);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData TreesStartWave;             // 树波首全刷 | 单位: 波 | 波首全刷
	ATTRIBUTE_ACCESSORS(ThisClass, TreesStartWave);
	UPROPERTY(BlueprintReadOnly) FGameplayAttributeData GainPctGoldStartWave;       // 波首材料复利 | 单位: % | 波首材料复利 %
	ATTRIBUTE_ACCESSORS(ThisClass, GainPctGoldStartWave);

	// 非 effects，RunData 直接字段，不走 DECLARE_ATTRIBUTE
	// RunData.gold          = 30  // 初始材料 | 单位: 点 | 开局初始材料（debug_service.gd:4 starting_gold = 30，run_data.gd:155）
	// RunData.max_weapons   = 6   // 最大武器数 | 单位: 个 | init_run 写入（run_data.gd:148），随存档保存；与 weapon_slot 相加用于 has_max_weapons()（run_data.gd:622-623），UE 以 weapon_slot 为唯一权威，不要两处叠加
		

};