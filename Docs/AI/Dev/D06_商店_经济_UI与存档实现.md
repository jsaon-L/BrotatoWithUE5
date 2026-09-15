# D06 · 商店、经济、UI 与存档实现

> **模块**：`BrotatoUI`（CommonUI）+ `BrotatoSave` + `BrotatoGameplay`（Shop / Run 流程）
> **里程碑**：M3 商店闭环 → M5 元进度与存档 → M7 本地化与设置
> **上游规格**：`Docs/05 §3 §4`、`Docs/08` 全文、`Docs/07 §4 §6`

---

## 1. 全局流程状态机

### 1.1 Subsystem 初始化顺序（= 原作单例依赖顺序）

```
GameInstanceSubsystem（跨局常驻）
 1 UBrotatoUtilsSubsystem      工具
 2 UBrotatoMenuDataSubsystem   菜单数据
 3 UBrotatoProgressSubsystem   ★ 存档 / 解锁 / 设置 / 统计
 4 UBrotatoItemServiceSubsystem 物品池、Tier 抽取
 5 UBrotatoZoneServiceSubsystem 区域 / 波次数据
 6 UBrotatoSoundSubsystem       2D 音效（USoundConcurrency + 去重窗口）
 7 UBrotatoMusicSubsystem
 8 UBrotatoTextSubsystem        本地化格式化
 9 UBrotatoChallengeSubsystem
10 UBrotatoInputSubsystem       手柄/键鼠状态

WorldSubsystem（每局/每关）
11 UBrotatoRunSubsystem        ★ 一局状态（gold / items / weapons / effects）
12 UBrotatoSimSubsystem         ★ 模拟阶段编排（13 阶段，可变 DeltaTime）
13 UBrotatoCombatQuerySubsystem 碰撞查询门面（UE 原生 / 空间哈希兜底）
14 UEnemySubsystem / UProjectileSubsystem / UPickupSubsystem
15 UBrotatoWaveManager
16 UFxSubsystem / UBrotatoSound2DSubsystem
```

### 1.2 状态与转换

用 `UCommonActivatableWidgetStack` + 一个 `UBrotatoFlowSubsystem` 驱动。

| 状态 | 转出事件 | 目标 | 关键副作用 |
|---|---|---|---|
| **TitleScreen** | Start | CharacterSelection | `Music.Tween(-5, 1.0)` |
| | Continue | Shop | `RunSubsystem->ResumeFromState(Progress->CurrentRunState)` |
| | Options / Progress / Credits | 同场景内页切换 | Push/Pop，**无转场动画** |
| | Quit | — | `RequestExit()` |
| **CharacterSelection** | `IA_Cancel` | TitleScreen | `CurrentZone = 0`，不重载音乐 |
| | 选定角色 | `WeaponSlot == 0 ? DifficultySelection : WeaponSelection` | `RunSubsystem->AddCharacter()` |
| **WeaponSelection** | `IA_Cancel` | CharacterSelection | `ApplyWeaponSelectionBack()`（清 weapons/items/effects） |
| | 选定武器 | DifficultySelection | `AddWeapon(bIsStarting=true)` + `ProcessStartingHooks()` |
| | 可选项 | — | `Character->StartingWeapons ∩ Progress->WeaponsUnlocked`；**不显示未解锁项** |
| **DifficultySelection** | `IA_Cancel` | WeaponSelection | 清 weapons/items/effects/bEndless/套装 → 重新 `AddCharacter` |
| | 确认 | **Arena** | 见下 |
| **Arena（战斗）** | EndWaveTimer 到 + `bLost \|\| bWon \|\| bLastWave` | EndRun | |
| | EndWaveTimer 到（否则） | Shop | `Music.Tween(-8)` → ItemBoxUI 串行 → UpgradesUI 串行 → 挑战弹窗 |
| **Shop** | Go | Arena | `SaveRunState()` → `++CurrentWave` → `Music.Tween(0)` |
| | Endless（**仅 W==19 且未开无尽**） | Arena | `bIsEndlessRun = true` 后同 Go |
| **EndRun** | Retry | Arena | `RunSubsystem->Reset(bKeepCharacter=true)`；`Music.Play(0)` |
| | NewRun | CharacterSelection | 记忆上次角色；`Reset()`；`Music.Tween(-5)` |
| | Exit | TitleScreen | `Reset()` |

难度确认的副作用（**顺序固定**）：
```cpp
Progress->SetDifficultySelected(Value);
RunSubsystem->SetCurrentDifficulty(Value);
RunSubsystem->InitElitesSpawn();                 // D05 §7.2
Progress->Save();
RunSubsystem->ApplyDifficultyEffects(Value);
Music->Tween(0.f, 1.0f);
RunSubsystem->SnapshotAccessibility(Settings.EnemyScaling);   // 开局快照，局内不变
```

### 1.3 波次内计时器一览（★ 全部以秒实现）

| 原作节点 | 时长(s) | ★ UE 实现方式 | 说明 |
|---|---|---|---|
| `EndWaveTimer` | 2.0 / 2.5 / 3.0 | `FTimerManager` | 正常 / 死亡 / 胜利 |
| `HarvestingTimer` | **0.667** | `FTimerManager` | 收获成长延迟（原作 40 帧） |
| `WaveTimer/TickTimer` | 1.0 | 循环 Timer | `TimeLeft < 6` 时启动滴答 |
| `FiveSecondsTimer` | **5.0** | **Periodic GE**（`Period = 5`） | `temp_stats_stacking`（D03 §6.3） |
| `WaveClearedLabel/Timer` | 0.1 | `WidgetAnimation` | 逐字 |
| `DimScreen/Tween` | 1.0 | `WidgetAnimation` | alpha 0→0.5 线性 |
| `ChallengeCompletedUI/HideTimer` | 2.0 | `WidgetAnimation` / Timer | 停留 |
| `UIProgressBar/Timer` | 0.2 | `WidgetAnimation` | 掉血闪白 |
| `ShopItem/_buy_delay` | 0.05 | `float BuyDelayLeft -= DeltaTime` 或 Timer | 防连点 |
| `MenuButton/_delay` | 0.05 | 同上 | 按下后短暂 disabled |
| `LifestealTimer` | **0.1** | **Duration GE** + `State.LifestealCd` | 吸血 CD（D03 §1） |
| `MovingTimer` / `NotMovingTimer` | 1.0 | 循环 Timer | `percent_materials` |
| `LoseHealthTimer` | 1.0 | **Periodic GE**（`Period = 1`） | `lose_hp_per_second` |
| `BurningTimer` | **0.5** | **Periodic GE**（`Spec->Period`） | 燃烧 tick（D03 §6.4） |
| `EntitySpawner/StructureTimer` | 1.0 | 循环 Timer | 结构物检查 |
| 武器冷却 | 变量 | **Cooldown GE**（SetByCaller） | D04 §3 |
| 无敌窗口 | 0.2–0.4 | **Duration GE** + `State.Invincible` | §02 §5.4 |

