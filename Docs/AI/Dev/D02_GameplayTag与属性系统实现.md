# D02 · GameplayTag 字典与属性系统实现

> **模块**：`Source/BrotatoGAS`（AttributeSets / Components / MMC）
> **里程碑**：M2 主属性 + Clamp + ExecCalc；M3 gain 乘区 + LinkedStats
> **上游规格**：`Docs/02` 全文、`Docs/09 §3 §4`

**本章解决一个核心问题**：Brotato 的三层属性模型
$$\text{FinalStat}(s)=\big(E[s]+T[s]+L[s]\big)\cdot\Big(1+\frac{E[\text{gain}\_s]}{100}\Big)$$
如何**无损**映射到 GAS 的 Aggregator
$$\text{CurrentValue}=\big(\text{Base}+\textstyle\sum\text{Additive}\big)\times\textstyle\prod\text{Multiplicative}$$

**结论：数学上完全等价**，只要把 `gain_*` 做成 Multiplicative 的 AttributeBased Modifier。详见 §4。

---

## 1. GameplayTag 字典

### 1.1 组织原则

| 命名空间 | 用途 | 声明方式 |
|---|---|---|
| `Brotato.Attribute.*` | Tag ↔ Attribute 映射、UI 查询、`scaling_stats` 引用 | ini + `DT_TagAttributeMap` |
| `Brotato.State.*` | 状态标记，驱动逻辑与表现 | ini |
| `Brotato.Effect.*` | GE 分类，用于**批量移除** | ini |
| `Brotato.Ability.*` | Ability 标签 / 阻断 / 事件触发 | ini |
| `Brotato.Cue.*` | GameplayCue 路径 | ini（必须与 Cue 资产路径一致） |
| `Brotato.Data.*` | SetByCaller 键 | ini |
| `Brotato.Rule.*` | 规则开关（对应原作 bool 型 effects） | ini |
| `Brotato.Hook.*` | HookRegistry 槽位键 | ini |
| `Brotato.Set.*` | 15 武器职业 | ini |

### 1.2 `Config/DefaultGameplayTags.ini`（骨架，实现时展开全部条目）

