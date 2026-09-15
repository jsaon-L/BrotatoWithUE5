# D04 · GameplayAbility / AbilityTask / Cue 与武器战斗实现

> **模块**：`BrotatoGAS`（Abilities / Tasks / Cues）+ `BrotatoGameplay`（Weapons / Projectiles）
> **里程碑**：M1 单武器自动攻击 → M4 全武器 → M6 表现打磨
> **上游规格**：`Docs/00` 全文（★ 最高仲裁）、`Docs/04` 全文、`Docs/07 §2 §4 §5`、`Docs/09 §8 §9`
> **v2.0 变更**：冷却改用 **GAS 原生 Cooldown GE（秒）**；挥击时序改用 `UAbilityTask` 自带 Tick + 秒 + `UCurveFloat`；删除「1 帧延迟 / 2 帧延迟」等 Godot 实现副作用。

---

## 0. 分工总览

```
FBrotatoWeaponMath (BrotatoCore, 纯逻辑, D01 §5)
  ├─ ComputeRuntimeStats(Base, Snapshot, Hooks) → FWeaponRuntimeStats   ★ 11 步
  ├─ GetNextCooldownSeconds(...)                                        ★ 含随机抖动（秒，不取整）
  ├─ ComputeSwingTiming(...)                                            ★ 三段时长
  └─ EaseExpoOut / EaseLinear

FWeaponFireCore (BrotatoGameplay, 非 GAS)
  ├─ SelectTarget(Candidates, SelfPos, MinRange) → Target
  ├─ ShouldFire(...) → bool
  └─ SpawnProjectiles(Weapon, ProjectileSubsystem)

        ↑ 被两处调用
        ├─ Tier A: UGA_WeaponAttack（玩家）→ 走 GAS，吃 Tag 拦截、播 Cue、发事件
        └─ Tier C: ATurret / ALandmine（结构物）→ 直接调用，无 ASC
```

---

## 1. `ABrotatoWeapon`（武器实体）

```cpp
UCLASS()
class BROTATOGAMEPLAY_API ABrotatoWeapon : public AActor
{
    GENERATED_BODY()
public:
    // ── 数据 ────────────────────────────────────────────
    UPROPERTY() const UBrotatoWeaponData* Data = nullptr;
    UPROPERTY() FBrotatoItemInstanceId InstanceId;
    UPROPERTY() int32 SlotIndex = 0;

    // ── 运行时 ──────────────────────────────────────────
    FWeaponRuntimeStats RuntimeStats;          // 脏时重算（模拟阶段 S11）
    int32  NbShotsTaken          = 0;
    bool   bIsShooting           = false;
    float  Rotation              = 0.f;        // 弧度
    float  IdleAngle             = 0.f;
    bool   bFlipV                = false;
    int32  TrackedValue          = 0;          // 统计（合成时继承）
    int32  DmgDealtLastWave      = 0;
    int32  KillCountForStatGain  = 0;          // GainStatEveryKilledEnemies

    // ── 目标 ────────────────────────────────────────────
    TArray<TWeakObjectPtr<AActor>> TargetsInRange;   // 由碰撞查询维护
    int32 CurrentTargetIndex = INDEX_NONE;           // 指向 FEnemyState 的索引

    // ── Hitbox（近战） ─────────────────────────────────
    FVector2D HitboxExtents  = FVector2D(72.f, 16.f);
    bool  bHitboxActive        = false;
    TSet<int32> IgnoredThisSwing;                    // 敌人索引（★ 同一次挥击的去重）

    // ── GAS ─────────────────────────────────────────────
    FGameplayAbilitySpecHandle AbilitySpecHandle;
    FActiveGameplayEffectHandle CooldownHandle;       // ★ Cooldown GE 句柄（供压缩/查询）

    /** 由 UBrotatoSimSubsystem 在阶段 S7a 调用（瞄准 + 选靶）；冷却由 GAS 计时 */
    void StepAim(float DeltaTime);

    void MarkStatsDirty() { bStatsDirty = true; }
    void RecomputeStats(const FBrotatoStatSnapshot& S, const FBrotatoHookView& H);
    void CompressCooldown();                         // 攻速变化 / 移动状态切换时

    void EnableHitbox()  { IgnoredThisSwing.Reset(); bHitboxActive = true; }   // ★ 立即生效
    void DisableHitbox() { bHitboxActive = false; IgnoredThisSwing.Reset(); }

    bool IsOnCooldown() const;                       // 查 Cooldown.Weapon.{SlotIndex} Tag
    float GetCooldownRemaining() const;              // ASC->GetActiveGameplayEffectRemainingDuration

    bool IsMelee() const { return Data->Type == EWeaponType::Melee; }
private:
    bool bStatsDirty = true;
};
```

### 1.1 挂点布局（★ 1–6 槽必须用常量表）

```cpp
// §04 §6.4：坐标相对 Weapons 容器，容器挂在玩家根 (0, -24)
static const TArray<TArray<FVector2D>> WeaponAttachPoints =
{
    /* 1 */ { {  0,  30} },
    /* 2 */ { { 40,  20}, {-40,  20} },
    /* 3 */ { { 40,  20}, {-40,  20}, {  0, -45} },
    /* 4 */ { { 40,  35}, {-40,  35}, {-40, -30}, { 40, -30} },
    /* 5 */ { { 35,  35}, {-35,  35}, {-55, -20}, { 55, -20}, {  0, -55} },
    /* 6 */ { { 35,  45}, {-35,  45}, {-60,   0}, { 60,   0}, {-35, -45}, { 35, -45} },
};

FVector2D GetAttachPoint(int32 Index, int32 Total)
{
    if (Total <= 6) return WeaponAttachPoints[Total - 1][Index];
    // >6 把（Multitasker 12 槽）才用圆形排布
    const float R = 60.f + (Total - 6) * 5.f;
    const float A = Index * 2.f * PI / Total;
    return FVector2D(R * FMath::Cos(A), R * FMath::Sin(A));
}
```

贴图翻转：`RotationDegrees ∈ (-90, 90)` 判定朝右 → `bFlipV = false`，否则 `true`。
武器列表变化（购买 / 回收 / 合成）时整体 `UpdateWeaponsPositions()` 重排。

### 1.2 `StepAim`（阶段 S7a）

```cpp
void ABrotatoWeapon::StepAim(float DeltaTime)
{
    // 瞄准
    if (RunSubsystem->IsManualAim())
        Rotation = (GetMouseWorldPos() - GetActorLocation2D()).GetSafeNormal().ToAngle();
    else
        Rotation = bIsShooting ? GetDirectionToCurrentTarget()
                               : GetDirectionAndRecalculateTarget();

    // ★ 无冷却递减代码：冷却由 Cooldown GE 计时（见 §3）
}
```

> **hitbox 不再延迟 1 帧**：原作 `set_deferred("disabled", ...)` 的 1 帧延迟是 Godot 的实现副作用，
> 其真实语义「同一次挥击对同一目标只命中一次」由 `IgnoredThisSwing` 保证（§00 §3.3）。

---

## 2. RuntimeStats 重算时机

```cpp
void ABrotatoWeapon::RecomputeStats(const FBrotatoStatSnapshot& S, const FBrotatoHookView& H)
{
    if (!bStatsDirty) return;
    bStatsDirty = false;

    const int32 SameCount = RunSubsystem->CountWeaponsWithId(Data->WeaponId);
    RuntimeStats = FBrotatoWeaponMath::ComputeRuntimeStats(
        Data->Stats, S, H, SameCount, RunSubsystem->GetLevel(),
        IsMelee(), /*bIsStructure*/ false);

    // ★ 攻速变化可能压缩当前冷却
    CompressCooldown();
    HitboxExtents = Data->Stats.HitboxExtents;
}
```

