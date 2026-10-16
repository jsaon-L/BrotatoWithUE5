# D03 · GameplayEffect 资产规范与 Effect 桥接层

> **模块**：`BrotatoGAS`（ExecCalc / Hooks）+ `BrotatoData`（EffectDef）+ `BrotatoGameplay`（RunSubsystem）
> **里程碑**：M3（Effect 框架 + 商店 + 物品）
> **上游规格**：`Docs/00` 全文（★ 最高仲裁）、`Docs/05` 全文、`Docs/02 §9.5`、`Docs/09 §5 §6 §7`
> **v2.0 变更**：**Duration / Periodic / Cooldown GE 全部启用**（旧版禁令作废）；无敌窗口、吸血 CD、燃烧、生命回复、每 5 s 叠加均改为 GAS 原生计时。
> **v2.1 变更**：§4 伤害管道按 UE GAS C++ 正确用法重写：ExecCalc 拆为 Player/Enemy 两个子类并注册捕获；参数标记改用 AssetTags；无敌只由 GE 组件拦截；新增唯一施加入口 `FBrotatoGasDamage::Apply`；新增 `GE_Damage_Enemy` / `GE_HitProtectionReset`。
> **配套同步（2026-10-16）**：D02 §1.3 / §2.6 补齐 Native Tag 与 Enemy 扣血回调；D01 / D04 / D08 统一入口和无敌验收；D05 §4 补齐 Boss 身份初始化；玩法设计/09 §7 统一闪避单位并引用本章实现。本文其余章节统一使用 `BrotatoTags::`，所在 C++ 文件须包含 `BrotatoNativeTags.h`，不再依赖旧 Tag 单例。

**核心任务**：把原作 21 种 `Effect` 子类 + 39 个数组钩子槽，桥接成
**GE（属性类，≈9 种）+ HookRegistry（钩子类，≈12 种）+ Loose Tag（规则类）**。

---

## 1. GE 资产总清单

所有 GE 资产放在 `Content/GAS/Effects/`。**任何新建 GE 必须登记进本表**。

| # | 资产 | Duration Policy | 主要 Modifier | GrantedTags | 应用者 | 备注 |
|---|---|---|---|---|---|---|
| 1 | `GE_StatGainMultipliers` | Infinite | 20 条 Multiplicative / AttributeBased | `Effect.Lifetime.Permanent` | 初始化 | D02 §4，永不移除 |
| 2 | `GE_ItemStats` | Infinite | **动态** Additive/Override + SetByCaller | `Effect.Source.Item`, `Effect.Lifetime.Permanent` | `AddItem` | ★ 一物品一 Spec |
| 3 | `GE_CharacterStats` | Infinite | 同上 | `Effect.Source.Character` | `AddCharacter` | |
| 4 | `GE_SetBonus` | Infinite | 同上 | `Effect.Source.Set` | `RebuildSetBonusGE` | 批量移除用 |
| 5 | `GE_DifficultyStats` | Infinite | 同上 | `Effect.Source.Difficulty` | 难度确认 | |
| 6 | `GE_ConditionalStats` | Infinite | 同上 | `Effect.Source.Conditional` | `UpdateItemRelatedEffects` | 4 类武器条件加成 |
| 7 | `GE_LinkedStats` | Infinite | 全属性 Additive + SetByCaller | **`Effect.Lifetime.Linked`** | LinkedStatsComponent | D02 §5 |
| 8 | `GE_Upgrade` | **Instant** | Additive | — | 升级选择 | 直接改 BaseValue |
| 9 | `GE_StatsEndOfWave` | **Instant** | Additive | — | 波末 | 永久（对齐原作） |
| 10 | `GE_Damage` | **Instant** | Execution = `UBrotatoDamageExecCalc_Player` | — | `FBrotatoGasDamage::Apply` | §4；★ GE 组件 `TargetTagRequirements`：`ApplicationTagRequirements.IgnoreTags = State.Invincible` |
| 10b | `GE_Damage_Bypass` | **Instant** | 同上 | — | 同上（`bypass_invincibility`） | ★ **不带**无敌 Tag 需求；是否施加 `GE_Invincible` 只取决于 `bDodgeable`（02 §5.4） |
| 10c | `GE_Damage_Enemy` | **Instant** | Execution = `UBrotatoDamageExecCalc_Enemy` | — | 同上 | Tier B（Boss / 精英）；§4.3 |
| 10d | `GE_HitProtectionReset` | **Instant** | `HitProtection` · Override · 0 | — | 波首 | 消耗计数归零；§4.6 |
| 11 | `GE_Heal` | **Instant** | `IncomingHealing` SetByCaller | — | 治疗 | |
| 11b | `GE_Lifesteal` | **Instant** | `IncomingHealing` = 1 | — | 吸血 | ★ `IgnoreTags = State.LifestealCd`，成功后施加 `GE_LifestealCd` |
| 12 | `GE_TempStats_WhileMoving` | Infinite | 动态 Additive | `Effect.Lifetime.Wave` | Tag 事件 | §6 |
| 13 | `GE_TempStats_WhileNotMoving` | Infinite | 同上 | `Effect.Lifetime.Wave` | Tag 事件 | |
| 14 | `GE_TempStats_OnHit` | Infinite | 同上 | `Effect.Lifetime.Wave`, `Effect.Stackable.OnHit` | 受击 | ★ **Stacking** |
| 15 | `GE_TempStats_OnDodge` | Infinite | 同上 | `Effect.Lifetime.Wave`, `Effect.Stackable.OnDodge` | 闪避 | Stacking |
| 16 | `GE_TempStats_Stacking` | **HasDuration + Periodic** | 同上 | `Effect.Lifetime.Wave`, `Effect.Stackable.Periodic5s` | 波首施加一次 | ★ `Period = 5.0`，**用 Periodic + Stacking**（§6.3） |
| 17 | `GE_StatsBelowHalfHealth` | Infinite | 同上 | `Effect.Lifetime.Wave` | Tag 事件 | Golem |
| 18 | `GE_StatsNextWave` | Infinite | 同上 | `Effect.Lifetime.Wave` | 波首 | item_peacock |
| 19 | `GE_PercentMaterials` | Infinite | 同上 | `Effect.Lifetime.Wave` | 1 s 采样 | Streamer |
| 20 | `GE_ConsumableStats` | Infinite | 同上 | `Effect.Lifetime.Wave` | 满血吃消耗品 | 带 max_value 上限 |
| 21 | `GE_Invincible` | ★ **HasDuration**（SetByCaller） | 无 Modifier | `State.Invincible`（GE 组件 `TargetTags`） | `FBrotatoGasDamage::Apply`（`bDodgeable` 时，含闪避） | 时长 = `GetInvincibilitySeconds()`；**GAS 原生到期自动移除** |
| 21b | `GE_LifestealCd` | ★ **HasDuration** = 0.1 s | 无 Modifier | `State.LifestealCd` | 吸血成功后 | 上限 10 HP/s |
| 22 | `GE_Burning` | ★ **HasDuration + Periodic** | Execution = `UBrotatoBurnExecCalc` | `State.Burning.*` | 点燃 | `Period` 与 `Duration` 均运行时写入（§6.4） |
| 23 | `GE_HpRegen` | ★ **HasDuration(Infinite) + Periodic** | `IncomingHealing` 走 MMC | `Effect.Lifetime.Permanent` | 开波 | `Period = HpRegenPeriodSeconds()`，属性变化时重建 |
| 24 | `GE_LoseHpPerSecond` | ★ **Infinite + Periodic** = 1 s | Execution（bypass 无敌） | `Effect.Lifetime.Wave` | 装备触发 | |
| 25 | `GE_WeaponCooldown` | ★ **HasDuration**（SetByCaller） | 无 Modifier | `Cooldown.Weapon.{Slot}`（动态） | `GA_WeaponAttack::ApplyCooldown` | D04 §3 |

> ★ **运行时写入时长/周期的两种手法**（§00 §3.1）：
> - 时长：`Duration Magnitude = SetByCaller` → `Spec->SetSetByCallerMagnitude(Tag_Data_Duration, Sec)`
> - 周期：`Spec->Period = Sec`（`Period` 是 `FScalableFloat`，Spec 上可直接改写，**无需为每个档位建资产**）

### 1.1 ★ 一物品一 GE Spec（性能红线）

```cpp
/** UBrotatoItemParentData::BuildStatSpec */
FGameplayEffectSpecHandle UBrotatoItemParentData::BuildStatSpec(
    UAbilitySystemComponent* ASC, TSubclassOf<UGameplayEffect> Template) const
{
    // 先判断该物品是否有任何 StatModifier 类 Effect
    if (!HasAnyStatModifier()) return FGameplayEffectSpecHandle();

    FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
    Ctx.AddSourceObject(this);
    FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Template, 1.f, Ctx);

    // ★ 把该物品的全部属性 Modifier 合并进一个 Spec 的 SetByCaller
    TMap<FGameplayTag, float> Sum;
    for (const FBrotatoEffectDef& Def : Effects)
    {
        switch (Def.Kind)
        {
        case EBrotatoEffectKind::StatModifier:
            if (Def.StorageMethod == EBrotatoStorageMethod::Replace)
                Spec.Data->SetSetByCallerMagnitude(MakeOverrideTag(Def.AttributeTag), Def.Value);
            else
                Sum.FindOrAdd(Def.AttributeTag) += Def.Value;
            break;
        case EBrotatoEffectKind::GainModifier:
            for (const FGameplayTag& G : Def.GainTargets)
                Sum.FindOrAdd(ToGainTag(G)) += Def.Value;
            break;
        default: break;   // 钩子类不进 GE
        }
    }
    for (const auto& KV : Sum) Spec.Data->SetSetByCallerMagnitude(KV.Key, KV.Value);
    return Spec;
}
```

> **收益**：Aggregator 条目数从 ~600 降到 ~60（假设 30 件物品 × 平均 3 条 Effect）。
> **代价**：`GE_ItemStats` 模板必须预先声明所有可能属性的 Modifier（Additive + SetByCaller + `bAllowNonExistentSetByCaller`）。用编辑器脚本按 `DT_TagAttributeMap` 自动生成，见 D07 §5。

---

## 2. `FBrotatoEffectDef`（数据定义层）

### 2.1 枚举