> ★ **不再使用帧计数器**。数值类计时优先级：**GE（Duration/Periodic/Cooldown）> AbilityTask > FTimerManager > `Remaining -= DeltaTime`**；
> 纯表现类计时用 `WidgetAnimation` / Timeline / Niagara（§00 §2.3）。

### 1.4 暂停语义

| 对象 | 暂停时 | 实现 |
|---|---|---|
| `UBrotatoSimSubsystem` | **停止全部模拟阶段** | `SetPaused(true)`；配合 `UGameplayStatics::SetGamePaused` |
| 输入服务 / 光标管理 | **继续** | `bTickEvenWhenPaused = true`；`IA_*` 设 `bExecuteWhenPaused` |
| SFX / 音乐 | **继续** | Submix 不暂停；UI 音 `bIsUISound = true` |
| 暂停菜单 UI | **继续** | CommonUI + `WidgetAnimation` |
| Niagara 特效 | **暂停** | 默认行为 |
| **GE 计时（Duration / Periodic / Cooldown）** | **自动暂停** ✅ | ★ 走 World 时间，`SetGamePaused` 时天然停走 —— 这是改用 GAS 原生计时白送的收益 |
| `FTimerManager` | **自动暂停** ✅ | World Timer 同样受 Pause 影响 |

失焦：`Settings.bPauseOnFocusLost`（**默认开**）→ 自动 Pause；`bMuteOnFocusLost` → 静音 Master。

---

## 2. 商店实现

### 2.1 商品生成

```cpp
TArray<FShopSlot> UBrotatoShopSubsystem::GetShopItems(int32 Wave)
{
    const int32 Number = BrotatoConst::NbShopItems;           // 4

    // ① 武器保底数
    int32 Guaranteed = (Wave <= 2) ? 2 : (Wave <= 5) ? 1 : 0;
    Guaranteed = FMath::Max(Guaranteed, (int32)Snapshot.MinimumWeaponsInShop);
    Guaranteed -= CountLockedWeapons();                       // ★ 锁定武器计入保底额

    // ③ 去重集合：本次已生成 ∪ 上一次商店列表
    TSet<FName> Excluded = PreviousShopItemIds;
    TArray<FShopSlot> Out;

    // 锁定项先填入
    for (const FLockedShopItem& L : RunSubsystem->GetLockedShopItems())
    { Out.Add({L.Data, L.WaveValue, /*bLocked*/true}); Excluded.Add(L.Data->MyId); }

    int32 WeaponsMade = CountWeapons(Out);
    while (Out.Num() < Number)
    {
        // ② 每格类型判定
        EBrotatoCategory Type;
        if (Snapshot.WeaponSlot <= 0)                       Type = EBrotatoCategory::Item;
        else if (Wave <= 2)  Type = (WeaponsMade < Guaranteed) ? EBrotatoCategory::Weapon : EBrotatoCategory::Item;
        else if (HasPendingGuaranteedItem())                Type = EBrotatoCategory::Item;   // 强制该 id
        else Type = (Rng.ChanceSuccess(BrotatoConst::ChanceWeapon) || WeaponsMade < Guaranteed)
                  ? EBrotatoCategory::Weapon : EBrotatoCategory::Item;

        const EBrotatoTier Tier = ItemService->GetTierFromWave(Wave, Snapshot.Luck);
        const UBrotatoItemParentData* D = (Type == EBrotatoCategory::Weapon)
            ? PickWeapon(Tier, Excluded) : PickItem(Tier, Excluded);
        if (!D) break;
        Excluded.Add(D->MyId);
        if (Type == EBrotatoCategory::Weapon) ++WeaponsMade;
        Out.Add({D, Wave, false});
    }
    PreviousShopItemIds = Excluded;
    return Out;
}
```

常量（`item_service.gd:20-28`）：

| 常量 | 值 |
|---|---|
| `NB_SHOP_ITEMS` | **4** |
| `CHANCE_WEAPON` | **0.35** |
| `CHANCE_SAME_WEAPON` | **0.20** |
| `CHANCE_SAME_WEAPON_SET` | **0.35** |
| `MAX_WAVE_TWO_WEAPONS_GUARANTEED` | **2** |
| `MAX_WAVE_ONE_WEAPON_GUARANTEED` | **5** |
| `CHANCE_WANTED_ITEM_TAG` | **0.05** |

### 2.2 武器倾向

```cpp
const UBrotatoWeaponData* UBrotatoShopSubsystem::PickWeapon(EBrotatoTier Tier, const TSet<FName>& Excluded)
{
    const float RandWanted = Rng.Rand();                            // ★ 单次
    const float BonusSameSet = FMath::Max(0.f, (6 - CurrentWave) * 0.03f);
    const float ChanceSameSet = BrotatoConst::ChanceSameWeaponSet + BonusSameSet;  // W=1 → 0.50

    const int32 ClampedTier = FMath::Clamp((int32)Tier,
        (int32)Snapshot.MinWeaponTier, (int32)Snapshot.MaxWeaponTier);
    TArray<const UBrotatoWeaponData*> Pool = ItemService->GetWeaponPool(ClampedTier);

    if (HasRule(Rule_NoMeleeWeapons))  Pool.RemoveAll(IsMelee);
    if (HasRule(Rule_NoRangedWeapons)) Pool.RemoveAll(IsRanged);

    if (RandWanted < BrotatoConst::ChanceSameWeapon)
    {   // 仅保留玩家已持有同 weapon_id 的武器（若有）
        auto F = Pool.FilterByPredicate([&](auto* W){ return RunSubsystem->HasWeaponId(W->WeaponId); });
        if (F.Num() > 0) Pool = MoveTemp(F);
    }
    else if (RandWanted < ChanceSameSet)
    {   // 仅保留与玩家武器共享职业的武器（若有）
        auto F = Pool.FilterByPredicate([&](auto* W){ return RunSubsystem->SharesAnySet(W); });
        if (F.Num() > 0) Pool = MoveTemp(F);
    }
    Pool.RemoveAll([&](auto* W){ return Excluded.Contains(W->MyId); });
    return Pool.Num() > 0 ? Rng.RandElement(Pool) : FallbackPick(ClampedTier);
}
```

### 2.3 物品倾向、max_nb 与三级回退