| 触发 MarkStatsDirty | 说明 |
|---|---|
| 开波 | 全量 |
| 任意属性变化（`OnAttributeChanged`） | 脏标记合批，阶段 S11 统一重算 |
| 装备/卸载武器 | 影响 WeaponStack、套装、条件加成 |
| 装备/卖出物品 | 影响 weapon_bonus / class_bonus |
| 升级选择 | 影响 `stat_levels` scaling |

> ⚠️ **绝不每 Tick 重算**。6 把武器 × 11 步 × 每 Tick = 无谓开销；且原作也是事件驱动。

---

## 3. 冷却管理（★ GAS 原生 Cooldown GE）

**v2.0 决议反转**：旧版以「帧精度 / 抖动 / 射击中暂停」为由拒绝 Cooldown GE，逐条给出原生解法：

| Brotato 需求 | ★ UE 原生解法 |
|---|---|
| 攻速下限（原作 2 帧）/ 抖动下限（1 帧） | 秒制常量 `MinWeaponCooldown = 1/30`、`MinJitteredCooldown = 1/60`；时间量不取整 |
| 每次射击 `±min(n×CD/5, n×0.0833s)` 随机抖动 | 重写 `ApplyCooldown()` + `SetByCaller` —— SetByCaller 的标准用途 |
| **射击过程中冷却不递减** | **攻击序列结束时才 Apply Cooldown GE**（与原作"开火即设定 + 射击期间冻结"数学等价） |
| 攻速变化立即压缩当前冷却 | `ModifyActiveGameplayEffectStartTime()` 或 Remove + Apply 剩余时长 |
| 大装填（每 X 发 × 倍率） | 在 `GetNextCooldownSeconds()` 内判断后写入 Spec |

### 3.1 GE 资产 `GE_WeaponCooldown`

| 字段 | 值 |
|---|---|
| Duration Policy | `HasDuration` |
| Duration Magnitude | **`SetByCaller`**（Tag = `Brotato.Data.Cooldown`） |
| Granted Tags | 运行时通过 `Spec->DynamicGrantedTags` 加 `Brotato.Cooldown.Weapon.{SlotIndex}` |
| Modifiers | 无（纯计时 + Tag） |

```cpp
// UGA_WeaponAttack 构造：CooldownGameplayEffectClass = UGE_WeaponCooldown::StaticClass();
// 该 Ability 的 CooldownTags 需含 Brotato.Cooldown.Weapon.{Slot}，
// 使基类 CheckCooldown() 自动生效（无需手写计数器判断）。

void UGA_WeaponAttack::ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
                                     const FGameplayAbilityActorInfo* ActorInfo,
                                     const FGameplayAbilityActivationInfo ActivationInfo) const
{
    ABrotatoWeapon* W = Cast<ABrotatoWeapon>(GetCurrentSourceObject());
    const float NextCd = FBrotatoWeaponMath::GetNextCooldownSeconds(
        W->RuntimeStats, RunSubsystem->Weapons.Num(), W->NbShotsTaken,
        W->Data->Stats, RunSubsystem->GetRandom(EBrotatoRngChannel::Combat));

    FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(
        CooldownGameplayEffectClass, GetAbilityLevel());
    Spec.Data->SetSetByCallerMagnitude(BrotatoTags::Data_Cooldown, NextCd);
    Spec.Data->DynamicGrantedTags.AddTag(W->GetCooldownTag());
    W->CooldownHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, Spec);
}
```

### 3.2 冷却压缩（对齐原作 `reset_cooldown`）

```cpp
void ABrotatoWeapon::CompressCooldown()
{
    if (!CooldownHandle.IsValid()) return;
    const float Remaining = ASC->GetActiveGameplayEffectRemainingDuration(CooldownHandle);
    const float Target    = FMath::Min(Remaining, RuntimeStats.CooldownSeconds);
    if (Target < Remaining)
        ASC->ModifyActiveGameplayEffectStartTime(CooldownHandle, Remaining - Target);
}
```

### 3.3 UI 与查询

```cpp
bool  ABrotatoWeapon::IsOnCooldown()        const { return ASC->HasMatchingGameplayTag(GetCooldownTag()); }
float ABrotatoWeapon::GetCooldownRemaining() const
{ return ASC->GetActiveGameplayEffectRemainingDuration(CooldownHandle); }
```

UI 显示的"总攻击间隔"（§04 §2.2）：
- 近战：`SwingTiming.TotalDuration() + RuntimeStats.CooldownSeconds`
- 远程：`RecoilDuration × 2 + RuntimeStats.CooldownSeconds`

**Tier C（炮塔 / 结构物，无 ASC）**：结构体内 `float CooldownRemaining -= DeltaTime`，公式仍调用
`FBrotatoWeaponMath::GetNextCooldownSeconds()`，与 Tier A 共享同一份秒制实现。

---

## 4. 选靶与开火判定 `FWeaponFireCore`

```cpp
namespace FWeaponFireCore
{
    /** 候选集由碰撞查询按半径 max_range + 200 维护 */
    int32 SelectTarget(const TArray<int32>& Candidates, const UEnemySubsystem* Enemies,
                       FVector2D SelfPos, float MinRange)
    {
        int32 Best = INDEX_NONE; float BestDistSq = TNumericLimits<float>::Max();
        for (int32 Idx : Candidates)
        {
            const FEnemyState& E = Enemies->Get(Idx);
            if (!E.bActive || E.bDead) continue;
            const float DSq = FVector2D::DistSquared(E.Pos, SelfPos);
            if (DSq < MinRange * MinRange) continue;        // ★ 小于 min_range 不选
            if (DSq < BestDistSq) { BestDistSq = DSq; Best = Idx; }
        }
        return Best;
    }

    bool ShouldFire(const ABrotatoWeapon& W, const UEnemySubsystem* Enemies,
                    bool bCanAttackWhileMoving, bool bPlayerMoving, bool bManualAim, bool bCleaningUp)
    {
        if (W.IsOnCooldown()) return false;       // ★ Cooldown GE 的 Tag 查询（Tier C 查 CooldownRemaining）
        if (!bCanAttackWhileMoving && bPlayerMoving) return false;       // Soldier
        if (bManualAim && !bCleaningUp) return true;
        if (W.CurrentTargetIndex == INDEX_NONE) return false;

        const float Dist = FVector2D::Distance(Enemies->Get(W.CurrentTargetIndex).Pos,
                                               W.GetActorLocation2D());
        const float Upper = W.RuntimeStats.MaxRange
                          + (W.IsSweepMode() ? 0.f : BrotatoConst::FireRangeAdd);   // ★ SWEEP 不加 50
        return Dist >= W.RuntimeStats.MinRange && Dist <= Upper;
    }
}
```

三种 range 的语义（切勿混用）：

| 名称 | 值 | 用途 |
|---|---|---|
| `MinRange` | 默认 0（`Rule.NoMinRange` → 0） | 小于此距离的敌人不被选为目标 |
| `MaxRange` | 近战 `max(25, base + range/2)`；远程 `max(25, base + range)` | 近战 = 位移距离；远程 = 子弹存活距离 |
| Detection | `MaxRange + 200` | 候选集查询半径 |
| 开火上界 | `MaxRange + 50`（SWEEP 时 `MaxRange`） | `ShouldFire` |
| 子弹销毁 | `MaxRange + 100`（手动瞄准 +50） | 飞行距离超出即回收 |

---

## 5. `UGA_WeaponAttack`

### 5.1 类定义

