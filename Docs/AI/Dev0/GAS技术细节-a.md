# GAS 技术细节：属性 / 道具数值 / Buff 与免疫 / 伤害结算

> 面向《土豆兄弟》Brotato-like 项目的 GAS 落地方案。
> 所有代码为 UE 5.4 风格，单机项目（但保留网络正确性写法，成本很低）。

---

## 目录
1. [GAS 核心概念到本项目的映射](#一gas-核心概念到本项目的映射)
2. [AttributeSet 设计（属性到底怎么分层）](#二attributeset-设计属性到底怎么分层)
3. [道具数值：GameplayEffect 的挂载与回滚](#三道具数值gameplayeffect-的挂载与回滚)
4. [SetByCaller：让一个 GE 资产服务所有道具](#四setbycaller让一个-ge-资产服务所有道具)
5. [叠加与乘区：Brotato 的真实计算顺序](#五叠加与乘区brotato-的真实计算顺序)
6. [Buff/Debuff 系统：Duration、Stacking、Period](#六buffdebuff-系统durationstackingperiod)
7. [免疫系统：五种实现层级](#七免疫系统五种实现层级)
8. [伤害结算：ExecutionCalculation 完整实现](#八伤害结算executioncalculation-完整实现)
9. [触发型道具：GameplayCue 与事件驱动](#九触发型道具gameplaycue-与事件驱动)
10. [性能：1000 敌人下 GAS 怎么活](#十性能1000-敌人下-gas-怎么活)
11. [调试与工具链](#十一调试与工具链)
12. [避坑清单](#十二避坑清单)

---

## 一、GAS 核心概念到本项目的映射

| GAS 概念 | 本项目用途 |
|---|---|
| `AbilitySystemComponent (ASC)` | **只挂在玩家和 Boss 上**，普通敌人不挂 |
| `AttributeSet` | 18 种属性 + 派生属性的存储与 Clamp |
| `GameplayEffect (GE)` | 道具属性、武器套装、Buff/Debuff、伤害载体 |
| `GameplayTag` | 物品分类、武器类别、状态标记、免疫标记 |
| `GameplayAbility (GA)` | 武器开火、主动道具、角色特殊能力 |
| `GameplayCue` | 特效/音效/跳字（与逻辑解耦） |
| `AttributeSet PreAttributeChange` | Clamp 与派生属性重算 |
| `ExecutionCalculation` | 伤害结算（唯一入口） |
| `ModifierMagnitudeCalculation (MMC)` | 动态数值（如"每 10 点幸运 +1 暴击"） |
| `GameplayEffectExecutionCalculation` | 复杂条件伤害 |
| `AbilitySystemGlobals` | 自定义 `FGameplayEffectContext` 携带暴击/武器信息 |

**核心原则**：
> **属性变更的唯一途径是 GameplayEffect。任何 `Attribute.SetCurrentValue()` 的直接调用都是 Bug。**

---

## 二、AttributeSet 设计（属性到底怎么分层）

### 2.1 BaseValue vs CurrentValue（GAS 最容易搞错的点）

```
CurrentValue = (BaseValue + Σ Additive) × (1 + Σ Multiplicative) × Π Compound  ... 再 Override
                 ↑                ↑
            Instant GE 改这里   Duration/Infinite GE 改这里
```

| 类型 | 修改目标 | 用途 |
|---|---|---|
| `Instant` GE | **BaseValue**（永久） | 伤害、治疗、升级永久加点 |
| `Duration` GE | **CurrentValue**（临时，到期自动还原） | 限时 Buff |
| `Infinite` GE | **CurrentValue**（挂着就有，移除即失效） | **道具属性（本项目主力）** |

> **关键结论**：Brotato 的道具属性**全部用 `Infinite` GE**，卖掉道具时 `RemoveActiveGameplayEffect(Handle)` 即可完美回滚。这是选 GAS 最大的收益 —— 自研系统在"多来源叠加 + 任意顺序移除"上极易出 Bug。

### 2.2 属性分类

```cpp
// PotatoAttributeSet.h
UCLASS()
class UPotatoAttributeSet : public UAttributeSet
{
    GENERATED_BODY()
public:
    // ---- 资源类（有 Current/Max 概念）----
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, Health)
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, MaxHealth)

    // ---- 战斗属性 ----
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, MeleeDamage)      // 近战伤害
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, RangedDamage)     // 远程伤害
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, ElementalDamage)  // 元素伤害
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, DamagePercent)    // 伤害% （乘区）
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, AttackSpeed)      // 攻速%
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, CritChance)
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, CritMultiplier)
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, Range)            // 射程%
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, LifeSteal)

    // ---- 防御属性 ----
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, Armor)            // 护甲（减伤%）
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, Dodge)            // 闪避%
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, HealthRegen)
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, MoveSpeed)

    // ---- 经济属性 ----
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, Harvesting)       // 收获
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, Luck)             // 幸运
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, Engineering)      // 工程
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, PickupRange)

    // ---- Meta Attribute（不持久化，仅作为结算管道）----
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, IncomingDamage)   // 关键！
    ATTRIBUTE_ACCESSORS(UPotatoAttributeSet, IncomingHealing)

    virtual void PreAttributeChange(const FGameplayAttribute& Attr, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
    virtual void PreAttributeBaseChange(const FGameplayAttribute& Attr, float& NewValue) const override;
};
```

### 2.3 Meta Attribute：伤害为什么不直接改 Health

**错误做法**：GE 直接 `Health -= 10`
**正确做法**：GE 修改 `IncomingDamage`，在 `PostGameplayEffectExecute` 里转换为 Health 变化。

好处：
1. 伤害结算有唯一拦截点，可插入护甲、闪避、免伤、吸血、反伤
2. `IncomingDamage` 不需要存档、不需要同步
3. 可以在这里发全局事件（OnDamaged / OnKilled），触发型道具全靠它

```cpp
void UPotatoAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
    {
        const float LocalDamage = GetIncomingDamage();
        SetIncomingDamage(0.f);                       // 立刻清零，它只是管道
        if (LocalDamage <= 0.f) return;

        const float NewHealth = GetHealth() - LocalDamage;
        SetHealth(FMath::Clamp(NewHealth, 0.f, GetMaxHealth()));

        // 广播事件 —— 触发型道具的订阅点
        FPotatoDamageEvent Evt;
        Evt.Target      = Data.Target.GetAvatarActor();
        Evt.Source      = Data.EffectSpec.GetContext().GetInstigator();
        Evt.Damage      = LocalDamage;
        Evt.bIsCrit     = static_cast<FPotatoEffectContext*>(
                             Data.EffectSpec.GetContext().Get())->bIsCriticalHit;
        Evt.SourceTags  = *Data.EffectSpec.CapturedSourceTags.GetAggregatedTags();

        UGameEventSubsystem::Get(this)->BroadcastDamage(Evt);

        if (GetHealth() <= 0.f)
        {
            UGameEventSubsystem::Get(this)->BroadcastDeath(Evt);
        }
    }
}
```

### 2.4 Clamp 与 MaxHealth 变化处理

```cpp
void UPotatoAttributeSet::PreAttributeChange(const FGameplayAttribute& Attr, float& NewValue)
{
    if (Attr == GetMoveSpeedAttribute())   NewValue = FMath::Clamp(NewValue, 50.f, 2000.f);
    if (Attr == GetDodgeAttribute())       NewValue = FMath::Clamp(NewValue, 0.f, 0.60f); // 闪避硬上限 60%
    if (Attr == GetArmorAttribute())       NewValue = FMath::Max(NewValue, -50.f);
    if (Attr == GetAttackSpeedAttribute()) NewValue = FMath::Max(NewValue, -0.80f);       // 攻速下限

    // MaxHealth 变化时按比例调整当前血量（道具给最大生命时不该白送满血）
    if (Attr == GetMaxHealthAttribute())
    {
        const float Old = GetMaxHealth();
        if (Old > 0.f && NewValue != Old)
        {
            const float Ratio = GetHealth() / Old;
            // 注意：这里不能直接 SetHealth，要在 PostAttributeChange 做
            PendingHealthRatio = Ratio;
        }
    }
}

void UPotatoAttributeSet::PostAttributeChange(const FGameplayAttribute& Attr, float Old, float New)
{
    if (Attr == GetMaxHealthAttribute() && PendingHealthRatio >= 0.f)
    {
        SetHealth(FMath::Clamp(New * PendingHealthRatio, 1.f, New));
        PendingHealthRatio = -1.f;
    }
}
```

> **设计选择**：Brotato 原作中获得最大生命是**直接加当前血**的。如果要复刻这个手感，改成 `SetHealth(GetHealth() + (New - Old))`。这属于数值手感决策，写在配置里更好。

---

## 三、道具数值：GameplayEffect 的挂载与回滚

### 3.1 道具数据定义

```cpp
USTRUCT(BlueprintType)
struct FItemStatModifier
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FGameplayAttribute Attribute;

    UPROPERTY(EditAnywhere)
    EGameplayModOp::Type ModOp = EGameplayModOp::Additive; // Additive / Multiplicitive / Override

    UPROPERTY(EditAnywhere)
    float Magnitude = 0.f;
};

UCLASS(BlueprintType)
class UItemDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Base")
    FGameplayTag ItemTag;                     // Item.Tier2.Alien_Eyes

    UPROPERTY(EditAnywhere, Category="Base")
    FGameplayTagContainer ItemCategories;     // Item.Category.Ranged, Item.Category.Crit

    UPROPERTY(EditAnywhere, Category="Base")
    int32 Tier = 1;
    UPROPERTY(EditAnywhere, Category="Base")
    int32 BasePrice = 10;
    UPROPERTY(EditAnywhere, Category="Base")
    float ShopWeight = 1.f;
    UPROPERTY(EditAnywhere, Category="Base")
    int32 MaxStack = 99;

    // 90% 的道具只需要这个
    UPROPERTY(EditAnywhere, Category="Stats")
    TArray<FItemStatModifier> StatModifiers;

    // 需要复杂逻辑（条件加成、动态计算）时才用
    UPROPERTY(EditAnywhere, Category="Advanced")
    TArray<TSubclassOf<UGameplayEffect>> CustomEffects;

    // 触发型效果（见第九节）
    UPROPERTY(EditAnywhere, Category="Advanced")
    TArray<FItemTriggerDefinition> Triggers;

    // 授予的被动 Ability（极少数道具需要）
    UPROPERTY(EditAnywhere, Category="Advanced")
    TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;
};
```

### 3.2 挂载与卸载（核心代码）

```cpp
// UInventoryComponent.h
USTRUCT()
struct FOwnedItemInstance
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<UItemDefinition> Def = nullptr;
    UPROPERTY() int32 StackCount = 1;
    // 一个道具可能挂多个 GE（通用属性 GE + 自定义 GE）
    TArray<FActiveGameplayEffectHandle> AppliedEffects;
    TArray<FGameplayAbilitySpecHandle>  GrantedAbilityHandles;
    TArray<FDelegateHandle>             TriggerHandles;
};
```

```cpp
// UInventoryComponent.cpp
void UInventoryComponent::AddItem(UItemDefinition* Def)
{
    check(Def);
    UAbilitySystemComponent* ASC = GetOwnerASC();
    if (!ASC) return;

    // 可叠加道具：已有则直接再挂一份（GAS 天然支持同 GE 多实例叠加）
    FOwnedItemInstance NewInst;
    NewInst.Def = Def;

    // ---- 1. 通用属性 GE：一个资产 + SetByCaller，服务所有道具 ----
    if (Def->StatModifiers.Num() > 0)
    {
        FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
        Ctx.AddSourceObject(Def);

        FGameplayEffectSpecHandle SpecHandle =
            ASC->MakeOutgoingSpec(UGE_ItemStats_Generic::StaticClass(), 1.f, Ctx);

        FGameplayEffectSpec* Spec = SpecHandle.Data.Get();
        Spec->DynamicGrantedTags.AddTag(Def->ItemTag);          // 便于查询"是否拥有该道具"
        Spec->DynamicGrantedTags.AppendTags(Def->ItemCategories);

        for (const FItemStatModifier& Mod : Def->StatModifiers)
        {
            // 见第四节：动态添加 Modifier
            AddDynamicModifier(*Spec, Mod);
        }

        NewInst.AppliedEffects.Add(ASC->ApplyGameplayEffectSpecToSelf(*Spec));
    }

    // ---- 2. 自定义 GE ----
    for (TSubclassOf<UGameplayEffect> GEClass : Def->CustomEffects)
    {
        FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
        Ctx.AddSourceObject(Def);
        FGameplayEffectSpecHandle H = ASC->MakeOutgoingSpec(GEClass, 1.f, Ctx);
        NewInst.AppliedEffects.Add(ASC->ApplyGameplayEffectSpecToSelf(*H.Data.Get()));
    }

    // ---- 3. 被动 Ability ----
    for (TSubclassOf<UGameplayAbility> AbilityClass : Def->GrantedAbilities)
    {
        FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE, Def);
        NewInst.GrantedAbilityHandles.Add(ASC->GiveAbility(Spec));
    }

    // ---- 4. 触发型效果订阅 ----
    for (const FItemTriggerDefinition& Trig : Def->Triggers)
    {
        NewInst.TriggerHandles.Add(
            UGameEventSubsystem::Get(this)->Subscribe(Trig, GetOwner()));
    }

    OwnedItems.Add(MoveTemp(NewInst));
    OnInventoryChanged.Broadcast();
}

void UInventoryComponent::RemoveItem(int32 Index)
{
    if (!OwnedItems.IsValidIndex(Index)) return;
    UAbilitySystemComponent* ASC = GetOwnerASC();
    FOwnedItemInstance& Inst = OwnedItems[Index];

    // 精确回滚：只移除这一件道具带来的效果，不影响同名道具的其他份
    for (const FActiveGameplayEffectHandle& H : Inst.AppliedEffects)
    {
        ASC->RemoveActiveGameplayEffect(H);
    }
    for (const FGameplayAbilitySpecHandle& H : Inst.GrantedAbilityHandles)
    {
        ASC->ClearAbility(H);
    }
    for (const FDelegateHandle& H : Inst.TriggerHandles)
    {
        UGameEventSubsystem::Get(this)->Unsubscribe(H);
    }

    OwnedItems.RemoveAt(Index);
    OnInventoryChanged.Broadcast();
}
```

> **这就是选 GAS 的核心理由**：`FActiveGameplayEffectHandle` 让"卖掉第 3 件相同道具"这种操作精确无误。自研系统要自己维护 SourceHandle + 重算栈，很容易出现"卖掉后属性没还原"或"还原多了"的问题。

---

## 四、SetByCaller：让一个 GE 资产服务所有道具

200 个道具不可能做 200 个 GE 蓝图资产。两种方案：

### 方案 A：SetByCaller（推荐用于固定属性集）
在 `UGE_ItemStats_Generic` 里预置所有 18 个属性的 Modifier，Magnitude 类型设为 `SetByCaller`，Tag 为 `SetByCaller.MeleeDamage` 等。未设置的默认为 0（**必须在 GE 里勾选 "Can Ever Be Zero" 或用 `SetSetByCallerMagnitude` 全量赋值，否则会报 Warning**）。

```cpp
void AddDynamicModifier(FGameplayEffectSpec& Spec, const FItemStatModifier& Mod)
{
    const FGameplayTag Tag = UPotatoStatLibrary::AttributeToSetByCallerTag(Mod.Attribute, Mod.ModOp);
    Spec.SetSetByCallerMagnitude(Tag, Mod.Magnitude);
}
```
- 优点：性能好，GE 资产唯一
- 缺点：需要为每个 (属性 × ModOp) 预置一条 Modifier，18 属性 × 2 = 36 条，Spec 略重

### 方案 B：运行时构造 GE（推荐用于任意属性组合）
```cpp
FActiveGameplayEffectHandle UInventoryComponent::ApplyDynamicStats(
    UAbilitySystemComponent* ASC, const UItemDefinition* Def)
{
    // 注意：用 NewObject 构造的 GE 必须持有引用防 GC，或缓存复用
    UGameplayEffect* GE = NewObject<UGameplayEffect>(
        GetTransientPackage(), FName(*FString::Printf(TEXT("GE_Item_%s"), *Def->GetName())));
    GE->DurationPolicy = EGameplayEffectDurationType::Infinite;

    GE->Modifiers.Reserve(Def->StatModifiers.Num());
    for (const FItemStatModifier& Mod : Def->StatModifiers)
    {
        FGameplayModifierInfo Info;
        Info.Attribute          = Mod.Attribute;
        Info.ModifierOp         = Mod.ModOp;
        Info.ModifierMagnitude  = FScalableFloat(Mod.Magnitude);
        GE->Modifiers.Add(Info);
    }

    GE->InheritableOwnedTagsContainer.AddTag(Def->ItemTag);

    FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
    Ctx.AddSourceObject(Def);
    return ASC->ApplyGameplayEffectToSelf(GE, 1.f, Ctx);
}
```
- 优点：完全数据驱动，加道具零成本
- 缺点：运行时创建 UObject（**务必在游戏开始时预构造并缓存到 `TMap<ItemId, UGameplayEffect*>`，加 `AddToRoot` 或用 `UPROPERTY()` 持有**）

**本项目建议**：
> 游戏启动时遍历所有 `UItemDefinition`，**预生成**对应的 `UGameplayEffect` CDO 缓存在 `UItemEffectRegistry`（GameInstanceSubsystem，`UPROPERTY()` 持有防 GC）。运行时只做 `ApplyGameplayEffectToSelf`，零分配、零 Warning、完全数据驱动。

### MMC：动态数值道具
"每 10 点幸运，暴击率 +1%" 这类需要用 `UGameplayModMagnitudeCalculation`：

```cpp
UCLASS()
class UMMC_CritFromLuck : public UGameplayModMagnitudeCalculation
{
    GENERATED_BODY()
public:
    UMMC_CritFromLuck()
    {
        LuckDef.AttributeToCapture = UPotatoAttributeSet::GetLuckAttribute();
        LuckDef.AttributeSource    = EGameplayEffectAttributeCaptureSource::Target;
        LuckDef.bSnapshot          = false;   // 关键：不快照，幸运变化时自动重算
        RelevantAttributesToCapture.Add(LuckDef);
    }

    virtual float CalculateBaseMagnitude_Implementation(
        const FGameplayEffectSpec& Spec) const override
    {
        FAggregatorEvaluateParameters Params;
        Params.SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
        Params.TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

        float Luck = 0.f;
        GetCapturedAttributeMagnitude(LuckDef, Spec, Params, Luck);
        return FMath::FloorToFloat(Luck / 10.f) * 0.01f;
    }
private:
    FGameplayEffectAttributeCaptureDefinition LuckDef;
};
```

> **`bSnapshot = false` 是精髓**：GAS 会自动把这个 GE 注册为依赖 Luck 的聚合器，Luck 一变，暴击加成自动重算。自研系统要实现这个"属性依赖属性"的响应式重算非常痛苦。
> **注意**：属性互相依赖时要小心**循环依赖**（A 依赖 B，B 依赖 A 会死循环），设计时必须保证依赖图无环。

---

## 五、叠加与乘区：Brotato 的真实计算顺序

### 5.1 GAS 默认的聚合公式
```
Final = ((Base + ΣAdditive) × (1 + ΣMultiplicitive)) × ΠCompoundMultiply → Override 覆盖
```
注意 `Multiplicitive`（GAS 里的拼写）是 **加算乘区**：两个 +50% 会变成 ×(1+0.5+0.5) = ×2.0，而不是 ×2.25。这恰好符合 Brotato 的设计。

### 5.2 Brotato 的伤害公式（复刻用）
```
武器单次伤害 =
  ⌊ (WeaponBaseDamage
      + MeleeDamage   × WeaponMeleeScaling
      + RangedDamage  × WeaponRangedScaling
      + ElementalDamage × WeaponElementalScaling)
    × (1 + DamagePercent)
    × (暴击 ? CritMultiplier : 1)
  ⌋
最终伤害 = max(1, 上式 × (1 - ArmorReduction))
```

关键点：
- **缩放系数在武器数据里**，不同武器吃不同属性（手枪吃远程、刀吃近战、双属性武器各吃一半）
- **向下取整发生在暴击之后、护甲之前**（具体位置要实测原作，做成可配置）
- **最低伤害 1 点**，防止高护甲无敌

### 5.3 护甲公式
Brotato 用的是递减曲线而非线性：
```
DamageReduction = Armor / (Armor + K)      // K 建议 15~30，做成 CurveTable
实际伤害 = ceil(Damage × (1 - DamageReduction))
```
负护甲则为增伤：`Armor < 0` 时 `Reduction = Armor / (|Armor| + K)` 为负值，即增伤。

> **强烈建议**：把 K、取整方式、最低伤害全部放进 `UCombatSettings : UDeveloperSettings`，在 Project Settings 里可调，方便平衡。

---

## 六、Buff/Debuff 系统：Duration、Stacking、Period

### 6.1 三种 Duration Policy 的用法
| Policy | 用途举例 |
|---|---|
| `Instant` | 瞬间伤害、治疗、材料获得（改 BaseValue） |
| `HasDuration` | 燃烧、减速、狂暴、护盾（限时） |
| `Infinite` | 道具属性、角色天赋、武器套装（手动移除） |

### 6.2 Stacking（叠层）配置

```cpp
// GE_Burning（燃烧，最多 5 层，每层独立计时）
StackingType         = EGameplayEffectStackingType::AggregateBySource; // 按来源分别叠
StackLimitCount      = 5;
StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
StackPeriodResetPolicy     = EGameplayEffectStackingPeriodPolicy::NeverReset;
StackExpirationPolicy      = EGameplayEffectStackingExpirationPolicy::RemoveSingleStackAndRefreshDuration;
```

| 配置 | 含义 | 典型用法 |
|---|---|---|
| `AggregateBySource` | 每个施法者独立一份栈 | 多个火源分别叠燃烧 |
| `AggregateByTarget` | 目标身上全局共享一份栈 | Boss 的破甲印记 |
| `RefreshOnSuccessfulApplication` | 每次叠加刷新总时长 | 常见 DoT |
| `NeverRefresh` | 时长不刷新 | 独立计时的 Buff |
| `RemoveSingleStackAndRefreshDuration` | 到期掉 1 层 | 层层衰减的燃烧 |
| `ClearEntireStack` | 到期全清 | 一次性状态 |

**叠层影响数值**：在 Modifier 的 Magnitude 里勾选 `Multiply by Stack Count`，或在 MMC 里读 `Spec.GetStackCount()`。

### 6.3 Periodic（周期性效果，DoT/HoT）

```cpp
// GE_Poison：每 0.5s 造成 3 点伤害，持续 5s
DurationPolicy  = HasDuration;
DurationMagnitude = FScalableFloat(5.f);
Period          = FScalableFloat(0.5f);
bExecutePeriodicEffectOnApplication = true;   // 挂上立刻跳一次
// Modifier: IncomingDamage += 3  (会走 ExecutionCalculation 或直接 Modifier)
```

> **重要坑**：Periodic GE 的每次 tick 都被当作 **Instant** 处理（改 BaseValue）。所以周期伤害要走 `IncomingDamage` Meta Attribute，不能直接改 Health（会绕过护甲和事件广播）。

### 6.4 Buff 的表现层（GameplayCue）

```cpp
// 在 GE 上配置
GameplayCues[0].GameplayCueTags = "GameplayCue.Status.Burning";
GameplayCues[0].MagnitudeAttribute = ...;
```
`GameplayCue` 有 `OnActive / WhileActive / Removed / Executed` 四个回调，分别对应挂上、持续、移除、瞬发。用 `AGameplayCueNotify_Looping` 做持续特效，`UGameplayCueNotify_Burst` 做瞬发。

> **性能提醒**：GameplayCue 走的是 `UGameplayCueManager` 的 Tag 路由，1000 个敌人同时挂燃烧会有明显开销。敌人身上的状态特效建议**绕过 GameplayCue**，直接用 Niagara 参数批量驱动。

### 6.5 Buff 之间的互斥与清除

```cpp
// GE_Frozen 挂上时移除所有"加速类"Buff
RemoveGameplayEffectsWithTags (在 GE 的 Removal 分类下):
    RemoveGameplayEffectQuery.OwningTagQuery = 
        FGameplayTagQuery::MakeQuery_MatchAnyTags({"Status.Buff.Speed"})

// 或代码方式
FGameplayEffectQuery Query;
Query.OwningTagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(
    FGameplayTagContainer(FGameplayTag::RequestGameplayTag("Status.Debuff")));
ASC->RemoveActiveEffects(Query, -1);  // -1 = 全部移除
```

---

## 七、免疫系统：五种实现层级

这是 GAS 里最容易被问到、也最容易做错的部分。按**优先级从高到低**：

### 层级 1：`ApplicationImmunityTags`（最正统，推荐）
在 **免疫用的 GE** 上配置（例如 `GE_Immunity_Fire`）：
```cpp
// GE_Immunity_Fire
DurationPolicy = HasDuration / Infinite
GrantedApplicationImmunityTags.RequireTags = "Damage.Type.Fire"  
// 任何带 Damage.Type.Fire 的 GE 都无法施加到本单位
```

**特点**：
- 由 ASC 在 `ApplyGameplayEffectSpecToSelf` 最早期拦截，**GE 根本不会生效**
- 会触发 `ASC->OnImmunityBlockGameplayEffectDelegate`，可以用来播"免疫"飘字
- 也可以用 `GrantedApplicationImmunityQuery` 做更复杂的查询（如"免疫来源带某 Tag 的效果"）

```cpp
// 监听免疫事件，播放"免疫"提示
ASC->OnImmunityBlockGameplayEffectDelegate.AddUObject(
    this, &AMyChar::OnImmunityBlocked);

void AMyChar::OnImmunityBlocked(const FGameplayEffectSpec& BlockedSpec,
                                const FActiveGameplayEffect* ImmunityGE)
{
    ShowFloatingText(TEXT("IMMUNE"));
}
```

### 层级 2：`ApplicationTagRequirements`（在被免疫的 GE 上配置）
```cpp
// GE_Burning 上配置
ApplicationTagRequirements.IgnoreTags = "Status.Immune.Fire"
// 目标身上有 Status.Immune.Fire 时，本 GE 无法施加
```
**特点**：控制权在"被免疫的效果"一侧。适合"这个 Debuff 对 Boss 无效"这类由效果方定义的规则。

### 层级 3：`OngoingTagRequirements`（挂着但不生效）
```cpp
OngoingTagRequirements.IgnoreTags = "Status.Shielded"
```
GE 仍然挂在身上并继续计时，但 Modifier **临时失效**；Tag 消失后自动重新生效。适合"开盾期间减速无效，盾破了减速继续"。

### 层级 4：`RemovalTagRequirements`（挂上后被清除）
```cpp
RemovalTagRequirements.IgnoreTags = "Status.Cleanse"
```
获得净化 Tag 时自动移除该 GE。

### 层级 5：伤害结算内的自定义免疫（最灵活）
在 `ExecutionCalculation` 里读 Tag 做条件判断：
```cpp
if (TargetTags->HasTag(FPotatoTags::Get().Status_Invulnerable))
{
    return; // 不输出任何 Modifier
}
if (TargetTags->HasTag(FPotatoTags::Get().Status_Immune_Crit))
{
    bIsCrit = false;
}
```

### 选型对照表
| 需求 | 推荐层级 |
|---|---|
| 无敌帧（受击后 0.5s 无敌） | 层级 1，`GE_IFrame` 带 `Immunity: Damage.Type.Any` |
| 某角色免疫燃烧 | 层级 1 或 2 |
| 护盾期间免疫控制 | 层级 3（盾没了控制恢复）或层级 1（彻底免疫） |
| Boss 免疫所有 Debuff | 层级 1 + `GrantedApplicationImmunityQuery` |
| 免疫暴击 / 减伤上限 | 层级 5 |
| 净化 | 层级 4 或主动 `RemoveActiveEffects(Query)` |

### 无敌帧完整示例
```cpp
// GE_InvulnerabilityFrame (HasDuration 0.5s)
GrantedTags: "Status.Invulnerable"
GrantedApplicationImmunityTags.RequireTags: "Effect.Damage"
GameplayCues: "GameplayCue.Player.IFrameFlash"   // 角色闪烁

// 在 PostGameplayEffectExecute 收到伤害后施加
if (LocalDamage > 0.f && bUseIFrames)
{
    FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
    ASC->ApplyGameplayEffectToSelf(GE_IFrame_CDO, 1.f, Ctx);
}
```
> Brotato 原作有受击无敌帧，这个必须实现，否则冲进怪堆会瞬间秒杀。

---

## 八、伤害结算：ExecutionCalculation 完整实现

### 8.1 为什么用 Execution 而不是 Modifier
| | Modifier + MMC | ExecutionCalculation |
|---|---|---|
| 能改几个属性 | 1 个 | 任意多个 |
| 能读 Source & Target 属性 | 能 | 能 |
| 能加条件分支 | 弱 | 强 |
| 能修改 EffectContext（写回暴击标记） | 不能 | **能** |
| 性能 | 更好 | 略差（但可接受） |

伤害需要：读双方十几个属性 + 暴击判定 + 闪避 + 护甲 + 写回暴击标记 → **必须用 Execution**。

### 8.2 自定义 EffectContext（携带暴击/武器信息）

```cpp
USTRUCT()
struct FPotatoEffectContext : public FGameplayEffectContext
{
    GENERATED_BODY()

    bool bIsCriticalHit = false;
    bool bIsDodged      = false;
    float RawDamage     = 0.f;       // 未经护甲的伤害，用于跳字和统计
    TWeakObjectPtr<const UWeaponDefinition> SourceWeapon;

    virtual UScriptStruct* GetScriptStruct() const override
    { return FPotatoEffectContext::StaticStruct(); }

    virtual FPotatoEffectContext* Duplicate() const override
    {
        FPotatoEffectContext* New = new FPotatoEffectContext(*this);
        if (GetHitResult()) New->AddHitResult(*GetHitResult(), true);
        return New;
    }

    virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
};

template<> struct TStructOpsTypeTraits<FPotatoEffectContext>
    : public TStructOpsTypeTraitsBase2<FPotatoEffectContext>
{ enum { WithNetSerializer = true, WithCopy = true }; };

// 别忘了在 UPotatoAbilitySystemGlobals 里重写
FGameplayEffectContext* UPotatoAbilitySystemGlobals::AllocGameplayEffectContext() const
{
    return new FPotatoEffectContext();
}
// 并在 DefaultGame.ini 设置：
// [/Script/GameplayAbilities.AbilitySystemGlobals]
// AbilitySystemGlobalsClassName="/Script/PotatoGameplay.PotatoAbilitySystemGlobals"
```

### 8.3 伤害 Execution 完整代码

```cpp
// PotatoDamageExecution.cpp
struct FDamageStatics
{
    // Source 侧
    DECLARE_ATTRIBUTE_CAPTUREDEF(MeleeDamage);
    DECLARE_ATTRIBUTE_CAPTUREDEF(RangedDamage);
    DECLARE_ATTRIBUTE_CAPTUREDEF(ElementalDamage);
    DECLARE_ATTRIBUTE_CAPTUREDEF(DamagePercent);
    DECLARE_ATTRIBUTE_CAPTUREDEF(CritChance);
    DECLARE_ATTRIBUTE_CAPTUREDEF(CritMultiplier);
    DECLARE_ATTRIBUTE_CAPTUREDEF(LifeSteal);
    // Target 侧
    DECLARE_ATTRIBUTE_CAPTUREDEF(Armor);
    DECLARE_ATTRIBUTE_CAPTUREDEF(Dodge);

    FDamageStatics()
    {
        // bSnapshot = true：开火瞬间快照属性（投射物飞行中属性变化不影响本次伤害）
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPotatoAttributeSet, MeleeDamage,     Source, true);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPotatoAttributeSet, RangedDamage,    Source, true);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPotatoAttributeSet, ElementalDamage, Source, true);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPotatoAttributeSet, DamagePercent,   Source, true);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPotatoAttributeSet, CritChance,      Source, true);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPotatoAttributeSet, CritMultiplier,  Source, true);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPotatoAttributeSet, LifeSteal,       Source, true);
        // bSnapshot = false：命中瞬间才读目标属性（目标破甲后应立即生效）
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPotatoAttributeSet, Armor,  Target, false);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPotatoAttributeSet, Dodge,  Target, false);
    }
};
static const FDamageStatics& DamageStatics()
{
    static FDamageStatics Inst;
    return Inst;
}

UPotatoDamageExecution::UPotatoDamageExecution()
{
    RelevantAttributesToCapture.Add(DamageStatics().MeleeDamageDef);
    RelevantAttributesToCapture.Add(DamageStatics().RangedDamageDef);
    RelevantAttributesToCapture.Add(DamageStatics().ElementalDamageDef);
    RelevantAttributesToCapture.Add(DamageStatics().DamagePercentDef);
    RelevantAttributesToCapture.Add(DamageStatics().CritChanceDef);
    RelevantAttributesToCapture.Add(DamageStatics().CritMultiplierDef);
    RelevantAttributesToCapture.Add(DamageStatics().LifeStealDef);
    RelevantAttributesToCapture.Add(DamageStatics().ArmorDef);
    RelevantAttributesToCapture.Add(DamageStatics().DodgeDef);
}

void UPotatoDamageExecution::Execute_Implementation(
    const FGameplayEffectCustomExecutionParameters& Params,
    FGameplayEffectCustomExecutionOutput& OutExec) const
{
    const FGameplayEffectSpec& Spec = Params.GetOwningSpec();
    FGameplayEffectContextHandle CtxHandle = Spec.GetContext();
    FPotatoEffectContext* Ctx = static_cast<FPotatoEffectContext*>(CtxHandle.Get());

    const FGameplayTagContainer* SrcTags = Spec.CapturedSourceTags.GetAggregatedTags();
    const FGameplayTagContainer* TgtTags = Spec.CapturedTargetTags.GetAggregatedTags();

    FAggregatorEvaluateParameters EvalParams;
    EvalParams.SourceTags = SrcTags;
    EvalParams.TargetTags = TgtTags;

    const FPotatoTags& T = FPotatoTags::Get();
    const UCombatSettings* Cfg = GetDefault<UCombatSettings>();

    // ===== 0. 绝对无敌检查 =====
    if (TgtTags->HasTag(T.Status_Invulnerable))
    {
        return;   // 不输出任何 Modifier
    }

    // ===== 1. 闪避判定 =====
    float Dodge = 0.f;
    ExecutionParams_GetCapturedAttributeMagnitude(
        Params, DamageStatics().DodgeDef, EvalParams, Dodge);
    Dodge = FMath::Clamp(Dodge, 0.f, Cfg->MaxDodge);   // 硬上限 0.6

    // 不可闪避的伤害类型（如 DoT、地形伤害）
    const bool bCanDodge = !Spec.GetDynamicAssetTags().HasTag(T.Damage_Undodgeable)
                        && !TgtTags->HasTag(T.Status_CannotDodge);

    if (bCanDodge && Dodge > 0.f)
    {
        // 用可复现的 RNG，不用 FMath::FRand
        if (UPotatoRandom::Roll(EPotatoRngChannel::Combat, Dodge))
        {
            Ctx->bIsDodged = true;
            // 输出一个 0 伤害，让 PostGameplayEffectExecute 能广播"闪避"事件
            OutExec.AddOutputModifier(FGameplayModifierEvaluatedData(
                UPotatoAttributeSet::GetIncomingDamageAttribute(),
                EGameplayModOp::Additive, 0.f));
            return;
        }
    }

    // ===== 2. 基础伤害（来自武器，通过 SetByCaller 传入）=====
    const float WeaponBase =
        Spec.GetSetByCallerMagnitude(T.SetByCaller_BaseDamage, false, 0.f);
    const float MeleeScale =
        Spec.GetSetByCallerMagnitude(T.SetByCaller_MeleeScaling, false, 0.f);
    const float RangedScale =
        Spec.GetSetByCallerMagnitude(T.SetByCaller_RangedScaling, false, 0.f);
    const float ElemScale =
        Spec.GetSetByCallerMagnitude(T.SetByCaller_ElementalScaling, false, 0.f);

    float Melee = 0.f, Ranged = 0.f, Elem = 0.f, DmgPct = 0.f;
    ExecutionParams_GetCapturedAttributeMagnitude(Params, DamageStatics().MeleeDamageDef,     EvalParams, Melee);
    ExecutionParams_GetCapturedAttributeMagnitude(Params, DamageStatics().RangedDamageDef,    EvalParams, Ranged);
    ExecutionParams_GetCapturedAttributeMagnitude(Params, DamageStatics().ElementalDamageDef, EvalParams, Elem);
    ExecutionParams_GetCapturedAttributeMagnitude(Params, DamageStatics().DamagePercentDef,   EvalParams, DmgPct);

    float Damage = WeaponBase
                 + Melee  * MeleeScale
                 + Ranged * RangedScale
                 + Elem   * ElemScale;

    // ===== 3. 伤害% 乘区 =====
    Damage *= (1.f + DmgPct);

    // ===== 4. 暴击 =====
    bool bCrit = false;
    if (!TgtTags->HasTag(T.Status_Immune_Crit)
        && !Spec.GetDynamicAssetTags().HasTag(T.Damage_CannotCrit))
    {
        float CritChance = 0.f, CritMult = 2.f;
        ExecutionParams_GetCapturedAttributeMagnitude(Params, DamageStatics().CritChanceDef,     EvalParams, CritChance);
        ExecutionParams_GetCapturedAttributeMagnitude(Params, DamageStatics().CritMultiplierDef, EvalParams, CritMult);

        // 武器自带暴击率（SetByCaller）
        CritChance += Spec.GetSetByCallerMagnitude(T.SetByCaller_WeaponCrit, false, 0.f);
        CritChance = FMath::Clamp(CritChance, 0.f, Cfg->MaxCritChance);

        // 必定暴击 Tag（某些道具）
        if (SrcTags->HasTag(T.Status_GuaranteedCrit)) { bCrit = true; }
        else { bCrit = UPotatoRandom::Roll(EPotatoRngChannel::Combat, CritChance); }

        if (bCrit) { Damage *= FMath::Max(1.f, CritMult); }
    }
    Ctx->bIsCriticalHit = bCrit;

    // ===== 5. 取整（原作行为，位置可配置）=====
    if (Cfg->bFloorDamageBeforeArmor) { Damage = FMath::FloorToFloat(Damage); }
    Ctx->RawDamage = Damage;

    // ===== 6. 护甲减伤 =====
    float Armor = 0.f;
    ExecutionParams_GetCapturedAttributeMagnitude(Params, DamageStatics().ArmorDef, EvalParams, Armor);

    // 护甲穿透（武器/道具，SetByCaller）
    const float ArmorPen = Spec.GetSetByCallerMagnitude(T.SetByCaller_ArmorPen, false, 0.f);
    Armor = FMath::Max(Armor - ArmorPen, Cfg->MinArmor);

    const float K = Cfg->ArmorK;   // 默认 20
    const float Reduction = Armor >= 0.f
        ? Armor / (Armor + K)
        : Armor / (FMath::Abs(Armor) + K);      // 负护甲 → 负减伤 = 增伤
    Damage *= (1.f - Reduction);

    // ===== 7. 易伤 / 减伤 Tag 乘区 =====
    if (TgtTags->HasTag(T.Status_Vulnerable))  { Damage *= Cfg->VulnerableMultiplier; }
    if (TgtTags->HasTag(T.Status_Shielded))    { Damage *= Cfg->ShieldedMultiplier; }

    // ===== 8. 最低伤害保底 =====
    Damage = FMath::Max(Damage, Cfg->MinDamage);   // 默认 1
    Damage = FMath::CeilToFloat(Damage);

    // ===== 9. 输出 =====
    OutExec.AddOutputModifier(FGameplayModifierEvaluatedData(
        UPotatoAttributeSet::GetIncomingDamageAttribute(),
        EGameplayModOp::Additive, Damage));

    // ===== 10. 吸血（对 Source 生效，用单独的 GE）=====
    float LifeSteal = 0.f;
    ExecutionParams_GetCapturedAttributeMagnitude(Params, DamageStatics().LifeStealDef, EvalParams, LifeSteal);
    if (LifeSteal > 0.f)
    {
        // 注意：Execution 不能直接修改 Source 属性，必须回施一个 GE
        if (UAbilitySystemComponent* SrcASC = Params.GetSourceAbilitySystemComponent())
        {
            FGameplayEffectContextHandle HealCtx = SrcASC->MakeEffectContext();
            FGameplayEffectSpecHandle HealSpec =
                SrcASC->MakeOutgoingSpec(Cfg->LifeStealHealGE, 1.f, HealCtx);
            HealSpec.Data->SetSetByCallerMagnitude(
                T.SetByCaller_HealAmount, Damage * LifeSteal);
            SrcASC->ApplyGameplayEffectSpecToSelf(*HealSpec.Data.Get());
        }
    }
}
```

> **关键坑**：`ExecutionCalculation` **不能直接修改 Source 的属性**（`AddOutputModifier` 只作用于 Target）。吸血、能量回复等对自己生效的效果，必须在 Execution 内**回施一个新 GE 给 Source**，或者在 `PostGameplayEffectExecute` 里处理。

### 8.4 武器如何发起伤害

```cpp
void UWeaponSlotComponent::ApplyDamageToTarget(AActor* Target)
{
    UAbilitySystemComponent* SrcASC = GetOwnerASC();
    UAbilitySystemComponent* TgtASC = UAbilitySystemBlueprintLibrary::
        GetAbilitySystemComponent(Target);

    // 敌人没有 ASC 的走轻量路径（见第十节）
    if (!TgtASC) { ApplyLightweightDamage(Target); return; }

    FGameplayEffectContextHandle Ctx = SrcASC->MakeEffectContext();
    Ctx.AddSourceObject(WeaponDef);
    Ctx.AddInstigator(GetOwner(), GetOwner());
    static_cast<FPotatoEffectContext*>(Ctx.Get())->SourceWeapon = WeaponDef;

    FGameplayEffectSpecHandle SpecH =
        SrcASC->MakeOutgoingSpec(UGE_WeaponDamage::StaticClass(), 1.f, Ctx);
    FGameplayEffectSpec* Spec = SpecH.Data.Get();

    const FPotatoTags& T = FPotatoTags::Get();
    Spec->SetSetByCallerMagnitude(T.SetByCaller_BaseDamage,        WeaponDef->BaseDamage);
    Spec->SetSetByCallerMagnitude(T.SetByCaller_MeleeScaling,      WeaponDef->MeleeScaling);
    Spec->SetSetByCallerMagnitude(T.SetByCaller_RangedScaling,     WeaponDef->RangedScaling);
    Spec->SetSetByCallerMagnitude(T.SetByCaller_ElementalScaling,  WeaponDef->ElementalScaling);
    Spec->SetSetByCallerMagnitude(T.SetByCaller_WeaponCrit,        WeaponDef->CritChance);
    Spec->SetSetByCallerMagnitude(T.SetByCaller_ArmorPen,          WeaponDef->ArmorPen);

    // 伤害类型 Tag，用于免疫判定
    Spec->AddDynamicAssetTag(WeaponDef->DamageTypeTag);   // Damage.Type.Fire

    SrcASC->ApplyGameplayEffectSpecToTarget(*Spec, TgtASC);
}
```

### 8.5 伤害管线全景
```
Weapon.Fire()
  └─ GA_WeaponAttack (可选，纯 C++ 也行)
       └─ 索敌 → 生成投射物 / 近战判定
            └─ 命中 → MakeOutgoingSpec(GE_WeaponDamage) + SetByCaller 武器数据
                 └─ ASC->ApplyGameplayEffectSpecToTarget
                      ├─ [1] ApplicationImmunityTags 检查 ← 免疫在这里拦截
                      ├─ [2] ApplicationTagRequirements 检查
                      ├─ [3] UPotatoDamageExecution::Execute
                      │       无敌→闪避→基础→伤害%→暴击→取整→护甲→易伤→保底
                      │       输出 IncomingDamage
                      ├─ [4] AttributeSet::PostGameplayEffectExecute
                      │       IncomingDamage → Health，清零 Meta
                      │       广播 OnDamaged / OnKilled
                      │       施加无敌帧 GE
                      └─ [5] GameplayCue 播特效/音效/跳字
                           └─ 触发型道具订阅 OnDamaged/OnKilled → 连锁效果
```

---

## 九、触发型道具：GameplayCue 与事件驱动

Brotato 有大量"击杀时有 X% 概率…"的道具。不要为每个都写一个类。

### 9.1 三段式数据结构
```cpp
USTRUCT(BlueprintType)
struct FItemTriggerDefinition
{
    GENERATED_BODY()

    // WHEN
    UPROPERTY(EditAnywhere) FGameplayTag EventTag;        // Event.OnKill / Event.OnHit / Event.OnWaveStart
    // IF
    UPROPERTY(EditAnywhere) float Chance = 1.f;           // 触发概率
    UPROPERTY(EditAnywhere) FGameplayTagQuery Condition;  // 对事件上下文 Tag 的查询
    UPROPERTY(EditAnywhere) float InternalCooldown = 0.f; // ICD，防连锁爆炸
    // THEN
    UPROPERTY(EditAnywhere, Instanced) TObjectPtr<UItemActionBase> Action;
};
```

`UItemActionBase` 的子类（十来个就能覆盖 90% 道具）：
| Action 类 | 效果 |
|---|---|
| `UItemAction_ApplyGE` | 给自己/目标施加一个 GE（Buff/Debuff） |
| `UItemAction_DealDamage` | 范围/单体伤害 |
| `UItemAction_Heal` | 治疗 |
| `UItemAction_SpawnProjectile` | 生成投射物 |
| `UItemAction_SpawnPickup` | 掉落材料 |
| `UItemAction_ModifyStat` | 永久加属性（本局内，Instant GE 改 BaseValue） |
| `UItemAction_Composite` | 组合多个 Action |

### 9.2 事件总线
```cpp
UCLASS()
class UGameEventSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    void BroadcastEvent(FGameplayTag EventTag, const FPotatoEventContext& Ctx);
    FDelegateHandle Subscribe(const FItemTriggerDefinition& Trig, AActor* Owner);
    void Unsubscribe(FDelegateHandle Handle);
private:
    TMap<FGameplayTag, TArray<FSubscription>> Subscriptions;
};
```

> 也可以用 GAS 自带的 `UAbilitySystemComponent::GenericGameplayEventCallbacks` +
> `UAbilityTask_WaitGameplayEvent`，但自写事件总线在本项目里更轻、更好调试、不依赖 Ability 实例。

### 9.3 防止连锁爆炸（必须做）
"击杀时爆炸 → 爆炸击杀 → 再爆炸"会无限递归。
- 每个 Trigger 带 `InternalCooldown`
- 全局递归深度计数器，超过 N（如 3）直接返回
- 爆炸伤害的 Spec 加 `Damage.NoTrigger` Tag，`Condition` 里排除掉

---

## 十、性能：1000 敌人下 GAS 怎么活

### 10.1 核心策略：分层
| 单位 | 方案 |
|---|---|
| 玩家（1 个） | 完整 ASC + AttributeSet + GE + Cue |
| Boss / 精英（1~10 个） | 完整 ASC，但简化 Cue |
| 普通敌人（1000 个） | **不挂 ASC**，用 `FEnemyStats` POD struct |

**没有 ASC 的敌人怎么受伤？**
```cpp
void UWeaponSlotComponent::ApplyLightweightDamage(AActor* Target)
{
    // 同样走 UPotatoDamageExecution 的公式，但把公式抽到 PotatoCore 的纯函数里
    FDamageCalcInput In;
    In.WeaponBase       = WeaponDef->BaseDamage;
    In.MeleeScaling     = WeaponDef->MeleeScaling;
    In.SourceMelee      = CachedPlayerStats.MeleeDamage;  // 玩家属性缓存，帧级更新
    In.TargetArmor      = EnemyMgr->GetStats(Target).Armor;
    /* ... */
    const FDamageCalcResult R = FPotatoDamageFormula::Calculate(In);

    EnemyMgr->ApplyDamage(TargetHandle, R.FinalDamage, R.bIsCrit);
}
```

> **关键设计**：把伤害公式抽成 `PotatoCore` 里的**纯函数** `FPotatoDamageFormula::Calculate()`，
> `UPotatoDamageExecution` 和轻量路径**调用同一个函数**。这样公式只有一份，不会出现"玩家打怪和怪打玩家算法不一致"的 Bug，且纯函数可以单元测试。

### 10.2 玩家属性缓存
敌人打玩家、玩家打敌人都需要读属性。不要每次都 `ASC->GetNumericAttribute()`（有虚函数+Map 查找）。
```cpp
// 每帧开始或属性变化回调时刷新一次
struct FCachedPlayerStats
{
    float MeleeDamage, RangedDamage, DamagePercent, CritChance, CritMultiplier, Armor, Dodge;
};
// 注册 ASC->GetGameplayAttributeValueChangeDelegate(Attr).AddUObject(...) 做脏标记
```

### 10.3 其他优化点
| 问题 | 对策 |
|---|---|
| GE 频繁 Apply/Remove 产生分配 | 预生成 GE 对象缓存；避免每帧挂 GE |
| Periodic GE 太多 | 1000 个敌人挂燃烧 → 用自己的 DoT 管理器（数组批处理），不用 GE |
| GameplayCue 风暴 | 敌人特效绕过 Cue，用 Niagara 数组批量驱动；跳字做频率限制与合并 |
| Attribute 变化回调过多 | 只订阅 UI 真正需要的属性 |
| `AggregatorEvaluate` 开销 | 减少 `bSnapshot=false` 的捕获数量；MMC 依赖链保持浅 |

---

## 十一、调试与工具链

### 11.1 GAS 自带调试
```
showdebug abilitysystem        # 显示属性、激活的 GE、Tags（Page Up/Down 切页）
AbilitySystem.Debug.NextTarget
GameplayCue.DisplayCueNotifies 1
AbilitySystem.DebugAbilityTags 1
```
以及 **Gameplay Debugger**（按 `'` 键，选 Abilities 类别）。

### 11.2 自建工具（强烈建议做）
1. **属性溯源面板**：显示每个属性的最终值 + 每个 Modifier 的来源道具名与数值。
   ```cpp
   // 遍历 ASC 上所有 ActiveGE，读 Spec->GetContext().GetSourceObject()
   ```
   这个面板能省下 50% 的数值 Bug 排查时间。
2. **DPS 统计**：记录每把武器每波的总伤害、暴击率、实际攻速，导出 CSV 做平衡。
3. **一键给道具/武器**：Cheat 命令 `Potato.GiveItem <Id>`、`Potato.SetWave <N>`。
4. **公式单元测试**：`FPotatoDamageFormula::Calculate()` 是纯函数，用 UE 的 Automation Test 覆盖边界（0 护甲、负护甲、必定暴击、最低伤害）。

### 11.3 GameplayTag 管理
- 用 **C++ 原生 Tag**（`UE_DEFINE_GAMEPLAY_TAG_STATIC` 或 `FNativeGameplayTag`），避免字符串查找和拼写错误
- 集中在 `PotatoGameplayTags.h/.cpp` 的 `FPotatoTags` 单例里
```cpp
struct FPotatoTags
{
    static const FPotatoTags& Get() { return Instance; }
    FGameplayTag Damage_Type_Fire;
    FGameplayTag Status_Invulnerable;
    FGameplayTag SetByCaller_BaseDamage;
    // ...
private:
    static FPotatoTags Instance;
    void Init(UGameplayTagsManager& Mgr);
};
```

---

## 十二、避坑清单

1. **不要直接 `SetHealth()` 造成伤害** —— 必须走 `IncomingDamage` Meta Attribute，否则护甲、事件、无敌帧全部失效。
2. **Meta Attribute 必须在 `PostGameplayEffectExecute` 里清零**，否则下次结算会累加。
3. **`ExecutionCalculation` 不能改 Source 属性** —— 吸血要回施 GE。
4. **`bSnapshot` 语义要想清楚**：Source 属性通常 `true`（开火瞬间锁定），Target 属性通常 `false`（命中时读最新）。搞反会导致"破甲后旧子弹不吃破甲"这类诡异 Bug。
5. **`Infinite` GE 必须记录 Handle**，否则无法精确移除，道具卖不掉。
6. **运行时 `NewObject<UGameplayEffect>` 必须防 GC** —— 用 `UPROPERTY()` 容器持有，最好预生成。
7. **属性依赖不能成环**（A 的 MMC 读 B，B 的 MMC 读 A）→ 死循环崩溃。
8. **`PreAttributeChange` 里不能 `SetXxx()`** —— 会递归。要在 `PostAttributeChange` 做。
9. **Clamp 要写在 `PreAttributeChange`（CurrentValue）和 `PreAttributeBaseChange`（BaseValue）两处**，只写一处会漏。
10. **RNG 必须用自管 Stream 且按用途分流**，`FMath::FRand` 不可复现，无法排查"这次暴击对不对"。
11. **触发型道具必须有 ICD 和递归深度保护**，否则连锁爆炸会卡死。
12. **闪避/暴击上限一定要有**，Brotato 后期属性可以堆到离谱，没有上限会直接破坏游戏。
13. **不要给 1000 个敌人挂 ASC** —— 这是 GAS 项目性能爆炸的头号原因。
14. **伤害公式抽成纯函数**，GAS 路径和轻量路径共用，避免两套逻辑。

---

## 附：实施顺序建议

| 阶段 | 内容 | 工期 |
|---|---|---|
| G1 | ASC 接入、AttributeSet 18 属性、Clamp、`showdebug` 能看到值 | 3 天 |
| G2 | `IncomingDamage` Meta + 简版 Execution（只有基础伤害）打通"能打死怪" | 3 天 |
| G3 | `UItemEffectRegistry` 预生成 GE、道具增删 + 属性溯源面板 | 5 天 |
| G4 | 完整 Execution（闪避/暴击/护甲/取整/吸血）+ 纯函数抽取 + 单元测试 | 5 天 |
| G5 | Buff 系统（Duration/Stack/Period）+ 无敌帧 + 免疫五层级 | 4 天 |
| G6 | 触发型道具三段式 + 事件总线 + ICD 保护 | 5 天 |
| G7 | MMC 动态属性、GameplayCue 表现层 | 3 天 |
| G8 | 性能分层（敌人去 ASC、属性缓存、轻量伤害路径） | 5 天 |

合计约 **6~7 周**，对应上一份路线图里的 M2 + M5 的 GAS 部分。
