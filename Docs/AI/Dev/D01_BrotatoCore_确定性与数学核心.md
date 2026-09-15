# D01 · BrotatoCore：时间基、取整契约与数学核心

> **模块**：`Source/BrotatoCore`（Runtime，零 UE Gameplay 依赖，可脱离引擎单测）
> **里程碑**：M0 搭骨架 → M2 完成全部公式
> **上游规格**：`Docs/00` 全文（★ 实现方式最高仲裁）、`Docs/02` 全文、`Docs/04 §1–§2`、`Docs/05 §3–§4`、`Docs/06 §3`、`Docs/09 §2 §7`
> **v2.0 变更**：删除固定帧驱动（`IFixedTickable` / `UFixedTickSubsystem`），改为**秒制时间基 + 可变 DeltaTime + 固定阶段顺序**；随机数改为**按用途分流**。

**本模块是整个工程的地基**：所有公式**只允许存在这一份实现**。GAS 的 ExecCalc、敌人的纯函数管道、结构物、UI 预览，全部调用这里。

---

## 0. 文件清单

```
BrotatoCore/
├─ Public/
│  ├─ BrotatoCoreTypes.h          枚举 + 基础 USTRUCT
│  ├─ BrotatoMathInt.h            ★ 取整契约（§3）
│  ├─ BrotatoRandom.h             ★ 分流随机（§2）
│  ├─ BrotatoTime.h               ★ 秒制常量 / 衰减 / 节流工具（§1）
│  ├─ BrotatoDamageMath.h         伤害/护甲/暴击/闪避/无敌窗口（§4）
│  ├─ BrotatoWeaponMath.h         RuntimeStats 11 步 + 冷却 + 挥击时序（§5）
│  ├─ BrotatoEconomyMath.h        定价/重掷/回收/掉落/Tier 抽取（§6）
│  ├─ BrotatoProgressionMath.h    XP/升级/收获/属性转换/无尽因子（§7）
│  └─ BrotatoStatSnapshot.h       ★ 计算用的属性快照（§8）
└─ Private/  对应 .cpp
```

---

## 1. 时间基与模拟编排

### 1.1 ★ 为什么不用固定帧（v2.0 决议）

原作所有冷却单位是物理帧（`cooldown = 60` 即 1 s），且 `Utils.physics_one(delta) = physics_fps × delta` 在 60 Hz 下恒等于 1 —— **那是 Godot 的实现方式，不是复刻目标**（见 `Docs/00`）。

把固定 60 Hz 逻辑帧搬进 UE 的代价远大于收益：与 GAS 计时、TimerManager、动画、Niagara、Pause/TimeDilation 全部割裂，还要自己写补帧、暂停清零、渲染插值。

**决议**：

| 项 | 规定 |
|---|---|
| 时间单位 | **秒（float）**；一局累计用 `double`。数据表与运行时**不存在帧字段** |
| 推进方式 | UE 标准可变 `DeltaTime`；一致性由**固定阶段顺序**保证（§1.4） |
| 换算 | 导入期一次性 `秒 = 帧/60`（D07 的 Commandlet 负责） |
| 帧率无关 | 任何随时间变化的量必须乘 `DeltaTime`；**禁止**「每 Tick −1 / ×0.9」 |
| 确定性 | 见 §2：RNG 按用途分流的**事件级可复现**；回归测试用引擎原生固定步长 |

原作 `x -= Utils.physics_one(delta)`（x 为帧计数）→ UE 侧 `X -= DeltaTime`（X 为秒）。

### 1.2 `BrotatoTime.h`（BrotatoCore）

```cpp
// BrotatoCore/Public/BrotatoTime.h
#pragma once
#include "CoreMinimal.h"

namespace BrotatoTime
{
    /** 原作帧 → 秒（仅供导入器与文档对照使用，运行时不应再出现帧） */
    FORCEINLINE constexpr float FramesToSeconds(int32 Frames) { return Frames / 60.f; }

    // ── 秒制常量（原作帧值见注释） ──
    constexpr float MinWeaponCooldown   = 1.f / 30.f;  // 原作 2 帧（攻速后下限）
    constexpr float MinJitteredCooldown = 1.f / 60.f;  // 原作 1 帧（抖动后下限）
    constexpr float LifestealCd         = 0.1f;        // 原作 6 帧
    constexpr float HitFlash            = 0.1f;        // 原作 6 帧
    constexpr float BurnTickBase        = 0.5f;        // 原作 30 帧
    constexpr float BurnTickMin         = 0.1f;        // 原作 6 帧
    constexpr float ProjectileLaunchDelay = 0.02f;     // 原作 0.02 s（≈1.2 帧，旧版取整成 2 帧）
    constexpr float HarvestingDelay     = 0.667f;      // 原作 40 帧
    constexpr float StackingPeriod      = 5.0f;        // 原作 300 帧
    constexpr float StructureSpawnCheck = 1.0f;        // 原作 60 帧
    constexpr float SpawnQueueInterval  = 0.05f;       // 原作"每 3 帧 1 个" → 20 个/s

    /** ★ 帧率无关的指数衰减：原作"每帧 ×k" → Rate = -60·ln(k) */
    FORCEINLINE float DecayRateFromPerFrameFactor(float K) { return -60.f * FMath::Loge(K); }
    constexpr float KnockbackDecayRate = 6.3216f;      // k = 0.9 → 0.364 s 剩 10%（原作 0.37 s）

    FORCEINLINE float ApplyExpDecay(float Value, float Rate, float DeltaTime)
    { return Value * FMath::Exp(-Rate * DeltaTime); }

    /** 按秒节流（替代"每 N 帧"计数） */
    struct FIntervalAccumulator
    {
        float Accum = 0.f, Interval = 1.f;
        int32 Advance(float DeltaTime)          // 返回本 Tick 应触发次数
        {
            Accum += DeltaTime;
            int32 N = 0;
            while (Accum >= Interval) { Accum -= Interval; ++N; }
            return N;
        }
    };
}
```

> **为什么常量放 Core**：它们是数值契约的一部分，需要能脱离引擎单测（验证 `FramesToSeconds` 换算与衰减等价性）。

### 1.3 `UBrotatoSimSubsystem`（BrotatoGameplay）

```cpp
UCLASS()
class BROTATOGAMEPLAY_API UBrotatoSimSubsystem : public UWorldSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    virtual void Tick(float DeltaTime) override
    {
        if (bPaused) return;                       // 商店/升级期间；GE 计时由 Pause 自动停走
        const float DT = FMath::Min(DeltaTime, 0.1f);   // ★ 单帧钳制：防卡顿导致穿墙/跳步
        RunTime += DT;
        StepAll(DT);                               // 固定阶段顺序，见 §1.4
    }
    virtual TStatId GetStatId() const override
    { RETURN_QUICK_DECLARE_CYCLE_STAT(BrotatoSim, STATGROUP_Tickables); }

    double GetRunTimeSeconds() const { return RunTime; }
    void   SetPaused(bool b)         { bPaused = b; }

private:
    void StepAll(float DT);
    double RunTime = 0.0;
    bool   bPaused = false;
};
```

> **单帧钳制 0.1 s** 是可变帧率下唯一必须的保护：极端卡顿时宁可慢放一瞬，也不要让位移/伤害在一帧内跳过整段距离。
> 不做"补帧"（旧方案的 `MaxCatchUpSteps`），因为没有固定步长需要追赶。

### 1.4 ★ 模拟阶段顺序（固定不可变，13 阶段）