```cpp
const UBrotatoItemData* UBrotatoShopSubsystem::PickItem(EBrotatoTier Tier, const TSet<FName>& Excluded)
{
    TArray<const UBrotatoItemData*> Pool   = ItemService->GetItemPool((int32)Tier);
    TArray<const UBrotatoItemData*> Backup = Pool;

    // 5% 只出角色偏好 tag 物品
    if (Rng.ChanceSuccess(BrotatoConst::ChanceWantedItemTag))
    {
        const TArray<FGameplayTag>& Wanted = RunSubsystem->GetCharacter()->WantedTags;
        auto F = Pool.FilterByPredicate([&](auto* I){ return HasAnyTag(I->Tags, Wanted); });
        if (F.Num() > 0) Pool = MoveTemp(F);
    }

    // max_nb 限制
    auto FilterMaxNb = [&](TArray<const UBrotatoItemData*>& P)
    {
        P.RemoveAll([&](const UBrotatoItemData* I)
        {
            if (Excluded.Contains(I->MyId)) return true;
            if (I->MaxNb == 1 && RunSubsystem->OwnsItemIncludingLocked(I->MyId)) return true;
            if (I->MaxNb != -1 && RunSubsystem->CountItem(I->MyId) >= I->MaxNb) return true;
            return false;      // max_nb == -1 无限叠加
        });
    };
    FilterMaxNb(Pool); FilterMaxNb(Backup);

    // ★ 三级回退
    if (Pool.Num()   > 0) return Rng.RandElement(Pool);
    if (Backup.Num() > 0) return Rng.RandElement(Backup);
    return ItemService->RawPick((int32)Tier, EBrotatoCategory::Item, Rng);
}
```

> `OwnsItemIncludingLocked` 必须把 `LockedShopItems` 里的 ItemData 也计入 owned（避免宝箱/商店重复给唯一物品）。
> `max_nb = 1` 的已知物品：`item_anvil`、`item_bloody_hand`、`item_sifds_relic`、`item_torture`、`item_crown`。

### 2.4 锁定（Lock）

| 规则 | 说明 |
|---|---|
| 存储 | `RunSubsystem->LockedShopItems`（`[ItemData, WaveValue]`），**跨波持久**并进存档 |
| 重掷限制 | `LockedShopItems.Num() < 4` 才允许重掷（全锁 = 不能重掷） |
| 补齐 | 锁定项先填入，剩余 `4 - Locked.Num()` 由 `GetShopItems()` 补 |
| **★ 价格冻结** | 锁定物品保留锁定时的 `WaveValue`，价格不随波次涨 |
| 设置项 | `Settings.bKeepLock` 决定进入新商店是否保留 |

验收 `E05`：W10 锁定 value=70（价 150），W15 再看 → **仍是 150**。

### 2.5 重掷

```cpp
void UBrotatoShopSubsystem::Reroll()
{
    if (RunSubsystem->GetLockedShopItems().Num() >= BrotatoConst::NbShopItems) return;

    if (FreeRerolls > 0)
    {
        RerollPrice = 0; --FreeRerolls;
        RunSubsystem->AddTracked("item_dangerous_bunny", LastRerollPrice);
        // ★ 免费重掷不推进 LastRerollPrice
    }
    else
    {
        RerollPrice = LastRerollPrice;
        RunSubsystem->RemoveCurrency(RerollPrice);
        LastRerollPrice = FBrotatoEconomyMath::GetRerollPrice(CurrentWave, LastRerollPrice, EndlessFactor);
    }
    ShopItems = GetShopItems(CurrentWave);

    // ★ 清空商店奖励
    if (ShopItems.Num() == 0)
    { ++FreeRerolls; if (RerollPrice == 0) ++FreeRerolls; }    // 免费重掷时再 +1（共 +2）
}
```

初始化：`LastRerollPrice = GetRerollPrice(Wave, Wave)` → 即 `W + max(1, floor(0.5W))`。
W=10（EF=0）：首价 **15**，之后 20/25/30/35…

免费重掷来源：`item_dangerous_bunny`（Tier II, value 35）`free_rerolls +1`；进店初始化 `FreeRerolls = Snapshot.FreeRerolls`；购物中新增的差额立刻累加；清空商店 +1/+2。

### 2.6 购买校验

拒绝条件（任一成立）：

```cpp
bool UBrotatoShopSubsystem::CanBuy(const FShopSlot& S, FText& OutReason) const
{
    if (RunSubsystem->GetCurrency() < GetDisplayPrice(S))            return false;  // ①
    if (S.IsWeapon())
    {
        const UBrotatoWeaponData* W = Cast<UBrotatoWeaponData>(S.Data);
        const bool bHasSlot = RunSubsystem->HasWeaponSlotAvailable(W->Type);
        const bool bCanAutoCombine = RunSubsystem->HasWeaponId(W->WeaponId)
                                  && W->UpgradesInto != nullptr
                                  && (int32)Snapshot.MaxWeaponTier >= (int32)W->UpgradesInto->Tier;
        if (!bHasSlot && !bCanAutoCombine)                           return false;  // ②
        if (W->Type == EWeaponType::Melee  && HasRule(Rule_NoMeleeWeapons))  return false; // ③
        if (W->Type == EWeaponType::Ranged && HasRule(Rule_NoRangedWeapons)) return false;
        if ((int32)W->Tier < (int32)Snapshot.MinWeaponTier
         || (int32)W->Tier > (int32)Snapshot.MaxWeaponTier)          return false;  // ④
    }
    if (BuyDelayLeft > 0.f)                                          return false;  // ⑤ 0.05 s 防连点
    return true;
}
```

### 2.7 HP 商店与价格显示

```cpp
int32 UBrotatoShopSubsystem::GetDisplayPrice(const FShopSlot& S) const
{
    const int32 Base = FBrotatoEconomyMath::GetValue(
        S.WaveValue, S.Data->Value, /*bAffected*/true, S.IsWeapon(),
        Snapshot.ItemsPrice, GetSpecificPrice(S.Data->MyId),
        Snapshot.Inflation, EndlessFactor);
    return HasRule(Rule_HpShop) ? FBrotatoEconomyMath::GetHpShopPrice(Base) : Base;
}
```

`Rule.HpShop`（Demon）→ 货币改为 `stat_max_hp`；`GetCurrency()` / `RemoveCurrency()` 按此分流。

### 2.8 其他商店行为

| 行为 | 说明 |
|---|---|
| 默认焦点 | **第 2 格（index 1）** 若买得起；全买不起 → `RerollPrice == 0` 时聚焦 Reroll，否则 Go |
| `Rule.DestroyWeapons` | Arms Dealer：**进店即 `RemoveAllWeapons()`** |
| `Hook.UpgradeRandomWeapon` | 铁砧：进店升级一把随机可升级武器；**无可升级则 `armor += 2`** |
| `ShopEffectsChecked` | 保证上述只执行一次（进存档） |
| 精英预告 | `ElitesSpawn` 中第一个 `Wave > CurrentWave` → `ELITE_APPEARING` / `HORDE_APPEARING`；下一波即精英波 → 面板边框变白 |
| `EndlessButton` | 第 **19** 波显示 |
| 优惠券追踪 | `coupon_effect = nb × (abs(coupon.value)/100)`；`tracked["item_coupon"] += base_value × coupon_effect`（`base_value` 不含价格 buff） |