```cpp
UCLASS()
class BROTATOGAS_API UGA_WeaponAttack : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UGA_WeaponAttack()
    {
        InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
        NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;   // 单机
        // ★ 使用 GAS 原生冷却（见 §3）
        CooldownGameplayEffectClass = UGE_WeaponCooldown::StaticClass();
        ActivationBlockedTags.AddTag(FBrotatoTags::Get().State_Dead);
        ActivationBlockedTags.AddTag(FBrotatoTags::Get().State_CleaningUp);
    }

    /** ★ CooldownTags 按槽位动态设置，使基类 CheckCooldown() 生效 */
    virtual const FGameplayTagContainer* GetCooldownTags() const override;

    /** ★ 用 SetByCaller 写入本次抖动后的冷却秒数（见 §3.1） */
    virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle,
                               const FGameplayAbilityActorInfo* ActorInfo,
                               const FGameplayAbilityActivationInfo ActivationInfo) const override;

    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                   const FGameplayAbilityActorInfo* Info,
                                   const FGameplayTagContainer* SourceTags,
                                   const FGameplayTagContainer* TargetTags,
                                   OUT FGameplayTagContainer* Relevant) const override;

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                 const FGameplayAbilityActorInfo* Info,
                                 const FGameplayAbilityActivationInfo Activation,
                                 const FGameplayEventData* TriggerData) override;
private:
    UFUNCTION() void OnHitboxEnable();
    UFUNCTION() void OnHitboxDisable();
    UFUNCTION() void OnSwingFinished();
};
```

### 5.2 `CanActivateAbility`

```cpp
bool UGA_WeaponAttack::CanActivateAbility(...) const
{
    if (!Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, Relevant)) return false;

    const ABrotatoWeapon* W = Cast<ABrotatoWeapon>(GetCurrentSourceObject());
    if (!W || W->bIsShooting) return false;
    // 冷却由基类 CheckCooldown()（GetCooldownTags 已返回槽位 Tag）在 Super 内判定

    UAbilitySystemComponent* ASC = Info->AbilitySystemComponent.Get();
    const FBrotatoTags& T = FBrotatoTags::Get();

    // Soldier：can_attack_while_moving == 0
    const bool bCanWhileMoving = ASC->HasMatchingGameplayTag(T.Rule_CanAttackWhileMoving);
    const bool bMoving         = ASC->HasMatchingGameplayTag(T.State_Moving);

    return FWeaponFireCore::ShouldFire(*W, EnemySubsystem, bCanWhileMoving, bMoving,
                                       RunSubsystem->IsManualAim(),
                                       ASC->HasMatchingGameplayTag(T.State_CleaningUp));
}
```

### 5.3 `ActivateAbility`

```cpp
void UGA_WeaponAttack::ActivateAbility(...)
{
    ABrotatoWeapon* W = Cast<ABrotatoWeapon>(GetCurrentSourceObject());
    W->bIsShooting = true;
    const FBrotatoTags& T = FBrotatoTags::Get();

    if (W->IsMelee())
    {
        auto* Task = UAbilityTask_MeleeSwing::Create(this, W);
        Task->OnHitboxEnable .AddDynamic(this, &ThisClass::OnHitboxEnable);
        Task->OnHitboxDisable.AddDynamic(this, &ThisClass::OnHitboxDisable);
        Task->OnFinished     .AddDynamic(this, &ThisClass::OnSwingFinished);
        Task->ReadyForActivation();
    }
    else
    {
        // ★ 远程：音效在最开头播（早于子弹生成）
        K2_ExecuteGameplayCue(T.Cue_Weapon_RangedFire);
        FWeaponFireCore::SpawnProjectiles(W, ProjectileSubsystem, Rng);

        // 后坐动画 + 冷却冻结期：recoil_duration × 2（秒）
        auto* Task = UAbilityTask_WaitDelay::WaitDelay(this, W->RuntimeStats.RecoilDuration * 2.f);
        Task->OnFinish.AddDynamic(this, &ThisClass::OnSwingFinished);
        Task->ReadyForActivation();
    }

    W->NbShotsTaken++;
    // ★ 冷却在攻击序列结束（OnSwingFinished → ApplyCooldown）时才施加，
    //   等价于原作"射击期间冷却不流逝"（见 §3）
}

void UGA_WeaponAttack::OnSwingFinished()
{
    if (ABrotatoWeapon* W = Cast<ABrotatoWeapon>(GetCurrentSourceObject()))
    { W->bIsShooting = false; W->DisableHitbox(); }
    // 先施加冷却，再结束能力
    ApplyCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo);
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
```

### 5.4 多武器 = 多 Spec 同类

```cpp
// 装备武器时
FGameplayAbilitySpec Spec(UGA_WeaponAttack::StaticClass(), /*Level*/1, /*InputID*/INDEX_NONE,
                          /*SourceObject*/WeaponActor);
WeaponActor->AbilitySpecHandle = ASC->GiveAbility(Spec);

// 卸载时
ASC->ClearAbility(WeaponActor->AbilitySpecHandle);
```

**逻辑上在阶段 S7b 统一驱动**：

```cpp
for (ABrotatoWeapon* W : Weapons)
    ASC->TryActivateAbility(W->AbilitySpecHandle);
```

> `InstancedPerActor` + 不同 `SourceObject` → 6 把武器共用同一 Ability 类但各自独立实例与 Cooldown GE。
> 验收 `B03`：6 把武器 6 个 Spec，各自独立 Cooldown GE（互不干扰）。

---

## 6. `UAbilityTask_MeleeSwing`（练 AbilityTask 的最佳素材）

### 6.1 类定义

```cpp
UCLASS()
class BROTATOGAS_API UAbilityTask_MeleeSwing : public UAbilityTask
{
    GENERATED_BODY()
public:
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSwingEvent);
    UPROPERTY(BlueprintAssignable) FSwingEvent OnHitboxEnable;
    UPROPERTY(BlueprintAssignable) FSwingEvent OnHitboxDisable;
    UPROPERTY(BlueprintAssignable) FSwingEvent OnFinished;

    static UAbilityTask_MeleeSwing* Create(UGameplayAbility* Owning, ABrotatoWeapon* W);

    UAbilityTask_MeleeSwing() { bTickingTask = true; }   // ★ AbilityTask 原生 Tick

    virtual void Activate() override;
    virtual void OnDestroy(bool bInOwnerFinished) override;
    virtual void TickTask(float DeltaTime) override;

private:
    enum class EPhase : uint8
    { Recoil, Forward, SweepFirstHalf, SweepSecondHalf, Return, Done };

    void AdvancePhase();
    void EnterPhase(EPhase P, float DurationSeconds)
    { Phase = P; PhaseDuration = FMath::Max(KINDA_SMALL_NUMBER, DurationSeconds); PhaseElapsed = 0.f; }
    void UpdateSpriteTransform();

    UPROPERTY() ABrotatoWeapon* Weapon = nullptr;
    FMeleeSwingTiming Timing;
    EPhase Phase = EPhase::Recoil;
    float  PhaseElapsed  = 0.f;      // ★ 秒
    float  PhaseDuration = 0.f;      // ★ 秒
    bool   bSweep = false;
    float  SideRange = 0.f;
    float  SweepRange = 0.f;
    FVector2D InitialLocalPos = FVector2D::ZeroVector;

    /** 缓动曲线资产（可选；未设置时回退到 EaseExpoOut / EaseLinear） */
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UCurveFloat> ThrustCurve;
    UPROPERTY(EditDefaultsOnly) TObjectPtr<UCurveFloat> SweepCurve;
};
```

> **为什么用 `bTickingTask` 而非自建 Tick 接口**：`UAbilityTask` 原生支持 Tick，随 Ability 生命周期自动启停，
> 且受 Pause / TimeDilation 正确影响；旧版实现 `IFixedTickable` 需要额外注册/反注册且会与 Task 生命周期打架。

### 6.2 `Activate`