```ini
[/Script/GameplayTags.GameplayTagsSettings]
; ──────────── Attribute.Primary（16 主属性） ────────────
+GameplayTagList=(Tag="Brotato.Attribute.Primary.MaxHp")
+GameplayTagList=(Tag="Brotato.Attribute.Primary.HpRegeneration")
+GameplayTagList=(Tag="Brotato.Attribute.Primary.Lifesteal")
+GameplayTagList=(Tag="Brotato.Attribute.Primary.PercentDamage")
+GameplayTagList=(Tag="Brotato.Attribute.Primary.MeleeDamage")
+GameplayTagList=(Tag="Brotato.Attribute.Primary.RangedDamage")
+GameplayTagList=(Tag="Brotato.Attribute.Primary.ElementalDamage")
+GameplayTagList=(Tag="Brotato.Attribute.Primary.AttackSpeed")
+GameplayTagList=(Tag="Brotato.Attribute.Primary.CritChance")
+GameplayTagList=(Tag="Brotato.Attribute.Primary.Engineering")
+GameplayTagList=(Tag="Brotato.Attribute.Primary.Range")
+GameplayTagList=(Tag="Brotato.Attribute.Primary.Armor")
+GameplayTagList=(Tag="Brotato.Attribute.Primary.Dodge")
+GameplayTagList=(Tag="Brotato.Attribute.Primary.Speed")
+GameplayTagList=(Tag="Brotato.Attribute.Primary.Luck")
+GameplayTagList=(Tag="Brotato.Attribute.Primary.Harvesting")
; 特殊键（scaling_stats 用，不是 Attribute）
+GameplayTagList=(Tag="Brotato.Attribute.Special.Levels")

; ──────────── Attribute.Gain（20 个乘区源） ────────────
; 16 个同名 + 4 个额外
+GameplayTagList=(Tag="Brotato.Attribute.Gain.MaxHp")
; …（16 项，与 Primary 同名）
+GameplayTagList=(Tag="Brotato.Attribute.Gain.ExplosionDamage")
+GameplayTagList=(Tag="Brotato.Attribute.Gain.PiercingDamage")
+GameplayTagList=(Tag="Brotato.Attribute.Gain.BounceDamage")
+GameplayTagList=(Tag="Brotato.Attribute.Gain.DamageAgainstBosses")

; ──────────── Attribute.Secondary（见 §2.3 全表） ────────────
+GameplayTagList=(Tag="Brotato.Attribute.Secondary.XpGain")
+GameplayTagList=(Tag="Brotato.Attribute.Secondary.NumberOfEnemies")
; …

; ──────────── Attribute.Limit（Override 语义） ────────────
+GameplayTagList=(Tag="Brotato.Attribute.Limit.HpCap")
+GameplayTagList=(Tag="Brotato.Attribute.Limit.DodgeCap")
+GameplayTagList=(Tag="Brotato.Attribute.Limit.SpeedCap")
+GameplayTagList=(Tag="Brotato.Attribute.Limit.WeaponSlot")

; ──────────── State ────────────
+GameplayTagList=(Tag="Brotato.State.Invincible")
+GameplayTagList=(Tag="Brotato.State.Moving")
+GameplayTagList=(Tag="Brotato.State.NotMoving")
+GameplayTagList=(Tag="Brotato.State.BelowHalfHealth")
+GameplayTagList=(Tag="Brotato.State.Burning.Elemental")
+GameplayTagList=(Tag="Brotato.State.Burning.Engineering")
+GameplayTagList=(Tag="Brotato.State.Slowed")
+GameplayTagList=(Tag="Brotato.State.Dead")
+GameplayTagList=(Tag="Brotato.State.CleaningUp")
+GameplayTagList=(Tag="Brotato.State.Shooting")
+GameplayTagList=(Tag="Brotato.State.ManualAim")

; ──────────── Effect（GE 分类，批量移除用） ────────────
+GameplayTagList=(Tag="Brotato.Effect.Source.Item")
+GameplayTagList=(Tag="Brotato.Effect.Source.Character")
+GameplayTagList=(Tag="Brotato.Effect.Source.Upgrade")
+GameplayTagList=(Tag="Brotato.Effect.Source.Set")
+GameplayTagList=(Tag="Brotato.Effect.Source.Difficulty")
+GameplayTagList=(Tag="Brotato.Effect.Source.Consumable")
+GameplayTagList=(Tag="Brotato.Effect.Source.Conditional")
+GameplayTagList=(Tag="Brotato.Effect.Lifetime.Permanent")
+GameplayTagList=(Tag="Brotato.Effect.Lifetime.Wave")
+GameplayTagList=(Tag="Brotato.Effect.Lifetime.Linked")
+GameplayTagList=(Tag="Brotato.Effect.Stackable.OnHit")
+GameplayTagList=(Tag="Brotato.Effect.Stackable.OnDodge")
+GameplayTagList=(Tag="Brotato.Effect.Stackable.Periodic5s")

; ──────────── Ability ────────────
+GameplayTagList=(Tag="Brotato.Ability.WeaponAttack.Melee")
+GameplayTagList=(Tag="Brotato.Ability.WeaponAttack.Ranged")
+GameplayTagList=(Tag="Brotato.Ability.Passive.OnHit")
+GameplayTagList=(Tag="Brotato.Ability.Passive.OnKill")
+GameplayTagList=(Tag="Brotato.Ability.Passive.OnDodge")
+GameplayTagList=(Tag="Brotato.Ability.Passive.OnPickupGold")
+GameplayTagList=(Tag="Brotato.Ability.Passive.OnHeal")
+GameplayTagList=(Tag="Brotato.Ability.Passive.OnWaveEnd")
+GameplayTagList=(Tag="Brotato.Ability.Passive.OnConsumable")

; ──────────── Cue（路径必须与 Content/GAS/Cues 一致） ────────────
+GameplayTagList=(Tag="Brotato.Cue.Hit.Normal")
+GameplayTagList=(Tag="Brotato.Cue.Hit.Crit")
+GameplayTagList=(Tag="Brotato.Cue.Hit.Dodge")
+GameplayTagList=(Tag="Brotato.Cue.Hit.Nullified")
+GameplayTagList=(Tag="Brotato.Cue.Player.Damaged")
+GameplayTagList=(Tag="Brotato.Cue.Player.Healed")
+GameplayTagList=(Tag="Brotato.Cue.Player.LevelUp")
+GameplayTagList=(Tag="Brotato.Cue.Player.Died")
+GameplayTagList=(Tag="Brotato.Cue.Enemy.Damaged")
+GameplayTagList=(Tag="Brotato.Cue.Enemy.Died")
+GameplayTagList=(Tag="Brotato.Cue.Weapon.MeleeSwing")
+GameplayTagList=(Tag="Brotato.Cue.Weapon.RangedFire")
+GameplayTagList=(Tag="Brotato.Cue.Explosion")
+GameplayTagList=(Tag="Brotato.Cue.Burning")
+GameplayTagList=(Tag="Brotato.Cue.StatGained")
+GameplayTagList=(Tag="Brotato.Cue.StatLost")
+GameplayTagList=(Tag="Brotato.Cue.Harvested")
+GameplayTagList=(Tag="Brotato.Cue.Screenshake.Small")
+GameplayTagList=(Tag="Brotato.Cue.Screenshake.Large")

; ──────────── Data（SetByCaller 键） ────────────
+GameplayTagList=(Tag="Brotato.Data.Damage")
+GameplayTagList=(Tag="Brotato.Data.Healing")
+GameplayTagList=(Tag="Brotato.Data.CritChance")
+GameplayTagList=(Tag="Brotato.Data.CritDamage")
+GameplayTagList=(Tag="Brotato.Data.KnockbackAmount")
+GameplayTagList=(Tag="Brotato.Data.BurnDamage")
+GameplayTagList=(Tag="Brotato.Data.BurnDuration")
+GameplayTagList=(Tag="Brotato.Data.BypassInvincibility")
+GameplayTagList=(Tag="Brotato.Data.NotDodgeable")
+GameplayTagList=(Tag="Brotato.Data.NoArmor")

; ──────────── Rule（21 个开关，对应原作 bool effects） ────────────
+GameplayTagList=(Tag="Brotato.Rule.NoMeleeWeapons")
+GameplayTagList=(Tag="Brotato.Rule.NoRangedWeapons")
+GameplayTagList=(Tag="Brotato.Rule.NoMinRange")
+GameplayTagList=(Tag="Brotato.Rule.OneShotTrees")
+GameplayTagList=(Tag="Brotato.Rule.CantStopMoving")
+GameplayTagList=(Tag="Brotato.Rule.GroupStructures")
+GameplayTagList=(Tag="Brotato.Rule.DoubleHpRegen")
+GameplayTagList=(Tag="Brotato.Rule.CanAttackWhileMoving")    ; ★ 默认持有（值 1）
+GameplayTagList=(Tag="Brotato.Rule.HpShop")
+GameplayTagList=(Tag="Brotato.Rule.DoubleBoss")
+GameplayTagList=(Tag="Brotato.Rule.WanderingBots")
+GameplayTagList=(Tag="Brotato.Rule.DoubleHpRegenBelowHalf")
+GameplayTagList=(Tag="Brotato.Rule.DestroyWeapons")
+GameplayTagList=(Tag="Brotato.Rule.NoHeal")
+GameplayTagList=(Tag="Brotato.Rule.TreeTurrets")
+GameplayTagList=(Tag="Brotato.Rule.SpecialEnemiesLastWave")
+GameplayTagList=(Tag="Brotato.Rule.UpgradedBaits")
+GameplayTagList=(Tag="Brotato.Rule.HealOnCritKill")
+GameplayTagList=(Tag="Brotato.Rule.MinimumWeaponsInShop")
+GameplayTagList=(Tag="Brotato.Rule.ExtraEnemiesNextWave")
+GameplayTagList=(Tag="Brotato.Rule.ExtraLootAliensNextWave")
+GameplayTagList=(Tag="Brotato.Rule.TreesStartWave")

; ──────────── Hook（39 个槽，全表见 D03 §5.3） ────────────
+GameplayTagList=(Tag="Brotato.Hook.DmgWhenPickupGold")
; …

; ──────────── Set（15 职业） ────────────
+GameplayTagList=(Tag="Brotato.Set.Blade")
+GameplayTagList=(Tag="Brotato.Set.Blunt")
+GameplayTagList=(Tag="Brotato.Set.Elemental")
+GameplayTagList=(Tag="Brotato.Set.Ethereal")
+GameplayTagList=(Tag="Brotato.Set.Explosive")
+GameplayTagList=(Tag="Brotato.Set.Gun")
+GameplayTagList=(Tag="Brotato.Set.Heavy")
+GameplayTagList=(Tag="Brotato.Set.Legendary")
+GameplayTagList=(Tag="Brotato.Set.Medical")
+GameplayTagList=(Tag="Brotato.Set.Medieval")
+GameplayTagList=(Tag="Brotato.Set.Precise")
+GameplayTagList=(Tag="Brotato.Set.Primitive")
+GameplayTagList=(Tag="Brotato.Set.Support")
+GameplayTagList=(Tag="Brotato.Set.Tool")
+GameplayTagList=(Tag="Brotato.Set.Unarmed")
```