---

## 3. UI 实现（CommonUI）

### 3.1 页栈结构

```
ABrotatoHUD
└─ WBP_RootLayout (UCommonUserWidget)
   ├─ GameLayerStack     (UCommonActivatableWidgetStack)   HUD / ItemBoxUI / UpgradesUI
   ├─ MenuLayerStack     (UCommonActivatableWidgetStack)   Shop / TitleScreen / Selection
   ├─ ModalLayerStack    (UCommonActivatableWidgetStack)   PauseMenu / Confirm / Restart
   └─ OverlayLayer                                          ChallengeCompletedUI / InfoPopup
```

原作 `Menus.switch(from, to)` / `back()` / `reset()` → CommonUI 的 `PushWidget` / `PopWidget` / `ClearWidgets`。
**★ 页面切换是瞬时 hide/show，无 Tween** —— 不要给 Activatable 加进出动画。

### 3.2 HUD 元素与刷新源

```
UI (CanvasLayer)
├─ DamageVignette          受伤暗角（Material + Param）
├─ DimScreen               暗屏（alpha 0→0.5，1.0 s 线性）
├─ WaveClearedLabel        逐字 0.1 s/字 + 打字音 −10 dB
├─ HUD
│  ├─ LifeContainer
│  │  ├─ UILifeBar + LifeLabel    "cur / max"
│  │  ├─ UIXPBar   + LevelLabel   "LV.n"
│  │  ├─ UIGold                   GOLD_COLOR (118,255,118)/255，bounce ×1.1
│  │  └─ UIBonusGold              value==0 隐藏，bounce ×1.2，hover → "INFO_BONUS_GOLD"
│  ├─ WaveContainer
│  │  ├─ CurrentWaveLabel         "WAVE n"
│  │  └─ WaveTimerLabel           ceil(TimeLeft)；<6s 变红；有半波转换时变 deepskyblue
│  └─ ThingsToProcessContainer
│     ├─ Upgrades                 待处理升级图标（hover "INFO_LEVEL_UP"）
│     └─ Consumables              待处理宝箱图标（hover "INFO_ITEM_BOX"）
└─ PlayerLifeBar                  角色头顶血条（满血自动隐藏）
```

| UI | 刷新源 |
|---|---|
| UILifeBar / LifeLabel / PlayerLifeBar / DamageVignette | `OnHealthChanged`（Attribute 变更委托） |
| 血条颜色 | 属性/Tag 变更时 `UpdateColorFromEffects()`（事件驱动，非每帧轮询） |
| UIXPBar | `OnXpAdded` |
| LevelLabel | `OnLevelledUp` |
| UIGold / UIBonusGold | `OnGoldChanged` / `OnBonusGoldChanged` |
| WaveTimerLabel | `NativeTick`（显示值 = `CeilToInt(WaveTimeLeft)`，变化时才刷新文本） |
| 待处理列表 | `OnConsumableToProcessAdded` / `OnUpgradeToProcessAdded` |

**Safe Area**：HUD 四角元素内缩 ≥ **48 px**。UI 不加黑边，铺满全屏；DPI Curve 以 1080p 为基准。

### 3.3 属性面板

两个 Tab `{Primary, Secondary}`，手柄 `IA_StatsTabPrev/Next`（LT/RT）切换。

**PrimaryStats（16 项）**：`Icon | Label | Spacer | Value`

```
STAT_MAX_HP, STAT_HP_REGENERATION, STAT_LIFESTEAL, STAT_PERCENT_DAMAGE,
STAT_MELEE_DAMAGE, STAT_RANGED_DAMAGE, STAT_ELEMENTAL_DAMAGE, STAT_ATTACK_SPEED,
STAT_CRIT_CHANCE, STAT_ENGINEERING, STAT_RANGE, STAT_ARMOR,
STAT_DODGE, STAT_SPEED, STAT_LUCK, STAT_HARVESTING
```

- 值 = `int(Utils.get_stat(key))` → `UBrotatoStatQuery::GetStatDisplayValue`
- **封顶时显示 `"值 | 上限"`**（`Dodge > DodgeCap`；`MaxHp` 且 `HpCap < 9999`；`Speed` 且 `SpeedCap < 9999`）
- 颜色：`>0` 绿 / `<0` 红 / `==0` 白（label 与 value 同时着色）

**SecondaryStats（恰好 20 项）**：`Label | Value`，无图标

```
CONSUMABLE_HEAL, CHANCE_HEAL_ON_GOLD, XP_GAIN, PICKUP_RANGE,
ITEMS_PRICE(reverse=true 越低越绿), EXPLOSION_DAMAGE, EXPLOSION_SIZE,
BOUNCE, PIERCING, PIERCING_DAMAGE, DAMAGE_AGAINST_BOSSES,
BURNING_COOLDOWN_REDUCTION, BURNING_SPREAD, KNOCKBACK, CHANCE_DOUBLE_GOLD,
ITEM_BOX_GOLD, FREE_REROLLS, TREES, PCT_NUMBER_OF_ENEMIES, PCT_ENEMY_SPEED
```

Tooltip 规则见 D02 §9。定位：元素在屏幕右半则左移面板宽，下半则上移面板高 + DIST，否则 `y + 40`。

### 3.4 ★ 焦点管理（双输入统一模型）

```cpp
UCLASS()
class BROTATOUI_API UBrotatoFocusManager : public UCommonUserWidget
{
    GENERATED_BODY()
public:
    /** ★ 鼠标 hover → 抢焦点 + 显示 Popup（把键鼠与手柄统一成"焦点"模型） */
    void OnElementHovered(UCommonButtonBase* Elem);
    void OnElementFocused(UCommonButtonBase* Elem);
    void OnElementPressed(UCommonButtonBase* Elem);   // 模态：屏蔽所有 hover/focus 变化

private:
    UPROPERTY() UCommonButtonBase* PressedElement = nullptr;   // != null 时模态
};
```

要点：
- `PressedElement != nullptr` 时**屏蔽所有 hover/focus 变化**
- `StatsContainer.SetNavigationRule(Right, GoButton)`
- 对 `FocusMode == None` 的按钮，hover 时临时升级为 `Focusable` 再 `SetFocus()`
- 默认焦点必须始终有落点（验收 U01）

### 3.5 升级与宝箱面板

**UpgradesUI**：固定 4 个 `UpgradeUI` + StatsContainer + RerollButton + StatPopup。
- 默认焦点**第 2 个**（`OnActivated` 时再抢一次）
- 选择 → `Deactivate()` → 广播 `OnUpgradeSelected` → `LinkedStats->MarkDirty()`
- 多次升级用**串行队列**处理（对齐原作 `yield`）：