```cpp
// BrotatoData/Public/BrotatoEffectDef.h
UENUM(BlueprintType)
enum class EBrotatoEffectKind : uint8
{
    StatModifier,   // → GE Additive / Override
    GainModifier,   // → GE Additive on Gain* Attribute
    ClassBonus,     // → Hook.WeaponClassBonus
    WeaponBonus,    // → Hook.WeaponBonus
    WeaponStack,    // → Hook.WeaponStack（写武器 base stats）
    StatLink,       // → Hook.StatLinks
    StatConvert,    // → Hook.ConvertStats*
    StatWithMax,    // → Hook（带 MaxValue）
    Hook,           // → 任意钩子槽（chance / healing / projectile …）
    Structure,      // → Hook.Structures
    Burning,        // → Hook.BurnChance 或武器覆写
    Exploding,      // → Hook.ExplodeOn* 或武器标记
    Rule,           // → Loose GameplayTag
    Marker          // NullEffect：纯标记，运行时类型检测
};

UENUM(BlueprintType)
enum class EBrotatoStorageMethod : uint8 { Sum = 0, KeyValue = 1, Replace = 2 };
```

> ★ **原作陷阱**：`custom_key != ""` **强制走数组分支**，即使 `storage_method == SUM`。导入器必须先判 `CustomKey` 非空 → `Kind = Hook`。

### 2.2 结构体

```cpp
USTRUCT(BlueprintType)
struct BROTATODATA_API FBrotatoEffectDef
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) EBrotatoEffectKind Kind = EBrotatoEffectKind::StatModifier;
    UPROPERTY(EditAnywhere) FGameplayTag AttributeTag;     // 目标属性（查 DT_TagAttributeMap）
    UPROPERTY(EditAnywhere) FGameplayTag HookTag;          // Kind == Hook / Structure / … 时使用
    UPROPERTY(EditAnywhere) float Value = 0.f;
    UPROPERTY(EditAnywhere) EBrotatoStorageMethod StorageMethod = EBrotatoStorageMethod::Sum;

    // 各 Kind 的扩展参数（用 meta=(EditCondition) 按 Kind 显隐）
    UPROPERTY(EditAnywhere) float Chance = 0.f;            // ChanceStatDamage / Exploding
    UPROPERTY(EditAnywhere) float MaxValue = 0.f;          // StatWithMax
    UPROPERTY(EditAnywhere) int32 NbStatScaled = 1;        // StatLink 分母
    UPROPERTY(EditAnywhere) FName ScaleSource;             // StatLink 缩放源
    UPROPERTY(EditAnywhere) bool  bPermOnly = false;
    UPROPERTY(EditAnywhere) float PctConverted = 1.f;      // StatConvert
    UPROPERTY(EditAnywhere) FGameplayTag ToAttributeTag;   // StatConvert 目标
    UPROPERTY(EditAnywhere) float ToValue = 0.f;
    UPROPERTY(EditAnywhere) FGameplayTag SetTag;           // ClassBonus 职业
    UPROPERTY(EditAnywhere) FName WeaponId;                // WeaponBonus / WeaponStack
    UPROPERTY(EditAnywhere) FBurningData Burning;
    UPROPERTY(EditAnywhere) TArray<FGameplayTag> GainTargets;  // StatGainsModification 多目标
    UPROPERTY(EditAnywhere) FBrotatoStructureSpec Structure;   // StructureEffect / TurretEffect
    UPROPERTY(EditAnywhere) FBrotatoWeaponStats  ProjectileStats;  // ProjectileEffect
    UPROPERTY(EditAnywhere) bool bAutoTargetEnemy = false;
    UPROPERTY(EditAnywhere) float CooldownSeconds = -1.f;   // ★ 秒（原作帧值 /60）
    UPROPERTY(EditAnywhere) TSubclassOf<UGameplayAbility> GrantedPassive;  // 可选被动

    // 文本渲染（§05 §5）
    UPROPERTY(EditAnywhere) FName TextKey;
    UPROPERTY(EditAnywhere) TArray<FBrotatoCustomArg> CustomArgs;
    UPROPERTY(EditAnywhere) EBrotatoEffectSign EffectSign = EBrotatoEffectSign::FromValue;
};
```

### 2.3 21 种 Effect → Kind 映射表

| 原类名 | `get_id()` | Kind | 目标 |
|---|---|---|---|
| `Effect`（基类，SUM） | `effect` | `StatModifier` | GE Additive |
| `Effect`（REPLACE） | `effect` | `StatModifier` | GE **Override** |
| `StatGainsModificationEffect` | `stat_gains_modifications` | `GainModifier` | GE Additive on `Gain*` |
| `StatCapEffect` | `stat_cap` | `StatModifier`(Replace) | GE Override on `HpCap`/`SpeedCap` |
| `StatWithMaxEffect` | `stat_with_max` | `StatWithMax` | Hook（带 MaxValue） |
| `ClassBonusEffect` | `class_bonus` | `ClassBonus` | Hook.WeaponClassBonus |
| `WeaponBonusEffect` | `weapon_bonus` | `WeaponBonus` | Hook.WeaponBonus |
| `WeaponStackEffect` | `weapon_stack` | `WeaponStack` | 写武器 base stats（RuntimeStats 步 4） |
| `GainStatForEveryStatEffect` | `gain_stat_for_every_stat` | `StatLink` | Hook.StatLinks → D02 §5 |
| `ConvertStatEffect` | `convert_stat` | `StatConvert` | Hook.ConvertStats* |
| `ChanceStatDamageEffect` | `chance_stat_damage` | `Hook` | 4 个 dmg_* 槽 |
| `HealingEffect` | `healing` | `Hook` | **Instant GE_Heal**，`Unapply` 空实现 |
| `ItemExplodingEffect` | `item_exploding` | `Exploding` | Hook.ExplodeOn* |
| `ProjectileEffect` | `projectile` | `Hook` | Hook.ProjectilesOnDeath / AlienEyes |
| `StructureEffect` | `structure` | `Structure` | Hook.Structures |
| `TurretEffect` | `turret` | `Structure` | 同上（+ 额外字段） |
| `BurnChanceEffect` | `burn_chance` | `Burning` | Hook.BurnChance（merge/remove 语义） |
| `NullEffect`（武器基类） | `weapon_null` | `Marker` | 运行时类型检测 |
| `BurningEffect`（武器） | `weapon_burning` | `Burning` | **覆写**武器 `BurningData` |
| `ExplodingEffect`（武器） | `weapon_exploding` | `Exploding` | 武器 `bIsExploding = true` |
| `GainStatEveryKilledEnemiesEffect` | `weapon_gain_stat_every_killed_enemies` | `Marker` | on_kill 累计 |
| `ProjectilesOnHitEffect` | `weapon_projectiles_on_hit` | `Marker` | on_hit 额外弹 |
| `SlowInZoneEffect` | `weapon_slow_in_zone` | `Marker` | 区域减速 |

### 2.4 三种 StorageMethod 的 GE 映射

| StorageMethod | 原作行为 | GAS 实现 |
|---|---|---|
| `Sum`(0) | `effects[key] += value` | GE Modifier `Additive` |
| `KeyValue`(1) | `effects[custom_key].push_back([key, value])` | **HookRegistry**（不进 GE） |
| `Replace`(2) | 备份 `base_value` 后覆写 | GE Modifier `Override` |

> ★ `StatCapEffect.unapply()` 原作**硬复位 999999** 而非还原 `base_value`。GAS 移除 GE 后 Aggregator 自动回落到 BaseValue 999999 → **行为一致 ✅**，无需特殊处理。

---

## 3. `UBrotatoRunSubsystem`：应用与卸载（★ 修复原作 bug）

### 3.1 句柄容器

```cpp
USTRUCT()
struct FBrotatoAppliedEffects
{
    GENERATED_BODY()
    TArray<FActiveGameplayEffectHandle>  GEHandles;       // GE 句柄
    TArray<FGuid>                        HookIds;         // HookRegistry 条目 ID
    TArray<FGameplayAbilitySpecHandle>   AbilityHandles;  // Passive Ability
    TArray<FGameplayTag>                 LooseTags;       // Rule.* 开关
};
```

### 3.2 AddItem / RemoveItem

```cpp
// UBrotatoRunSubsystem
TMap<FBrotatoItemInstanceId, FBrotatoAppliedEffects> AppliedMap;   // ★ 实例级 key，不是 ItemData

FBrotatoItemInstanceId UBrotatoRunSubsystem::AddItem(const UBrotatoItemData* Item)
{
    const FBrotatoItemInstanceId Id = FBrotatoItemInstanceId::New();   // FGuid
    Items.Add({Id, Item});
    FBrotatoAppliedEffects& Applied = AppliedMap.Add(Id);

    // 1. 全部 StatModifier/GainModifier 合成 ★一个★ GE Spec
    if (FGameplayEffectSpecHandle Spec = Item->BuildStatSpec(ASC, GE_ItemStats))
        Applied.GEHandles.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data));

    // 2. 钩子类写入 Registry，记录 GUID
    for (const FBrotatoEffectDef& Def : Item->Effects)
        if (IsHookKind(Def.Kind))
            Applied.HookIds.Add(HookRegistry->Add(Def, Id, Item));

    // 3. Rule 开关（Loose Tag）
    for (const FBrotatoEffectDef& Def : Item->Effects)
        if (Def.Kind == EBrotatoEffectKind::Rule)
        { ASC->AddLooseGameplayTag(Def.HookTag); Applied.LooseTags.Add(Def.HookTag); }

    // 4. Passive Ability
    for (const FBrotatoEffectDef& Def : Item->Effects)
        if (Def.GrantedPassive)
            Applied.AbilityHandles.Add(ASC->GiveAbility(
                FGameplayAbilitySpec(Def.GrantedPassive, 1, INDEX_NONE, const_cast<UBrotatoItemData*>(Item))));

    UpdateItemRelatedEffects();      // §3.4
    LinkedStats->MarkDirty();
    OnItemsChanged.Broadcast();
    return Id;
}

void UBrotatoRunSubsystem::RemoveItem(FBrotatoItemInstanceId Id)
{
    FBrotatoAppliedEffects Applied;
    if (!AppliedMap.RemoveAndCopyValue(Id, Applied)) return;

    for (auto H  : Applied.GEHandles)      ASC->RemoveActiveGameplayEffect(H);
    for (auto G  : Applied.HookIds)        HookRegistry->Remove(G);      // ★ GUID 精确移除
    for (auto AH : Applied.AbilityHandles) ASC->ClearAbility(AH);
    for (auto T  : Applied.LooseTags)      ASC->RemoveLooseGameplayTag(T);

    Items.RemoveAll([Id](const FRuntimeItem& I){ return I.Id == Id; });
    UpdateItemRelatedEffects();
    LinkedStats->MarkDirty();
    OnItemsChanged.Broadcast();
}
```

> **★ 修复原作 bug**：原作用 `Array.erase([key, value])` 靠**值比较**，同一物品叠 3 份时移除会错删（可能删掉别的物品的同值条目）。
> 本实现用 `FActiveGameplayEffectHandle` + `FGuid` + **实例级 Id**，精确对应。
> 验收：`E01` 装 3 个相同物品再卖 1 个 → 属性恰好减 1 份。

