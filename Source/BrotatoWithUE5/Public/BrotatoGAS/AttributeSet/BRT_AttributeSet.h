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

// 一个宏相当于这两行定义
// UPROPERTY(BlueprintReadOnly)FGameplayAttributeData Health;
// ATTRIBUTE_ACCESSORS(ThisClass,Health);
#define DECLARE_ATTRIBUTE(ClassName, PropertyName, ...) \
    UPROPERTY(BlueprintReadOnly, ##__VA_ARGS__) \
    FGameplayAttributeData PropertyName; \
    ATTRIBUTE_ACCESSORS(ClassName, PropertyName)

UCLASS()
class UBRT_AttributeSet : public UAttributeSet
{
	GENERATED_BODY()
public:
	
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	
	
	DECLARE_ATTRIBUTE(ThisClass,Health);
	
	//主属性
	DECLARE_ATTRIBUTE(ThisClass, MaxHp);             // 最大生命 | 单位: 点 | 唯一非 0 初始值；每次升级 +1
	DECLARE_ATTRIBUTE(ThisClass, HpRegeneration);    // 生命回复 | 单位: 点 | 决定回血周期，每次回 1
	DECLARE_ATTRIBUTE(ThisClass, Lifesteal);         // 吸血 | 单位: % | 命中触发概率，回固定 1 HP
	DECLARE_ATTRIBUTE(ThisClass, PercentDamage);     // 伤害% | 单位: % | 唯一全局伤害乘区
	DECLARE_ATTRIBUTE(ThisClass, MeleeDamage);       // 近战伤害 | 单位: 点 | 仅对声明了 scaling 的武器生效
	DECLARE_ATTRIBUTE(ThisClass, RangedDamage);      // 远程伤害 | 单位: 点 | 同上
	DECLARE_ATTRIBUTE(ThisClass, ElementalDamage);   // 元素伤害 | 单位: 点 | 同上 + 燃烧伤害加成
	DECLARE_ATTRIBUTE(ThisClass, AttackSpeed);       // 攻击速度 | 单位: % | 缩短武器冷却
	DECLARE_ATTRIBUTE(ThisClass, CritChance);        // 暴击率 | 单位: % | 加到武器基础暴击率
	DECLARE_ATTRIBUTE(ThisClass, Engineering);       // 工程 | 单位: 点 | 炮塔/工具类武器的伤害成长源
	DECLARE_ATTRIBUTE(ThisClass, Range);             // 射程 | 单位: px | 近战吃一半，远程吃全额
	DECLARE_ATTRIBUTE(ThisClass, Armor);             // 护甲 | 单位: 点 | 玩家侧乘法减伤（无上限，渐近 0）
	DECLARE_ATTRIBUTE(ThisClass, Dodge);             // 闪避 | 单位: % | 硬上限 60%（dodge_cap）
	DECLARE_ATTRIBUTE(ThisClass, Speed);             // 移动速度 | 单位: % | 基础 450 px/s × (1+%)
	DECLARE_ATTRIBUTE(ThisClass, Luck);              // 幸运 | 单位: % | 提升稀有度与掉落率
	DECLARE_ATTRIBUTE(ThisClass, Harvesting);        // 收获 | 单位: 点 | 波末固定给材料+经验，每波自增 5%
	
	
	//次级属性
	DECLARE_ATTRIBUTE(ThisClass, XpGain);                     // 经验获取 | 单位: % | 经验获取 %
	DECLARE_ATTRIBUTE(ThisClass, ConsumableHeal);             // 消耗品额外回血 | 单位: 点 | 消耗品额外回血
	DECLARE_ATTRIBUTE(ThisClass, BurningCooldownReduction);   // 燃烧冷却缩减 | 单位: % | 燃烧 tick 间隔缩减 %
	DECLARE_ATTRIBUTE(ThisClass, BurningSpread);              // 燃烧扩散 | 单位: - | 燃烧扩散
	DECLARE_ATTRIBUTE(ThisClass, Piercing);                   // 穿透次数 | 单位: 次 | 穿透次数
	DECLARE_ATTRIBUTE(ThisClass, PiercingDamage);             // 穿透伤害保留 | 单位: % | 穿透伤害保留 %
	DECLARE_ATTRIBUTE(ThisClass, Bounce);                     // 弹射次数 | 单位: 次 | 弹射次数
	DECLARE_ATTRIBUTE(ThisClass, BounceDamage);               // 弹射伤害保留 | 单位: % | 弹射伤害保留 %
	DECLARE_ATTRIBUTE(ThisClass, PickupRange);                // 拾取半径 | 单位: % | 拾取半径 %
	DECLARE_ATTRIBUTE(ThisClass, ChanceDoubleGold);           // 材料翻倍概率 | 单位: % | 材料翻倍概率 %
	DECLARE_ATTRIBUTE(ThisClass, HealWhenPickupGold);         // 拾取回血概率 | 单位: % | 拾取回血概率 %
	DECLARE_ATTRIBUTE(ThisClass, ItemBoxGold);                // 开宝箱额外材料 | 单位: 点 | 开宝箱额外材料
	DECLARE_ATTRIBUTE(ThisClass, Knockback);                  // 击退加值 | 单位: 点 | 击退加值
	DECLARE_ATTRIBUTE(ThisClass, HpCap);                      // 最大生命硬上限 | 单位: 点 | 默认 999999，最大生命硬上限
	DECLARE_ATTRIBUTE(ThisClass, SpeedCap);                   // 速度属性硬上限 | 单位: % | 默认 999999，速度属性硬上限
	DECLARE_ATTRIBUTE(ThisClass, LoseHpPerSecond);            // 每秒扣血 | 单位: 点 | 每秒扣血
	DECLARE_ATTRIBUTE(ThisClass, MapSize);                    // 地图尺寸 | 单位: % | 地图尺寸 %
	DECLARE_ATTRIBUTE(ThisClass, DodgeCap);                   // 闪避上限 | 单位: % | 默认 60，闪避上限 %
	DECLARE_ATTRIBUTE(ThisClass, GoldDrops);                  // 材料掉落率 | 单位: % | 默认 100，材料掉落率基准（100 = 1.0）


	DECLARE_ATTRIBUTE(ThisClass, ExplosionSize);              // 爆炸范围 | 单位: % | 爆炸范围 %
	DECLARE_ATTRIBUTE(ThisClass, ExplosionDamage);            // 爆炸伤害 | 单位: % | 爆炸伤害 %
	DECLARE_ATTRIBUTE(ThisClass, DamageAgainstBosses);        // 对 Boss 伤害 | 单位: % | 对 Boss 伤害 %
	DECLARE_ATTRIBUTE(ThisClass, GiantCritDamage);            // 巨人腰带暴击追伤 | 单位: % | 巨人腰带：暴击追伤（按目标当前血量 %）
	DECLARE_ATTRIBUTE(ThisClass, WeaponSlot);                 // 武器槽位 | 单位: 个 | 默认 6，商店槽位判断与 UI 显示 x/6 读此值
	DECLARE_ATTRIBUTE(ThisClass, ItemsPrice);                 // 物价（物品） | 单位: % | 物价 %
	DECLARE_ATTRIBUTE(ThisClass, WeaponsPrice);               // 物价（武器） | 单位: % | 物价 %
	DECLARE_ATTRIBUTE(ThisClass, HarvestingGrowth);           // 收获每波成长 | 单位: % | 默认 5，收获每波成长 %
	DECLARE_ATTRIBUTE(ThisClass, HitProtection);              // 免伤次数 | 单位: 次 | 免伤次数
	DECLARE_ATTRIBUTE(ThisClass, Inflation);                  // 难度通胀 | 单位: - | 难度通胀（进入定价公式）
	DECLARE_ATTRIBUTE(ThisClass, Accuracy);                   // 精度加值 | 单位: % | 精度加值 %
	DECLARE_ATTRIBUTE(ThisClass, Projectiles);                // 弹数加值 | 单位: 发 | 弹数加值
	DECLARE_ATTRIBUTE(ThisClass, FreeRerolls);                // 免费重掷次数 | 单位: 次 | 免费重掷次数
	DECLARE_ATTRIBUTE(ThisClass, InstantGoldAttracting);      // 材料立即吸附概率 | 单位: % | 材料立即吸附概率 %
	DECLARE_ATTRIBUTE(ThisClass, RecyclingGains);             // 回收率加成 | 单位: % | 回收率加成 %
	DECLARE_ATTRIBUTE(ThisClass, DiffGoldDrops);              // 难度掉落修正 | 单位: % | 难度掉落修正
	DECLARE_ATTRIBUTE(ThisClass, NeutralGoldDrops);           // 中立掉落量 | 单位: % | 中立/敌人掉落量 %
	DECLARE_ATTRIBUTE(ThisClass, EnemyGoldDrops);             // 敌人掉落量 | 单位: % | 中立/敌人掉落量 %
	
	//敌人属性
	DECLARE_ATTRIBUTE(ThisClass, NumberOfEnemies);            // 敌人数量 | 单位: % | 敌人数量 %
	
	DECLARE_ATTRIBUTE(ThisClass, BossStrength);               // Boss 强度 | 单位: % | Boss 强度
	DECLARE_ATTRIBUTE(ThisClass, EnemyHealth);                // 敌人生命 | 单位: % | 敌人属性 %
	DECLARE_ATTRIBUTE(ThisClass, EnemyDamage);                // 敌人伤害 | 单位: % | 敌人属性 %
	DECLARE_ATTRIBUTE(ThisClass, EnemySpeed);                 // 敌人速度 | 单位: % | 敌人属性 %
	
	DECLARE_ATTRIBUTE(ThisClass, Pacifist);                   // 角色专属 | 单位: - | 角色专属（pacifist）
	DECLARE_ATTRIBUTE(ThisClass, Cryptid);                    // 角色专属 | 单位: - | 角色专属（cryptid）
	DECLARE_ATTRIBUTE(ThisClass, Torture);                    // 角色专属 | 单位: - | 角色专属（torture）
	DECLARE_ATTRIBUTE(ThisClass, Trees);                      // 树的数量 | 单位: 棵 | 树的数量
	DECLARE_ATTRIBUTE(ThisClass, TreesStartWave);             // 树波首全刷 | 单位: 波 | 波首全刷
	DECLARE_ATTRIBUTE(ThisClass, GainPctGoldStartWave);       // 波首材料复利 | 单位: % | 波首材料复利 %

	// 非 effects，RunData 直接字段，不走 DECLARE_ATTRIBUTE
	// RunData.gold          = 30  // 初始材料 | 单位: 点 | 开局初始材料（debug_service.gd:4 starting_gold = 30，run_data.gd:155）
	// RunData.max_weapons   = 6   // 最大武器数 | 单位: 个 | init_run 写入（run_data.gd:148），随存档保存；与 weapon_slot 相加用于 has_max_weapons()（run_data.gd:622-623），UE 以 weapon_slot 为唯一权威，不要两处叠加
		

};
 