```cpp
void UAbilityTask_MeleeSwing::Activate()
{
    const FWeaponRuntimeStats& O = Weapon->RuntimeStats;

    // 攻击类型：alternate_attack_type 时 THRUST/SWEEP 交替（Sword / Fighting Stick / Excalibur）
    bSweep = Weapon->Data->Stats.bAlternateAttackType
           ? (Weapon->NbShotsTaken % 2 == 1)
           : (Weapon->Data->Stats.AttackType == EMeleeAttackType::Sweep);

    float EffectiveRange = O.MaxRange;
    if (bSweep)
    {
        const float DistToTarget = Weapon->GetDistanceToCurrentTarget();
        EffectiveRange = FBrotatoWeaponMath::SweepAtkDistance(O.MaxRange, DistToTarget);
        SideRange  = EffectiveRange * 0.5f;
        SweepRange = EffectiveRange;
    }

    Timing = FBrotatoWeaponMath::ComputeSwingTiming(
        Snapshot.AttackSpeed, Weapon->Data->Stats.AttackSpeedMod,
        EffectiveRange, O.RecoilDuration);

    InitialLocalPos = Weapon->GetSpriteLocalPos();
    EnterPhase(EPhase::Recoil, Timing.RecoilDuration);
}
```

### 6.3 `TickTask` 与阶段推进（★ 秒 + 余量结转）

```cpp
void UAbilityTask_MeleeSwing::TickTask(float DeltaTime)
{
    PhaseElapsed += DeltaTime;
    if (PhaseElapsed < PhaseDuration) { UpdateSpriteTransform(); return; }

    // ★ 余量结转：低帧率时一次 Tick 可能跨过整个阶段，
    //   把超出部分带进下一阶段，保证总时长准确（关键！）
    const float Carry = PhaseElapsed - PhaseDuration;
    AdvancePhase();
    PhaseElapsed = FMath::Min(Carry, PhaseDuration);
    UpdateSpriteTransform();
}

void UAbilityTask_MeleeSwing::AdvancePhase()
{
    const bool bDmgOnReturn = Weapon->Data->Stats.bDealDmgOnReturn;
    switch (Phase)
    {
    case EPhase::Recoil:
        // ★ 音效在此播放（早于 hitbox 开启，原作如此 —— 给玩家预判感）
        AbilitySystemComponent->ExecuteGameplayCue(FBrotatoTags::Get().Cue_Weapon_MeleeSwing);
        OnHitboxEnable.Broadcast();
        EnterPhase(bSweep ? EPhase::SweepFirstHalf : EPhase::Forward,
                   bSweep ? Timing.AtkDuration / 4.f : Timing.AtkDuration / 2.f);
        break;

    case EPhase::Forward:
        if (!bDmgOnReturn) OnHitboxDisable.Broadcast();
        EnterPhase(EPhase::Return, Timing.BackDuration);
        break;

    case EPhase::SweepFirstHalf:
        EnterPhase(EPhase::SweepSecondHalf, Timing.AtkDuration / 4.f);
        break;

    case EPhase::SweepSecondHalf:
        if (!bDmgOnReturn) OnHitboxDisable.Broadcast();
        EnterPhase(EPhase::Return, Timing.BackDuration);
        break;

    case EPhase::Return:
        if (bDmgOnReturn) OnHitboxDisable.Broadcast();
        Phase = EPhase::Done;
        OnFinished.Broadcast();
        EndTask();
        break;
    default: break;
    }
}
```

### 6.4 位移插值（★ 曲线必须照抄）

```cpp
void UAbilityTask_MeleeSwing::UpdateSpriteTransform()
{
    const float T = PhaseDuration > 0.f
                  ? FMath::Clamp(PhaseElapsed / PhaseDuration, 0.f, 1.f) : 1.f;
    const float Recoil = Weapon->RuntimeStats.Recoil;
    const float Sign   = Weapon->bFlipV ? -1.f : 1.f;

    switch (Phase)
    {
    case EPhase::Recoil:
    {   // 后拉：EXPO / OUT
        const float A = FBrotatoWeaponMath::EaseExpoOut(T);
        FVector2D P = InitialLocalPos;
        P.X -= Recoil * A;
        if (bSweep) { P.Y += Sign * SideRange * A; Weapon->SetSpriteRotation(Sign * 0.9f * PI * A); }
        Weapon->SetSpriteLocalPos(P);
        break;
    }
    case EPhase::Forward:
    {   // 前刺：EXPO / OUT
        const float A = FBrotatoWeaponMath::EaseExpoOut(T);
        Weapon->SetSpriteLocalPos({ InitialLocalPos.X - Recoil + (Recoil + Weapon->RuntimeStats.MaxRange) * A,
                                    InitialLocalPos.Y });
        break;
    }
    case EPhase::SweepFirstHalf:
    {   // ★ 扫动：LINEAR（保证判定均匀）
        const float A = FBrotatoWeaponMath::EaseLinear(T);
        Weapon->SetSpriteLocalPos({ InitialLocalPos.X - Recoil + (Recoil + SweepRange * 0.75f) * A,
                                    InitialLocalPos.Y + Sign * SideRange * (1.f - A) });
        Weapon->SetSpriteRotation(Sign * 0.9f * PI * (1.f - A));
        break;
    }
    case EPhase::SweepSecondHalf:
    {   // ★ LINEAR
        const float A = FBrotatoWeaponMath::EaseLinear(T);
        Weapon->SetSpriteLocalPos({ InitialLocalPos.X + SweepRange * 0.75f
                                    - (SweepRange * 0.75f + Recoil) * A,
                                    InitialLocalPos.Y - Sign * SideRange * A });
        Weapon->SetSpriteRotation(-Sign * 0.9f * PI * A);
        break;
    }
    case EPhase::Return:
    {   // 收回：EXPO / OUT
        const float A = FBrotatoWeaponMath::EaseExpoOut(T);
        Weapon->SetSpriteLocalPos(FMath::Lerp(Weapon->GetSpriteLocalPos(), InitialLocalPos, A));
        Weapon->SetSpriteRotation(FMath::Lerp(Weapon->GetSpriteRotation(), 0.f, A));
        break;
    }
    default: break;
    }
}
```

### 6.5 THRUST vs SWEEP 对照

| 维度 | THRUST | SWEEP |
|---|---|---|
| 位移目标 | `x + max_range` | 先 `y ± side_range` 起手，再横扫至 `x + sweep_range×0.75` |
| 旋转 | 不旋转 | `±0.9π → 0 → ∓0.9π`（≈162°） |
| 命中段 | `AtkDuration/2`（1 段） | `AtkDuration/4 × 2`（2 段，合计相同） |
| 命中段曲线 | EXPO/OUT | **LINEAR** |
| 距离来源 | 固定 `max_range` | `min(max_range, max(250, 到目标距离))` |
| 开火判定上界 | `max_range + 50` | **`max_range`** |

`bDealDmgOnReturn = true` 时 hitbox 保持到收回结束；但 `IgnoredThisSwing` 未清空，**已命中的敌人不会二次受伤**，只能打到新进入的敌人。

### 6.6 Ability 内的等待：用 GAS 原生 Task

```cpp
// ★ 直接用引擎自带的 UAbilityTask_WaitDelay（秒）
auto* Task = UAbilityTask_WaitDelay::WaitDelay(this, DurationSeconds);
Task->OnFinish.AddDynamic(this, &ThisClass::OnSwingFinished);
Task->ReadyForActivation();
```

> 旧版自建的 `UAbilityTask_WaitFixedFrames`（按整数帧倒数）**已删除**：
> `UAbilityTask_WaitDelay` 走 World Timer，能正确响应 Pause 与 TimeDilation，且无需注册到自建 Tick 表。
> 若需"等待某个 GameplayEvent"，用 `UAbilityTask_WaitGameplayEvent` 而非轮询。

---

## 7. 近战命中去重与 Hitbox

```cpp
// 模拟阶段 S9：旋转矩形命中查询（UE 原生；兜底走空间哈希）
void UCombatSubsystem::ProcessMeleeHitboxes()
{
    for (ABrotatoWeapon* W : Weapons)
    {
        if (!W->bHitboxActive) continue;
        TArray<int32> Hits;
        // UE 原生：FCollisionShape::MakeBox + FQuat 直接表达旋转矩形
        CombatQuery->QueryRotatedBox(W->GetHitboxCenter(), W->HitboxExtents,
                                     W->Rotation, EBrotatoLayer::Enemies, Hits);
        for (int32 Idx : Hits)
        {
            if (W->IgnoredThisSwing.Contains(Idx)) continue;   // ★ 一次挥击每敌 1 命中
            W->IgnoredThisSwing.Add(Idx);
            ApplyWeaponHit(*W, Idx);
        }
    }
}
```