### 3.3 Loose Tag 的引用计数问题

`AddLooseGameplayTag` **本身带引用计数**（`TagCountContainer`），装 2 件同 Rule 的物品 → Count=2，移除 1 件 → Count=1，Tag 仍存在。**行为正确，无需额外处理**。

⚠️ 但 `Rule.CanAttackWhileMoving` 是**默认持有**（初始化时加一次）。Soldier 角色需要"取消"它 —— 实现方式：Soldier 的 Effect 是 `Rule` Kind 但 `Value = 0` → 导入器转成 **`RemoveLooseGameplayTag`** 语义，记录到 `Applied.LooseTags` 的"负向列表"，卸载时反向补回。

```cpp
// 建议扩展：
struct FBrotatoAppliedEffects { /* … */ TArray<FGameplayTag> RemovedTags; };
// Apply:  ASC->RemoveLooseGameplayTag(T);  RemovedTags.Add(T);
// Unapply: ASC->AddLooseGameplayTag(T);
```

### 3.4 四步幂等重算 `UpdateItemRelatedEffects`

原作 4 类"条件加成"（依赖武器列表），**顺序固定**：

```cpp
void UBrotatoRunSubsystem::UpdateItemRelatedEffects()
{
    // 每类都是「移除旧 GE → 按当前武器重算 → Apply 新 GE」的幂等重算
    RebuildConditionalGE(EConditional::UniqueWeapon,     GetUniqueWeaponIdCount());
    RebuildConditionalGE(EConditional::AdditionalWeapon, Weapons.Num());
    RebuildConditionalGE(EConditional::TierIVWeapon,     CountWeaponsOfTier(EBrotatoTier::Legendary));
    RebuildConditionalGE(EConditional::TierIWeapon,      CountWeaponsOfTier(EBrotatoTier::Common));
    RebuildSetBonusGE();
}

void UBrotatoRunSubsystem::RebuildConditionalGE(EConditional Kind, int32 Multiplier)
{
    if (ConditionalHandles[Kind].IsValid())
        ASC->RemoveActiveGameplayEffect(ConditionalHandles[Kind]);
    ConditionalHandles[Kind].Invalidate();
    if (Multiplier <= 0) return;

    const FGameplayTag HookTag = ConditionalHookTag(Kind);
    FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GE_ConditionalStats, 1.f, ASC->MakeEffectContext());
    TMap<FGameplayTag, float> Sum;
    for (const FBrotatoHookEntry& E : HookRegistry->Get(HookTag))
        Sum.FindOrAdd(E.AttributeTag) += E.Value * Multiplier;
    for (const auto& KV : Sum) Spec.Data->SetSetByCallerMagnitude(KV.Key, KV.Value);
    ConditionalHandles[Kind] = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
}
```

### 3.5 套装重算（★ 不叠加，只取当前档）

```cpp
void UBrotatoRunSubsystem::RebuildSetBonusGE()
{
    // 1. 移除全部套装 GE
    ASC->RemoveActiveEffectsWithGrantedTags(
        FGameplayTagContainer(BrotatoTags::Effect_Source_Set));

    // 2. 统计件数（一把武器可属多个 set）
    TMap<FGameplayTag, int32> SetCounts;
    for (const FRuntimeWeapon& W : Weapons)
        for (const UBrotatoSetData* S : W.Data->Sets)
            SetCounts.FindOrAdd(S->SetTag)++;

    // 3. ★ 只取当前件数对应的那一档
    for (const auto& KV : SetCounts)
    {
        if (KV.Value < 2) continue;
        const int32 Idx = FMath::Min(KV.Value - 2, 4);      // 2件=档0 … 6件=档4，7件+仍是档4
        const UBrotatoSetData* Set = SetRegistry->Find(KV.Key);
        ApplyEffectDefs(Set->Bonuses[Idx], GE_SetBonus, BrotatoTags::Effect_Source_Set);
    }
}
```

15 职业各 5 档数值见 `Docs/04 §6.6`。验收：`W07` Blade 2 件 → 近战+1/吸血+1%；**7 件 → 仍是 6 件档（+5/+5%）**。

---

## 4. 伤害管道（★ 双路径共享核心）

> **v2.1 修订（按 UE GAS C++ 正确用法校正，旧版 §4.1 作废）**：
> 1. ExecCalc **必须在构造函数注册 `RelevantAttributesToCapture`**，否则 GE 不捕获，`AttemptCalculateCapturedAttributeMagnitude` 静默返回 0。
> 2. 按目标拆成 `UBrotatoDamageExecCalc_Player` / `_Enemy` 两个子类。玩家用 Core/Secondary/Limit 属性集，Tier B 用 `UBrotatoEnemyAttributeSet`（D02 §2.6），**不能共用一套捕获定义**（旧版对 Boss 读出的 Armor 恒为 0，输出到 Core 集的 `IncomingDamage` 也会被丢弃 → Boss 不掉血）。
> 3. 参数标记（`Data.NotDodgeable` / `Data.NoArmor`）改走 **AssetTags**（`AddDynamicAssetTag` / `GetAllAssetTags`），**禁止**借用 `DynamicGrantedTags`。
> 4. `Dodge` 属性是百分数（30 = 30%），进 `Resolve` 前 **/100**（D01 §4.2 要求 0~1；02 §4.2 原作同样 `/100.0`）。
> 5. 删除 ExecCalc 内的无敌兜底判断：无敌**只**由 `GE_Damage` 的 GE 组件拦截，`GE_Damage_Bypass` 不再被误拦。
> 6. 随机流由调用方经 Context 注入；无尽因子走 SetByCaller。原因：`BrotatoGAS` **不得依赖** `BrotatoGameplay`（`UBrotatoRunSubsystem` 所在模块，D00 §3.1）。
> 7. 受击后处理（命中表现 / 无敌窗口 / 闪避钩子）统一放在施加入口 `FBrotatoGasDamage::Apply`（§4.5），**闪避分支同样覆盖**。
> 8. Tier B 补齐 `damage_against_bosses` 乘区与巨人腰带追伤，与 Tier C（§4.7）逐行对应。

### 4.0 总体流程

```
命中判定（D01 S9a）
 └─ FBrotatoGasDamage::Apply(SourceASC, TargetASC, Kind, Req)      ← BrotatoGameplay，Tier A/B 唯一入口
     ├─ ① 新建 Context（注入 CombatRng / KnockDir / EffectScale）     ← 每次命中、每个目标各一份
     ├─ ② MakeOutgoingSpec + SetByCaller + AddDynamicAssetTag
     ├─ ③ ApplyGameplayEffectSpecToTarget（Instant，同步执行）
     │     ├─ TargetTagRequirements 组件：目标有 State.Invincible → 整个 GE 被拒，返回句柄 !WasSuccessfullyApplied()
     │     └─ ExecCalc_Player / ExecCalc_Enemy：捕获 → FBrotatoDamageMath::Resolve → 输出 IncomingDamage，结果写回 Context
     │           └─ AttributeSet::PostGameplayEffectExecute：IncomingDamage → Health（D02 §6.2）
     └─ ④ 读 Context 结果 → QueueHitCue（限流）/ GE_Invincible / OnDodge 钩子
```

### 4.1 GE 资产配置

| 资产 | 目标 | Execution | GE 组件（UE 5.3+ GEComponents） |
|---|---|---|---|
| `GE_Damage` | 玩家 | `UBrotatoDamageExecCalc_Player` | `UTargetTagRequirementsGameplayEffectComponent`：`ApplicationTagRequirements.IgnoreTags = State.Invincible` |
| `GE_Damage_Bypass` | 玩家（`bypass_invincibility`） | `UBrotatoDamageExecCalc_Player` | 无 Tag 需求 |
| `GE_Damage_Enemy` | Tier B（Boss / 精英） | `UBrotatoDamageExecCalc_Enemy` | 无 |

三者共同项：

| 项 | 值 |
|---|---|
| `DurationPolicy` | **Instant** |
| `GameplayCues` | **留空**。命中表现由 `Apply` 读结果后走 `UFxSubsystem::QueueHitCue`（D04 §9.3 限流入口，与 Tier C 共用）。原因：① 闪避/免伤时 ExecCalc 无 Modifier 输出，而 `bRequireModifierSuccessToTriggerCues` 默认为 true，挂在 GE 上的 Cue 不会播；② GE 上的 Cue Tag 是静态的，不能按暴击/闪避切换；③ 绕过限流会突破 100 特效上限 |
| SetByCaller（通用） | `Data.Damage` / `Data.CritChance`（**0~1**）/ `Data.CritDamage` |
| SetByCaller（仅 `_Enemy` 读取） | `Data.DamageAgainstBosses`（百分数）/ `Data.GiantCritDamage` / `Data.EndlessFactor` |
| 参数标记（AssetTags） | `Data.NotDodgeable` / `Data.NoArmor`。语义固定的写进资产的 `UAssetTagsGameplayEffectComponent`，按次变化的由 `Spec.AddDynamicAssetTag()` 添加；ExecCalc 用 `GetAllAssetTags()` 读两者的并集 |

> ⚠️ **为什么不用 `DynamicGrantedTags` 传标记**：它的语义是「GE **生效期间**授予目标 ASC 的 Tag」。Instant GE 不会授予（旧写法只是碰巧能读到）；Duration/Periodic GE（如 §6.4 燃烧）则会把 `Data.NoArmor` **真的挂到目标身上**，污染所有 Tag 查询。
> ⚠️ **为什么不在 ExecCalc 里判无敌**：GE 组件拦截发生在 Execution 之前，被拒的 GE 根本不会执行。ExecCalc 内再判一次，只会把共用同一 ExecCalc 的 `GE_Damage_Bypass` 误拦。
> ⚠️ 击退（`KnockbackAmount`）不经 GE：`Apply` 返回后由调用方按 `Req.KnockDir × Req.KnockAmount` 处理，与 Tier C 写 `E.KnockbackVector` 对应。旧版列出的 `Data.KnockbackAmount` SetByCaller 键 ExecCalc 从不读取，已移除。

### 4.2 ExecCalc 声明