任何新增系统必须明确挂在哪一阶段，**不得**新增乱序回调。

| 阶段 | 内容 | 归属模块 | 文档 |
|---|---|---|---|
| S1 | 输入采样（EnhancedInput） | Gameplay | D06 §4 |
| S2 | `WaveManager` 推进 —— **按整秒采样**触发刷怪组 | Gameplay | D05 §3 |
| S3 | `EntitySpawner` 分摊生成 —— 5 条队列，每队列 `SpawnQueueInterval` 出 1 个 | Gameplay | D05 §4 |
| S4 | 移动行为求解 → 期望速度合成 | Gameplay | D05 §2 |
| S5 | 位移与碰撞解算（margin 0.08，两段 sweep / `SafeMoveUpdatedComponent`） | Gameplay | D05 §6 |
| S6 | 击退与外力衰减 —— `ApplyExpDecay(V, KnockbackDecayRate, DT)` | Gameplay | D05 §2.4 |
| S7a | 选靶 + 开火判定（冷却状态查 Cooldown GE / `CooldownRemaining`） | Gameplay | D04 §4 |
| S7b | Tier A: `TryActivateAbility(GA_WeaponAttack)` / Tier C: `FWeaponFireCore::Fire()` | GAS / Gameplay | D04 §5 |
| S7c | 挥击时序推进（`UAbilityTask` 自带 Tick，按秒 + Curve） | GAS | D04 §6 |
| S8 | 投射物批更新（含 `ProjectileLaunchDelay` 累减） | Gameplay | D04 §8 |
| S9a | 命中判定 → 玩家/Boss → `ApplyGameplayEffectToTarget(GE_Damage)` | GAS | D03 §4 |
| S9b | 命中判定 → 普通敌人 → `FBrotatoDamagePipeline::ApplyToEnemy()` | Gameplay | D05 §5 |
| S10 | 拾取物吸附与结算 | Gameplay | D05 §7 |
| S11 | 属性脏标记统一处理：`LinkedStats GE 重算` → `武器 RuntimeStats 重算` → `广播 UI` | GAS / Gameplay / UI | D02 §5 / D04 §2 / D06 §3 |
| S12 | 死亡清理 + 对象回收 | Gameplay | D05 §9 |
| S13 | 表现层提交（Cue / 飘字 / 屏震 / 音效） | GAS / UI | D04 §9 |

**已从旧版 16 步中删除的阶段**（改由 GAS 原生承担，见 D03）：
- 「武器冷却帧计数递减」 → Cooldown GE
- 「状态效果帧推进（燃烧/回血/掉血/吸血 CD）」 → Periodic / Duration GE
- 「`ASC.FixedTickASC()`」 → ASC 保持引擎默认 Tick
- 「结构物生成检查（每 60 帧）」 → 1 s 循环 Timer 或 Periodic GE

> **周期值对照（原作帧 → 秒）**：`5 s（300 帧）`、`1 s（60 帧）`、`0.5 s 燃烧 tick（30 帧）`、`0.1 s 吸血 CD / 闪白（6 帧）`、`0.667 s 收获延迟（40 帧）`。
> 旧版因「0.66×60 = 39.6 要凑整」而规定"统一用 40 帧"的取整争议**自然消失** —— 秒制下直接用 `0.667`。

---

## 2. 随机数与可复现性

### 2.1 `FBrotatoRandom` 与流分区

**目标口径（§00 §5）**：不追求逐帧确定性，而是**按用途分流**，让「同 seed → 同商店 / 同掉落 / 同刷怪序列」在**任意帧率下都成立**。

```cpp
// BrotatoCore/Public/BrotatoRandom.h
#pragma once
#include "CoreMinimal.h"
#include "Math/RandomStream.h"

/** ★ 随机流分区：离散事件流（Shop/Loot/Spawn）与帧率无关 → 严格可复现 */
UENUM()
enum class EBrotatoRngChannel : uint8
{
    Shop,        // 商店抽取、重掷、Tier 抽取
    Loot,        // 掉落、材料、宝箱
    Spawn,       // 波次刷怪、精英抽取、兽潮
    Combat,      // 暴击、闪避、燃烧点燃、吸血、冷却抖动
    Cosmetic,    // 纯表现：屏震、音高、粒子（不参与复现）
    Count
};

/**
 * 单个随机流的封装。
 * ★ 玩法逻辑内禁止 FMath::FRand / FMath::RandRange（CI 检查）；Cosmetic 用途例外。
 * ★ 边界：与 Godot RNG 算法不同，保证「同 seed → 同结果」的工程内自洽复现，
 *   不是与原作比特级一致。
 */
class BROTATOCORE_API FBrotatoRandom
{
public:
    void Seed(int32 InSeed) { Stream.Initialize(InSeed); CallCount = 0; }
    int32 GetInitialSeed() const { return Stream.GetInitialSeed(); }

    /** [0, 1)，对齐 Godot randf() */
    float Rand()                       { ++CallCount; return Stream.FRand(); }
    float RandRange(float A, float B)  { ++CallCount; return A + (B - A) * Stream.FRand(); }
    /** [A, B] 闭区间，对齐 Godot randi_range */
    int32 RandInt(int32 A, int32 B)    { ++CallCount; return Stream.RandRange(A, B); }

    template<typename T>
    const T& RandElement(const TArray<T>& Arr)
    { checkf(Arr.Num() > 0, TEXT("RandElement on empty array")); return Arr[RandInt(0, Arr.Num()-1)]; }

    /** 洗牌（商店池、精英抽取），必须走本函数以保证确定性 */
    template<typename T> void Shuffle(TArray<T>& Arr)
    { for (int32 i = Arr.Num()-1; i > 0; --i) { Arr.Swap(i, RandInt(0, i)); } }

    // ── ★ 严格对齐原作的两种判定：比较符不同，切勿统一 ──────────────
    /** Utils.get_chance_success：randf() <= chance，且 chance == 0 直接 false
     *  用于：暴击、消耗品掉落、材料掉落、爆炸触发、燃烧触发、刷怪组 spawn_chance */
    bool ChanceSuccess(float Chance)
    { return Chance != 0.f && Rand() <= Chance; }

    /** 闪避判定：randf() < chance（严格小于）
     *  用于：dodge、吸血 roll、特效播放概率 randf() < effect_scale */
    bool StrictRoll(float Chance) { return Rand() < Chance; }

    /** 确定性追踪用（LogBrotatoDet） */
    int64 GetCallCount() const { return CallCount; }

private:
    FRandomStream Stream;
    int64 CallCount = 0;
};
```

### 2.2 获取方式

```cpp
// 必须显式指明用途流
FBrotatoRandom& Rng = GetWorld()->GetSubsystem<UBrotatoRunSubsystem>()
                          ->GetRandom(EBrotatoRngChannel::Combat);
// UBrotatoRunSubsystem 内：Streams[i].Seed(HashCombine(RunSeed, (int32)i));
```

| 场景 | 规定 |
|---|---|
| 一局开始 | 用 `RunSeed` 派生全部 `Count` 个流；`RunSeed` 存入 `run_state`，续关恢复 |
| Daily Run | `RunSeed = 日期哈希` |
| ExecCalc / MMC 内 | **必须**从 `UBrotatoRunSubsystem` 取 `Combat` 流，不得新建 Stream |
| 屏震 / 音高 / 粒子 | 用 `Cosmetic` 流（或直接 `FMath::FRand()`），**不得**污染其他流的调用序 |
| UI 预览 / 纯展示 | 允许用独立临时 Stream |