```cpp
void UBrotatoFlowSubsystem::ProcessPendingUpgrades()
{
    if (PendingUpgrades.Num() == 0) { ProcessPendingItemBoxes(); return; }
    const int32 Level = PendingUpgrades[0]; PendingUpgrades.RemoveAt(0);
    UpgradesUI->Show(ItemService->GetUpgrades(Level, 4, LastUpgradeIds));
    UpgradesUI->OnUpgradeSelected.AddWeakLambda(this, [this](auto*)
    { ProcessPendingUpgrades(); });   // 递归串行
}
```

升级选项去重：与本级上一次刷新结果按 `upgrade_id` 去重，最多重抽 **50** 次。

升级 Tier 硬性覆写（`GetTierFromWave` 传入 `level`）：
`level == 5 → II`；`level ∈ {10,15,20} → III`；其他 `level % 5 == 0 → IV`。

升级固定收益：
```cpp
void OnLevelledUp(int32 Level)
{
    PendingUpgrades.Add(Level);                                   // 入队，波末处理
    RunSubsystem->AddStatPermanent(Attr_MaxHp, 1);                // ★ Instant GE
    for (const auto& E : HookRegistry->Get(Hook_StatsOnLevelUp))  // Apprentice
        RunSubsystem->AddStatPermanent(E.AttributeTag, E.Value);
    RunSubsystem->ApplyHeal(1);                                   // 立即回 1 血
}
```

升级池 64 条数值（16 属性 × 4 档）见 `Docs/02 §6.5`。
**未启用残留（复刻时忽略）**：`accuracy` / `crit_damage` / `damage` / `weapon_slot` 的升级条目。

**ItemBoxUI**：`ItemDescription` + `TakeButton` / `DiscardButton` + StatsContainer。
- `DiscardButton.Text = Loc("MENU_RECYCLE") + " (+" + GetRecyclingValue(...) + ")"`
- 默认焦点 `TakeButton`
- `consumable_legendary_item_box` 强制 `FixedTier = Legendary`
- 弃取 → `AddGold(GetRecyclingValue(CurrentWave, ItemData->Value))`

### 3.6 输入映射

| 动作 | 绑定 |
|---|---|
| 移动 | 方向键 + WASD/ZQSD/IJKL + DPad + 左摇杆 |
| 手动瞄准 | 鼠标位置 / 右摇杆（axis 2/3） |
| 属性面板切页 | LT / RT（JoyBtn 4/6、5/7） |
| 暂停 | ESC / Start（JoyBtn 11） |
| 取消 | ESC / JoyBtn1 |

```cpp
static constexpr float MinMoveDist = 20.f;           // 鼠标移动模式死区
static constexpr float ManualAimMouseDist = 200.f;   // 手柄手动瞄准光标距离
static constexpr float DpadEchoDelay = 0.2f;         // D-Pad 长按重复

/** 手动瞄准判定（progress_data.gd:824） */
bool IsManualAim() const
{
    const bool bTrying = bMouseLeftDown || (bUsingGamepad && RightStick != FVector2D::ZeroVector);
    return Settings.bManualAim || (Settings.bManualAimOnMousePress && bTrying);
}
```

`Rule.CantStopMoving`：静止时强制沿上一 Tick 方向（首次随机 `RandRange(-PI, PI)`）。
手柄检测：按钮按下或 `|axis| > 0.25` → `bUsingGamepad = true`；键盘输入 → false。
光标：默认 `T_Cursor`（热点 3,3）；手动瞄准 `T_ManualCursor`（热点 35,35）。

### 3.7 字体与本地化

| 项 | 值 |
|---|---|
| `SMALLEST_FONT_BASE_SIZE` | **20** |
| 实际字号 | `20 × Settings.FontSize` |
| 内联图标宽 | `20 × FontSize` |
| 字号档位 | 20 / 24 / 28 / 40（结算） |
| 语言 | 13 语言（**先中英**）；`translations.csv` 已丢失，需重建 |

`UBrotatoTextSubsystem::Text(Key, Args, Signs)` 负责格式化与着色（正向 `#00ff00` / 负向 `red` / 次要 `#555555`），`DT_TextFormatRules` 记录需要 `+` / `%` 的参数下标。

---

## 4. 存档实现

### 4.1 存档结构

```cpp
UCLASS()
class BROTATOSAVE_API UBrotatoSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 SaveVersion = CurrentSaveVersion;

    // 12 个数据块（对应原作每行一个 JSON）
    UPROPERTY() TArray<int32>  ZonesUnlocked;
    UPROPERTY() TArray<FName>  CharactersUnlocked;      // my_id
    UPROPERTY() TArray<FName>  UpgradesUnlocked;        // ★ upgrade_id
    UPROPERTY() TArray<FName>  ConsumablesUnlocked;     // my_id
    UPROPERTY() TArray<FName>  WeaponsUnlocked;         // ★ weapon_id（一次解锁 4 阶）
    UPROPERTY() TArray<FName>  ItemsUnlocked;           // my_id
    UPROPERTY() TArray<FName>  ChallengesCompleted;     // my_id = Steam 成就 ID
    UPROPERTY() TArray<FCharacterDifficultyInfo> DifficultiesUnlocked;
    UPROPERTY() FBrotatoSettings Settings;
    UPROPERTY() TMap<FName, int64> Data;                // 统计
    UPROPERTY() FBrotatoRunState RunState;              // 续关快照
    UPROPERTY() TArray<FName>  InactiveMods;            // 保留字段

    static constexpr int32 CurrentSaveVersion = 1;
};
```

### 4.2 三级备份

```cpp
// 路径：<SavedDir>/SaveGames/<SteamId 或 "user">/
static const TCHAR* SlotMain   = TEXT("save");
static const TCHAR* SlotLatest = TEXT("save_latest");
static const TCHAR* SlotStable = TEXT("save_stable");
static constexpr int32 MaxSavesWithoutBackup = 5;

void UBrotatoSaveSubsystem::Save()
{
    UGameplayStatics::SaveGameToSlot(Save, SlotMain, 0);
    if (++SavesSinceBackup >= MaxSavesWithoutBackup)
    { CopySlot(SlotMain, SlotLatest); SavesSinceBackup = 0; }
}

EBrotatoLoadResult UBrotatoSaveSubsystem::Load()
{
    if (TryLoad(SlotMain))   return EBrotatoLoadResult::Ok;
    if (TryLoad(SlotLatest)) return EBrotatoLoadResult::CorruptedSave;        // 提示并用 latest
    if (TryLoad(SlotStable)) return EBrotatoLoadResult::CorruptedSaveLatest;
    return RecreateFromAchievements();   // 无 Steam → CorruptedAllSavesNoSteam，且不写档
}
```