| 层 | 规则 |
|---|---|
| 近战一次挥击 | 命中后加入 `IgnoredThisSwing`；`DisableHitbox()` 时清空 |
| 投射物一次命中 | `Ignored = {ThingHit}`（**赋值覆盖，不是 append**）→ 穿透时可再次打到之前打过的目标 |
| 启用时机 | `EnableHitbox()` **立即生效**（★ 不复刻原作 `set_deferred` 的 1 帧延迟，§00 §3.3） |
| 多段命中 | 只靠高攻速 / 低 cooldown（Drill CD = 0.0167 s），**不是** hitbox 重复触发 |

Hitbox extents 全表见 `Docs/04 §8.5`，逐武器照抄进 `DT_Weapons.HitboxExtents`。

---

## 8. 远程与投射物

### 8.1 生成流程

```cpp
void FWeaponFireCore::SpawnProjectiles(ABrotatoWeapon* W, UProjectileSubsystem* PS, FBrotatoRandom& Rng)
{
    const FWeaponRuntimeStats& O = W->RuntimeStats;

    // ① 精度：整把武器的 rotation 偏移，每次射击 roll 一次
    const float Spread = Rng.RandRange(-(1.f - O.Accuracy), (1.f - O.Accuracy));
    const float BaseRot = W->Rotation + Spread;

    // ② 每发子弹独立 roll 散射；★ 同一 Tick 全部生成，没有时间 burst
    for (int32 i = 0; i < FMath::Max(1, O.NbProjectiles); ++i)
    {
        const float Rot = Rng.RandRange(BaseRot - O.ProjectileSpread, BaseRot + O.ProjectileSpread);
        FProjectileInstance P;
        P.Pos = P.SpawnPos = W->GetMuzzleWorldPos();
        P.Velocity = FVector2D(FMath::Cos(Rot), FMath::Sin(Rot)) * O.ProjectileSpeed;
        P.Damage = O.Damage; P.CritChance = O.CritChance; P.CritDamage = O.CritDamage;
        P.Piercing = O.Piercing; P.PiercingDmgReduction = O.PiercingDmgReduction;
        P.Bounce   = O.Bounce;   P.BounceDmgReduction   = O.BounceDmgReduction;
        P.MaxRange = O.MaxRange; P.KnockbackAmount = O.Knockback;
        // ★ 击退方向取负
        P.KnockDir = -FVector2D(FMath::Cos(Rot), FMath::Sin(Rot));
        P.LaunchDelay = BrotatoConst::ProjectileLaunchDelay;   // 0.02 s
        P.RotationSpeed = W->Data->Stats.ProjectileRotationSpeed;
        P.SourceWeaponIndex = W->SlotIndex;
        PS->Spawn(P);
    }
}
```

### 8.2 `FProjectileInstance`（Tier D，非 Actor）

```cpp
USTRUCT()
struct BROTATOGAMEPLAY_API FProjectileInstance
{
    GENERATED_BODY()
    FVector2D Pos, Velocity, SpawnPos, KnockDir;
    float Damage = 0.f, CritChance = 0.f, CritDamage = 1.5f;
    int32 Piercing = 0, Bounce = 0;
    float PiercingDmgReduction = 0.5f, BounceDmgReduction = 0.5f;
    float MaxRange = 0.f, KnockbackAmount = 0.f;
    float LaunchDelay = 0.02f;              // ★ 出膛停顿（秒），累减到 0 才开始移动
    float RotationSpeed = 0.f;              // 贴图旋转（手里剑）：度/秒（原作每帧 25° → 1500）
    uint8 bActive : 1;
    int32 SourceWeaponIndex = INDEX_NONE;
    TArray<int32> Ignored;                  // ★ 赋值覆盖语义
};

UCLASS()
class UProjectileSubsystem : public UWorldSubsystem
{
    TArray<FProjectileInstance> Pool;       // 预分配 2048
    TArray<int32> FreeList;
public:
    void Step(float DeltaTime);             // 模拟阶段 S8：批量位移 + 命中查询 + 结算
    void SyncToRender();                    // 一次性写入 Niagara / ISM
};
```

| 参数 | 值 | 出处 |
|---|---|---|
| Hitbox | Circle `r = 44`，偏移 `(23, 0)` | §04 §4.3 |
| Sprite 偏移 | `(24, 0)` | 同上 |
| 碰撞层 | `PlayerProjectiles`（自定义 Object Channel） | §06 §11.2 |
| 出膛延迟 | **0.02 s**（原作 0.02 s；旧版曾取整为 2 帧，现直接用秒） | §04 §4.3 |
| 射程销毁 | 飞行距离 > `MaxRange + 100`（手动瞄准 +50） | 同上 |
| 离屏销毁 | 离屏后 **1.0 s** | 同上 |
| 贴图旋转 | `RotationSpeed != 0` → `Angle += RotationSpeed * DeltaTime`（1500 °/s） | 同上 |
| 透明度 | `Settings.ProjectileOpacity` | §07 §6 |

### 8.3 穿透 / 弹射（★ bounce 优先）

```cpp
void UProjectileSubsystem::OnHit(FProjectileInstance& P, int32 EnemyIdx, FBrotatoRandom& Rng)
{
    P.Ignored = { EnemyIdx };                              // ★ 赋值覆盖

    if (P.Bounce > 0)                                      // ★ bounce 优先于 piercing
    {
        --P.Bounce;
        const int32 NewTarget = EnemySubsystem->PickRandomAlive(Rng);
        const float Dir = (NewTarget != INDEX_NONE)
                        ? (EnemySubsystem->Get(NewTarget).Pos - P.Pos).GetSafeNormal().ToAngle()
                        : Rng.RandRange(-PI, PI);
        P.Velocity = FVector2D(FMath::Cos(Dir), FMath::Sin(Dir)) * P.Velocity.Size();  // 速率不变
        P.MaxRange = 99999.f;                              // ★ 弹射后不再受射程限制
        P.KnockbackAmount = 0.f;                           // ★ 击退清零
        P.Damage = FMath::Max(1.f, P.Damage - P.Damage * P.BounceDmgReduction);
    }
    else if (P.Piercing <= 0)
    {
        Destroy(P);
    }
    else
    {
        --P.Piercing;
        P.Damage = FMath::Max(1.f, P.Damage - P.Damage * P.PiercingDmgReduction);
    }

    RunSubsystem->ManageLifesteal(P.SourceWeaponIndex);    // 每次命中独立 roll
}

/** 暴击时的特殊效果 */
void UProjectileSubsystem::OnCriticalHit(FProjectileInstance& P)
{
    // "effect_bounce_on_crit" → ++Bounce，effect.value -= 1（用尽则移除）
    // "effect_pierce_on_crit" → ++Piercing，同上
}
```

### 8.4 吸血（★ 与伤害数值无关）

```cpp
void UBrotatoRunSubsystem::ManageLifesteal(int32 WeaponIndex)
{
    const float Chance = Weapons[WeaponIndex]->RuntimeStats.Lifesteal;
    if (!Rng.StrictRoll(Chance)) return;                        // ★ randf() <
    // ★ CD 由 Duration GE 的 Tag 表达（0.1 s），不用计数器
    if (ASC->HasMatchingGameplayTag(BrotatoTags::State_LifestealCd)) return;
    ApplyLifestealCdGE(BrotatoConst::LifestealCd);               // Duration GE 授予 Tag
    ApplyHeal(1);                                                // ★ 固定 1 HP，与伤害无关
}
```