> ⚠️ **流内调用序即状态**。在 `Shop / Loot / Spawn` 流里"顺手多掷一次骰子"会让后续分叉 → 必须同步更新黄金基线并在 PR 说明。
> 反之，把表现层随机隔离到 `Cosmetic` 流之后，**表现层改动再也不会影响玩法复现** —— 这是分流的主要收益。
> `Combat` 流与命中顺序相关，只保证统计分布一致（可变帧率下的必然代价，不影响任何玩法验收项，见 §00 §8）。

---

## 3. ★ 取整契约（最隐蔽的偏差源）

### 3.1 语义差异表

| 语义 | Godot | UE 默认 | 一致？ | 本项目封装 |
|---|---|---|---|---|
| 四舍五入 | `round(x)`：**half away from zero**<br>`0.5→1`、`-0.5→-1`、`2.5→3`、`-2.5→-3` | `FMath::RoundToInt`：**half to +∞**<br>`0.5→1`、`-0.5→0`、`2.5→3`、`-2.5→-2` | ❌ 负半整数分叉 | `BrotatoRound()` |
| 浮点→整数 | `int(x)` / `as int`：截断向零 | `FMath::TruncToInt`：截断向零 | ✅ | `BrotatoTruncInt()` |
| 向下取整 | `floor(x)` | `FMath::FloorToInt` | ✅ | 直接用 |
| 向上取整 | `ceil(x)` | `FMath::CeilToInt` | ✅ | 直接用 |
| 整数除法 | `int / int` → 向下 | 需显式 `FloorToInt` | ⚠️ | `FloorToInt(a / (float)b)` |

### 3.2 实现

```cpp
// BrotatoCore/Public/BrotatoMathInt.h
#pragma once
#include "CoreMinimal.h"

namespace BrotatoMathInt
{
    /** 对齐 Godot round()：half away from zero */
    FORCEINLINE int32 BrotatoRound(float X)
    {
        return X >= 0.f ?  FMath::FloorToInt( X + 0.5f)
                        : -FMath::FloorToInt(-X + 0.5f);
    }
    FORCEINLINE int64 BrotatoRound64(double X)
    {
        return X >= 0.0 ?  (int64)FMath::FloorToDouble( X + 0.5)
                        : -(int64)FMath::FloorToDouble(-X + 0.5);
    }

    /** 对齐 Godot `as int` / int()：截断向零 */
    FORCEINLINE int32 BrotatoTruncInt(float X) { return FMath::TruncToInt(X); }

    /** 整数除法（LinkedStats 的 scaled / nb） */
    FORCEINLINE int32 IntDiv(float Scaled, float Nb)
    { return Nb == 0.f ? 0 : FMath::FloorToInt(Scaled / Nb); }
}
```

### 3.3 逐表达式归属（实现时逐条核对）

**必须 `BrotatoRound()`（可能出现负值）**

| 位置 | 表达式 | 为何可能为负 |
|---|---|---|
| 武器 pct 乘区 | `max(1, round(dmg × (pct + exp)))` | Renegade `DMG% = -400` → 乘区 = −3 |
| 暴击重算 | `round(value × crit_damage)` | 同上（原始伤害已可为负） |
| 玩家护甲 | `round(dmg × armor_coef)` | 负护甲 → 系数 > 1，dmg 可能为负 |
| `percent_materials` | `(pct/100) × gold` | Streamer 的负向条目 |
| 敌人 HP/DMG/SPD/ARM 成长 | `round(base × (Acc + S))` | `enemy_health` 等可被角色调负 |
| 敌人伤害二段 | `round(new_damage × (1 + EF))` | — |
| 波末收获 | `round(N_enemies × pacifist/100)` | — |
| 属性转换 Δ | `Δ_from = -N × value` | 转出为负 |
| 爆炸烟雾量 | `round(scale × base_smoke_amount)` | — |

**必须 `BrotatoTruncInt()`**

| 位置 | 表达式 |
|---|---|
| 冷却抖动 | ❌ **不再取整**（秒制 float，§00 §6） |
| 攻速冷却 | `max(2, base.cooldown × ...) as int` |
| 定价 / 重掷 / 回收 | `... as int`（值恒正，等价 floor，仍走统一封装） |
| 无尽 | `int((index/10.0)×2)`、`int(100 × (1.25 + i×0.01))` |
| 刷怪数 | `int(1 + bonus_enemies_per_group)`、`int(round(rand_range(...)) + bonus)` |
| 最大生命 | `clamp(stat_max_hp, 1, hp_cap) as int` |
| 巨人腰带追伤 | `int((HP × giant/100) / max(1, EF×0.2))` |
| 敌人子弹伤害一段 | `int(base × (Acc.damage + Sd))` |
| 007 光环血量 | `int(max_health × (1 + hp_coef))` |

**必须 `FloorToInt()`**：LinkedStats `floor(scaled / nb)`；属性转换 `floor((From × pct/100) / value)`；中立单位期望取整的整数部分。

**必须 `CeilToInt()`**：收获自增 `ceil(0.05H)` / 无尽衰减 `ceil(0.20H)`；HP 商店价 `ceil(value/20)`；无尽附加精英数 `ceil((W-20)/10)`；波次计时显示 `ceil(time_left)`。

### 3.4 单测（`Tests/Core/IntSemanticsTest.cpp`，M0 必过）

```
T13-1 BrotatoRound( 0.5) ==  1
T13-2 BrotatoRound(-0.5) == -1      ← FMath::RoundToInt 会给 0，这就是分叉点
T13-3 BrotatoRound( 2.5) ==  3
T13-4 BrotatoRound(-2.5) == -3      ← FMath::RoundToInt 会给 -2
T14-1 BrotatoTruncInt( 1.9) ==  1
T14-2 BrotatoTruncInt(-1.9) == -1
T14-3 IntDiv(5, 3) == 1             ← 非 1.67
T14-4 IntDiv(-5, 3) == -2           ← floor 语义
```

---

## 4. 伤害数学核心 `FBrotatoDamageMath`

### 4.1 护甲（★ 双公式，切勿抽象统一）

```cpp
namespace FBrotatoDamageMath
{
    /** 玩家：乘法减伤（§02 §4.3）
     *  A >= 0 : 10 / (10 + |A|/1.5)
     *  A <  0 : 2 - 上式（负护甲 → 增伤） */
    FORCEINLINE float ArmorCoefMultiplicative(float Armor)
    {
        const float P = 10.f / (10.f + FMath::Abs(Armor) / 1.5f);
        return Armor < 0.f ? (2.f - P) : P;
    }

    /** 玩家侧最终取值 */
    FORCEINLINE int32 PlayerDmgValue(float Raw, float Armor, bool bArmorApplied)
    {
        using namespace BrotatoMathInt;
        return bArmorApplied
             ? FMath::Max(1, BrotatoRound(Raw * ArmorCoefMultiplicative(Armor)))
             : BrotatoRound(Raw);
    }

    /** 敌人侧：固定减法（§02 §4.3） */
    FORCEINLINE int32 EnemyDmgValue(float Raw, float Armor, bool bArmorApplied)
    {
        using namespace BrotatoMathInt;
        return bArmorApplied ? FMath::Max(1, BrotatoRound(Raw - Armor))
                             : BrotatoRound(Raw);
    }
}
```

护甲对照（验收用）：

| Armor | 0 | 3 | 5 | 10 | 15 | 20 | 30 | 50 | 60 | 100 | −10 | −20 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 系数 | 1.000 | 0.833 | 0.750 | 0.600 | 0.500 | 0.4286 | 0.3333 | 0.2308 | 0.200 | 0.1304 | **1.400** | **1.5714** |