### 1.3 C++ 访问（禁止字面量）

```cpp
// BrotatoGAS/Public/BrotatoGameplayTags.h
struct BROTATOGAS_API FBrotatoTags
{
    static const FBrotatoTags& Get() { return Instance; }

    FGameplayTag Attribute_Primary_MaxHp;
    FGameplayTag Attribute_Primary_Armor;
    // … 全部
    FGameplayTag State_Invincible;
    FGameplayTag Effect_Lifetime_Wave;
    FGameplayTag Effect_Lifetime_Linked;
    FGameplayTag Data_Damage;
    FGameplayTag Rule_CanAttackWhileMoving;
    // …

    static void InitializeNativeTags();   // 在模块 StartupModule 调用
private:
    static FBrotatoTags Instance;
};
```

`InitializeNativeTags()` 内用 `UGameplayTagsManager::Get().RequestGameplayTag(FName("..."), /*ErrorIfNotFound*/ true)` 逐个解析，**任何拼写错误在启动时立即 assert**。

### 1.4 `DT_TagAttributeMap`（Tag → FGameplayAttribute）

| 列 | 类型 | 说明 |
|---|---|---|
| `RowName` | Name | = Tag 全名，如 `Brotato.Attribute.Primary.Armor` |
| `AttributeTag` | FGameplayTag | 冗余，便于查表 |
| `Attribute` | FGameplayAttribute | 反射解析结果 |
| `Category` | Enum | Primary / Secondary / Gain / Limit |
| `DefaultBase` | float | BaseValue 初值 |
| `bIsPercent` | bool | UI 显示是否加 `%` |
| `IconRow` | Name | 属性图标（19 项 StatData） |

> 作用：**Effect 数据只填 Tag**，运行时解析成 Attribute。这样 DataTable 不依赖 C++ 反射路径字符串，重命名 Attribute 不会炸数据。

---

## 2. AttributeSet 划分

80+ 属性拆成 4 个 Set，避免单文件爆炸，也便于 UI 按组查询。

### 2.1 `UBrotatoCoreAttributeSet`（16 主属性 + 2 元属性 + Health）

```cpp
// BrotatoGAS/Public/AttributeSets/BrotatoCoreAttributeSet.h
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName)            \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName)  \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)                \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)                \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class BROTATOGAS_API UBrotatoCoreAttributeSet : public UAttributeSet
{
    GENERATED_BODY()
public:
    UBrotatoCoreAttributeSet();

    // ── 非 Brotato 概念，但 GAS 需要 ─────────────────────
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(UBrotatoCoreAttributeSet, Health)

    // ── 16 主属性（BaseValue 见 §2.2） ───────────────────
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData MaxHp;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData HpRegeneration;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Lifesteal;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData PercentDamage;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData MeleeDamage;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData RangedDamage;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData ElementalDamage;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData AttackSpeed;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData CritChance;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Engineering;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Range;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Armor;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Dodge;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Speed;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Luck;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Harvesting;
    ATTRIBUTE_ACCESSORS(UBrotatoCoreAttributeSet, MaxHp)
    // …（其余 15 个同样展开）

    // ── 元属性（Meta Attribute，仅伤害管道中转，不持久化） ──
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData IncomingDamage;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData IncomingHealing;
    ATTRIBUTE_ACCESSORS(UBrotatoCoreAttributeSet, IncomingDamage)
    ATTRIBUTE_ACCESSORS(UBrotatoCoreAttributeSet, IncomingHealing)

    virtual void PreAttributeChange(const FGameplayAttribute& Attr, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
};
```