| 规则 | 说明 |
|---|---|
| 解锁列表 | `AppendWithoutDuplicates()`，**只增不减** |
| `Settings` / `Data` | 用 `MergeWithDefaults()` 合并 → 新增设置项自动带默认值 |
| 损坏恢复 | 级联降级；全坏且有 Steam → 遍历 challenges 用成就重建，`Data.*` 从 Steam Stat 拉回 |
| **绝不写回损坏档** | 恢复失败时只读不写 |

### 4.3 保存触发点

`CompleteChallenge` / 难度选择确认 / `CleanUpRoom`（波末）/ 设置面板关闭 / `Shop.OnActivated` / `Shop.OnPaused` / `Shop.GoButton`（`SaveRunState`）/ `ApplyRunWon` 与 `OnPlayerDied`（`ResetRunState`）。

### 4.4 `FBrotatoRunState` 字段全表

```
bHasRunState, EnemyScaling{health,damage,speed}, NbOfWaves, CurrentZone,
CurrentLevel, CurrentXp, MaxWeapons, CurrentWave, CurrentDifficulty,
Gold, BonusGold, ElitesSpawn[], bShopEffectsChecked,
Weapons[{MyId, InstanceId, TrackedValue, DmgDealtLastWave, NbShotsTaken}],
Items[{MyId, InstanceId}], Effects（分三类序列化，见下）,
ChallengesCompletedThisRun[], CharacterId, StartingWeaponId,
ActiveSetEffects[], UniqueEffects[], AdditionalWeaponEffects[],
TierIVWeaponEffects[], TierIWeaponEffects[],
LockedShopItems[{MyId, Wave}], CurrentBackground, AppearancesDisplayed[], ActiveSets{},
DifficultyUnlocked, bMaxEndlessWaveRecordBeaten, bIsEndlessRun,
ChalHoarderValue/Completed, ChalRecyclingValue/Completed/Current,
ChalHungryValue/Completed, ConsumablesPickedUpThisRun,
ShopItems[{MyId, Wave}], RerollPrice, LastRerollPrice, InitialFreeRerolls, FreeRerolls,
TrackedItemEffects{}, ★ RunSeed（新增，原作无，用于确定性复现）
```

序列化规则：

| 对象 | 规则 |
|---|---|
| `Weapons` / `Items` | 只存 `MyId` + `InstanceId`；读档 `ItemService->Resolve()` 还原（道具先查 items 再查 characters） |
| `CurrentBackground` | `Name.ToLower()` |
| `LockedShopItems` / `ShopItems` | `[MyId, Wave]` |
| `ActiveSetEffects` | `[SetId, EffectDef]`；读档按 `EffectId` 查 Kind 再重建 |
| `Effects` 分三类 | ① `EffectKeysFullSerialization`（`burn_chance`/`structures`/`explode_on_*`/`convert_stats_*`）走完整序列化；② `EffectKeysWithWeaponStats`（`projectiles_on_death`/`alien_eyes`）的 WeaponStats 单独序列化；③ 其余原样存 |
| **★ 每个 Effect 实例 GUID** | 必须序列化，避免同效果产生两个对象导致 `Unapply` 失效 |
| `FActiveGameplayEffectHandle` | **不序列化**，读档后按 D03 §7.2 顺序重放重建 |
| 版本容错 | 读档用 `Has(key)` 判断，缺字段用默认值 |

### 4.5 版本迁移

```cpp
bool UBrotatoSaveSubsystem::Migrate(UBrotatoSaveGame* S)
{
    while (S->SaveVersion < UBrotatoSaveGame::CurrentSaveVersion)
    {
        switch (S->SaveVersion)
        {
        // case 1: Migrate_1_to_2(S); break;
        default: return false;      // 未知版本 → 视为损坏，走三级备份
        }
        ++S->SaveVersion;
    }
    return true;
}
```

| 场景 | 方案 |
|---|---|
| 物品/武器 **id 重命名** | `DT_IdRemap`（`OldId → NewId`），反序列化解析 `MyId` 前统一过一遍 |
| 物品**被删除** | 映射到 `NAME_None` → 静默丢弃 |
| 设置项**语义变更** | 在 `Migrate_N_to_N+1` 显式转换 |
| `RunState` **结构变更** | **直接丢弃 RunState**（易失数据），保留解锁与统计 |
| 新增字段 | 继续用 `MergeWithDefaults`，无需迁移 |

**铁律**：① `SaveVersion` 与游戏 `Version` **分离**；② 迁移**只增不减**；③ 迁移失败走三级备份，**绝不写回损坏档**；④ 每次改结构必须 `++CurrentSaveVersion` + 写 Migrate 分支 + 补旧档回归测试。

### 4.6 统计 `Data`

```cpp
void UBrotatoProgressSubsystem::AddData(FName Key, int64 Delta = 1)
{
    Data.FindOrAdd(Key) += Delta;
    // Steam（P2）：SetStatInt(Key, Value); StoreStats();
}
```

键：`enemies_killed`、`materials_collected`（仅被玩家吸取的）、`trees_killed`。

---

## 5. 元进度与挑战

### 5.1 `ChallengeData`

```cpp
UENUM() enum class EChallengeRewardType : uint8
{ Item = 0, Weapon = 1, Zone = 2, StartingWeapon = 3, Consumable = 4, Upgrade = 5, Character = 6, Difficulty = 7 };

UCLASS() class UBrotatoChallengeData : public UBrotatoItemParentData
{
    UPROPERTY(EditDefaultsOnly) FName Description;          // 本地化键
    UPROPERTY(EditDefaultsOnly) EChallengeRewardType RewardType;
    UPROPERTY(EditDefaultsOnly) TSoftObjectPtr<UBrotatoItemParentData> Reward;
    UPROPERTY(EditDefaultsOnly) int32 Number = 0;           // 同名挑战序号
    UPROPERTY(EditDefaultsOnly) FGameplayTag StatTag;       // 非空 → 属性型挑战
    UPROPERTY(EditDefaultsOnly) TArray<FString> AdditionalArgs;
};
```

### 5.2 完成与解锁