### 4.2 结算主函数（唯一实现，双路径共用）

```cpp
struct BROTATOCORE_API FResolveParams
{
    float Raw        = 0.f;
    float Armor      = 0.f;
    float Dodge      = 0.f;      // 0~1
    float DodgeCap   = 60.f;     // 百分数，判定时 /100
    float CritChance = 0.f;      // 0~1
    float CritDamage = 1.5f;
    int32 HitProtection = 0;
    bool  bDodgeable    = true;
    bool  bArmorApplied = true;
    bool  bIsPlayer     = false;
};

struct BROTATOCORE_API FResolveResult
{
    int32 Final = 0;
    bool bDodged = false, bProtected = false, bCrit = false;
    int32 HitProtectionLeft = 0;
};

/**
 * ★ 唯一的伤害结算实现（§02 §4.2）。顺序不可改：
 *   ① 取值 → ② 闪避 → ③（elif）免伤 → ④ 暴击用【原始 Raw】重算
 */
FResolveResult Resolve(const FResolveParams& P, FBrotatoRandom& Rng)
{
    using namespace BrotatoMathInt;
    FResolveResult R;
    auto DmgValue = [&](float V)
    {
        return P.bIsPlayer ? PlayerDmgValue(V, P.Armor, P.bArmorApplied)
                           : EnemyDmgValue (V, P.Armor, P.bArmorApplied);
    };

    R.Final             = DmgValue(P.Raw);
    R.HitProtectionLeft = P.HitProtection;

    // ② 闪避：严格 < ，且双重 clamp
    if (P.bDodgeable && Rng.StrictRoll(FMath::Min(P.Dodge, P.DodgeCap / 100.f)))
    {
        R.Final = 0; R.bDodged = true;
    }
    // ③ 免伤（与闪避互斥）
    else if (R.HitProtectionLeft > 0)
    {
        --R.HitProtectionLeft; R.Final = 0; R.bProtected = true;
    }

    // ④ 暴击：ChanceSuccess 用 <= 且 chance==0 → false
    if (R.Final != 0 && Rng.ChanceSuccess(P.CritChance))
    {
        R.Final = DmgValue((float)BrotatoRound(P.Raw * P.CritDamage));  // ★ 用原始 Raw 重算
        R.bCrit = true;
    }
    return R;
}
```

> **G-陷阱**：`R.Final = DmgValue(Raw × CritDamage)`，**不是** `已减甲值 × CritDamage`。
> 验收：`raw=10, crit_dmg=2.0, enemy_armor=3` → `max(1, 20-3) = 17`（错误实现会得 `(10-3)×2 = 14`）。

### 4.3 巨人腰带追伤（暴击后追加）

```cpp
/** §02 §4.2 ⑤ */
FORCEINLINE int32 GiantCritBonus(int32 TargetCurrentHealth, float GiantCritDamage,
                                 bool bIsBoss, float EndlessFactor)
{
    if (GiantCritDamage <= 0.f) return 0;
    const float Div = bIsBoss ? 1000.f : 100.f;
    const float EF  = FMath::Max(1.f, EndlessFactor * 0.2f);
    return BrotatoMathInt::BrotatoTruncInt((TargetCurrentHealth * (GiantCritDamage / Div)) / EF);
}
```

### 4.4 无敌窗口

```cpp
static constexpr float MinIFrameSeconds = 0.2f;   // §02 §4.6
static constexpr float MaxIFrameSeconds = 0.4f;

/** @return 秒；直接作为 GE_Invincible 的 SetByCaller 时长 */
FORCEINLINE float GetInvincibilitySeconds(float DamageTaken, float MaxHealth, float EndlessFactor)
{
    const float Pct = MaxHealth > 0.f ? DamageTaken / MaxHealth : 0.f;
    const float Div = FMath::Max(1.f, EndlessFactor);
    return FMath::Clamp((Pct * (MaxIFrameSeconds / Div)) / 0.15f,
                        MinIFrameSeconds / Div, MaxIFrameSeconds / Div);
}
```

| 单次受伤 / 最大血量 | ≥ 15% | 11.25% | ≤ 7.5% |
|---|---|---|---|
| 无敌时长 | 0.4 s | 0.3 s | 0.2 s |

> **实现（D03）**：受击结算后 Apply `GE_Invincible`（Duration GE，时长 = 上式，授予 `State.Invincible`）；
> `GE_Damage` 的 `ApplicationTagRequirements.IgnoreTags` 含 `State.Invincible` → 窗口内伤害**自动被拒**，无需任何 if。
> `bypass_invincibility = true` 的伤害（如 `lose_hp_per_second`）用**另一个 GE**（不带该 Tag 需求，且不施加 `GE_Invincible`）。
> **闪避成功也进入无敌窗口**（只要 `bDodgeable`）。

### 4.5 生命回复周期

```cpp
/** §02 §5.2：T = 5.0 / (1 + |R-1|/2.25)；R <= 0 → 99 s */
FORCEINLINE float HpRegenPeriodSeconds(float Regen)
{
    return Regen <= 0.f ? 99.f : 5.f / (1.f + FMath::Abs(Regen - 1.f) / 2.25f);
}
```

| regen | 1 | 2 | 3 | 5 | 10 | 15 | 20 | 30 | 50 |
|---|---|---|---|---|---|---|---|---|---|
| 周期(s) | 5.000 | 3.462 | 2.647 | 1.800 | 1.000 | 0.692 | 0.529 | 0.360 | 0.220 |

特殊分支（实现在 `BrotatoGameplay` 的状态机，不在 Core）：
- `torture > 0` → 周期强制 **1.0 s**，每次回 `torture` 点（item_torture = 4）
- `double_hp_regen` → 每次回 2 HP
- `double_hp_regen_below_half_health` 且 HP < 50% → 回复量翻倍

### 4.6 周期性状态的实现归属（★ v2.0：改用 GAS 原生计时）

旧版在此定义了一组 `int32 *FramesLeft` 字段并规定"禁止 Timer / Periodic GE"。**该规定已作废**。
`BrotatoCore` 只提供**周期/时长的计算公式**（纯函数），**计时本身**交给 GAS 或批处理循环：

| 机制 | 公式（Core 提供） | 计时实现（Tier A/B） | 计时实现（Tier C，无 ASC） |
|---|---|---|---|
| 无敌窗口 | `GetInvincibilitySeconds()`（§4.4） | Duration GE + `State.Invincible` Tag 拦截 | — |
| 吸血 CD | 常量 `BrotatoTime::LifestealCd = 0.1f` | Duration GE + `State.LifestealCd` | — |
| 生命回复 | `HpRegenPeriodSeconds()`（§4.5） | Periodic GE（`Spec->Period = T`） | — |
| `lose_hp_per_second` | 常量 1.0 s | Periodic GE（`bypass_invincibility`） | — |
| 燃烧 | `BurnTickInterval()`（下方） | Periodic GE：`Period = T`、`Duration = NumTicks × T` | `FBurningRuntime` 里 `TickTimer -= DeltaTime` |
| 受击闪白 | 常量 `HitFlash = 0.1f` | Timed GameplayCue / MID | `FlashTimeLeft -= DeltaTime` |
| 每 5 s 叠加 | 常量 `StackingPeriod = 5.0f` | Periodic GE + Stacking | — |
| `percent_materials` 采样 | 常量 1.0 s | `FIntervalAccumulator` 或 1 s 循环 Timer | — |