```cpp
// BrotatoGAS/Public/GAS/BrotatoDamageExecCalc.h
#pragma once
#include "GameplayEffectExecutionCalculation.h"
#include "BrotatoDamageMath.h"
#include "BrotatoDamageExecCalc.generated.h"

/** 公共基类：只放与目标属性集无关的读写辅助。★ ExecCalc 在 CDO 上执行，禁止任何可变成员 */
UCLASS(Abstract)
class BROTATOGAS_API UBrotatoDamageExecCalc : public UGameplayEffectExecutionCalculation
{
    GENERATED_BODY()
protected:
    /** SetByCaller + AssetTags → FResolveParams（Raw / Crit / bDodgeable / bArmorApplied） */
    static void ReadSpecParams(const FGameplayEffectSpec& Spec, FBrotatoDamageMath::FResolveParams& P);
    /** 从 Context 取调用方注入的 Combat 随机流 */
    static FBrotatoRandom& GetCombatRandom(const FGameplayEffectSpec& Spec);
    /** 结算结果写回 Context，供 FBrotatoGasDamage::Apply 在同步返回后读取 */
    static void WriteOutcome(const FGameplayEffectSpec& Spec,
                             const FBrotatoDamageMath::FResolveResult& R, int32 Final);
};

/** Tier A：玩家受击（乘法护甲 + 闪避 + 免伤） */
UCLASS()
class BROTATOGAS_API UBrotatoDamageExecCalc_Player : public UBrotatoDamageExecCalc
{
    GENERATED_BODY()
public:
    UBrotatoDamageExecCalc_Player();
    virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& Exec,
                                        FGameplayEffectCustomExecutionOutput& Out) const override;
};

/** Tier B：Boss / 精英受击（减法护甲 + Boss 乘区 + 巨人腰带） */
UCLASS()
class BROTATOGAS_API UBrotatoDamageExecCalc_Enemy : public UBrotatoDamageExecCalc
{
    GENERATED_BODY()
public:
    UBrotatoDamageExecCalc_Enemy();
    virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& Exec,
                                        FGameplayEffectCustomExecutionOutput& Out) const override;
};
```

### 4.3 ExecCalc 实现

```cpp
// BrotatoGAS/Private/GAS/BrotatoDamageExecCalc.cpp
#include "GAS/BrotatoDamageExecCalc.h"
#include "GAS/BrotatoEffectContext.h"
#include "Attributes/BrotatoCoreAttributeSet.h"
#include "Attributes/BrotatoSecondaryAttributeSet.h"
#include "Attributes/BrotatoLimitAttributeSet.h"
#include "Attributes/BrotatoEnemyAttributeSet.h"
#include "BrotatoNativeTags.h"
#include "BrotatoMathInt.h"

namespace
{
    // ── 玩家侧捕获定义（Target、非快照：取命中瞬间的当前值） ──
    struct FPlayerDamageStatics
    {
        DECLARE_ATTRIBUTE_CAPTUREDEF(Armor);
        DECLARE_ATTRIBUTE_CAPTUREDEF(Dodge);
        DECLARE_ATTRIBUTE_CAPTUREDEF(DodgeCap);
        DECLARE_ATTRIBUTE_CAPTUREDEF(HitProtection);
        DECLARE_ATTRIBUTE_CAPTUREDEF(IncomingDamage);   // ★ 仅用作输出目标，不注册捕获

        FPlayerDamageStatics()
        {
            DEFINE_ATTRIBUTE_CAPTUREDEF(UBrotatoCoreAttributeSet,      Armor,          Target, false);
            DEFINE_ATTRIBUTE_CAPTUREDEF(UBrotatoCoreAttributeSet,      Dodge,          Target, false);
            DEFINE_ATTRIBUTE_CAPTUREDEF(UBrotatoLimitAttributeSet,     DodgeCap,       Target, false);
            DEFINE_ATTRIBUTE_CAPTUREDEF(UBrotatoSecondaryAttributeSet, HitProtection,  Target, false);
            DEFINE_ATTRIBUTE_CAPTUREDEF(UBrotatoCoreAttributeSet,      IncomingDamage, Target, false);
        }
    };
    const FPlayerDamageStatics& PlayerStatics() { static FPlayerDamageStatics S; return S; }

    // ── Tier B 捕获定义（★ 来自 UBrotatoEnemyAttributeSet，不是 Core 集） ──
    struct FEnemyDamageStatics
    {
        DECLARE_ATTRIBUTE_CAPTUREDEF(Armor);
        DECLARE_ATTRIBUTE_CAPTUREDEF(Health);           // 巨人腰带按「受击前」当前血量追伤
        DECLARE_ATTRIBUTE_CAPTUREDEF(IncomingDamage);   // ★ 仅用作输出目标，不注册捕获

        FEnemyDamageStatics()
        {
            DEFINE_ATTRIBUTE_CAPTUREDEF(UBrotatoEnemyAttributeSet, Armor,          Target, false);
            DEFINE_ATTRIBUTE_CAPTUREDEF(UBrotatoEnemyAttributeSet, Health,         Target, false);
            DEFINE_ATTRIBUTE_CAPTUREDEF(UBrotatoEnemyAttributeSet, IncomingDamage, Target, false);
        }
    };
    const FEnemyDamageStatics& EnemyStatics() { static FEnemyDamageStatics S; return S; }

    FAggregatorEvaluateParameters MakeEvalParams(const FGameplayEffectSpec& Spec)
    {
        FAggregatorEvaluateParameters Eval;
        Eval.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
        Eval.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
        return Eval;
    }
}

// ───────────────────────── 公共辅助 ─────────────────────────

void UBrotatoDamageExecCalc::ReadSpecParams(const FGameplayEffectSpec& Spec,
                                            FBrotatoDamageMath::FResolveParams& P)
{
    // Data.Damage 必填：漏写时打 Warning 便于排查；其余给出与 D01 一致的默认值
    P.Raw        = Spec.GetSetByCallerMagnitude(BrotatoTags::Data_Damage,     /*WarnIfNotFound*/ true,  0.f);
    P.CritChance = Spec.GetSetByCallerMagnitude(BrotatoTags::Data_CritChance, /*WarnIfNotFound*/ false, 0.f);
    P.CritDamage = Spec.GetSetByCallerMagnitude(BrotatoTags::Data_CritDamage, /*WarnIfNotFound*/ false, 1.5f);

    // ★ 资产 AssetTags ∪ 运行时 DynamicAssetTags（不是 DynamicGrantedTags）
    FGameplayTagContainer AssetTags;
    Spec.GetAllAssetTags(AssetTags);
    P.bDodgeable    = !AssetTags.HasTagExact(BrotatoTags::Data_NotDodgeable);
    P.bArmorApplied = !AssetTags.HasTagExact(BrotatoTags::Data_NoArmor);
}

FBrotatoRandom& UBrotatoDamageExecCalc::GetCombatRandom(const FGameplayEffectSpec& Spec)
{
    const FBrotatoEffectContext* Ctx = FBrotatoEffectContext::FromHandle(Spec.GetContext());
    checkf(Ctx && Ctx->CombatRng,
           TEXT("GE_Damage* 必须经 FBrotatoGasDamage::Apply 施加（未注入 Combat 随机流）"));
    return *Ctx->CombatRng;
}

void UBrotatoDamageExecCalc::WriteOutcome(const FGameplayEffectSpec& Spec,
                                          const FBrotatoDamageMath::FResolveResult& R, int32 Final)
{
    // Spec 是 const，但 Context 是共享对象，可安全写入；前提是每个目标独享 Context（§4.5 ①）
    if (FBrotatoEffectContext* Ctx = FBrotatoEffectContext::FromHandle(Spec.GetContext()))
    {
        Ctx->bCrit       = R.bCrit;
        Ctx->bDodged     = R.bDodged;
        Ctx->bProtected  = R.bProtected;
        Ctx->FinalDamage = Final;
    }
}

// ───────────────────────── Tier A：玩家 ─────────────────────────

UBrotatoDamageExecCalc_Player::UBrotatoDamageExecCalc_Player()
{
    // ★ 必须注册：GE 依据此列表在 Apply 时创建捕获规格，漏注册 → 捕获失败、值恒为 0
    RelevantAttributesToCapture.Add(PlayerStatics().ArmorDef);
    RelevantAttributesToCapture.Add(PlayerStatics().DodgeDef);
    RelevantAttributesToCapture.Add(PlayerStatics().DodgeCapDef);
    RelevantAttributesToCapture.Add(PlayerStatics().HitProtectionDef);
    // IncomingDamage 是元属性，只写不读，不注册
}

void UBrotatoDamageExecCalc_Player::Execute_Implementation(
    const FGameplayEffectCustomExecutionParameters& Exec,
    FGameplayEffectCustomExecutionOutput& Out) const
{
    const FGameplayEffectSpec& Spec = Exec.GetOwningSpec();
    const FAggregatorEvaluateParameters Eval = MakeEvalParams(Spec);

    // ① 读参数
    FBrotatoDamageMath::FResolveParams P;
    ReadSpecParams(Spec, P);
    P.bIsPlayer = true;                                   // ★ 乘法护甲

    float Dodge = 0.f, DodgeCap = 60.f, Prot = 0.f;
    Exec.AttemptCalculateCapturedAttributeMagnitude(PlayerStatics().ArmorDef,         Eval, P.Armor);
    Exec.AttemptCalculateCapturedAttributeMagnitude(PlayerStatics().DodgeDef,         Eval, Dodge);
    Exec.AttemptCalculateCapturedAttributeMagnitude(PlayerStatics().DodgeCapDef,      Eval, DodgeCap);
    Exec.AttemptCalculateCapturedAttributeMagnitude(PlayerStatics().HitProtectionDef, Eval, Prot);

    P.Dodge         = Dodge / 100.f;                      // ★ 属性为百分数 → Resolve 要求 0~1
    P.DodgeCap      = DodgeCap;                           // 百分数，Resolve 内部 /100
    P.HitProtection = FMath::Max(0, FMath::TruncToInt(Prot));

    // ② ★ 调用共享核心（绝不在这里写公式）
    const FBrotatoDamageMath::FResolveResult R = FBrotatoDamageMath::Resolve(P, GetCombatRandom(Spec));

    // ③ 输出：伤害走元属性，由 PostGameplayEffectExecute 扣血（D02 §6.2）
    if (R.Final > 0)
    {
        Out.AddOutputModifier(FGameplayModifierEvaluatedData(
            PlayerStatics().IncomingDamageProperty, EGameplayModOp::AddBase, (float)R.Final));
    }

    // ④ 免伤消耗：Instant 输出改 BaseValue，每波由 GE_HitProtectionReset 归零（§4.6）
    if (R.bProtected)
    {
        Out.AddOutputModifier(FGameplayModifierEvaluatedData(
            PlayerStatics().HitProtectionProperty, EGameplayModOp::AddBase, -1.f));
    }

    WriteOutcome(Spec, R, R.Final);
}

// ───────────────────────── Tier B：Boss / 精英 ─────────────────────────

UBrotatoDamageExecCalc_Enemy::UBrotatoDamageExecCalc_Enemy()
{
    RelevantAttributesToCapture.Add(EnemyStatics().ArmorDef);
    RelevantAttributesToCapture.Add(EnemyStatics().HealthDef);
}

void UBrotatoDamageExecCalc_Enemy::Execute_Implementation(
    const FGameplayEffectCustomExecutionParameters& Exec,
    FGameplayEffectCustomExecutionOutput& Out) const
{
    using namespace BrotatoMathInt;
    const FGameplayEffectSpec& Spec = Exec.GetOwningSpec();
    const FAggregatorEvaluateParameters Eval = MakeEvalParams(Spec);

    // ① 读参数（与 §4.7 Tier C 的 ApplyToEnemy 逐行对应）
    FBrotatoDamageMath::FResolveParams P;
    ReadSpecParams(Spec, P);
    P.bIsPlayer     = false;                              // ★ 减法护甲
    P.Dodge         = 0.f;                                // 敌人无闪避 / 免伤
    P.HitProtection = 0;

    float Health = 0.f;
    Exec.AttemptCalculateCapturedAttributeMagnitude(EnemyStatics().ArmorDef,  Eval, P.Armor);
    Exec.AttemptCalculateCapturedAttributeMagnitude(EnemyStatics().HealthDef, Eval, Health);

    const bool  bIsBoss   = Eval.TargetTags && Eval.TargetTags->HasTag(BrotatoTags::Enemy_Boss);
    const float DmgVsBoss = Spec.GetSetByCallerMagnitude(BrotatoTags::Data_DamageAgainstBosses, false, 0.f);
    const float GiantCrit = Spec.GetSetByCallerMagnitude(BrotatoTags::Data_GiantCritDamage,     false, 0.f);
    const float EF        = Spec.GetSetByCallerMagnitude(BrotatoTags::Data_EndlessFactor,       false, 0.f);

    // ② Boss 乘区（damage_against_bosses），★ 在 Resolve 之前
    if (bIsBoss && DmgVsBoss > 0.f)
        P.Raw = (float)BrotatoTruncInt(P.Raw * (1.f + DmgVsBoss / 100.f));

    // ③ 共享核心
    const FBrotatoDamageMath::FResolveResult R = FBrotatoDamageMath::Resolve(P, GetCombatRandom(Spec));

    // ④ 巨人腰带追伤（暴击后追加，按受击前血量）
    int32 Final = R.Final;
    if (R.bCrit)
        Final += FBrotatoDamageMath::GiantCritBonus(BrotatoTruncInt(Health), GiantCrit, bIsBoss, EF);

    // ⑤ 输出到 Enemy 集的元属性（★ 不是 Core 集；目标没有该集时 GAS 会直接丢弃修改）
    if (Final > 0)
    {
        Out.AddOutputModifier(FGameplayModifierEvaluatedData(
            EnemyStatics().IncomingDamageProperty, EGameplayModOp::AddBase, (float)Final));
    }

    WriteOutcome(Spec, R, Final);
}
```