| 规则 | 值 |
|---|---|
| 判定 | 每次命中独立 roll，`randf() <` |
| 回复量 | **固定 1 HP** |
| CD | **0.1 s**（Duration GE + `State.LifestealCd` Tag）→ 上限 10 HP/s |
| 触发点 | 近战命中、投射物命中 |
| 炮塔 | **不吃** lifesteal |

> **更优写法**：把治疗做成一个 Ability 或 GE，直接在 `GE_Lifesteal` 上配
> `ApplicationTagRequirements.IgnoreTags = State.LifestealCd`，让 CD 拦截由 GAS 完成，代码里连 if 都不需要。

---

## 9. GameplayCue：让 144 个无 ASC 敌人也能用 Cue

### 9.1 Cue Proxy ASC

```cpp
UCLASS()
class BROTATOGAS_API ABrotatoCueProxy : public AActor, public IAbilitySystemInterface
{
    GENERATED_BODY()
    UPROPERTY() UAbilitySystemComponent* ASC;
public:
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return ASC; }

    /** 在任意世界位置播放 Cue，无需目标 ASC */
    void ExecuteCueAtLocation(FGameplayTag CueTag, FVector Loc, FVector Dir, float Magnitude)
    {
        FGameplayCueParameters P;
        P.Location     = Loc;
        P.Normal       = Dir;
        P.RawMagnitude = Magnitude;
        P.SourceObject = this;
        ASC->ExecuteGameplayCue(CueTag, P);
    }
};
```

在 `L_Arena` 中放置一个隐藏实例，或由 `UFxSubsystem` 在 `OnWorldBeginPlay` 动态生成。

### 9.2 Cue 分工

| Cue | 播放者 | 表现 |
|---|---|---|
| `Cue.Player.Damaged` | 玩家 ASC | 屏震 5 px + 红色飘字 + 暗角更新 |
| `Cue.Player.Healed` / `LevelUp` / `Died` | 玩家 ASC | 音效 + 飘字 |
| `Cue.Hit.Normal` / `Crit` / `Dodge` / `Nullified` | 玩家 ASC（受击）/ **Proxy**（敌人受击） | 白闪 + 粒子 + 飘字 |
| `Cue.Enemy.Damaged` / `Died` | **Proxy ASC** | 同上 |
| `Cue.Weapon.MeleeSwing` / `RangedFire` | 玩家 ASC（Ability 内）/ Proxy（结构物） | 音效 |
| `Cue.Explosion` / `Burning` | **Proxy ASC** | 粒子 + 音效 |
| `Cue.Screenshake.*` | 玩家 ASC | 相机偏移 |

### 9.3 ★ 限流（在调用前，不在 CueNotify 内部）

```cpp
void UFxSubsystem::QueueHitCue(FVector2D Pos, int32 Dmg, bool bCrit, bool bDodge, float EffectScale)
{
    if (!Settings.bVisualEffects) return;
    if (ActiveGraphicalEffects >= BrotatoConst::MaxGraphicalEffects) return;   // ★ 100 上限
    if (!Rng.StrictRoll(EffectScale)) return;   // ★ 原作 randf() < effect_scale（注意括号！）

    const FGameplayTag Tag = bDodge ? FBrotatoTags::Get().Cue_Hit_Dodge
                           : bCrit  ? FBrotatoTags::Get().Cue_Hit_Crit
                                    : FBrotatoTags::Get().Cue_Hit_Normal;
    CueProxy->ExecuteCueAtLocation(Tag, FVector(Pos.X, Pos.Y, 0.f), LastKnockDir, Dmg);
    ++ActiveGraphicalEffects;
}
```

> ⚠️ **不要写成 `!Rng.Rand() < EffectScale`** —— 运算符优先级陷阱，原作是 `randf() < effect_scale`。
> 限流**必须在调用前**，否则 Cue 生命周期管理会乱（`ActiveGraphicalEffects` 计数无法回收）。

### 9.4 CueNotify 类型选择

| 效果 | CueNotify 类型 | 参数 |
|---|---|---|
| 命中粒子 | `UGameplayCueNotify_Burst` | lifetime 0.5，spread **180°**，velocity 600±50%，scale 0.5±0.25→0 |
| 方向性命中粒子 | 同上 | amount **5**，spread **10°**，velocity 1200±70% |
| 命中特效贴图 | 同上 | **3 张序列帧 @20 fps = 0.15 s** |
| 爆炸 / 飘字 / 屏震 | 同上 | 见 §10 |
| 燃烧粒子 | `UGameplayCueNotify_Looping` | `WhileActive` / `OnRemove` |
| **白闪材质切换** | ❌ 不用 Cue | 直接改材质（0.1 s，MID + Timer/曲线控制） |

**预加载**：把全部 Cue 资产加入 `AssetManager` 的启动预加载列表，避免首次触发加载卡顿。
`UBrotatoAbilitySystemGlobals::InitGlobalData()` 里遍历 `Content/GAS/Cues` 同步加载。

---

## 10. 反馈参数（照抄，不得"优化"）

### 10.1 屏震（★ 用 UE 原生 CameraShake）

**实现方式**：`ULegacyCameraShake` 资产（每种强度一个，或一个资产 + `Scale` 参数）通过
`UGameplayStatics::GetPlayerCameraManager()->StartCameraShake(ShakeClass, Scale)` 播放；
`Settings.bScreenshake` 关闭时直接不播。

```cpp
void UFxSubsystem::Shake(float Intensity, float DurationSec)
{
    if (!Settings.bScreenshake) return;
    // ★ 覆盖需「强度 > 当前 且 时长 > 当前」；全游戏 duration 恒为 0.1
    //   → 一段震动期内后续请求永远无法覆盖，生效的是第一个触发者（照抄，§00 P1）
    if (!(Intensity > CurIntensity && DurationSec > CurDuration)) return;
    CurIntensity = Intensity;
    CurDuration  = DurationSec;

    if (ActiveShake) CameraManager->StopCameraShake(ActiveShake, /*bImmediately*/ true);
    ActiveShake = CameraManager->StartCameraShake(ShakeClass, Intensity / RefIntensity);
    // 到点复位（用 Timer 而非帧计数）
    GetWorld()->GetTimerManager().SetTimer(ShakeResetHandle,
        [this]{ CurIntensity = 0.f; CurDuration = 0.f; ActiveShake = nullptr; },
        DurationSec, false);
}
```

**Shake 资产参数**（对齐原作特征，`ULegacyCameraShake`）：

| 字段 | 值 | 说明 |
|---|---|---|
| `OscillationDuration` | 0.1 | 与原作一致 |
| `OscillationBlendInTime` / `OutTime` | 0 / 0 | ★ 原作**无衰减曲线**，硬起硬停 |
| `LocOscillation.X/Y` | `Amplitude = 1`、`Frequency` 高值 | 幅度由 `StartCameraShake` 的 `Scale` 传入 |
| 偏移方向 | ★ **只向正方向**（右下） | 原作用 `randf() ∈ [0,1)`，是识别特征，不要改成对称 |

> 若 `ULegacyCameraShake` 的对称振荡无法表达"只向右下"，则用自定义 `UCameraShakeBase` 子类，
> 在 `UpdateAndApplyCameraShake` 内写 `Offset = FVector2D(Rng.Rand(), Rng.Rand()) * Intensity`（`Rng` 取 `Cosmetic` 流）。

| 来源 | 强度 | 时长 |
|---|---|---|
| 敌人受击 | `min(dmg/3, 3)` px | 0.1 s |
| 玩家受伤 | **5** px | 0.1 s |

> **★ 无 Hitstop**：全代码无 `Engine.time_scale`。**不要擅自添加**。

### 10.2 飘字