```cpp
/** 燃烧 tick 间隔（秒）：可被 burning_cooldown_reduction 缩短，下限 0.1 s */
FORCEINLINE float BurnTickInterval(float BurningCooldownReductionPct)
{
    return FMath::Max(BrotatoTime::BurnTickMin,
                      BrotatoTime::BurnTickBase * (1.f - BurningCooldownReductionPct / 100.f));
}
/** 燃烧总时长（秒）= tick 次数 × 间隔 —— 原作 duration 字段是 tick 次数，不是秒 */
FORCEINLINE float BurnTotalDuration(int32 NumTicks, float TickInterval)
{ return NumTicks * TickInterval; }
```

> **语义保持不变**：`burning duration` 仍是 **tick 次数**（决定总伤害），只是在 GE 上表达为 `Duration = NumTicks × Period`。
> 叠加仍**逐字段取最大值刷新、不叠层**（GE 侧用 `StackLimitCount = 1` + `StackDurationRefreshPolicy = RefreshOnSuccessfulApplication`，并在 Apply 前逐字段取 max）。
> tick 伤害仍 `bDodgeable = false, bArmorApplied = false`。

---

## 5. 武器数学 `FBrotatoWeaponMath`

### 5.1 RuntimeStats 11 步（§04 §1 / §02 §5.1）

```cpp
/**
 * ★ 触发时机：开波时 + 任何属性/装备变动时全量重算（脏标记，模拟阶段 S11）
 *   绝不每 Tick 调用。
 */
FWeaponRuntimeStats ComputeRuntimeStats(
    const FBrotatoWeaponStats& Base0,
    const FBrotatoStatSnapshot& S,
    const FBrotatoHookView& H,
    int32 SameWeaponCount,          // WeaponStack 用
    int32 CurrentLevel,             // scaling 的 "stat_levels" 特殊键
    bool  bIsMelee,
    bool  bIsStructure)
{
    using namespace BrotatoMathInt;
    FBrotatoWeaponStats B = Base0;              // 步 1：深拷贝（含 BurningData）
    FWeaponRuntimeStats O;

    // 步 2：weapon_bonus（按 weapon_id 家族粒度匹配）
    for (const FBrotatoHookEntry& E : H.WeaponBonus)
        if (E.ExtraName == B.WeaponId) B.AddStat(E.AttributeTag, E.Value);

    // 步 3：weapon_class_bonus（按 set 匹配）★ lifesteal 单独 /100
    for (const FBrotatoHookEntry& E : H.WeaponClassBonus)
        if (B.Sets.Contains(E.ExtraName))
            B.AddStat(E.AttributeTag, E.IsLifesteal() ? E.Value / 100.f : E.Value);

    // 步 4：武器自带 Effect
    //   BurningEffect → B.BurningData = 覆写（spread 归 0）
    //   WeaponStack   → B[stat] += value × max(0, SameWeaponCount - 1)   ★ 第 1 把不算
    //   Exploding     → B.bIsExploding = true
    ApplyIntrinsicEffects(B, SameWeaponCount);

    // 步 5：攻速 → 冷却 / 后坐（★ 结构物不吃攻速）
    const float AtkSpd = bIsStructure ? 0.f
                       : (S.AttackSpeed + B.AttackSpeedMod) / 100.f;
    if (AtkSpd > 0.f)
    {
        // ★ 秒制：下限 MinWeaponCooldown = 1/30 s（原作 2 帧），不取整
        O.CooldownSeconds = FMath::Max(BrotatoTime::MinWeaponCooldown,
                                       B.CooldownSeconds * (1.f / (1.f + AtkSpd)));
        O.Recoil         = B.Recoil         / (1.f + AtkSpd);
        O.RecoilDuration = B.RecoilDuration / (1.f + AtkSpd);
    }
    else
    {
        O.CooldownSeconds = FMath::Max(BrotatoTime::MinWeaponCooldown,
                                       B.CooldownSeconds * (1.f + FMath::Abs(AtkSpd)));
        O.Recoil         = B.Recoil;            // ★ AtkSpd <= 0 时不做除算
        O.RecoilDuration = B.RecoilDuration;
    }

    // 步 6：scaling（加法区）★ 只对武器声明了的属性生效，不是全局加成
    float Scaling = 0.f;
    for (const FScalingStat& SS : B.ScalingStats)
    {
        if (bIsStructure && SS.IsBlockedForStructure()) continue;   // 结构物不吃 crit/lifesteal/range
        Scaling += SS.IsLevelKey() ? CurrentLevel * SS.Coef
                                   : S.Get(SS.AttributeTag) * SS.Coef;
    }
    float Damage = FMath::Max(1.f, B.Damage + Scaling);

    // 步 7：percent damage（唯一乘法区）★ 结构物不吃 stat_percent_damage
    const float Pct = bIsStructure ? 1.f : (1.f + S.PercentDamage / 100.f);
    const float Exp = B.bIsExploding ? (S.ExplosionDamage / 100.f) : 0.f;
    O.Damage = FMath::Max(1, BrotatoRound(Damage * (Pct + Exp)));

    // 步 8：暴击（玩家无 crit_damage 属性）
    O.CritDamage = B.CritDamage;                                        // 默认 1.5
    O.CritChance = bIsStructure ? B.CritChance
                                : B.CritChance + S.CritChance / 100.f;  // 默认 0.03
    // 步 9：精度
    O.Accuracy   = B.Accuracy + H.Effects.Accuracy / 100.f;
    // 步 10：吸血（★ 是概率，回固定 1 HP；结构物不吃）
    O.Lifesteal  = bIsStructure ? B.Lifesteal : B.Lifesteal + S.Lifesteal / 100.f;
    // 步 11：击退 + 射程
    O.Knockback  = FMath::Max(0.f, B.Knockback + H.Effects.Knockback);
    O.MinRange   = H.Effects.bNoMinRange ? 0 : B.MinRange;
    const float RangeStat = bIsStructure ? 0.f : S.Range;
    O.MaxRange   = FMath::Max(25.f, B.MaxRange + (bIsMelee ? RangeStat * 0.5f : RangeStat)); // ★ 近战只吃一半

    // 远程额外
    O.ProjectileSpread     = B.ProjectileSpread + H.Effects.Projectiles * 0.1f;
    O.NbProjectiles        = B.NbProjectiles > 0 ? B.NbProjectiles + H.Effects.Projectiles : B.NbProjectiles;
    O.Piercing             = B.Piercing + H.Effects.Piercing;
    O.PiercingDmgReduction = FMath::Clamp(B.PiercingDmgReduction - H.Effects.PiercingDamage/100.f, 0.f, 1.f);
    O.Bounce               = B.Bounce + H.Effects.Bounce;
    O.BounceDmgReduction   = FMath::Clamp(B.BounceDmgReduction   - H.Effects.BounceDamage/100.f, 0.f, 1.f);
    if (B.bIncreaseProjSpeedWithRange)
        O.ProjectileSpeed = FMath::Clamp(B.ProjectileSpeed + (B.ProjectileSpeed/300.f) * RangeStat, 50.f, 6000.f);
    else
        O.ProjectileSpeed = B.ProjectileSpeed;
    return O;
}
```

**校验样例**：匕首 T1 `damage=6, scaling=[melee 0.5]`，玩家近战 20 / 伤害% 30
→ `6 + 20×0.5 = 16` → `round(16 × 1.3) = 21`

### 5.2 冷却与抖动（§04 §2，★ 秒制）