> **单机不做属性复制**：不写 `GAMEPLAYATTRIBUTE_REPNOTIFY`、不注册 `GetLifetimeReplicatedProps`，省掉大量样板。

### 2.2 BaseValue 初值

| 属性 | Base | 出处 |
|---|---|---|
| `MaxHp` | **10** | `run_data.gd:init_stats()` |
| 其余 15 个主属性 | **0** | 同上 |
| `Health` | 10（开局 = MaxHp） | — |
| `IncomingDamage` / `IncomingHealing` | 0 | 元属性 |

在构造函数里 `InitMaxHp(10.f); InitHealth(10.f);`，其余默认 0。

### 2.3 `UBrotatoSecondaryAttributeSet`（数值型次级属性）

按 §02 §3 的键位全量建 Attribute。**非 0 默认值必须在构造函数显式 Init**：

| Attribute | Base | Attribute | Base |
|---|---|---|---|
| `HpCap`※ | 999999 | `GoldDrops` | **100** |
| `SpeedCap`※ | 999999 | `HarvestingGrowth` | **5** |
| `DodgeCap`※ | 60 | `WeaponSlot`※ | **6** |
| `HpStartWave` | **100** | `HpStartNextWave` | **100** |
| `MaxRangedWeapons` | **999** | `MaxMeleeWeapons` | **999** |
| `MinWeaponTier` | 0 | `MaxWeaponTier` | **99** |
| 其余全部 | **0** | | |

※ 这 4 个放在 `UBrotatoLimitAttributeSet`（见 §2.5），不在 Secondary。

Secondary 全量键（对齐 §02 §3.1，实现时逐条建）：
```
XpGain, NumberOfEnemies, ConsumableHeal, BurningCooldownReduction, BurningSpread,
Piercing, PiercingDamage, Bounce, BounceDamage, PickupRange, ChanceDoubleGold,
HealWhenPickupGold, ItemBoxGold, Knockback, LoseHpPerSecond, MapSize, GoldDrops,
EnemyHealth, EnemyDamage, EnemySpeed, BossStrength, ExplosionSize, ExplosionDamage,
DamageAgainstBosses, GiantCritDamage, ItemsPrice, WeaponsPrice, HarvestingGrowth,
HitProtection, Inflation, Accuracy, Projectiles, FreeRerolls, InstantGoldAttracting,
RecyclingGains, DiffGoldDrops, NeutralGoldDrops, EnemyGoldDrops, Pacifist, Cryptid,
Torture, Trees, GainPctGoldStartWave, HpStartWave, HpStartNextWave,
MaxRangedWeapons, MaxMeleeWeapons, MinWeaponTier, MaxWeaponTier
```

### 2.4 `UBrotatoGainAttributeSet`（20 个乘区源）

```cpp
UCLASS()
class BROTATOGAS_API UBrotatoGainAttributeSet : public UAttributeSet
{
    GENERATED_BODY()
public:
    // 16 个与主属性同名
    UPROPERTY() FGameplayAttributeData GainMaxHp;
    UPROPERTY() FGameplayAttributeData GainHpRegeneration;
    // … GainLifesteal / GainPercentDamage / GainMeleeDamage / GainRangedDamage
    //    GainElementalDamage / GainAttackSpeed / GainCritChance / GainEngineering
    //    GainRange / GainArmor / GainDodge / GainSpeed / GainLuck / GainHarvesting
    // 4 个额外
    UPROPERTY() FGameplayAttributeData GainExplosionDamage;
    UPROPERTY() FGameplayAttributeData GainPiercingDamage;
    UPROPERTY() FGameplayAttributeData GainBounceDamage;
    UPROPERTY() FGameplayAttributeData GainDamageAgainstBosses;
    // 全部 Base = 0
};
```

### 2.5 `UBrotatoLimitAttributeSet`（Override 语义）

```cpp
UCLASS()
class BROTATOGAS_API UBrotatoLimitAttributeSet : public UAttributeSet
{
    GENERATED_BODY()
public:
    UPROPERTY() FGameplayAttributeData HpCap;       // Base 999999
    UPROPERTY() FGameplayAttributeData DodgeCap;    // Base 60
    UPROPERTY() FGameplayAttributeData SpeedCap;    // Base 999999
    UPROPERTY() FGameplayAttributeData WeaponSlot;  // Base 6
};
```

> **为什么单独一个 Set**：这 4 个用 `EGameplayModOp::Override`，语义与其他属性完全不同；且 Ghost(dodge_cap 90) / Cryptid(70) 不会共存，无冲突。
> **卸载行为对齐**：原作 `StatCapEffect.unapply()` 是**硬复位 999999**；GAS 移除 GE 后 Aggregator 自动回落到 BaseValue 999999 → **行为一致 ✅**。

### 2.6 `UBrotatoEnemyAttributeSet`（Tier B 专用，精简）