| 参数 | 值 |
|---|---|
| 方向 | `(0, -80)`（上移 80 px） |
| 扩散 | `±PI/4`（±22.5°） |
| 时长 | **0.5 s**（属性/收获 **1.0 s**） |
| 上限 | **100**（对象池预热 50） |
| 动画 | 三条并行：position **ELASTIC/OUT**、scale→0 **ELASTIC/IN_OUT**、alpha→0 **LINEAR** |
| 颜色 | 暴击=黄 / miss=深灰 / 治疗=绿 / 玩家受伤=红 |
| 开关 | `Settings.bDamageDisplay` 关闭时直接 return |
| 例外 | 玩家受伤飘字 `bAlwaysDisplay = true`，不受上限丢弃 |

### 10.3 命中特效

```cpp
// 生成位置：unit.pos + knockback_dir × (sprite宽 × 0.25)
const FVector2D FxPos = EnemyPos + KnockDir * (SpriteWidth * 0.25f);
// 粒子数：max(1, amount × effect_scale)
const int32 Amount = FMath::Max(1, FMath::RoundToInt(BaseAmount * EffectScale));
// 旋转：direction.angle() - PI
```

`EffectScale` 默认 1.0（step 0.1），**SMG = 0.2**；`is_miss` 时 `sc = effect_scale / 2`。

### 10.4 爆炸

| 参数 | 值 |
|---|---|
| 基础半径 | **147.34 px**（Sprite scale 1.36719） |
| 实际半径 | `147.34 × max(0.1, scale × (1 + explosion_size/100))` |
| 命中窗口 | **0.05 s** —— 敌人在此之后进入范围不受伤 |
| 存在时长 | **3.0 s** |
| 烟雾 | `round(scale × base_smoke_amount)`（默认 40，场景默认 20） |
| 音量 | `sound_db_mod = -10` |

### 10.5 击退

```cpp
FVector2D FEnemyState::GetNextKnockbackValue(float DeltaTime)
{
    const FVector2D V = KnockbackVector * (100.f - KnockbackResistance * 100.f);
    // ★ 帧率无关的指数衰减（原作每帧 lerp(→0, 0.1) 即 ×0.9）
    KnockbackVector = BrotatoTime::ApplyExpDecay(KnockbackVector,
                          BrotatoConst::KnockbackDecayRate, DeltaTime);
    return V;
}
// velocity = MoveInput - KnockbackValue      ★ 减法合入
```

| 项 | 值 |
|---|---|
| 玩家 KBR | 0 → 乘 **100** |
| 衰减 | `Exp(-6.3216·dt)` → 0.364 s 到 10%，0.728 s 到 1%（原作 0.37 / 0.73 s，P1 容差内） |
| 攻击方向 | **取负** `-Vector2(cos θ, sin θ)` |
| 死亡击退下限 | **15.0** |
| 击退期间 | 敌人 Hitbox 关闭（`KnockbackLen > MoveInputLen`） |
| 敌人命中玩家后 | 自身 `AddDecayingSpeed(-200)` |

### 10.6 音效时机（★ 近战音早于 hitbox）

| 事件 | 时刻 | dB | pitch_rand |
|---|---|---|---|
| 近战攻击 | **后坐结束、开 hitbox 之前** | `sound_db_mod`（默认 −5） | 0.2 |
| 远程射击 | `ActivateAbility` 最开头 | 同上 | 0.2 |
| 受击 / 暴击 / 闪避 | 结算内 | 0 | 0.2 |
| 燃烧点燃 / tick | 点燃 / 每 0.5 s | `base_effect_scale = 0.1` | 0.2 |
| 爆炸 | 生成时 | **−10** | 0.2 |
| 脚步 | 移动中 | **−6** | 0.1 |
| 治疗 | 触发时 | 周期 <2.5 s → −10；<1.0 s → −15；否则 0 | — |
| 属性获得 / 失去 | 飘字时 | `-3 + db_mod` / `-8 + db_mod` | — |

音效并发：用 **`USoundConcurrency`** 资产（上限 **12**，超出时 `StopOldest`）；
同源同类音效加 **0.03 s 去重窗口**（替代原作"队列长 32、每帧出队 1 条"的手写节流）。

### 10.7 燃烧

**Tier A / B（有 ASC）：用 Periodic GE，不写计时代码**

```cpp
// GE_Burning：Duration = HasDuration(SetByCaller)、Period = 运行时写入、
//             Stacking = AggregateBySource + StackLimitCount 1 + RefreshOnSuccessfulApplication
void ApplyBurningToASC(UAbilitySystemComponent* ASC, const FBurningData& BD,
                       const FBrotatoStatSnapshot& S)
{
    const float Interval = FBrotatoDamageMath::BurnTickInterval(S.BurningCooldownReduction);

    FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(GE_Burning, 1.f, Ctx);
    Spec.Data->Period = Interval;                                   // ★ 秒
    Spec.Data->SetSetByCallerMagnitude(Tag_Data_Duration, BD.NumTicks * Interval);
    Spec.Data->SetSetByCallerMagnitude(Tag_Data_Damage, TickDamage(BD, S));
    ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
}
```

**Tier C（普通敌人，无 ASC）：批处理累减秒数**

```cpp
void UEnemySubsystem::StepBurning(FEnemyState& E, float DeltaTime)
{
    if (E.Burning.TicksLeft <= 0) return;
    E.Burning.TickTimer -= DeltaTime;
    if (E.Burning.TickTimer > 0.f) return;

    E.Burning.TickTimer += E.Burning.TickInterval;   // ★ 累加而非重置，避免长帧丢 tick
    --E.Burning.TicksLeft;                           // ★ 原作 duration 是 tick 次数

    FDamageRequest Req;
    Req.Damage = FMath::Max(1, BrotatoMathInt::BrotatoRound(
        (E.Burning.Damage + Snapshot.ElementalDamage) * (1.f + Snapshot.PercentDamage / 100.f)));
    Req.bDodgeable = false;     // ★ 无视闪避
    Req.bArmorApplied = false;  // ★ 无视护甲
    FBrotatoDamagePipeline::ApplyToEnemy(E, Req, Rng);
}

/** 点燃：★ 逐字段取最大值刷新，不叠层 */
void ApplyBurning(FEnemyState& E, const FBurningData& BD, float Reduction)
{
    E.Burning.Damage    = FMath::Max(E.Burning.Damage,    BD.Damage);
    E.Burning.TicksLeft = FMath::Max(E.Burning.TicksLeft, BD.NumTicks);
    E.Burning.Type      = BD.Type;
    E.Burning.TickInterval = FBrotatoDamageMath::BurnTickInterval(Reduction);
    if (E.Burning.TickTimer <= 0.f) E.Burning.TickTimer = E.Burning.TickInterval;
}
```

> **`TickTimer += Interval`（而非 `= Interval`）** 是保证低帧率下 tick 总数正确的关键：
> 单次 Tick 若跨过多个间隔，循环累减即可补齐全部 tick，总伤害与 60 fps 完全一致。

武器燃烧数值（§04 §7.3）：火把 I–IV dmg 3/5/8/12 dur 3/4/5/8 chance 1.0；火焰指虎 II–IV dmg 8/12/15 dur 5/6/7；喷火器 II–IV dmg 3/4/5 dur 5/6/8。

---

## 11. 后坐参数（直接影响 DPS，不只是视觉）

默认 `Recoil = 25`、`RecoilDuration = 0.1`。**185 个阶位中只有下列覆写默认**，其余（含全部近战，Drill 除外）一律默认：

| 武器 | 阶位 | recoil | recoil_duration |
|---|---|---:|---:|
| **Drill** | IV | **0** | **0.01** |
| SMG | I–IV | 10 | 0.05 |
| Minigun | III / **IV** | 10 / **3** | 0.02 |
| Chain Gun / Gatling Laser | IV | 3 | 0.02 |
| Flamethrower | II–IV | 5 | 0.02 |
| Potato Thrower | II / III / IV | 25 / 15 / 10 | 0.1 |
| Crossbow / Shredder / Slingshot | I–IV | 30 / 40 / 40 | 0.15 |
| Wand | I–IV | 40 | 0.1 |
| Laser Gun / Sniper III–IV / Obliterator III–IV | — | 40 | 0.2 |
| Rocket / Nuclear Launcher | II–IV / III–IV | 25 | **0.142** |