**GAS 要点对照（评审时逐条核对）**：

| # | 要点 | 本文写法 | 常见错误 |
|---|---|---|---|
| E1 | 捕获注册 | 构造函数 `RelevantAttributesToCapture.Add(...)` | 只写 Statics 不注册 → 值恒为 0、无报错 |
| E2 | 捕获来源与快照 | 目标防御属性一律 `Target, false` | 用快照会读到 Spec 创建时的旧值 |
| E3 | 元属性 | `IncomingDamage` 只作输出，不注册捕获 | 注册捕获浪费 Aggregator |
| E4 | 属性集归属 | 按目标分 `_Player` / `_Enemy` | 一个 ExecCalc 混用两个集 → 目标缺集时捕获失败、输出被丢弃 |
| E5 | 参数标记 | `AddDynamicAssetTag` + `GetAllAssetTags` | `DynamicGrantedTags`：Duration GE 会把 Tag 授予目标 |
| E6 | 无状态 | 随机流从 Context 取，无成员变量 | 在 ExecCalc 上缓存任何状态（CDO 全局共享） |
| E7 | 模块依赖 | ExecCalc 不 include `BrotatoGameplay` | ExecCalc 里 `GetSubsystem<UBrotatoRunSubsystem>()` → 违反 D00 §3.1 |
| E8 | ModOp 命名 | 本章统一使用 `EGameplayModOp::AddBase` | Epic 当前 API 同时列出 `AddBase` 与兼容名称 `Additive = 0`，同值；不能把旧名视为非法 API，也不据此推断首次引入版本 |
| E9 | 无敌拦截 | 只在 GE 组件 `TargetTagRequirements` 配置 | ExecCalc 内重复判断 → 误拦 Bypass GE |

API 核对依据：[Epic `EGameplayModOp::Type`](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/GameplayAbilities/EGameplayModOp__Type)（2026-10-16 核对）。此项是接口文档核查，不代表本文示例已在 UE 5.8 工程中编译验证。

### 4.4 自定义 `FGameplayEffectContext`

```cpp
// BrotatoGAS/Public/GAS/BrotatoEffectContext.h
USTRUCT()
struct BROTATOGAS_API FBrotatoEffectContext : public FGameplayEffectContext
{
    GENERATED_BODY()

    // ── 输入（由 FBrotatoGasDamage::Apply 填写） ──
    UPROPERTY() TWeakObjectPtr<const UObject> SourceItem;   // 伤害来源物品（统计追踪）
    UPROPERTY() int32     SourceWeaponIndex = INDEX_NONE;
    UPROPERTY() float     EffectScale = 1.f;                 // 特效缩放（SMG = 0.2）
    UPROPERTY() FVector2D KnockDir = FVector2D::ZeroVector;

    /** ★ Combat 随机流注入点。BrotatoGAS 不能依赖 BrotatoGameplay（D00 §3.1），
     *  由调用方在同步 Apply 期间填入、返回后清空；非 UPROPERTY、不序列化。
     *  只允许用于 Instant GE（Periodic GE 的 Context 会长期存活，指针可能失效）。 */
    FBrotatoRandom* CombatRng = nullptr;

    // ── 输出（ExecCalc 写入，Apply 同步返回后读取） ──
    bool  bCrit = false, bDodged = false, bProtected = false;
    int32 FinalDamage = 0;

    /** ★ 带类型校验的取用；不要直接 static_cast（AllocGameplayEffectContext 未接管时会越界写） */
    static FBrotatoEffectContext* FromHandle(FGameplayEffectContextHandle Handle)
    {
        FGameplayEffectContext* Base = Handle.Get();
        return (Base && Base->GetScriptStruct()->IsChildOf(StaticStruct()))
             ? static_cast<FBrotatoEffectContext*>(Base) : nullptr;
    }

    virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }

    virtual FBrotatoEffectContext* Duplicate() const override
    {
        FBrotatoEffectContext* NewCtx = new FBrotatoEffectContext(*this);
        if (GetHitResult())
            NewCtx->AddHitResult(*GetHitResult(), /*bReset*/ true);   // ★ HitResult 是 TSharedPtr，需深拷贝
        return NewCtx;
    }
    // 单机不复制：沿用基类 NetSerialize，输出字段与 CombatRng 均不参与序列化
};

template<> struct TStructOpsTypeTraits<FBrotatoEffectContext>
    : public TStructOpsTypeTraitsBase2<FBrotatoEffectContext>
{ enum { WithNetSerializer = true, WithCopy = true }; };   // ★ 子类必须重新声明，否则拷贝时被切片
```

```cpp
// BrotatoGAS/Private/GAS/BrotatoAbilitySystemGlobals.cpp（ini 配置见 D00 §4.2）
FGameplayEffectContext* UBrotatoAbilitySystemGlobals::AllocGameplayEffectContext() const
{
    return new FBrotatoEffectContext();
}
```

### 4.5 施加入口 `FBrotatoGasDamage::Apply`（Tier A/B 唯一入口）

与 Tier C 的 `FBrotatoDamagePipeline::ApplyToEnemy` 对称：**同一个 `FDamageRequest`** 同时驱动两条路径（G7）。

```cpp
// BrotatoGameplay/Public/Combat/BrotatoGasDamage.h
enum class EBrotatoGasDamageKind : uint8
{
    Player,          // GE_Damage        ：受 State.Invincible 拦截
    PlayerBypass,    // GE_Damage_Bypass ：bypass_invincibility
    Enemy,           // GE_Damage_Enemy  ：Tier B
};

struct FBrotatoGasDamageResult
{
    bool  bApplied = false;     // false = 被无敌窗口拦截（或参数无效）
    int32 Final    = 0;
    bool  bCrit = false, bDodged = false, bProtected = false;
};

namespace FBrotatoGasDamage
{
    BROTATOGAMEPLAY_API FBrotatoGasDamageResult Apply(UAbilitySystemComponent* SourceASC,
                                                      UAbilitySystemComponent* TargetASC,
                                                      EBrotatoGasDamageKind Kind,
                                                      const FDamageRequest& Req);
}
```

