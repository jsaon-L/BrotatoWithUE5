# 土豆兄弟 · GAS 复刻版（PotatoRogue）

使用 **UE5 C++ + Gameplay Ability System (GAS)** 复刻《土豆兄弟 / Brotato》核心玩法的 2D 俯视角肉鸽游戏。

> 整套代码**零 Content 资源即可运行**：默认物品、武器、敌人全部由 C++ 在运行时构造（`PRDefaultContent`），
> 角色/弹丸/掉落物使用引擎自带 `BasicShapes` 网格，UI 使用纯 Slate 构建（不需要任何 UMG Widget 蓝图）。

---

## 1. 玩法闭环（对标土豆兄弟）

```
开始 ──► 波次战斗（限时刷怪、自动攻击）──► 波次结束（清怪、收获结算金币）
        ▲                                        │
        │                                        ▼
        └────────── 选择强化（升级四选一） ◄── 商店（买物品/武器、重随）
                                     │
                                     └──► 死亡 → 结算 → 重新开始
```

| 系统 | 对应土豆兄弟规则 | 实现位置 |
|---|---|---|
| 自动攻击武器（最多 6 把、同名叠加） | ✅ | `UPRWeaponComponent` / `UPRGameplayAbility`（攻速驱动循环） |
| 属性面板（伤害/攻速/暴击/护甲/闪避/幸运/收获…） | ✅ | `UPRAttributeSet`（GAS Attribute） |
| 护甲减伤 `100/(100+护甲)`、闪避上限 60% | ✅ | `UPRDamageExecution`（GameplayEffect 执行计算） |
| 升级四选一属性强化 | ✅ | `UPRLevelUpWidget` + `PRGameplayEffects::ApplyStatMod` |
| 波次限时、出怪密度随波次增长 | ✅ | `APRGameState`（波次管理器） |
| 波次结束进商店、购买/重随/开下波 | ✅ | `APRGameState::BuildShopOffers` / `UPRShopWidget` |
| 金币 / 经验掉落、拾取范围磁吸 | ✅ | `APRPickup` |
| 收获（Harvesting）波次结算金币 | ✅ | `APRGameState::EndWave` |
| 幸运影响掉落与商店品质权重 | ✅ | `APRGameState::NotifyEnemyKilled` / `MakeRandomOffer` |
| 冲刺 + 无敌帧 | ✅ | `UPRGameplayAbility_Dash`（`State.Dashing` Tag 免疫） |

---

## 2. 搭建与运行

### 环境
- Unreal Engine **5.4**（5.3 / 5.5 亦可，需调整 `BuildSettingsVersion` 与 `EngineIncludeOrderVersion`）
- 已启用插件：**Gameplay Abilities**（`.uproject` 中已声明）

### 步骤
1. 打开 `PotatoRogue/PotatoRogue.uproject`（首次会提示编译模块，选 **Yes**）。
   - 若提示缺失模块：右键 `.uproject` → **Generate Visual Studio project files**，再编译 `PotatoRogueEditor` 目标。
2. 编辑器中 **File → New Level → Empty Level**（或 Basic），保存为 `Content/Maps/PR_Arena`。
3. **World Settings → GameMode Override** 不用设置（`DefaultEngine.ini` 已把全局默认模式设为 `PRGameMode`）。
4. 想要固定出生点：拖一个 **PlayerStart** 到原点（没有也能跑，会在原点生成）。
5. **Play** 即可游玩。

### 操作
| 按键 | 动作 |
|---|---|
| `WASD` / 方向键 / 左摇杆 | 移动 |
| `Space` / 手柄 A | 冲刺（带无敌帧） |
| `Tab` | （预留）属性面板 |

武器自动索敌、自动开火，无需射击输入——与土豆兄弟一致。

---

## 3. 工程结构

```
PotatoRogue/
├── PotatoRogue.uproject
├── Config/                       # 引擎/输入/项目配置
└── Source/PotatoRogue/
    │  ── GAS 核心 ──
    ├── PRGameplayTags.h/.cpp        原生 Gameplay Tag（Ability/State/Data/Cooldown）
    ├── PRAttributeSet.h/.cpp        18 项属性 + Meta 属性（IncomingDamage/Heal）
    ├── PRAbilitySystemComponent.h   ASC 子类（Spec 构造/属性读写工具）
    ├── PRGameplayAbility.h/.cpp     技能基类：自动循环攻击、攻速换算
    ├── PRWeaponAbilities.h/.cpp     远程射击 / 近战挥击 / 冲刺
    ├── PRDamageExecution.h/.cpp     伤害执行计算 + UPRDamageEffect / UPRHealEffect
    ├── PRGameplayEffectLib.h/.cpp   伤害/治疗/属性修改的统一 GE 入口
    │  ── 数据 ──
    ├── PRGameTypes.h/.cpp           属性枚举、稀有度、商店报价、文本工具
    ├── PRDataAssets.h/.cpp          UPRItemDefinition / UPRWeaponDefinition / UPREnemyDefinition
    ├── PRDefaultContent.h/.cpp      运行时生成的默认物品/武器/敌人（免美术资源）
    │  ── 角色与战斗 ──
    ├── PRCharacterBase.h/.cpp       GAS 装配、自动索敌、死亡
    ├── PRPlayerCharacter.h/.cpp     正交俯视相机、移动输入、冲刺、武器挂件
    ├── PREnemyCharacter.h/.cpp      追击 AI、接触伤害、波次属性缩放、血条
    ├── PRProjectile.h/.cpp          弹丸（GAS 伤害结算）
    ├── PRWeaponComponent.h/.cpp     武器槽、技能授予、叠加、DPS 估算
    ├── PRPickup.h/.cpp              金币/经验/治疗掉落 + 磁吸
    │  ── 流程 ──
    ├── PRGameMode.h/.cpp            数据池注入、初始武器、波次推进、重开
    ├── PRGameState.h/.cpp           波次管理、敌人注册表、商店生成、难度曲线
    ├── PRPlayerState.h/.cpp         金币/等级/经验/持有物品
    ├── PRPlayerController.h/.cpp    UI 流程、购买/升级/重随逻辑
    │  ── UI（纯 Slate）──
    ├── PRHUDWidget.h/.cpp           血条/经验条/波次/金币
    ├── PRShopWidget.h/.cpp          商店
    ├── PRMenuWidgets.h/.cpp         升级四选一 + 结算
    └── PREnemyHealthBar.h/.cpp      敌人头顶血条
```