```cpp
/** 每次射击后调用，返回下次冷却秒数（不取整） */
float GetNextCooldownSeconds(const FWeaponRuntimeStats& O, int32 WeaponCount,
                             int32 NbShotsTaken, const FBrotatoWeaponStats& Base,
                             FBrotatoRandom& Rng)   // Rng = Combat 流
{
    // 大装填（Chain Gun IV：每 100 发 × 60），★ 不带抖动
    if (Base.AdditionalCooldownEveryXShots != -1
        && NbShotsTaken % Base.AdditionalCooldownEveryXShots == 0)
        return O.CooldownSeconds * Base.AdditionalCooldownMultiplier;

    const int32 N = FMath::Min(WeaponCount, 6);                     // ★ 上限 6
    // ★ 双上限取小：N×CD/5 与 N×(5帧=0.0833s)，最大 ±0.5 s
    const float R = FMath::Min(N * O.CooldownSeconds / 5.f, N * (5.f / 60.f));
    return Rng.RandRange(FMath::Max(BrotatoTime::MinJitteredCooldown, O.CooldownSeconds - R),
                         O.CooldownSeconds + R);
}
```

**冷却的施加与重算（GAS 侧，D04 §3）**：

```cpp
// 攻击序列结束时 Apply Cooldown GE（等价于原作"射击期间冷却不流逝"）
Spec->SetSetByCallerMagnitude(BrotatoTags::Data_Cooldown, NextCooldownSeconds);

// 攻速变化时压缩当前冷却（对齐原作 reset_cooldown）：
//   取 min(剩余时长, 新冷却) → ASC->ModifyActiveGameplayEffectStartTime() 或 Remove+Apply
const float Remaining = ASC->GetActiveGameplayEffectRemainingDuration(CooldownHandle);
const float Target    = FMath::Min(Remaining, NewCooldownSeconds);
```

| 规则 | 值 |
|---|---|
| 攻速后冷却下限 | **1/30 s**（原作 2 帧） |
| 抖动后下限 | **1/60 s**（原作 1 帧；两者是不同的下限，勿混） |
| 计时起点 | **攻击序列结束**（近战整个挥击结束 / 远程 `RecoilDuration × 2` 结束）后开始流逝 |
| 取整 | ❌ 不取整（旧版"截断向零"条目已作废，§00 §6） |

验收（`CD` 为秒）：
- `W01` Dagger I（原作 27 帧 = 0.45 s）、攻速 0、武器数 1 → `R = min(0.45/5, 0.0833) = 0.0833` → 区间 `[0.367, 0.533] s`
- `W02` 武器数 6 → `R = min(6×0.45/5, 0.5) = 0.5` → 区间 `[0.0167, 0.95] s`
- `W03` Chain Gun 第 100 发 → `0.0167 × 60 = 1.0 s`
- `W04` ★ 帧率无关：30 / 60 / 144 fps 下连续 100 次攻击的平均间隔误差 < 1%

### 5.3 近战挥击时序（§04 §3）

```cpp
struct FMeleeSwingTiming
{
    float AtkDuration    = 0.f;   // 命中段总时长
    float BackDuration   = 0.f;   // 收回
    float RecoilDuration = 0.f;   // 起手后拉（已被攻速除算）
    float TotalDuration() const { return AtkDuration * 0.5f + BackDuration + RecoilDuration; }
};

FMeleeSwingTiming ComputeSwingTiming(float AttackSpeedStat, float AttackSpeedMod,
                                     float EffectiveRange, float RecoilDurationAfterAtkSpd)
{
    const float A = (AttackSpeedStat + AttackSpeedMod) / 100.f;
    const float RangeFactor = FMath::Max(0.f,
        EffectiveRange / FMath::Clamp(70.f * (1.f + A / 3.f), 70.f, 120.f));
    FMeleeSwingTiming T;
    T.AtkDuration    = FMath::Max(0.01f, 0.2f - A / 10.f) + RangeFactor * 0.15f;
    T.BackDuration   = A > 0.f ? 0.2f / (1.f + A * 3.f) : 0.2f;
    T.RecoilDuration = RecoilDurationAfterAtkSpd;
    return T;
}

/** SWEEP 时的有效距离：min(max_range, max(250, 到目标距离))，MIN_SWEEP_DISTANCE = 250 */
FORCEINLINE float SweepAtkDistance(float MaxRange, float DistToTarget)
{ return FMath::Min(MaxRange, FMath::Max(250.f, DistToTarget)); }

// SWEEP 常量
static constexpr float SweepAngle = 0.9f * PI;   // ≈162°
// sweep_half_duration = AtkDuration / 4；side_range = atk_distance / 2

/** 缓动：严格对齐 Godot TRANS_EXPO/EASE_OUT 与 TRANS_LINEAR */
FORCEINLINE float EaseExpoOut(float T) { return T >= 1.f ? 1.f : 1.f - FMath::Pow(2.f, -10.f * T); }
FORCEINLINE float EaseLinear (float T) { return T; }
```

> **手感密码**：起手/收回用 **EXPO-OUT**，扫动段用 **LINEAR**（保证判定均匀）。统一成同一曲线会毁掉横扫手感。

### 5.4 选靶与射程语义

```cpp
static constexpr float MinRangeConst      = 25.f;    // 运行时 max_range 硬下限
static constexpr float DetectionRangeAdd  = 200.f;   // 候选集半径 = max_range + 200
static constexpr float FireRangeAdd       = 50.f;    // 开火判定上界（SWEEP 时为 0）
static constexpr float ProjDestroyAdd     = 100.f;   // 子弹销毁（手动瞄准时 50）
```

选靶：`Utils.get_nearest` —— **距该武器自身位置最近、且距离 ≥ min_range** 的敌人；射击过程中锁定当前目标；无目标用 `idle_angle`（朝右 = `idle`，朝左 = `PI - idle`）。

开火条件（全部满足）：
1. 冷却已就绪（Tier A：`CheckCooldown()` 通过，即无 `Cooldown.Weapon.{Slot}` Tag；Tier C：`CooldownRemaining <= 0`）
2. `Rule.CanAttackWhileMoving` 为真 **或** 玩家当前无移动输入
3. 目标有效且 `dist ∈ [MinRange, MaxRange + FireRangeAdd]`
4. 或处于手动瞄准模式且未清场

---

## 6. 经济数学 `FBrotatoEconomyMath`

### 6.1 Tier 抽取（★ 单次掷骰，商店/宝箱/升级共用）

```cpp
struct FTierWeights { int32 MinWave; float Base; float WaveBonus; float Max; };
// DT_TierWeights：
//  I   COMMON    : MinWave 0, Base 1.0, WB 0.0,    Max 1.0
//  II  UNCOMMON  : MinWave 0, Base 0.0, WB 0.06,   Max 0.60
//  III RARE      : MinWave 2, Base 0.0, WB 0.02,   Max 0.25
//  IV  LEGENDARY : MinWave 6, Base 0.0, WB 0.0023, Max 0.08

EBrotatoTier GetTierFromWave(int32 Wave, float LuckStat,
                             const TArray<FTierWeights>& W, FBrotatoRandom& Rng)
{
    const float Rand = Rng.RandRange(0.f, 1.f);     // ★ 只掷一次
    const float L    = LuckStat / 100.f;
    for (int32 i = 3; i >= 0; --i)                  // ★ LEGENDARY → COMMON 倒序
    {
        const float WaveBase   = FMath::Max(0.f, ((Wave - 1) - W[i].MinWave) * W[i].WaveBonus);
        const float WaveChance = L >= 0.f ? WaveBase * (1.f + L)
                                          : WaveBase / (1.f + FMath::Abs(L));
        const float Chance = W[i].Base + WaveChance;
        if (Rand <= FMath::Min(Chance, W[i].Max)) return (EBrotatoTier)i;
    }
    return EBrotatoTier::Common;
}
```