> 0.2 s 后坐（往返 0.4 s）= 重武器"沉"感；0.02–0.05 s 则让 CD 0.0167–0.0667 s 的连射真正连得起来。**统一成 0.1 会毁掉两类武器**。
> `Recoil` 位移量与伤害判定无关；近战 hitbox 在后拉**结束后**才开启。

---

## 12. 武器合成与槽位

```cpp
bool UBrotatoRunSubsystem::CanCombine(const UBrotatoWeaponData* D) const
{
    const int32 Dup = CountWeaponsWithMyId(D->MyId);        // ★ my_id 含 tier
    return Dup >= 2 && D->UpgradesInto != nullptr
        && (int32)D->Tier < (int32)GetMaxWeaponTier();
}

void UBrotatoRunSubsystem::CombineWeapons(const UBrotatoWeaponData* D, bool bIsUpgrade)
{
    const int32 NbToRemove = bIsUpgrade ? 1 : 2;            // 铁砧免费升级只消耗 1 把
    int32 SumTracked = 0, LastDmg = 0;
    for (int32 i = 0; i < NbToRemove; ++i)
    {
        const FRuntimeWeapon* W = FindWeaponByMyId(D->MyId);
        SumTracked += W->TrackedValue; LastDmg = W->DmgDealtLastWave;
        RemoveWeapon(W->InstanceId);
    }
    FBrotatoItemInstanceId NewId = AddWeapon(D->UpgradesInto.LoadSynchronous());
    GetWeapon(NewId)->TrackedValue = SumTracked;            // 统计继承
    if (bIsUpgrade) GetWeapon(NewId)->DmgDealtLastWave = LastDmg;
}
```

| 规则 | 说明 |
|---|---|
| 合成条件 | 同 `my_id` × 2 → 1 把 tier+1；判定用 `my_id`，解锁/`weapon_bonus` 用 `weapon_id` |
| 槽位 | `Weapons.Num() < WeaponSlot`（默认 6）；按类型还需 `< min(WeaponSlot, Max*Weapons)` |
| 购买自动合成 | 槽位已满但持有同名可升级武器 → 商店自动 `AddWeapon` 再 `Combine`（净效果 3 合 1） |
| 铁砧 | 随机一把可升级武器免费升阶（消耗 1 把）；**无可升级时 `armor += 2`** |
| 破坏武器 | Arms Dealer 的 `Rule.DestroyWeapons` → **进店即 `RemoveAllWeapons()`** |
| 回收价 | `max(1, GetValue(...) × clamp(0.25 + recycling_gains/100, 0.01, 1.0))` |

---

## 13. 结构物（复用 `FWeaponFireCore`，无 ASC）

```cpp
UCLASS()
class ATurret : public AActor
{
    UPROPERTY() FBrotatoWeaponStats Stats;
    FWeaponRuntimeStats Runtime;
    float CooldownRemaining = 0.f;      // ★ 秒

    virtual void Tick(float DeltaTime) override
    {
        CooldownRemaining -= DeltaTime;
        if (CooldownRemaining > 0.f) return;

        // ★ 炮塔冷却随机化：rand_range(max(1/60, cd×0.7), cd×1.3)，不取整
        CooldownRemaining = Rng.RandRange(
            FMath::Max(BrotatoConst::MinJitteredCooldown, Runtime.CooldownSeconds * 0.7f),
            Runtime.CooldownSeconds * 1.3f);

        if (const int32 T = FWeaponFireCore::SelectTarget(...); T != INDEX_NONE)
        {
            FWeaponFireCore::SpawnProjectiles(this, ProjectileSubsystem, Rng);
            CueProxy->ExecuteCueAtLocation(Cue_Weapon_RangedFire, GetActorLocation(), ...);
        }
    }
};
```

**结构物不吃**：`attack_speed`、`stat_percent_damage`、`crit_chance`、`lifesteal`、`stat_range`。
伤害：`D = max(1, round((D_base + Σ Stat_i × k_i) × (1 + explosion_damage/100)))`。

工程系数（§02 §9.3）：标准炮塔 10 / ×0.8；激光 20 / ×1.25；火箭 25 / ×1.5；治疗 3 / ×0.05；地雷 10 / ×1.0。

冷却缩减（秒）：`new_cd = max(1/30, base_cd × 1/(1+total))`（total>0）或 `max(1/30, base_cd × (1+|total|))`。
生成半径：`min(600, 400 + Hook.Structures.Num() × 10)`；`spawn_cooldown == -1` 表示**仅波首生成一次**。
生成检查周期：**1.0 s**（用 `FTimerManager` 循环，原作每 60 帧）。

> Boss / 精英 / 结构物的行为状态机用 **`StateTree`**（§09 §10.3）；普通小兵继续走数据驱动批处理。

---

## 14. 本章验收清单

| # | 项 | 标准 |
|---|---|---|
| W01 | 冷却抖动 | Dagger I（CD 0.45 s）、攻速 0、1 把武器 → 区间 `[0.367, 0.533] s` |
| W02 | 冷却抖动 | 6 把武器 → 区间 `[0.0167, 0.95] s` |
| W03 | 大装填 | Chain Gun 第 100 发 → 1.0 s，且不带抖动 |
| W04 | 交替攻击 | Sword 连续攻击 → THRUST / SWEEP 交替 |
| W05 | 命中去重 | 一次 SWEEP 内同一敌人只受 1 次伤害 |
| W06 | 穿透衰减 | Obliterator 穿透 99 次，每次伤害 ×0.5 |
| W07 | 套装档位 | Blade 2 件 → +1/+1%；7 件 → 仍是 6 件档 |
| W10 | 近战射程 | `stat_range = 100` → `max_range += 50`（只加一半） |
| W11 | 后坐 | Rocket Launcher II 的 `RecoilDuration = 0.142 s` |
| W12 | 挂点 | 6 把武器坐标 = §1.1 表第 6 行（相对玩家再 y−24） |
| **W13** | ★ **帧率无关** | 30 / 60 / 144 / 240 fps 下连续 100 次攻击的平均间隔误差 < 1%，总攻击次数误差 < 2% |
| B01 | Tag 拦截 | Soldier 移动时 `CanActivateAbility == false` |
| B02 | 无敌窗口 | 窗口内 `GE_Damage` 被 `ApplicationTagRequirements` 拒绝（GE 未进入 ExecCalc） |
| B03 | 多 Spec | 6 把武器 6 个 Spec，各自独立 Cooldown GE（互不干扰） |
| B04 | Task 时序 | MeleeSwing 三段时长与理论秒值误差 < 2%（含低帧余量结转正确） |
| B05 | Cooldown GE | 攻击序列进行中冷却不流逝；结束后开始计时；UI 读数与实际一致 |
| V01 | 白闪 | 恰好 0.1 s；闪避/免伤时不白闪 |
| V02 | 屏震方向 | 采样 1000 次，偏移 x/y 全部 ≥ 0 |
| V03 | 屏震覆盖 | 同一 Tick 先受 3 伤(强度1)再受 30 伤(强度3) → 最终强度 **1** |
| V05 | 飘字上限 | 同屏 >100 被丢弃，但玩家受伤飘字仍显示 |
| V06 | 子弹延迟 | 发射后 0.02 s 内原地不动，之后开始移动 |
| V07 | 爆炸窗口 | 敌人在 0.1 s 后进入范围 → 不受伤 |
| V08 | 音效并发 | 连续 50 次命中 → 并发不超 12（`USoundConcurrency` 生效） |
| V09 | 音效时机 | 近战音效播放时刻 < hitbox 开启时刻 |
| **V10** | ★ 燃烧 tick 完整性 | 在 15 fps 极限下燃烧总 tick 数与 60 fps 一致（累加式计时验证） |
| L10 | 吸血 | 连续密集命中在 0.1 s 内只回 1 HP |