```cpp
UCLASS()
class BROTATOGAS_API UBrotatoEnemyAttributeSet : public UAttributeSet
{
    GENERATED_BODY()
public:
    UPROPERTY() FGameplayAttributeData Health;
    UPROPERTY() FGameplayAttributeData MaxHealth;
    UPROPERTY() FGameplayAttributeData Damage;
    UPROPERTY() FGameplayAttributeData Armor;
    UPROPERTY() FGameplayAttributeData Speed;
    UPROPERTY() FGameplayAttributeData IncomingDamage;
};
```

---

## 3. ★ 三层 → Aggregator 映射方案

| Brotato 概念 | GAS 实现 | 生命周期管理 |
|---|---|---|
| 初值（`MaxHp=10`，其余 0） | **BaseValue** | 构造函数 |
| 永久层 `E[s]`：物品 / 角色 / 套装 / 难度 | **Infinite GE + `EGameplayModOp::Additive`** | `FActiveGameplayEffectHandle` 精确移除 |
| 升级（永久且不可撤销） | **Instant GE + Additive**（直接改 BaseValue） | 无需移除，省 Aggregator 条目 |
| 波内层 `T[s]` | **Infinite GE + GrantedTag `Effect.Lifetime.Wave`** | 波末 `RemoveActiveEffectsWithGrantedTags` 一行清空 |
| 联动层 `L[s]` | **Infinite GE + GrantedTag `Effect.Lifetime.Linked`** | 脏时整体移除重建（§5） |
| `gain_*` 乘区 | **Infinite GE + Multiplicative + AttributeBased 非快照** | 开局 Apply，永不移除（§4） |
| `hp_cap` / `dodge_cap` / `speed_cap` / `weapon_slot` | **Infinite GE + `EGameplayModOp::Override`** | 同永久层 |
| 规则开关（bool） | **Loose GameplayTag**（`Rule.*`） | `AddLooseGameplayTag` / `Remove` |
| 数组钩子槽（39） | **`UBrotatoHookRegistry`**（非 GAS，见 D03 §5） | `FGuid` 精确移除 |

### 3.1 为什么三层相加后只乘一次 gain

原作是「**每层各自乘 gain**」：
```
Utils.get_stat(s) = RunData.get(s)×(1+g/100) + TempStats.get(s)×(1+g/100) + LinkedStats.get(s)×(1+g/100)
```
由乘法分配律，等价于 `(E+T+L) × (1+g/100)`，而这正是 GAS Aggregator 的形态。**无损映射，无需妥协。**

---

## 4. ★ gain 乘区实现（AttributeBased 非快照 Modifier）

### 4.1 `GE_StatGainMultipliers` 资产配置

| 项 | 值 |
|---|---|
| 资产路径 | `Content/GAS/Effects/GE_StatGainMultipliers` |
| `DurationPolicy` | **Infinite** |
| 应用时机 | 玩家 ASC 初始化时 Apply，**永不移除** |
| `GrantedTags` | `Effect.Lifetime.Permanent` |
| Modifier 数量 | **20**（16 主属性 + 4 额外） |

每条 Modifier（以 Speed 为例）：

```
Attribute            = Brotato.Attribute.Primary.Speed （UBrotatoCoreAttributeSet.Speed）
ModifierOp           = Multiplicative
Magnitude Calculation Type = AttributeBased
  ├─ Backing Attribute        = UBrotatoGainAttributeSet.GainSpeed
  ├─ Attribute Source         = Target
  ├─ Snapshot                 = false          ★ 必须 false
  ├─ Attribute Curve          = (none)
  ├─ Coefficient              = 0.01
  ├─ PreMultiplyAdditiveValue = 100
  └─ PostMultiplyAdditiveValue= 0
→ Magnitude = (GainSpeed + 100) × 0.01 = 1 + GainSpeed/100   ✅
```

### 4.2 关键点

| 点 | 说明 |
|---|---|
| `Snapshot = false` | 非快照才会注册依赖：`GainSpeed` 变化时 `Speed` 的 CurrentValue **自动重算**。设 true 会永久卡在 Apply 瞬间的值 |
| `Coefficient = 0.01` + `PreMultiplyAdditive = 100` | 精确得到 `1 + g/100`，不要用 `PostMultiply` |
| `GainSpeed = -100` | 乘区 = 0 → Speed 归零（Mage / Chunky 的 `gain_stat_speed -100`）。GAS 的 Multiplicative 允许 0，行为正确 ✅ |
| `GainSpeed = +250` | 乘区 = 3.5（Cyborg 的 `gain_stat_ranged_damage +250`） |
| 性能 | 20 条非快照 AttributeBased Modifier 会注册 20 个依赖回调。实测开销可忽略（每 Tick 最多触发一次重算） |

### 4.3 验收（`Tests/GAS/AttributeAggregationTest.cpp`）

```
A01 三层 Additive + gain Multiplicative 的结果 == (E+T+L)×(1+g/100)
A02 GainSpeed = -100 → Speed == 0
A03 GainRangedDamage = +50，RangedDamage 基础 10 → CurrentValue == 15
A04 Override：DodgeCap 60 → Apply Ghost(90) → 90 → 移除 → 回落 60
A05 MaxHp clamp 到 [1, HpCap]；HpCap 被 StatCapEffect 覆写后立即生效
```

---

## 5. ★ LinkedStats：属性依赖属性

### 5.1 问题