```cpp
void UBrotatoChallengeSubsystem::CompleteChallenge(FName Id)
{
    if (RunSubsystem->IsTesting()) return;
    // Steam: SetAchievement(Id); StoreStats();
    if (Progress->ChallengesCompleted.Contains(Id)) return;
    Progress->ChallengesCompleted.Add(Id);
    UnlockReward(GetChallenge(Id));
    Progress->Save();
    OnChallengeCompleted.Broadcast(GetChallenge(Id));    // 带队列的弹窗
}

void UBrotatoChallengeSubsystem::UnlockReward(const UBrotatoChallengeData* C)
{
    switch (C->RewardType)
    {
    case Character:       AppendUnique(Progress->CharactersUnlocked,   C->Reward->MyId);     break;
    case Item:            AppendUnique(Progress->ItemsUnlocked,        C->Reward->MyId);     break;
    case Weapon:
    case StartingWeapon:  AppendUnique(Progress->WeaponsUnlocked,      WeaponIdOf(C->Reward)); break;  // ★ weapon_id
    case Zone:            AppendUnique(Progress->ZonesUnlocked,        ZoneIdOf(C->Reward));  break;
    case Consumable:      AppendUnique(Progress->ConsumablesUnlocked,  C->Reward->MyId);     break;
    case Upgrade:         AppendUnique(Progress->UpgradesUnlocked,     UpgradeIdOf(C->Reward)); break;
    case Difficulty:      /* 仅 UI 展示，ProgressionUI 主动过滤 */                            break;
    }
}
```

### 5.3 5 种完成条件与判定时机

| 类型 | 判定位置 | 说明 |
|---|---|---|
| **A. 角色通关** | `ApplyRunWon()` | `CompleteChallenge("chal_" + 角色名)` |
| **B. 危险度通关** | `ApplyRunWon()` | `CompleteChallenge("chal_difficulty_" + N)` |
| **C. 属性阈值** | `CheckStatChallenges()` —— **属性变化时** | `value>0 → >=`；`<0 → <=`；`==0 → ==`。特例 `chal_advanced_technology` 需同时 `structures.Num() >= AdditionalArgs[0]` |
| **D. 累计统计** | `CheckCountedChallenges()` —— **每波结束** | `CHAL_SURVIVOR ← enemies_killed`、`CHAL_GATHERER ← materials_collected`、`CHAL_LUMBERJACK ← trees_killed` |
| **E. 局内事件** | 各处硬编码 | 见下表 |

E 类明细：

| 挑战 | 触发 |
|---|---|
| `chal_student` | `LevelUp()` 里 `Level >= 20` |
| `chal_hoarder` | `AddGold` 里 `Gold >= 3000` |
| `chal_recycling` | `AddRecycled()` 累计 **12** |
| `chal_hungry` | `OnConsumablePickedUp` 累计 **20** |
| `chal_baited` | 持有 **2** 个 `item_bait` |
| `chal_scavenger` | **10** 种不同 Common 道具 |
| `chal_bourgeoisie` | **3** 把 Legendary 武器 |
| `chal_reckless` | **波末** HP == 1 |
| `chal_forest` | **波末**场上树 ≥ **10** |
| `chal_rookie` | `OnPlayerDied`（首次死亡） |
| `chal_fireworks` | 单次爆炸击杀 ≥ **15** |
| `chal_giant_slayer` | Boss `Die()` 时 `ElapsedSeconds <= chal.value` |
| `chal_medicine` | 单波治疗 **200** |
| `chal_turrets` | 建筑数 **5** |

### 5.4 危险度进度（★ 全局共享解锁）

```cpp
void UBrotatoFlowSubsystem::ApplyRunWon()
{
    const int32 Cur = RunSubsystem->GetCurrentDifficulty();
    CompleteChallenge(FName(*FString::Printf(TEXT("chal_%s"), *CharacterName)));
    if (Progress->GetMaxSelectableDifficulty() < Cur + 1 && Cur + 1 <= 5)
        RunSubsystem->SetDifficultyUnlocked(Cur + 1);
    CompleteChallenge(FName(*FString::Printf(TEXT("chal_difficulty_%d"), Cur)));

    // ★ 所有角色 / 所有区域：max_selectable_difficulty = clamp(Cur+1, 旧值, 5)
    Progress->UnlockDifficultyGlobally(Cur + 1);

    Progress->SetMaxDifficultyBeaten(CharacterId, ZoneId, Cur, Wave, Acc.H, Acc.D, Acc.S, false);
    Progress->ResetRunState();
}
```

数据结构：
```
CharacterDifficultyInfo { CharacterId, ZonesDifficultyInfo[] }
ZoneDifficultyInfo      { ZoneId, DifficultySelectedValue（记忆上次选择）,
                          MaxSelectableDifficulty, MaxDifficultyBeaten:DifficultyScore,
                          MaxEndlessWaveBeaten:DifficultyScore }
DifficultyScore         { DifficultyValue(-1), WaveNumber(-1), EnemyHealth/Damage/Speed(1.0) }
```

**最高分比较** `IsDifficultyAbove`：先比 `DifficultyValue`，再比 `Accessibility = (H+D+S)×100`，再比 `WaveNumber`。
`OverallMaxSelectableDifficulty`：反序列化取全局最大值并回填所有条目（防旧档丢危险度）。

### 5.5 无尽 UI 展示

| 项 | 规则 |
|---|---|
| `MaxEndlessWaveBeaten` 写入 | `CleanUpRoom()` 中 `CurrentWave >= 20` 时 `SetInfo(..., bIsEndless=true, bReplaceOnlyIfAbove=true)` |
| 比较优先级 | `HighestWave` → 先比 `WaveNumber`；`HighestDifficulty` → 先比 `DifficultyValue`，再 Accessibility，再 Wave |
| 角色选择界面 | `WaveNumber > 20` 才显示"最高无尽波次"徽标 |
| `difficulty_6..10` | 仅渲染色块与文字，**不可选中** |
| 无尽结算标题 | `CurrentWave > 20` 死亡 → `RUN_WON`，下方显示"到达第 N 波" |

### 5.6 死亡与胜利

```cpp
void UBrotatoFlowSubsystem::OnPlayerDied()
{
    PlayerLifeBar->Hide();
    // ★ 无尽超 20 波死亡算胜利
    if (CurrentWave <= BrotatoConst::NbOfWaves) bRunLost = true; else bRunWon = true;
    CleanUpRoom();
    StartEndWaveTimer(2.5f);                 // ★ 2.5 s（FTimerManager）
    Progress->ResetRunState();
    CompleteChallenge("chal_rookie");
}
```

---

## 6. 双 ID 体系（易错点）

| 资源 | 目录 | `MyId` | 分组 ID | 解锁字段 |
|---|---|---|---|---|
| 角色 | `items/characters/<name>/` | `character_<name>` | — | `CharactersUnlocked`(MyId) |
| 道具（179） | `items/all/<name>/` | `item_<name>` | — | `ItemsUnlocked`(MyId) |
| 武器（185 阶位） | `weapons/{melee,ranged}/<name>/<1-4>/` | `weapon_<name>_<1..4>` | **`WeaponId`** | `WeaponsUnlocked`(**WeaponId**) |
| 升级（64） | `items/upgrades/<stat>/<1-4>/` | `upgrade_<stat>_<1..4>` | **`UpgradeId`** | `UpgradesUnlocked`(**UpgradeId**) |
| 消耗品（3） | `items/consumables/<name>/` | `consumable_<name>` | — | `ConsumablesUnlocked` |
| 挑战（88） | `challenges/<name>_data.tres` | `chal_<name>` / `unlock_difficulty_<N>` | — | `ChallengesCompleted` = Steam 成就 ID |
| 难度（6+5） | `items/difficulties/<N>/` | `difficulty_<N>` | — | `MaxSelectableDifficulty`（数值） |
| 区域 | `zones/zone_<N>/` | **int（0 基）** | — | `ZonesUnlocked`(int) |