---

## 4. GAS 数据流（伤害为例）

```
UPRGameplayAbility_RangedAttack::PerformAttack
   │  面板伤害 = 武器基础伤害 × 叠加 × 远程倍率
   ▼
APRProjectile::OnOverlap
   ▼
PRGameplayEffects::ApplyDamage(SourceASC, TargetASC, BaseDamage)
   │  生成 UPRDamageEffect Spec，SetByCaller(Data.Damage) = BaseDamage
   ▼
UPRDamageExecution::Execute
   │  ① 闪避判定（Dodge，上限 60%）
   │  ② 全局伤害倍率（DamageMultiplier）
   │  ③ 暴击判定（CritChance / CritMultiplier）
   │  ④ 护甲减伤（ArmorToMultiplier）
   ▼
UPRAttributeSet::PostGameplayEffectExecute
      IncomingDamage → Health 扣减 → 死亡广播 HandleDeath()
```

**技能循环**：武器技能被 `UPRWeaponComponent::AddWeapon` 以 `SourceObject = UPRWeaponInstance`
授予 ASC 后自动激活，`UPRGameplayAbility` 通过 `AbilityTask_WaitDelay` 以
`BaseCooldown / 攻速倍率` 的间隔循环执行 `PerformAttack()`。

**属性修改**：物品购买 → `PRGameplayEffects::ApplyStatMods` → 动态构造瞬时 `UGameplayEffect`
加算到对应 Attribute（百分比属性按百分点换算为小数增量）。

---

## 5. 常用扩展点

| 想做的事 | 改哪里 |
|---|---|
| 加新武器 | `PRDefaultContent::CreateWeapons` 加一条，或新建 `UPRWeaponDefinition` DataAsset 填入 `GameState->WeaponPool` |
| 加新物品/属性 | `EPRStat` 加枚举 → `UPRAttributeSet` 加属性 → `StatToAttribute` 加映射 → `PRDefaultContent::CreateItems` |
| 加新敌人 | `UPREnemyDefinition` 新建；特殊行为可继承 `APREnemyCharacter` 并重写 `Tick` |
| 换成 Paper2D/精灵 | 给 `BodyMesh` 换 `UPaperSpriteComponent`，或在蓝图子类里替换 Mesh |
| 加音效/特效 | `PerformAttack` / `NotifyDamageTaken` / `HandleDeath` 中的 `TODO` 处挂 GameplayCue |
| 联机 | ASC 已按单机配置（`SetIsReplicated(false)`）；联机需打开复制并启用 `NetExecutionPolicy` 预测 |
| 手柄震动/键位 | `Config/DefaultInput.ini` |

---

## 6. 数值速查（默认值）

- 玩家：100 HP、移速 520、暴击 5%、暴伤 150%、拾取 250
- 护甲公式：`减伤 = 100/(100+护甲)`（负护甲对称放大）；闪避上限 60%
- 波次时长：`min(20 + 4×波次, 60)` 秒；出怪间隔 `max(1.4 - 0.12×波次, 0.28)` 秒
- 敌人成长：`血量 ×(1 + 0.35×(波次-1) + 0.02×波次²)`，`伤害 ×(1 + 0.18×(波次-1))`
- 经验曲线：`10 + 4×等级 + 0.8×等级²`
- 武器叠加：同名武器每把 `+35%` 基础伤害，武器上限 6 把

---

## 7. 已知取舍 / TODO

- [ ] 未接入 GameplayCue（音效/特效留了 `TODO` 挂点）
- [ ] 未做暂停菜单 / 属性面板（`Tab` 预留）
- [ ] 未做工程塔类武器（`Engineering` 属性已就绪）
- [ ] 商店暂不支持出售武器（`UPRWeaponComponent::RemoveWeapon` 已实现，UI 未接）
- [ ] 复制/联机未实现（纯单机设计）

License: MIT