> ⚠️ 逐档独立掷骰会得到**完全不同**的分布。必须单次 rand 复用 + 从高到低遍历。
> 幸运只放大「波次增量项」，不影响 Base，且受 Max 硬封顶。
> **升级面板复用此函数，但把 `level` 当 `wave` 传入**，且有硬性覆写：`level==5 → II`；`level ∈ {10,15,20} → III`；其他 `level%5==0 → IV`。

零幸运参考：W10 → `P(IV)=0.0069, P(III)=0.14, P(II)=0.54`；W20 → `0.0299 / 0.25(封顶) / 0.60(封顶)`。

### 6.2 定价 / 重掷 / 回收

```cpp
/** §05 §3.6 */
int32 GetValue(int32 Wave, int32 BaseValue, bool bAffectedByItemsPrice, bool bIsWeapon,
               float ItemsPricePct, float SpecificPricePct, float InflationPct, float EndlessFactor)
{
    float V = BaseValue;
    if (bIsWeapon && bAffectedByItemsPrice) V = BaseValue * (1.f + WeaponsPricePct / 100.f);
    const float PriceFactor = 1.f + (ItemsPricePct + SpecificPricePct) / 100.f;
    const float DiffFactor  = InflationPct / 100.f;
    const float EF          = EndlessFactor / 5.f;
    return BrotatoMathInt::BrotatoTruncInt(
        FMath::Max(1.f, (V + Wave + V * Wave * (0.1f + DiffFactor)) * PriceFactor * (1.f + EF)));
}

/** §05 §3.7：累进重掷 */
int32 GetRerollPrice(int32 Wave, int32 LastRerollValue, float EndlessFactor)
{
    return BrotatoMathInt::BrotatoTruncInt(
        FMath::Max(1.f, LastRerollValue + FMath::Max(1.f, 0.5f * Wave * FMath::Sqrt(1.f + EndlessFactor))));
}

/** §05 §3.9：基础 25%，clamp [0.01, 1.0]；无尽波不吃 items_price */
int32 GetRecyclingValue(int32 Wave, int32 FromValue, bool bIsWeapon, bool bAffected,
                        float RecyclingGainsPct, bool bWithinNormalWaves)
{
    const bool bActuallyAffected = bAffected && bWithinNormalWaves;
    const float Rate = FMath::Clamp(0.25f + RecyclingGainsPct / 100.f, 0.01f, 1.f);
    return BrotatoMathInt::BrotatoTruncInt(
        FMath::Max(1.f, GetValue(Wave, FromValue, bActuallyAffected, bIsWeapon, ...) * Rate));
}

/** HP 商店（Demon）：★ 向上取整 */
FORCEINLINE int32 GetHpShopPrice(int32 Value) { return FMath::CeilToInt(Value / 20.f); }
```

验收：`E02` value=70, W=15，无加成 → **190**；`E03` W=10 首次 **15**，后续 20/25/30；`E04` value=100, W=10, recycling_gains=35 → `floor(210 × 0.60) = 126`。

### 6.3 掉落概率

```cpp
/** 材料掉落（§05 §4.3） */
float GoldDropChance(int32 Wave, float GoldDropsPct, float EnemyGoldDropsPct,
                     float DiffGoldDropsPct, bool bIsEnemy, bool bHordeWave, bool bAlwaysDrop)
{
    float G = GoldDropsPct / 100.f;                      // 默认 100 → 1.0
    if (bIsEnemy) G += EnemyGoldDropsPct / 100.f;
    const float D = DiffGoldDropsPct / 100.f;
    float P = (Wave < 5) ? (1.f + D) * G
                         : FMath::Max(0.5f * G, (1.f - Wave * 0.015f + D) * G);
    if (bHordeWave) P *= 0.65f;
    if (bAlwaysDrop) P = 1.f;
    return P;                                            // 中性单位无视此概率
}

/** 消耗品与宝箱（§05 §4.9）★ luck 在此是线性 (1+L)，与 Tier 抽取不同 */
float ConsumableDropChance(float BaseDropChance, float LuckStat, float EndlessFactor, bool bEndless)
{
    float P = FMath::Min(1.f, BaseDropChance * (1.f + LuckStat / 100.f));
    if (bEndless) P /= (1.f + EndlessFactor);
    return P;
}
/** ★ 反刷宝箱：分母随本波已掉宝箱数递增 */
float ItemBoxChance(float ItemDropChance, float LuckStat, int32 BoxesSpawnedThisWave)
{ return (ItemDropChance * (1.f + LuckStat / 100.f)) / (1.f + BoxesSpawnedThisWave); }
```

---

## 7. 成长数学 `FBrotatoProgressionMath`

```cpp
/** XP 曲线：XP_need(L) = (3+L)^2（§02 §6.1） */
FORCEINLINE int32 XpNeededForLevel(int32 Level) { return (3 + Level) * (3 + Level); }

/** 无尽因子（§02 §9.4 / §06 §3） */
float GetEndlessFactor(int32 Wave)
{
    const float EW   = FMath::Max(0, Wave - 20);
    const float Mult = 2.f + FMath::Max(0.f, (Wave - 35) * 0.2f);
    return FMath::Max(0.f, ((EW * (EW + 1.f)) / 2.f) / 100.f) * Mult;
}

/** 敌人属性随波次成长（§06 §8.1）★ 三处 round */
int32 EnemyMaxHealth(int32 Base, float IncPerWave, int32 Wave, float AccHealth,
                     float EnemyHealthPct, float EF)
{
    using namespace BrotatoMathInt;
    const int32 V = BrotatoRound((Base + IncPerWave * (Wave - 1)) * (AccHealth + EnemyHealthPct / 100.f));
    return BrotatoRound(V * (1.f + EF * 2.25f));
}
int32 EnemyDamage(int32 Base, float IncPerWave, int32 Wave, float DmgCoef,
                  float AccDamage, float EnemyDamagePct, float EF)
{
    using namespace BrotatoMathInt;
    const float BaseDmg = (Base + IncPerWave * (Wave - 1)) * (1.f + DmgCoef);
    const int32 New     = BrotatoRound(BaseDmg * (AccDamage + EnemyDamagePct / 100.f));
    return BrotatoRound(New * (1.f + EF));
}
int32 EnemySpeed(int32 Base, float EnemySpeedPct, float SpeedCoef, float AccSpeed, float EF)
{
    using namespace BrotatoMathInt;
    const float BaseSpd = (Base + Base * EnemySpeedPct / 100.f) * (1.f + SpeedCoef);
    const int32 New     = BrotatoRound(BaseSpd * AccSpeed);
    return BrotatoRound(New * (1.f + FMath::Min(1.75f, EF / 13.33f)));   // ★ 速度乘子封顶 2.75
}

/** 收获自增长（§02 §7.2）★ ceil */
FORCEINLINE int32 HarvestingGrowth(int32 H, float GrowthPct, bool bEndless)
{
    return bEndless ? -FMath::CeilToInt(0.20f * H)               // ENDLESS_HARVESTING_DECREASE = 20
                    :  FMath::CeilToInt(H * GrowthPct / 100.f);  // 默认 5%
}

/** 属性转换（§02 §7.1）★ floor */
struct FConvertResult { int32 DeltaFrom; int32 DeltaTo; };
FConvertResult ConvertStat(float FromStat, float PctConverted, float Value, float ToValue)
{
    const int32 N = FMath::FloorToInt((FromStat * PctConverted / 100.f) / Value);
    return { -N * (int32)Value, +N * (int32)ToValue };
}

/** 玩家移动速度（§02 §9.1） */
FORCEINLINE float PlayerSpeed(float SpeedStat, float SpeedCap)
{ return 450.f * (1.f + FMath::Min(SpeedStat, SpeedCap) / 100.f); }
```