> **★ 命名不一致警告（导入器必须处理）**：
> - `weapons/melee/cactus_mace/` 的 `MyId = "weapon_cacti_club_1"`，`WeaponId = "weapon_cacti_club"` —— **目录名 ≠ id**
> - `knuckles` 的 `WeaponId = ""` → **不进商店池**
> - tier 1 文件名通常不带后缀，但 `MyId` 带 `_1`
> - 角色的 `Tier` 字段运行时被 UI 改写成"已通关危险度"，**不是真 tier**

---

## 7. 设置项全表

### 7.1 顶层

| key | 默认 |
|---|---|
| `Version` | 每次 `ResetRunState` 写入当前版本 |
| `bEndlessModeToggled` | false |

### 7.2 General

| key | 默认 | 生效方式 |
|---|---|---|
| `Volume.Master` | **0.5** | Submix `Master` 音量 = `LinearToDb(v)` |
| `Volume.Sound` | **0.75** | Submix `Sound` |
| `Volume.Music` | **0.25** | Submix `Music` |
| `bFullscreen` | **true** | `r.SetRes` / WindowMode |
| `bScreenshake` | **true** | 屏震开关 |
| `Language` | `"en"` | `SetCurrentCulture`；启动时被 Steam 语言覆盖 |
| `Background` | 0 | 0=随机；调 `ResetBackground()` |
| `bVisualEffects` | true | 特效开关 |
| `bDamageDisplay` | **true** | 飘字开关 |
| `bOptimizeEndWaves` | false | 波末不做金币吸附动画 |
| `bLimitFps` | false | `t.MaxFPS = 60 : 0` |
| `bMuteOnFocusLost` | false | |
| `bPauseOnFocusLost` | **true** | |

### 7.3 Gameplay

| key | 默认 | 说明 |
|---|---|---|
| `bMouseOnly` | false | 纯鼠标操作 |
| `bManualAim` | false | 手动瞄准常开 |
| `bManualAimOnMousePress` | false | 按住鼠标 / 推右摇杆时才手动瞄准 |
| `bHpBarOnCharacter` / `bHpBarOnBosses` | true / true | 头顶血条（信号即时生效） |
| `bKeepLock` | true | 进下一波保留商店锁定 |
| `EndlessScoreStoring` | 0 | `{HighestWave=0, HighestDifficulty=1}` |
| `EnemyScaling.Health/Damage/Speed` | 1.0 / 1.0 / 1.0 | **辅助难度**，开局快照到 `CurrentRunAccessibility` |
| `ExplosionOpacity` / `ProjectileOpacity` | 1.0 / 1.0 | |
| `FontSize` | 1.0 | `20 × v` |
| `bCharacterHighlighting` / `bWeaponHighlighting` / `bProjectileHighlighting` | false | 描边，即时应用 |
| `bAltGoldSounds` | false | 切换拾取音效 |
| `bDarkenScreen` | true | 低血暗角；关闭时 multiplier 固定 0.8 |

`DefaultButton` → `Settings.Merge(InitGameplayOptions(), /*bOverwrite*/true)` + 回填 UI。
`ApplySettings()`（仅启动引导场景调用一次）：三条 Submix 音量 → Culture → 窗口模式 → 字号 → `ResetBackground()` → FPS 限制。

暗角：`Multiplier = clamp(Hp/MaxHp, 0.3, 0.8)`；关闭"暗化屏幕"时固定 0.8。

---

## 8. 本章验收清单

| # | 项 | 标准 |
|---|---|---|
| U01 | 完整流程 | 标题→角色→武器→难度→W1→商店→W2 无卡死，焦点始终有落点 |
| U02 | 暂停 | ESC 暂停，敌人/计时器全停，SFX 队列仍工作 |
| U03 | 失焦暂停 | `bPauseOnFocusLost=true` 时自动暂停 |
| U04 | 手柄 | 全流程可操作；D-Pad 长按 0.2 s 后连续导航 |
| U05 | 鼠标焦点 | 悬停商店格 → 该格抢焦点 + 弹出 ItemPopup |
| U06 | 锁定 | 锁 2 格后重掷 → 只刷新另 2 格，锁定格价格不变 |
| U07 | 属性封顶 | dodge 堆到 80 → 显示 `"80 | 60"` |
| U08 | Tooltip | armor=10 → 文案显示减伤 40% |
| U09 | 备份 | 写 5 次后 `save_latest` 与 `save` 一致 |
| U10 | 损坏恢复 | 破坏 `save` → 提示 `CorruptedSave` 且从 latest 恢复 |
| U11 | 续关 | 商店中退出 → Continue → 回到同一商店（含锁定项、重掷价、免费次数） |
| U12 | 危险度全局 | 角色 A 在 D0 通关 → 角色 B 也能选 D1 |
| U13 | 挑战弹窗 | 击杀累计达 300 → 弹窗 + 解锁，弹窗期间新挑战排队 |
| U14 | 字号 | 改字号 → 所有文字与内联图标同步缩放 |
| U15 | 迁移成功 | 旧版存档正常升级，解锁与统计保留 |
| U16 | 迁移失败 | 未知版本 → 走三级备份，不写回损坏档 |
| U17 | 暂停音频 | 暂停期间连续触发 50 个音效 → 仍能播放（Submix 不暂停），并发受 `USoundConcurrency` 限制；Niagara 冻结 |
| **U19** | ★ 暂停与 GE 计时 | 暂停 5 s 后恢复：无敌窗口 / 燃烧 / 武器冷却的剩余时长**不减少**（GE 走 World 时间） |
| U18 | 属性面板 | SecondaryStats 恰好显示 20 项 |
| E02 | 定价 | value=70, W=15，无加成 → **190** |
| E03 | 重掷 | W=10 首次 15，连续 4 次付费 → 15/20/25/30 |
| E04 | 回收 | value=100, W=10, recycling_gains=35 → **126** |
| E06 | bonus_gold | `bonus_gold=5` → 下波前 5 个材料 value=2，第 6 个起 value=1 |
| E01T | Tier 分布 | W=10, Luck=0，固定 seed 10000 次 → P(IV)≈0.0069, P(III)≈0.133, P(II)≈0.400 |