```cpp
// BrotatoGameplay/Private/Combat/BrotatoGasDamage.cpp
FBrotatoGasDamageResult FBrotatoGasDamage::Apply(UAbilitySystemComponent* SourceASC,
                                                 UAbilitySystemComponent* TargetASC,
                                                 EBrotatoGasDamageKind Kind,
                                                 const FDamageRequest& Req)
{
    FBrotatoGasDamageResult Res;
    if (!SourceASC || !TargetASC) return Res;
    UWorld* World = TargetASC->GetWorld();

    // ① ★ 每次命中、每个目标都新建 Context。ExecCalc 会往 Context 写结果，
    //    若多个目标共用一个 SpecHandle/Context（如爆炸），结果会互相覆盖
    FGameplayEffectContextHandle CtxHandle = SourceASC->MakeEffectContext();
    FBrotatoEffectContext* Ctx = FBrotatoEffectContext::FromHandle(CtxHandle);
    checkf(Ctx, TEXT("AllocGameplayEffectContext 未接管：检查 D00 §4.2 的 AbilitySystemGlobalsClassName"));
    Ctx->CombatRng   = &World->GetSubsystem<UBrotatoRunSubsystem>()->GetRandom(EBrotatoRngChannel::Combat);
    Ctx->KnockDir    = Req.KnockDir;
    Ctx->EffectScale = Req.EffectScale;

    // ② 构建 Spec
    const TSubclassOf<UGameplayEffect> GE =
        Kind == EBrotatoGasDamageKind::Enemy        ? GE_Damage_Enemy  :
        Kind == EBrotatoGasDamageKind::PlayerBypass ? GE_Damage_Bypass : GE_Damage;
    FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(GE, 1.f, CtxHandle);
    FGameplayEffectSpec& Spec = *SpecHandle.Data;

    Spec.SetSetByCallerMagnitude(BrotatoTags::Data_Damage,     Req.Damage);
    Spec.SetSetByCallerMagnitude(BrotatoTags::Data_CritChance, Req.CritChance);   // 0~1
    Spec.SetSetByCallerMagnitude(BrotatoTags::Data_CritDamage, Req.CritDamage);
    if (Kind == EBrotatoGasDamageKind::Enemy)
    {
        Spec.SetSetByCallerMagnitude(BrotatoTags::Data_DamageAgainstBosses, Req.DamageAgainstBosses);
        Spec.SetSetByCallerMagnitude(BrotatoTags::Data_GiantCritDamage,     Req.GiantCritDamage);
        Spec.SetSetByCallerMagnitude(BrotatoTags::Data_EndlessFactor,       Req.EndlessFactor);
    }
    if (!Req.bDodgeable)    Spec.AddDynamicAssetTag(BrotatoTags::Data_NotDodgeable);
    if (!Req.bArmorApplied) Spec.AddDynamicAssetTag(BrotatoTags::Data_NoArmor);

    // ③ Instant GE 同步执行；被 ApplicationTagRequirements 拒绝时返回 !WasSuccessfullyApplied()
    const FActiveGameplayEffectHandle Handle = SourceASC->ApplyGameplayEffectSpecToTarget(Spec, TargetASC);
    Ctx->CombatRng = nullptr;                    // 注入指针只在本次同步执行内有效
    if (!Handle.WasSuccessfullyApplied())
        return Res;                              // 无敌窗口内：完全无效（02 §5.4）

    // ④ 读结果（ExecCalc 写入的就是本 Context 对象）
    Res.bApplied   = true;
    Res.Final      = Ctx->FinalDamage;
    Res.bCrit      = Ctx->bCrit;
    Res.bDodged    = Ctx->bDodged;
    Res.bProtected = Ctx->bProtected;

    // ⑤ 命中表现：与 Tier C 共用限流入口（D04 §9.3），闪避也会播 Hit.Dodge
    const FVector Loc = TargetASC->GetAvatarActor()->GetActorLocation();
    World->GetSubsystem<UFxSubsystem>()->QueueHitCue(
        FVector2D(Loc), Res.Final, Res.bCrit, Res.bDodged, Req.EffectScale);

    // ⑥ 玩家无敌窗口：★ 只看 dodgeable（闪避成功、免伤同样触发），与是否 bypass 无关
    //    原作：if dodgeable: _invincibility_timer.start(get_iframes(dmg))（02 §5.4）
    if (Kind != EBrotatoGasDamageKind::Enemy && Req.bDodgeable)
    {
        const float MaxHp = TargetASC->GetNumericAttribute(UBrotatoCoreAttributeSet::GetMaxHpAttribute());
        const float Sec   = FBrotatoDamageMath::GetInvincibilitySeconds((float)Res.Final, MaxHp, Req.EndlessFactor);
        FGameplayEffectSpecHandle Inv = TargetASC->MakeOutgoingSpec(GE_Invincible, 1.f, TargetASC->MakeEffectContext());
        Inv.Data->SetSetByCallerMagnitude(BrotatoTags::Data_Duration, Sec);
        TargetASC->ApplyGameplayEffectSpecToSelf(*Inv.Data);
    }

    // ⑦ 闪避钩子：dmg_on_dodge / heal_on_dodge / temp_stats_on_dodge（02 §4.2），经 HookRegistry 派发（§5）
    //    ★ 闪避时 ExecCalc 无输出 → PostGameplayEffectExecute 不会被调用，因此只能在这里派发
    return Res;
}
```

> **调用约束**：
> - `GE_Damage*` **只能**经 `FBrotatoGasDamage::Apply` 施加。直接 `ApplyGameplayEffectToTarget` 时没有注入随机流，`GetCombatRandom` 的 `checkf` 会直接触发。
> - `GE_Damage_Enemy` 的目标必须挂 `UBrotatoEnemyAttributeSet`，并实现与 D02 §6.2 同构的 `PostGameplayEffectExecute`（`IncomingDamage → Health` → 死亡）。
> - 真 Boss 在生成时 `AddLooseGameplayTag(Enemy.Boss)`，精英不加；`_Enemy` 通过 `CapturedTargetTags` 读取该 Tag 判定 Boss 乘区。
> - `Req.DamageAgainstBosses` / `GiantCritDamage` / `EndlessFactor` 与 Tier C 取自同一份 `FBrotatoStatSnapshot`，保证双路径输入一致。

### 4.6 免伤次数每波重置：`GE_HitProtectionReset`

`hit_protection` 是**每波**次数（原作 `player.gd` 每波重建时 `_hit_protection = get_stat("hit_protection")`）。在 GAS 中的表达方式：

| 层 | 内容 |
|---|---|
| 次数来源 | 物品 / 角色的 Infinite GE 对 `HitProtection` 做 `AddBase`（改 CurrentValue） |
| 消耗 | ExecCalc 输出 Instant `AddBase −1`，作用于 **BaseValue**（§4.3 ④） |
| 重置 | 波首对玩家施加 `GE_HitProtectionReset`：Instant，Modifier = `HitProtection` · `Override` · `0` → BaseValue 归零，Current 回到 0 + 物品提供的次数 |

> ⚠️ 漏掉重置时，BaseValue 会跨波累计成负数，后续波次的免伤会被提前耗尽。ExecCalc 对捕获值做了 `Max(0, …)`，只能防止出现负次数，不能代替重置。

### 4.7 Tier C：纯函数管道

```cpp
namespace FBrotatoDamagePipeline
{
    void ApplyToEnemy(FEnemyState& E, const FDamageRequest& Req, FBrotatoRandom& Rng)
    {
        using namespace BrotatoMathInt;
        FBrotatoDamageMath::FResolveParams P;
        P.Raw           = Req.Damage;
        P.Armor         = E.Armor;
        P.Dodge         = 0.f;                   // 普通敌人 dodge = 0
        P.CritChance    = Req.CritChance;
        P.CritDamage    = Req.CritDamage;
        P.bArmorApplied = Req.bArmorApplied;
        P.bDodgeable    = Req.bDodgeable;
        P.bIsPlayer     = false;                 // ★ 减法护甲

        // Boss 额外乘区（damage_against_bosses），★ 在 Resolve 之前
        if (E.bIsBoss && Req.DamageAgainstBosses > 0.f)
            P.Raw = BrotatoTruncInt(P.Raw * (1.f + Req.DamageAgainstBosses / 100.f));

        // 穿透/弹射衰减（已在投射物侧处理，Req.Damage 已是衰减后的值）
        const auto R = FBrotatoDamageMath::Resolve(P, Rng);

        int32 Final = R.Final;
        if (R.bCrit)
            Final += FBrotatoDamageMath::GiantCritBonus(E.Health, Req.GiantCritDamage,
                                                        E.bIsBoss, Req.EndlessFactor);

        E.Health          = FMath::Max(0, E.Health - Final);
        E.KnockbackVector = Req.KnockDir * Req.KnockAmount;
        if (!R.bDodged && !R.bProtected) E.FlashTimeLeft = BrotatoConst::HitFlash;   // 0.1 s

        FxSubsystem->QueueHitCue(E.Pos, Final, R.bCrit, R.bDodged, Req.EffectScale);
        if (E.Health <= 0) EnemySubsystem->Kill(E, Req);
    }
}
```

> **G7 铁律**：ExecCalc 与 Pipeline 都**只做「捕获 → 调 Math → 写回」**。
> `Tests/GAS/DualPathConsistencyTest.cpp` 用同 seed、同一个 `FDamageRequest` 分别跑 `FBrotatoGasDamage::Apply(…, Enemy, Req)`（目标为挂 `UBrotatoEnemyAttributeSet` 的测试 Actor）与 `FBrotatoDamagePipeline::ApplyToEnemy`，
> 断言 `Final / bCrit / bDodged / RngCalls` 完全一致；用例须覆盖 **Boss（带 `Enemy.Boss`）+ `DamageAgainstBosses` + `GiantCritDamage` + 暴击** 组合。旧版用例里 Tier C 没有 Boss，测不出 GAS 路径漏掉的这两条规则。

---

## 5. HookRegistry（40 个钩子键）

### 5.1 数据结构

```cpp
USTRUCT()
struct BROTATOGAS_API FBrotatoHookEntry
{
    GENERATED_BODY()
    UPROPERTY() FGuid        Id;                  // ★ 精确移除用
    UPROPERTY() FGameplayTag AttributeTag;        // 目标属性 / 目标键
    UPROPERTY() float        Value = 0.f;
    UPROPERTY() float        ExtraA = 0.f;        // chance / max_value / nb_stat_scaled
    UPROPERTY() FName        ExtraName;           // scale_source / set_id / weapon_id
    UPROPERTY() bool         bPermOnly = false;
    UPROPERTY() FBrotatoItemInstanceId OwnerInstance;          // 归属实例
    UPROPERTY() TWeakObjectPtr<const UObject> SourceAsset;     // 统计追踪用
    UPROPERTY() const FBrotatoEffectDef* Def = nullptr;        // 复杂钩子（结构物/投射物/爆炸）
};

UCLASS()
class BROTATOGAS_API UBrotatoHookRegistry : public UObject
{
    GENERATED_BODY()
public:
    FGuid Add(const FBrotatoEffectDef& Def, FBrotatoItemInstanceId Owner, const UObject* Src);
    void  Remove(const FGuid& Id);
    const TArray<FBrotatoHookEntry>& Get(FGameplayTag HookTag) const;
    int32 Num(FGameplayTag HookTag) const;

    /** BurnChance 特化：merge / remove 语义（各字段加减，非覆盖） */
    void MergeBurnChance(const FBurningData& BD, const FGuid& Id);
    void RemoveBurnChance(const FGuid& Id);
    const FBurningData& GetBurnChance() const { return BurnChanceAccum; }

private:
    TMap<FGameplayTag, TArray<FBrotatoHookEntry>> Slots;
    FBurningData BurnChanceAccum;
    TMap<FGuid, FBurningData> BurnContributions;
};
```

### 5.2 触发风格

**风格 1 · 直接遍历（高频钩子）**

```cpp
void UBrotatoRunSubsystem::OnGoldPickedUp(int32 Value)
{
    for (const FBrotatoHookEntry& E : HookRegistry->Get(BrotatoTags::Hook_DmgWhenPickupGold))
        if (Rng.ChanceSuccess(E.ExtraA))
            DealStatDamage(E.AttributeTag, E.Value);    // dmg = get_dmg((value/100) × Utils.get_stat(key))
}
```

**风格 2 · GameplayEvent + Triggered Ability（低频/复杂钩子，更 GAS-native）**