Brotato 有 20+ 个「每 N 点 X 获得 Y」效果（Chunky / Generalist / Knight / Speedy / Hunter…）。三个难点：

1. `L[s]` 依赖其他属性当前值 → 需要动态重算
2. **存在循环依赖**：Generalist「每 1 远程 → +2 近战」且「每 2 近战 → +1 远程」
3. 原作用**整数除法 + 单次快照计算**天然避免震荡

### 5.2 方案对比

| 方案 | 做法 | 结果 |
|---|---|---|
| A. MMC 快照 | `UMMC_LinkedStat` 捕获源属性，`bSnapshot=true` | Infinite GE 只在 Apply 时算一次，源属性变化后不更新 ❌ |
| B. MMC 非快照 | `bSnapshot=false` | **递归重算 → 循环依赖直接死循环** ❌ |
| **C. 移除 + 重建** | 脏标记 → Tick 末（阶段 S11a）移除旧 GE → 单次快照计算 → Apply 新 GE | ✅ 与原作行为完全一致 |

### 5.3 方案 C 实现

```cpp
// BrotatoGAS/Public/Components/BrotatoLinkedStatsComponent.h
UCLASS()
class BROTATOGAS_API UBrotatoLinkedStatsComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    void MarkDirty() { bDirty = true; }

    /** 模拟阶段 S11a 调用，每 Tick 最多执行一次 */
    void FlushIfDirty()
    {
        if (!bDirty) return;
        bDirty = false;

        // 1. ★ 先移除旧的 Linked GE
        //    这样下一步读 GetNumericAttribute 时天然得到「Permanent + Temp」（不含 Linked）
        ASC->RemoveActiveEffectsWithGrantedTags(
            FGameplayTagContainer(FBrotatoTags::Get().Effect_Lifetime_Linked));

        // 2. ★ 单次快照计算：所有 Link 读同一时刻的值，不迭代、不递归
        FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
        FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GE_LinkedStats, 1.f, Ctx);
        if (!Spec.IsValid()) return;

        bool bAny = false;
        TMap<FGameplayTag, float> Accum;                 // 同一目标属性的多条 Link 需累加
        for (const FBrotatoHookEntry& Link : HookRegistry->Get(FBrotatoTags::Get().Hook_StatLinks))
        {
            const float Scaled = ResolveScaleSource(Link.ExtraName, Link.bPermOnly);
            const int32 Chunks = BrotatoMathInt::IntDiv(Scaled, Link.ExtraA);   // ★ 整数除法
            Accum.FindOrAdd(Link.AttributeTag) += Link.Value * Chunks;
            bAny = true;
        }
        if (!bAny) return;

        for (const auto& KV : Accum)
            Spec.Data->SetSetByCallerMagnitude(KV.Key, KV.Value);

        LinkedHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
    }

private:
    /** 对应原作 8 种缩放源 */
    float ResolveScaleSource(FName Source, bool bPermOnly) const;

    UPROPERTY() TSubclassOf<UGameplayEffect> GE_LinkedStats;
    FActiveGameplayEffectHandle LinkedHandle;
    bool bDirty = false;
};
```

`ResolveScaleSource` 的 8 种源：

| `ExtraName` | 取值 |
|---|---|
| `materials` | `RunSubsystem->Gold` |
| `structure` | `HookRegistry->Get(Hook.Structures).Num()` |
| `living_enemy` | `EnemySubsystem->GetLivingCount()` |
| `living_tree` | `EnemySubsystem->GetLivingTreeCount()` |
| `common_item` | `RunSubsystem->CountDistinctItemsOfTier(Common)` |
| `legendary_item` | `RunSubsystem->CountDistinctItemsOfTier(Legendary)` |
| `item_xxx` | `RunSubsystem->CountItem(Source)` |
| 其他（属性名） | `bPermOnly ? 只读 Permanent 层 : Permanent + Temp`（★ **不含 Linked**） |

> **循环依赖为什么不会炸**：第 1 步已移除 Linked GE，此时 `GetNumericAttribute` 读到的天然就是 `Permanent + Temp`。这与原作 `perm_only ? RunData.get_stat(s) : RunData.get_stat(s) + TempStats.get_stat(s)` 完全一致 —— **原作也从不把 LinkedStats 计入源**。

### 5.4 `GE_LinkedStats` 资产配置

| 项 | 值 |
|---|---|
| `DurationPolicy` | Infinite |
| `GrantedTags` | **`Effect.Lifetime.Linked`** |
| Modifiers | 为**所有可能被链接的属性**各一条：`Additive` + `SetByCaller`（Tag = 对应 `Attribute.*`） |
| SetByCaller 选项 | 勾选 **`bAllowNonExistentSetByCaller`**（未设置的键默认 0，不报错） |

> 也可以按「被链接属性集合」拆成 2–3 个 GE 以减少条目，但**必须共用同一个 `Effect.Lifetime.Linked` Tag**。

### 5.5 MarkDirty 触发点

`AddItem` / `RemoveItem` / `AddWeapon` / `RemoveWeapon` / `AddGold`（仅当存在 materials 链）/ 升级选择完成 / 波末结算 / 结构物增减 / 敌人与树的存活数变化（**若存在 `living_enemy` / `living_tree` 链，则计数变化时置脏**）。

> ⚠️ `living_enemy` / `living_tree` 链会让 LinkedStats 频繁重建。实现时加一个优化：只有当计数**实际变化**才 MarkDirty；重建统一在阶段 S11a 执行，每 Tick 至多一次。