无尽因子参考：

| 波 | 20 | 21 | 25 | 30 | 35 | 40 | 50 | 60 |
|---|---|---|---|---|---|---|---|---|
| EF | 0 | 0.02 | 0.30 | 1.10 | 2.40 | 6.30 | 23.25 | 57.40 |

---

## 8. 属性快照 `FBrotatoStatSnapshot`

计算核心**不依赖 GAS**，所以需要一个中间层把 ASC 的 Attribute 与 `FEnemyState` 都投影成同一结构：

```cpp
USTRUCT() struct BROTATOCORE_API FBrotatoStatSnapshot
{
    GENERATED_BODY()
    // 16 主属性（已含三层叠加与 gain 乘区的最终 CurrentValue）
    float MaxHp = 10.f, HpRegeneration = 0.f, Lifesteal = 0.f, PercentDamage = 0.f;
    float MeleeDamage = 0.f, RangedDamage = 0.f, ElementalDamage = 0.f, AttackSpeed = 0.f;
    float CritChance = 0.f, Engineering = 0.f, Range = 0.f, Armor = 0.f;
    float Dodge = 0.f, Speed = 0.f, Luck = 0.f, Harvesting = 0.f;
    // 常用次级
    float ExplosionDamage = 0.f, ExplosionSize = 0.f, DamageAgainstBosses = 0.f;
    float GiantCritDamage = 0.f, DodgeCap = 60.f, HpCap = 999999.f, SpeedCap = 999999.f;
    int32 HitProtection = 0;
    int32 CurrentLevel = 0;

    float Get(FGameplayTag AttributeTag) const;   // Tag → 字段，供 scaling_stats 使用
};
```

| 来源 | 填充方式 |
|---|---|
| Tier A/B（有 ASC） | `UBrotatoStatSnapshotBuilder::FromASC(ASC)`，模拟阶段 S11 脏时重建一次 |
| Tier C（敌人） | 开波时由 `FBrotatoProgressionMath` 直接算入 `FEnemyState`，不需要快照 |
| 结构物 | 复用玩家快照，但 `bIsStructure = true` 屏蔽 5 个属性 |

> **性能要点**：快照每 Tick **最多重建一次**（脏标记），武器 RuntimeStats 重算也只在同一阶段。

---

## 9. 常量对照表（`BrotatoCoreTypes.h` 集中定义）

```cpp
namespace BrotatoConst
{
    // 时间（★ 全部为秒；括号内是原作帧值，仅供溯源）
    constexpr float LifestealCd          = 0.1f;    // 6 帧
    constexpr float HitFlash             = 0.1f;    // 6 帧
    constexpr float BurnTickBase         = 0.5f;    // 30 帧（可缩，下限 0.1 s）
    constexpr float BurnTickMin          = 0.1f;    // 6 帧
    constexpr float StackingPeriod       = 5.0f;    // 300 帧
    constexpr float HarvestDelay         = 0.667f;  // 40 帧
    constexpr float SpawnQueueInterval   = 0.05f;   // 3 帧 → 20 个/s
    constexpr float ProjectileLaunchDelay= 0.02f;   // 出膛停顿（原作 0.02 s）
    constexpr float ExplosionHitWindow   = 0.05f;   // 3 帧
    constexpr float ExplosionLifetime    = 3.0f;    // 180 帧
    constexpr float MaxSimDeltaTime      = 0.1f;    // ★ 单帧钳制上限
    constexpr float MinWeaponCooldown    = 1.f/30.f;// 2 帧
    constexpr float MinJitteredCooldown  = 1.f/60.f;// 1 帧

    // 距离 / 尺寸（px = uu）
    constexpr float PlayerBaseSpeed      = 450.f;
    constexpr float MinDistFromPlayer    = 300.f;
    constexpr float EdgeMapDist          = 64.f;
    constexpr float TileSize             = 64.f;
    constexpr float MinRange             = 25.f;
    constexpr float MinSweepDistance     = 250.f;
    constexpr float ExplosionBaseRadius  = 147.34f;
    constexpr float PickupBaseRadius     = 150.f;
    constexpr float PickupMinRadius      = 30.f;
    constexpr float CollisionCellSize    = 128.f;
    constexpr float MoveMargin           = 0.08f;

    // 数量上限
    constexpr int32 MaxGolds             = 50;
    constexpr int32 MaxStructures        = 100;
    constexpr int32 MaxGraphicalEffects  = 100;
    constexpr int32 MaxTexts             = 100;
    constexpr int32 MaxSoundsQueued      = 32;
    constexpr int32 MaxSoundPlayers      = 12;
    constexpr int32 DefaultMaxEnemies    = 100;

    // 规则默认值
    constexpr int32 NbOfWaves            = 20;
    constexpr int32 DefaultWeaponSlot    = 6;
    constexpr float DefaultDodgeCap      = 60.f;
    constexpr float DefaultHpCap         = 999999.f;
    constexpr float DefaultSpeedCap      = 999999.f;
    constexpr float DefaultGoldDrops     = 100.f;
    constexpr float DefaultHarvestGrowth = 5.f;
    constexpr float MinDeathKnockback    = 15.f;
    constexpr float KnockbackDecayRate   = 6.3216f; // = -60·ln(0.9)，原作 lerp(→0, 0.1)/帧
    constexpr int32 NbShopItems          = 4;
    constexpr float ChanceWeapon         = 0.35f;
    constexpr float ChanceSameWeapon     = 0.20f;
    constexpr float ChanceSameWeaponSet  = 0.35f;
    constexpr float ChanceWantedItemTag  = 0.05f;
    constexpr float RecyclingBaseRate    = 0.25f;
}
```

---

## 10. 本模块单测清单（`Tests/Core/`）

| 文件 | 覆盖 | 关键用例 |
|---|---|---|
| `IntSemanticsTest.cpp` | §3.4 | T13-1…T14-4（8 条） |
| `DamageMathTest.cpp` | §4 | armor_coef(0/10/20/−10)、暴击用原始值重算(=17)、闪避双 clamp、iframes 三档、最小伤害恒 1 |
| `WeaponMathTest.cpp` | §5 | 匕首 21 点、冷却抖动区间 W01/W02、大装填 W03、挥击三段时长、近战射程只吃一半 |
| `EconomyMathTest.cpp` | §6 | E02/E03/E04、Tier 抽取 10 万次蒙特卡洛 vs 理论值、掉落概率曲线 |
| `ProgressionTest.cpp` | §7 | XP 累计(15→2095, 20→4310, 30→12515)、EF(30)=1.10 / (50)=23.25、速度乘子 51 波封顶 2.75、收获 ±5%/−20% |
| `RandomTest.cpp` | §2 | 同 seed 复现、`ChanceSuccess(0)==false`、`<=` vs `<` 边界 |

> 全部测试**不依赖引擎启动**，可用 `-nullrhi -unattended` 秒级跑完，作为 pre-commit 门禁。