```cpp
FGameplayEventData Data;
Data.EventTag       = BrotatoTags::Ability_Passive_OnPickupGold;
Data.EventMagnitude = Value;
Data.Instigator     = Player;
ASC->HandleGameplayEvent(Data.EventTag, &Data);
// UGA_Passive_* 配置 AbilityTriggers = { GameplayEvent, 上述 Tag }
```

| 钩子类别 | 建议风格 | 理由 |
|---|---|---|
| `dmg_when_pickup_gold` / `temp_stats_on_hit` / `gold_on_crit_kill` | 1 | 每秒可能触发数十次 |
| `explode_on_hit` / `_on_death` / `_on_consumable` | 2 | 涉及生成爆炸 + Cue + 伤害 |
| `structures` | 2 | 低频（每秒检查一次） |
| `projectiles_on_death` / `alien_eyes` | 2 | 生成投射物 |
| `stats_end_of_wave` / `convert_stats_*` | 1 | 每波一次 |

> **学习路径建议**：先全部用风格 1 跑通（保数值正确），再把 3–5 个典型钩子重构成风格 2。不要一上来全用 Ability。

### 5.3 钩子槽全表（★ 全工程唯一口径：40 个键）

| # | Tag | 触发时机 | 载荷（Value / ExtraA / ExtraName） | 来源样例 |
|---|---|---|---|---|
| 1 | `Hook.DmgWhenPickupGold` | 拾取材料 | value / chance / — | item_baby_elephant |
| 2 | `Hook.DmgWhenDeath` | 敌人死亡 | value / chance / — | item_cyberball |
| 3 | `Hook.DmgWhenHeal` | 玩家被治疗 | value / chance / — | character_lich |
| 4 | `Hook.DmgOnDodge` | 闪避成功 | value / chance / — | item_riposte |
| 5 | `Hook.HealOnDodge` | 闪避成功 | value / — / — | item_adrenaline |
| 6 | `Hook.RemoveSpeed` | 命中时减敌速 | value / max_value / — | item_ugly_tooth |
| 7 | `Hook.StartingItem` | 开局 | count / — / item_id | 角色 |
| 8 | `Hook.StartingWeapon` | 开局 | count / — / weapon_id | 角色 |
| 9 | `Hook.ProjectilesOnDeath` | 敌人死亡 | nb / cd / — (+Def) | — |
| 10 | `Hook.BurnChance` | 常驻 | — / — / — (+BurningData) | item_scared_sausage |
| 11 | `Hook.WeaponClassBonus` | 武器 stats 重算 | value / — / set_id | 角色职业加成 |
| 12 | `Hook.WeaponBonus` | 武器 stats 重算 | value / — / weapon_id | — |
| 13 | `Hook.WeaponStack` | 武器 stats 重算 | value / — / weapon_id | Stick / Drill |
| 14 | `Hook.UniqueWeaponEffects` | 武器列表变更 | value / — / — | item_focus / item_spider |
| 15 | `Hook.AdditionalWeaponEffects` | 武器列表变更 | value / — / — | Multitasker / Excalibur |
| 16 | `Hook.TierIVWeaponEffects` | 武器列表变更 | value / — / — | character_king |
| 17 | `Hook.TierIWeaponEffects` | 武器列表变更 | value / — / — | character_king |
| 18 | `Hook.GoldOnCritKill` | 暴击击杀 | value / — / — | item_hunting_trophy |
| 19 | `Hook.TempStatsWhileNotMoving` | 移动状态切换 | value / — / — | item_statue |
| 20 | `Hook.TempStatsWhileMoving` | 移动状态切换 | value / — / — | item_chameleon |
| 21 | `Hook.TempStatsOnHit` | 受击 | value / — / — | item_triangle_of_power |
| 22 | `Hook.TempStatsOnDodge` | 闪避 | value / — / — | Cryptid |
| 23 | `Hook.TempStatsStacking` | **每 5 s**（Periodic GE） | value / — / — | item_wisdom / item_medikit |
| 24 | `Hook.StatsEndOfWave` | 波末（**永久**） | value / — / — | item_vigilante_ring |
| 25 | `Hook.StatsNextWave` | 下一波 | value / — / — | item_peacock |
| 26 | `Hook.StatsOnLevelUp` | 升级 | value / — / — | Apprentice |
| 27 | `Hook.StatsBelowHalfHealth` | HP < 50% | value / — / — | Golem |
| 28 | `Hook.StatLinks` | 常驻动态 | value / nb_stat_scaled / stat_scaled (+bPermOnly) | Chunky / Generalist |
| 29 | `Hook.Structures` | 周期生成 | count / — / — (+Def) | 炮塔 / 地雷 / 花园 |
| 30 | `Hook.StructuresCooldownReduction` | 常驻 | pct / — / — | item_improved_tools |
| 31 | `Hook.ExplodeOnHit` | 命中 | — / chance / — (+Def) | Bull |
| 32 | `Hook.ExplodeOnDeath` | 敌人死亡 | — / chance / — (+Def) | Lich |
| 33 | `Hook.ExplodeOnConsumable` | 拾取消耗品 | — / chance / — (+Def) | Glutton |
| 34 | `Hook.ConvertStatsEndOfWave` | 波末 | value / pct / to_stat (+Def) | Demon |
| 35 | `Hook.ConvertStatsHalfWave` | 波次过半 | 同上 | Cyborg |
| 36 | `Hook.AlienEyes` | 周期 | nb / cd / — (+Def) | item_alien_eyes |
| 37 | `Hook.ConsumableStatsWhileMax` | 满血吃消耗品 | value / max_value / — | item_extra_stomach |
| 38 | `Hook.UpgradeRandomWeapon` | 进商店 | value / — / fallback_stat | item_anvil |
| 39 | `Hook.GuaranteedShopItems` | 商店生成 | — / — / item_id | Fisherman |
| 40 | `Hook.SpecificItemsPrice` | 定价 | pct / — / item_id | Fisherman |

> ★ **40 的构成（源码核对）**：`RunData.effects` 里 `init_effects`（`run_data.gd:1116-1163`）有 **38 个数组型键** + `burn_chance`（`BurningData` 对象）= **39**；
> 再加 **`weapon_stack`**（`WeaponStackEffect`，是**武器级**效果，存在武器自己的 effects 里，不在 `RunData.effects` 字典中；UE 侧统一进 HookRegistry）= **40**。
> 因此「RunData.effects 的钩子槽」是 39，「UE HookRegistry 的键」是 40，两个数不矛盾，写文档时要说清是哪一个。
>
> 原文档 §05 §2.5 表格为 34 行（含斜杠合并键），**展开后为 40 个键**（上表）。以本表为准；
> `Docs/02`/`05`/`09`/`10`、`D02`、`D08` 中凡出现 "36 / 38 / 39 个钩子槽" 的口径均已统一为 **40**。

### 5.4 爆炸统一结算（★ 概率相加、伤害相加、只播一次）

```cpp
void UBrotatoRunSubsystem::HandleExplosion(FGameplayTag HookTag, FVector2D Pos)
{
    const TArray<FBrotatoHookEntry>& Entries = HookRegistry->Get(HookTag);
    if (Entries.Num() == 0) return;

    float Chance = 0.f;
    for (const auto& E : Entries) Chance += E.ExtraA;     // ★ 线性相加，可 > 100%
    if (!Rng.ChanceSuccess(Chance)) return;

    int32 Dmg = 0;
    for (const auto& E : Entries)
        Dmg += FBrotatoWeaponMath::ComputeRuntimeStats(E.Def->ProjectileStats, Snapshot, Hooks,
                                                        1, Level, false, false).Damage;
    // ★ 只生成 1 次爆炸，视觉参数取第 1 个
    ExplosionSubsystem->Spawn(*Entries[0].Def, Pos, Dmg);
}
```

爆炸参数：基础半径 **147.34 px**，`R = 147.34 × max(0.1, scale × (1 + explosion_size/100))`；命中窗口 **0.05 s**；存在 **3.0 s**；烟雾 `round(scale × base_smoke_amount)`（默认 40）；`sound_db_mod = -10`。

---

## 6. TempStats：练 GE 动态增删与 Stacking

| 原作钩子 | GE 配置 | 触发方式 | 练到的 GAS 特性 |
|---|---|---|---|
| `temp_stats_while_moving` / `_while_not_moving` | `GE_TempStats_While*`，Infinite + `Effect.Lifetime.Wave` | `State.Moving` 切换时 Apply/Remove，**并压缩所有武器冷却** | GE 动态增删 + Tag 驱动 |
| `temp_stats_on_hit` | `GE_TempStats_OnHit`，Infinite + **Stacking** | 每次受击 Apply（自动叠层） | ★ **GE Stacking** |
| `temp_stats_on_dodge` | 同上 | 闪避成功 | 同上 |
| `temp_stats_stacking` | `GE_TempStats_Stacking`，**Periodic（`Period = 5.0`）+ Stacking** | **波首 Apply 一次，之后由 GAS 每 5 s 自动叠层** | ★ **Periodic + Stacking 组合**（§6.3） |
| `stats_below_half_health` | `GE_StatsBelowHalfHealth`，Infinite | `RegisterGameplayTagEvent(State.BelowHalfHealth)` | ★ Tag 事件 |
| `stats_end_of_wave` | `GE_StatsEndOfWave`，**Instant** | 波末 | BaseValue vs CurrentValue 的区别 |
| `stats_next_wave` | `GE_StatsNextWave`，Infinite + Wave | 波首 | |
| `stats_on_level_up` | `GE_Upgrade`，Instant | 升级 | |
| `percent_materials` | `GE_PercentMaterials`，Infinite + Wave | **1 s 循环 Timer / Periodic GE** 重算（移动/静止分别计时） | |
| 无敌窗口 / 吸血 CD | `GE_Invincible` / `GE_LifestealCd`，**HasDuration** | 受击后（含闪避，§4.5）/ 吸血成功后 | ★ **Duration GE + GE 组件 `TargetTagRequirements` 的 `ApplicationTagRequirements` 拦截** |
| 燃烧 / 生命回复 / 每秒掉血 | `GE_Burning` / `GE_HpRegen` / `GE_LoseHpPerSecond`，**Periodic** | 点燃 / 开波 / 装备 | ★ **Periodic GE**（§6.4） |
| **波末清空** | — | `ASC->RemoveActiveEffectsWithGrantedTags(Effect.Lifetime.Wave)` | ★ **一行清空所有波内效果** |

### 6.1 Stacking GE 配置