### 5.6 验收（`Tests/GAS/LinkedStatsTest.cpp`）

```
L01 Chunky：每 3 MaxHp → +1 PercentDamage；MaxHp=100 → +33（非 33.3）
L02 Generalist 循环依赖：RNG→MEL 且 MEL→RNG，连续 100 次重算不震荡、值收敛
L03 整数除法：Scaled=5, Nb=3 → Chunks=1
L04 源属性排除 Linked 层（重建 10 次后值不自我放大）
L05 materials 链：gold 从 100 → 101 只触发一次重建
```

---

## 6. Clamp 与联动

### 6.1 `PreAttributeChange`（只 clamp CurrentValue）

```cpp
void UBrotatoCoreAttributeSet::PreAttributeChange(const FGameplayAttribute& Attr, float& NewValue)
{
    Super::PreAttributeChange(Attr, NewValue);
    UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
    if (!ASC) return;

    if (Attr == GetMaxHpAttribute())
    {
        // §02 §5.3：clamp(stat_max_hp, 1, hp_cap)
        const float Cap = ASC->GetNumericAttribute(UBrotatoLimitAttributeSet::GetHpCapAttribute());
        NewValue = FMath::Clamp(NewValue, 1.f, Cap);
    }
    else if (Attr == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHp());
    }
    else if (Attr == GetDodgeAttribute())
    {
        // ★ 属性侧 clamp；判定侧还有第二道（§02 §4.4 双重 clamp）
        NewValue = FMath::Min(NewValue,
            ASC->GetNumericAttribute(UBrotatoLimitAttributeSet::GetDodgeCapAttribute()));
    }
    else if (Attr == GetSpeedAttribute())
    {
        NewValue = FMath::Min(NewValue,
            ASC->GetNumericAttribute(UBrotatoLimitAttributeSet::GetSpeedCapAttribute()));
    }
}
```

### 6.2 `PostGameplayEffectExecute`（Instant GE 后处理）

```cpp
void UBrotatoCoreAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
    {
        const float Dmg = GetIncomingDamage();
        SetIncomingDamage(0.f);                       // ★ 元属性用完即清零
        if (Dmg > 0.f)
        {
            SetHealth(FMath::Clamp(GetHealth() - Dmg, 0.f, GetMaxHp()));
            OnDamaged.Broadcast(Dmg, Data.EffectSpec);
            if (GetHealth() <= 0.f) OnDied.Broadcast();
        }
    }
    else if (Data.EvaluatedData.Attribute == GetIncomingHealingAttribute())
    {
        const float Heal = GetIncomingHealing();
        SetIncomingHealing(0.f);
        if (Heal > 0.f && !HasRuleNoHeal())
            SetHealth(FMath::Clamp(GetHealth() + Heal, 0.f, GetMaxHp()));
    }
    else if (Data.EvaluatedData.Attribute == GetMaxHpAttribute())
    {
        // ★ MaxHp 变小时必须同步 clamp Health（PreAttributeChange 管不到）
        SetHealth(FMath::Min(GetHealth(), GetMaxHp()));
    }
    else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
        // 半血阈值 Tag 驱动 stats_below_half_health（Golem）
        UpdateBelowHalfHealthTag();
    }
}
```

### 6.3 半血 Tag

```cpp
void UBrotatoCoreAttributeSet::UpdateBelowHalfHealthTag()
{
    const bool bBelow = GetHealth() < GetMaxHp() * 0.5f;
    UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
    const FGameplayTag& T = FBrotatoTags::Get().State_BelowHalfHealth;
    if (bBelow && !ASC->HasMatchingGameplayTag(T))       ASC->AddLooseGameplayTag(T);
    else if (!bBelow && ASC->HasMatchingGameplayTag(T))  ASC->RemoveLooseGameplayTag(T);
}
```

`State.BelowHalfHealth` 的增删由 `RegisterGameplayTagEvent` 监听，驱动 `stats_below_half_health` 的 GE Apply/Remove（D03 §6）。

> ⚠️ Infinite GE 改的是 CurrentValue，**不会**触发 `PostGameplayEffectExecute`。`MaxHp` 由物品降低时（如 Legendary 套装 −100 HP）需要在 `PreAttributeChange` 之外，用 `GetGameplayAttributeValueChangeDelegate(GetMaxHpAttribute())` 回调里同步 clamp Health。**两条路径都要覆盖**。

---

## 7. `UBrotatoAbilitySystemComponent`

```cpp
UCLASS()
class BROTATOGAS_API UBrotatoAbilitySystemComponent : public UAbilitySystemComponent
{
    GENERATED_BODY()
public:
    // ★ 不重写 BeginPlay 关闭 Tick：ASC 保持引擎默认驱动（§00 §3.4）
    //   Duration / Periodic / Cooldown GE 的计时全部由 GAS 原生承担。

    /** 便捷封装：一物品一 Spec 的应用与精确回收 */
    FActiveGameplayEffectHandle ApplySpecSelf(const FGameplayEffectSpecHandle& Spec);
    void RemoveHandles(TArray<FActiveGameplayEffectHandle>& Handles);

    /** 便捷封装：Apply 一个带 SetByCaller 时长的 Duration GE（无敌窗口 / CD 类） */
    FActiveGameplayEffectHandle ApplyTimedTagGE(TSubclassOf<UGameplayEffect> GE, float DurationSeconds);
};
```

**GE 类型使用边界（v2.0 修订，旧禁令作废）**：

| GE 类型 | 允许用途 | 计时归属 |
|---|---|---|
| **Instant** | 升级永久属性、伤害、治疗、`stats_end_of_wave` | 无计时 |
| **Infinite** | 物品 / 角色 / 套装 / 难度 / 波内 / 联动 / gain 乘区 | 无计时（显式 Remove） |
| **Duration** | ✅ **无敌窗口、吸血 CD、武器冷却、临时 Buff、减速** | ✅ GAS 原生（World 时间，正确响应 Pause / TimeDilation） |
| **Periodic** | ✅ **燃烧 tick、生命回复、每秒掉血、每 5 s 临时属性叠加** | ✅ GAS 原生（长帧自动补齐 period） |

> 运行时决定时长 → `Duration Magnitude = SetByCaller`；运行时决定周期 → `Spec->Period = Sec`。详见 D03 §6.3 / §6.4。

---

## 8. 属性初始化流程（开局顺序，不可乱）

```cpp
void ABrotatoPlayer::InitAbilitySystem()
{
    // 1. 创建 ASC + 4 个 AttributeSet（BaseValue 已在构造函数设好）
    // 2. ★ 先 Apply GE_StatGainMultipliers（Infinite，永不移除）
    //    必须在任何 gain_* 修改之前，否则乘区不会追溯生效
    ASC->ApplyGameplayEffectToSelf(GE_StatGainMultipliers.GetDefaultObject(), 1.f, Ctx);

    // 3. 授予默认 Rule Tag
    ASC->AddLooseGameplayTag(FBrotatoTags::Get().Rule_CanAttackWhileMoving);  // 默认值 1

    // 4. 应用难度效果（Danger 3/4/5 的 enemy_health / enemy_damage）
    RunSubsystem->ApplyDifficultyEffects(CurrentDifficulty);

    // 5. 应用角色效果（含 starting_item / starting_weapon 钩子）
    RunSubsystem->AddCharacter(CharacterData);

    // 6. 应用起始武器 → 触发套装重算
    RunSubsystem->AddWeapon(StartingWeapon, /*bIsStarting*/ true);

    // 7. 处理 starting_item / starting_weapon 钩子
    RunSubsystem->ProcessStartingHooks();

    // 8. LinkedStats 首次重建
    LinkedStats->MarkDirty();
    LinkedStats->FlushIfDirty();

    // 9. Health = MaxHp
    ASC->SetNumericAttributeBase(UBrotatoCoreAttributeSet::GetHealthAttribute(),
        ASC->GetNumericAttribute(UBrotatoCoreAttributeSet::GetMaxHpAttribute()));
}
```

---

## 9. UI 查询接口

UI **不得**直接读 Attribute，必须走 `UBrotatoStatQuery`：

```cpp
UCLASS()
class BROTATOGAS_API UBrotatoStatQuery : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** 返回 UI 展示用的整数值：int(Utils.get_stat(key)) */
    UFUNCTION(BlueprintPure) static int32 GetStatDisplayValue(const UObject* Ctx, FGameplayTag AttrTag);

    /** 是否封顶，用于显示 "值 | 上限"（§08 §3.4） */
    UFUNCTION(BlueprintPure) static bool IsStatCapped(const UObject* Ctx, FGameplayTag AttrTag, int32& OutCap);

    /** 属性 Tooltip 参数（armor 显示实际减伤%、harvesting 显示成长等） */
    UFUNCTION(BlueprintPure) static TArray<FText> GetStatTooltipArgs(const UObject* Ctx, FGameplayTag AttrTag);
};
```

封顶显示规则（§08 §3.4）：
- `Dodge > DodgeCap` → 显示 `"80 | 60"`
- `MaxHp` 且 `HpCap < 9999` → 显示 `"值 | 上限"`
- `Speed` 且 `SpeedCap < 9999` → 同上

Tooltip 特例：
- `Armor` → `[abs(round((1 - ArmorCoef(v)) × 100))]`（显示实际减伤%）
- `Harvesting` 且 v ≥ 0 → 键名追加 `_LIMITED`，参数 `[abs(v), HarvestingGrowth, NbOfWaves, 20]`
- `Lifesteal` → `[abs(v), "10"]`
- `HpRegeneration` → `[每次量(1或2), stepify(周期,0.01), stepify(量/周期,0.01)]`
- `Dodge` → `[abs(v), DodgeCap + "%"]`

---

## 10. 本章验收清单

| # | 项 | 标准 |
|---|---|---|
| G-A1 | Tag 解析 | 启动时 `InitializeNativeTags()` 无 assert |
| G-A2 | BaseValue | `MaxHp=10`、`DodgeCap=60`、`WeaponSlot=6`、`GoldDrops=100` |
| G-A3 | gain 乘区 | A01–A03 测试通过 |
| G-A4 | Override | A04 通过（DodgeCap 60→90→60） |
| G-A5 | Clamp | MaxHp 降低时 Health 同步（两条路径都覆盖） |
| G-A6 | LinkedStats | L01–L05 通过 |
| G-A7 | ASC Tick | ASC 保持引擎默认 Tick（**未**被 `SetComponentTickEnabled(false)`）；Pause 时 GE 计时正确停走 |
| G-A8 | Periodic / Duration | `GE_Burning` tick 次数、`GE_Invincible` 窗口时长符合理论值（±2%）；无自建 `*FramesLeft` 字段 |
| G-A9 | Aggregator 条目 | 满配（6 武器 + 30 物品）时 < 800（`stat AbilitySystem`） |