| 项 | 值 |
|---|---|
| `DurationPolicy` | Infinite（`OnHit` / `OnDodge`）／**HasDuration + Periodic**（`Stacking` 每 5 s 档） |
| `StackingType` | **`AggregateBySource`** |
| `StackLimitCount` | **0**（无限叠） |
| `StackDurationRefreshPolicy` | `RefreshOnSuccessfulApplication`（仅 HasDuration 档有意义） |
| `StackPeriodResetPolicy` | `NeverReset`（保证 5 s 周期不被新叠层打断） |
| `StackExpirationPolicy` | `ClearEntireStack` |
| `GrantedTags` | `Effect.Lifetime.Wave` + `Effect.Stackable.*` |

> ⚠️ Stacking GE 的 Modifier 值会**按层数线性放大**。若某个 TempStat 需要"每层不同值"，必须拆成多个 GE 或改用直接遍历。

### 6.3 ★ `temp_stats_stacking`：Periodic + Stacking（取代帧计数器）

原作用一个 300 帧计数器周期性 `AddStack`。GAS 侧直接用 Periodic GE 表达：

```cpp
// 波首施加一次即可，之后每 5 s 自动叠一层
FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GE_TempStats_Stacking, 1.f, Ctx);
Spec.Data->Period = BrotatoConst::StackingPeriod;   // 5.0 s（原作 300 帧）
// Duration = 本波剩余时长（或 Infinite + 波末统一清除）
Spec.Data->SetSetByCallerMagnitude(BrotatoTags::Data_Duration, WaveManager->GetRemainingSeconds());
ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
```

**收益**：暂停/时间缩放自动正确；长帧时 GAS 会补齐跨过的多个 period，叠层总数与 60 fps 一致。

### 6.4 ★ 燃烧：Periodic GE（时长与周期均运行时写入）

```cpp
void ApplyBurning(UAbilitySystemComponent* ASC, const FBurningData& BD,
                  const FBrotatoStatSnapshot& S)
{
    // 间隔：max(0.1, 0.5 × (1 - burning_cooldown_reduction/100))
    const float Interval = FBrotatoDamageMath::BurnTickInterval(S.BurningCooldownReduction);

    // ★ 叠加语义：逐字段取最大值刷新、不叠层（对齐原作）
    //   → 先读当前活跃 GE 的 SetByCaller，取 max 后重新 Apply
    const float Damage    = FMath::Max(GetActiveBurnDamage(ASC), (float)TickDamage(BD, S));
    const int32 NumTicks  = FMath::Max(GetActiveBurnTicksLeft(ASC), BD.NumTicks);

    FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GE_Burning, 1.f, Ctx);
    Spec.Data->Period = Interval;                                        // ★ 周期
    Spec.Data->SetSetByCallerMagnitude(BrotatoTags::Data_Duration, NumTicks * Interval);  // ★ 总时长
    Spec.Data->SetSetByCallerMagnitude(BrotatoTags::Data_Damage,   Damage);
    // ★ 无视闪避 / 护甲是燃烧的固定语义：Data.NotDodgeable / Data.NoArmor 直接配在 GE_Burning 资产的
    //   UAssetTagsGameplayEffectComponent 上（§4.1），UBrotatoBurnExecCalc 用 Spec.GetAllAssetTags() 读取。
    //   ⚠️ 不能用 DynamicGrantedTags：Periodic GE 生效期间会把这两个 Tag 真的授予目标。
    ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
}
```

| 语义 | 表达方式 |
|---|---|
| `duration` 是 **tick 次数** | `GE Duration = NumTicks × Period` |
| tick 间隔可被缩减（下限 0.1 s） | `Spec->Period = BurnTickInterval(Reduction)` |
| 叠加取最大值、不叠层 | `StackLimitCount = 1` + `RefreshOnSuccessfulApplication`，Apply 前逐字段取 max |
| tick 伤害无视护甲/闪避 | GE 资产 **AssetTags** 带 `Data.NoArmor` / `Data.NotDodgeable`（不是 GrantedTags） |

> Tier C（无 ASC 的普通敌人）用 `FBurningRuntime` 的 `TickTimer -= DeltaTime` 累减实现，公式与本节共享（D04 §10.7）。

### 6.2 Tag 事件驱动示例

```cpp
void ABrotatoPlayer::BindTagEvents()
{
    ASC->RegisterGameplayTagEvent(BrotatoTags::State_BelowHalfHealth,
                                  EGameplayTagEventType::NewOrRemoved)
       .AddUObject(this, &ABrotatoPlayer::OnBelowHalfHealthChanged);
}

void ABrotatoPlayer::OnBelowHalfHealthChanged(const FGameplayTag Tag, int32 NewCount)
{
    if (NewCount > 0)
    {
        FGameplayEffectSpecHandle Spec = BuildHookSpec(
            BrotatoTags::Hook_StatsBelowHalfHealth, GE_StatsBelowHalfHealth);
        BelowHalfHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
    }
    else if (BelowHalfHandle.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(BelowHalfHandle);
        BelowHalfHandle.Invalidate();
    }
}
```

### 6.3 移动状态切换（★ 附带重置武器冷却）

```cpp
void ABrotatoPlayer::UpdateMovingState(bool bMovingNow)
{
    if (bMovingNow == bWasMoving) return;
    bWasMoving = bMovingNow;
    ASC->RemoveLooseGameplayTag(bMovingNow ? BrotatoTags::State_NotMoving : BrotatoTags::State_Moving);
    ASC->AddLooseGameplayTag   (bMovingNow ? BrotatoTags::State_Moving    : BrotatoTags::State_NotMoving);

    SwapTempStatsGE(bMovingNow);
    // ★ 原作：移动状态切换时重置所有武器冷却（player.gd:125-173）
    for (ABrotatoWeapon* W : Weapons) W->ResetCooldown();
}
```

---

## 7. 序列化与读档重建

### 7.1 不可序列化的东西

| 对象 | 处理 |
|---|---|
| `FActiveGameplayEffectHandle` | **不序列化**。读档后按 ItemId 列表重新 `AddItem` 重建 |
| `FGameplayAbilitySpecHandle` | 同上 |
| `FGuid`（HookEntry Id） | 可序列化，但重建时重新生成也可 —— **推荐重新生成**，避免跨版本冲突 |
| `FBrotatoItemInstanceId` | **必须序列化**（追踪统计、唯一物品判定要用） |

### 7.2 读档重建顺序

```cpp
void UBrotatoRunSubsystem::RestoreFromState(const FBrotatoRunState& S)
{
    // 1. 重置 ASC（移除所有 GE、清空 HookRegistry、清 Loose Tag）
    ResetAll();
    // 2. 按原顺序重放：难度 → 角色 → 武器 → 物品
    ApplyDifficultyEffects(S.CurrentDifficulty);
    AddCharacter(Resolve(S.CharacterId));
    for (const FSavedWeapon& W : S.Weapons) AddWeapon(Resolve(W.MyId), W.InstanceId);
    for (const FSavedItem&   I : S.Items)   AddItem  (Resolve(I.MyId), I.InstanceId);
    // 3. 恢复非属性状态
    Gold = S.Gold; BonusGold = S.BonusGold; CurrentLevel = S.Level; CurrentXp = S.Xp;
    LockedShopItems = S.LockedShopItems; RerollPrice = S.RerollPrice; /* … */
    TrackedItemEffects = S.TrackedItemEffects;
    // 4. 重建派生
    UpdateItemRelatedEffects();
    LinkedStats->MarkDirty(); LinkedStats->FlushIfDirty();
    // 5. ★ 波内层不恢复（TempStats 本就每波清空），符合原作语义
}
```

### 7.3 `DT_IdRemap`（版本迁移）

反序列化解析 `my_id` 前统一过一遍：`OldId → NewId`，映射到 `NAME_None` 表示静默丢弃。详见 D06 §6。

---

## 8. Effect 文本渲染（§05 §5）

```cpp
FText UBrotatoEffectTextBuilder::GetText(const FBrotatoEffectDef& Def, bool bColored)
{
    // 1. key_text = TextKey 为空 ? AttributeTag 末段大写 : TextKey 大写
    // 2. args     = 基类 [str(Value), Loc(AttributeKey)]，各 Kind 重写
    // 3. signs[i] = SignFromValue(EffectSign, Value)
    // 4. 遍历 CustomArgs：按 ArgValue 求值覆盖 args[ArgIndex]，按 ArgSign 覆盖符号，
    //    按 Format ∈ {Usual, Percent, ArgValueAsNumber} 格式化
    // 5. 组装：正向 #00ff00 / 负向 red / 次要 #555555
}
```

`ArgValue` 动态来源：

| 枚举 | 计算 |
|---|---|
| `UniqueWeapons`(3) | `Value × UniqueWeaponIdCount` |
| `AdditionalWeapons`(4) | `Value × Weapons.Num()` |
| `TierIVWeapons`(9) | `Value × CountWeaponsOfTier(Legendary)` |
| `TierIWeapons`(10) | `Value × CountWeaponsOfTier(Common)` |
| `Tier`(5) | `"TIER_I" .. "TIER_IV"` |
| `ScalingStat`(6) | `GetScalingStatText(key, value/100)` |
| `ScalingStatValue`(7) | `GetScalingStatsValue([[key, value/100]])` |
| `MaxNbOfWaves`(8) | `NbOfWaves`（20） |
| `Usual`/`Value`/`Key`(0/1/2) | 默认 args |

追踪文本：`GetEffectsText()` 末尾追加 `TrackedItemEffects[MyId]`（**27 个可追踪物品**，如 item_coupon / item_metal_detector / item_dangerous_bunny / item_cute_monkey）。

---

## 9. 本章验收清单

| # | 项 | 标准 |
|---|---|---|
| E01 | Effect 卸载 | 装 3 个相同物品再卖 1 个 → 属性恰好减 1 份 |
| E02 | 波末清空 | `RemoveActiveEffectsWithGrantedTags(Effect.Lifetime.Wave)` 清空全部波内层 |
| E03 | Stacking | 连续受击 10 次 → 10 层；波末清空 |
| E04 | 套装 | 7 件 = 6 件档（不叠加） |
| E05 | StatCap 卸载 | 移除 Ghost 的 dodge_cap GE → 回落 60 |
| E06 | 一物品一 GE | 30 件物品 → `ActiveGE` 数 ≈ 30 + 常驻若干，不是 90+ |
| E07 | 爆炸叠加 | 两个 chance 0.5 → 触发率 100%，伤害相加，**只播一次** |
| E08 | 燃烧叠加 | 先 dmg5/dur8，再 dmg3/dur3 → dmg5/dur8（逐字段取最大） |
| E09 | 双路径一致 | `DualPathConsistencyTest` 1000 seed × 全用例全绿 |
| E10 | Rule 负向 | Soldier 移除 `Rule.CanAttackWhileMoving`；卸下角色后恢复 |
| E11 | 读档重建 | 续关后属性、HookRegistry 条目数、套装档位与存档前完全一致 |
