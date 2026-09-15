# UE5 复刻《土豆兄弟 / Brotato》技术路线

> 目标：用 UE5 做一款 Brotato-like（竞技场波次生存 + 自动攻击 + Roguelite 构筑）游戏，单机、PC 优先。
> 本文档给出玩法解构 → 技术选型 → 架构设计 → 里程碑 → 风险控制的完整路线。

---

## 一、先把原作拆清楚（决定技术选型的前提）

复刻的难点不在渲染，而在**数据驱动的属性/词条系统**和**大量单位的性能**。

### 1.1 核心循环
```
角色选择 → 波次战斗(20~90s) → 结算掉落材料 → 升级(4选1属性) → 商店(4格,可锁定/刷新)
   ↑                                                                        │
   └────────────────────────────── 下一波 ←───────────────────────────────────┘
                            共 20 波，第 10/20 波为 Boss 波
```

### 1.2 必须实现的系统清单
| 系统 | 关键点 | 复刻难度 |
|---|---|---|
| 属性系统 | ~18 种属性，加法/百分比/乘区叠加，来源可追溯并可移除 | ★★★★★ |
| 武器系统 | 6 槽位、自动索敌攻击、武器类别、同类套装加成、武器分级(1~4星)与合并 | ★★★★☆ |
| 物品/词条 | 200+ 物品，多为纯属性，部分为触发型特殊效果（事件驱动） | ★★★★★ |
| 敌人生成 | 波次生成表、按波次预算刷怪、精英/Boss、场外刷新点 | ★★★☆☆ |
| 大量单位 | 后期同屏 300~1000+ 敌人 + 数千投射物 | ★★★★★ |
| 经济与商店 | 材料掉落/吸附、商店权重抽取、刷新涨价、锁定、回收 | ★★★☆☆ |
| 升级曲线 | 经验/等级、4 选 1 属性抽卡（按幸运/等级加权） | ★★☆☆☆ |
| 角色 | 40+ 角色，起始属性修正 + 规则级特殊效果（如“无法使用远程武器”） | ★★★★☆ |
| 元进度 | 解锁、危险等级、成就、存档 | ★★☆☆☆ |

> **结论**：这是一个**规则引擎 + 数据表驱动**的项目，UE 的“蓝图拼玩法”方式会在物品数量到 50+ 时崩溃。必须一开始就用 C++ 做框架。

---

## 二、技术选型

### 2.1 引擎与版本
- **UE 5.4 或 5.5 LTS 线**。不要用最新预览版（Mass/Niagara API 变动大）。
- **C++ 为主 + 蓝图为胶水**。比例建议 **C++ 80% / BP 20%**（BP 只做 UI 绑定、特效表现、关卡摆放）。
- IDE：Rider for Unreal（强烈推荐）或 VS2022 + UnrealVS。
- 版本管理：**Git + Git LFS**（美术资源少，2D 项目 LFS 足够，不必上 Perforce）。

### 2.2 视觉方案（关键决策）
原作是 2D 像素俯视。UE 里有三条路，推荐 **方案 B**。

| 方案 | 做法 | 优点 | 缺点 |
|---|---|---|---|
| A. Paper2D / PaperZD | 纯 Sprite + Flipbook | 最接近原作 | UE 的 2D 工具链弱、批次合并差、动画编辑难受 |
| **B. 3D 场景 + 正交/远距透视俯视相机（推荐）** | 敌人用 Sprite 或低模，地面用 3D Plane | 拿到 Niagara/Lumen/材质全部能力，Instanced 渲染好优化 | 需要自己搭 2.5D 相机与排序 |
| C. 完全 3D | 全 3D 模型 | 卖相好，差异化 | 工作量与性能成本激增，单人不建议 |

**建议**：Actor 用 3D，表现层用 `Niagara Sprite Renderer` 或 `HISM` 批量渲染敌人，相机用固定角度正交，关闭 Lumen/VSM，Forward 或 Deferred + 简化后处理。

### 2.3 属性系统：GAS 还是自研？
| | GAS (GameplayAbilitySystem) | 自研 StatComponent |
|---|---|---|
| 属性叠加 | `GameplayEffect` 天然支持 Additive/Multiplicative/Override + 来源移除 | 需自己写 Modifier 栈 |
| 网络同步 | 成熟（本项目单机，用不上） | 无 |
| 学习成本 | 高（2~4 周） | 低 |
| 性能 | ASC 较重，1000 敌人各挂 ASC 会爆 | 极轻，可做成纯数据 struct |

**推荐混合方案**：
- **玩家角色**：用 GAS（`AttributeSet` + `GameplayEffect` + `GameplayTag`）。物品/武器/套装全部表达为 `GameplayEffect`，天生支持“获得/失去物品时属性正确回滚”，这是自研最容易写出 Bug 的地方。
- **敌人**：**不挂 ASC**。用轻量 `FEnemyStats` struct（HP/伤害/速度/护甲），由波次系统按难度曲线缩放。
- 若你 GAS 完全不熟且想快速出 Demo：自研 `UStatsComponent`，内部为
  `TMap<EStatType, TArray<FStatModifier>>`，Modifier 带 `SourceHandle`，脏标记缓存最终值。**但请从第一天就实现“按来源移除”和“计算顺序（基础 → 加法 → 百分比 → 乘算）”**。

### 2.4 大量单位：Actor 还是 Mass Entity？
- **第一阶段（Demo~垂直切片）**：普通 `APawn` + **对象池**（禁用 Tick 逐帧全量、用 `TickInterval` + 分帧批处理）。目标同屏 200。
- **第二阶段（性能优化期）**：
  - 敌人移动/索敌迁移到 **MassEntity（Mass AI）** 或自写数据导向的 `EnemyManagerSubsystem`（SoA 数组 + `ParallelFor`）。实测自写 Manager 通常比 Mass 更可控，Mass 的学习/调试成本很高。
  - 渲染用 `HierarchicalInstancedStaticMesh` 或 Niagara 从数组回读位置。
- **投射物**：**绝不要一颗子弹一个 Actor**。做 `ProjectileSubsystem`：
  - 数据：`TArray<FProjectileState>`（pos/vel/damage/pierce/lifetime）
  - 碰撞：自写空间哈希网格（Uniform Grid）做圆形重叠检测，不走 UE 物理
  - 渲染：Niagara `Sprite Renderer` + `Niagara Data Channel` 或 `SetNiagaraArrayVector`

### 2.5 其他子系统
| 需求 | UE5 方案 |
|---|---|
| 输入 | **Enhanced Input**（IMC/IA），支持键鼠+手柄 |
| UI | **UMG + CommonUI**（CommonUI 解决手柄焦点导航，商店/升级面板必需） |
| 数据表 | `DataTable`(CSV 导入，便于批量平衡) + `PrimaryDataAsset`(引用资源) 双轨 |
| 事件总线 | 自写 `UGameEventSubsystem`（物品触发型效果全靠它，如 OnKill/OnHit/OnWaveStart） |
| 音频 | MetaSound + 同音效并发限制（Concurrency），否则千发子弹会炸音频 |
| 存档 | `USaveGame` + 版本号字段；元进度与运行局内存档分离 |
| 本地化 | 从第一天用 `NSLOCTEXT`/StringTable，中英双语 |
| 随机数 | **自管 `FRandomStream`**，按用途分流（商店/掉落/升级），支持 seed 复现 Bug |

---

## 三、架构设计

### 3.1 模块划分（C++ Module）
```
Source/
├─ PotatoCore/          # 无引擎玩法依赖的纯逻辑：属性计算、词条求解、RNG、公式
├─ PotatoGameplay/      # GameMode/波次/敌人管理/投射物/拾取物/玩家
├─ PotatoData/          # DataAsset/DataTable 定义、数据校验
├─ PotatoUI/            # UMG C++ 基类、ViewModel
└─ PotatoEditor/        # 数据校验工具、平衡性调试面板（Editor-only）
```
拆模块的目的：**PotatoCore 可脱离引擎单元测试**（数值系统必须有测试，否则 200 个物品的组合 Bug 不可控）。

### 3.2 运行时类结构
```
APotatoGameMode           局流程状态机：Prepare→Wave→WaveEnd→Shop→NextWave→Win/Lose
APotatoGameState          当前波次、材料数、局内统计
UWaveDirectorSubsystem    读波次表，按"生成预算"分帧刷怪 + 难度缩放
UEnemyPoolSubsystem       敌人对象池 + SoA 批量移动/索敌
UProjectileSubsystem      投射物模拟 + 空间哈希碰撞
UPickupSubsystem          材料/掉落物，吸附与合并（数量多要合并显示）
UShopSubsystem            商店抽取、刷新涨价、锁定、价格公式
UInventoryComponent       6 武器槽 + 物品列表，负责挂/卸 GameplayEffect
UWeaponSlotComponent      每把武器独立计时器、独立索敌、独立射程
UGameEventSubsystem       OnEnemyKilled/OnPlayerHit/OnWaveStart... 物品效果订阅点
UStatsComponent / ASC     属性最终值提供者（含脏标记缓存）
```

### 3.3 数据驱动设计（最重要）

**武器定义**（`UWeaponDefinition : UPrimaryDataAsset`）
```
Id / DisplayName / Icon / Tier(1~4)
Tags: Weapon.Type.Gun, Weapon.Type.Precise ...   ← 套装加成靠 Tag 统计
BaseDamage / DamageScaling(近战%/远程%/元素% 各自系数)
Cooldown / Range / CritChance / CritMultiplier / Knockback / Pierce
AttackPattern: 单发/散射/回旋/近战扇形 (策略对象 UAttackPatternBase)
ProjectileVisual / SFX
```
> **关键**：Brotato 的武器伤害缩放是 `最终伤害 = (基础伤害 + 近战伤害*系数A + 远程伤害*系数B) * (1+伤害%)`。把系数放进数据表，不要硬编码。

**物品定义**（`UItemDefinition`）
```
Id / Icon / Tier / Price / ShopWeight / MaxStack
StatModifiers: TArray<FStatModifier>      ← 90% 物品只需要这个
GrantedEffects: TArray<TSubclassOf<UGameplayEffect>>
TriggerEffects: TArray<FItemTrigger>      ← { EventTag, Condition, Action, Cooldown }
```
把"触发型物品"抽象成 **Event → Condition → Action** 三段式，新增物品 90% 情况只填表不写代码。这是能否做到 200+ 物品的分水岭。

**角色定义**（`UCharacterDefinition`）：起始属性修正 + 起始武器 + `TArray<FGameRuleModifier>`（如禁用某类武器、商店只出某类物品）。规则修正要做成**可查询接口**，而不是 if-else。

**波次表**（`DataTable`）：`WaveIndex / Duration / SpawnBudget / EnemyWeights / EliteChance / BossId`，敌人属性按波次做曲线缩放（HP/伤害/数量各一条曲线 `FRuntimeFloatCurve`）。

### 3.4 伤害管线（统一入口）
```
Weapon.Fire() → 生成投射物/近战判定
   → Hit 检测命中 → FDamageContext{Attacker, Target, BaseDamage, Tags, IsCrit}
   → 事件总线 PreDamage（物品可修改：护甲穿透、额外伤害、易伤）
   → 目标护甲/闪避结算 → 扣血
   → 事件总线 PostDamage / OnKill（吸血、爆炸、掉材料、连锁）
```
所有伤害走唯一函数，方便下断点、出日志、做 DPS 统计面板。

---

## 四、里程碑路线（单人/小团队，按周估算）

### M0 — 环境与骨架（1 周）
- UE 5.4 空 C++ 工程，建好 5 个 Module，Git + LFS，命名/目录规范
- Enhanced Input 移动，固定俯视相机，一个占位角色
- **产出**：能跑能动的空场景

### M1 — 战斗最小闭环（2 周）
- 1 把武器自动索敌 + 开火（先用普通 Actor 投射物，够用）
- 1 种敌人：直线追击玩家、接触伤害、死亡掉材料
- 波次计时（30s）→ 波次结束 → 重开
- **产出**：可玩 1 分钟的原型。这是验证"手感"的关键节点，先别做任何系统。

### M2 — 属性与背包系统（2~3 周）
- `StatsComponent` 或 GAS AttributeSet：18 种属性齐全，Modifier 可挂可卸
- 6 武器槽、物品列表、装备即时生效/卸下即时回滚
- 3 把武器 + 10 个纯属性物品 + 调试面板（实时显示最终属性与来源）
- **PotatoCore 单元测试**：属性叠加顺序、回滚正确性
- **产出**：构筑可感知，属性面板正确

### M3 — 局内经济循环（2 周）
- 经验/升级 4 选 1（加权抽取）
- 商店：抽取、价格公式、刷新涨价、锁定、卖出
- 材料掉落 + 吸附 + 收获(Harvesting)属性
- **产出**：完整"战斗→升级→商店→下一波"闭环（3~5 波）

### M4 — 内容扩容（3~4 周）
- 20 波完整波次表 + 敌人属性曲线 + 2 个 Boss
- 8~12 种敌人（远程、冲锋、分裂、召唤、爆炸）
- 20~30 把武器（覆盖近战/枪械/元素/精准/重型）+ 武器套装加成
- 60~80 个物品，含 15 个触发型
- 6~8 个角色
- **产出**：Playtest 可用版本

### M5 — 性能优化（2~3 周，可与 M4 交叉）
- 敌人对象池 → SoA 批处理移动/避让（Boids 简化版分离力）
- 投射物迁移到 `ProjectileSubsystem` + 空间哈希 + Niagara 渲染
- 敌人渲染改 HISM/Niagara 批渲染，关 Lumen/VSM/复杂后处理
- 目标：**1000 敌人 + 3000 投射物 @ 60fps（中端 PC）**
- 用 `Unreal Insights` + `stat unit/stat game` 定位，不要凭感觉优化

### M6 — UI / 音频 / 存档（2 周）
- CommonUI：主菜单、角色选择、商店、升级、属性面板、暂停、结算
- 手柄全流程可用
- MetaSound + 音效并发限制；存档与元进度（解锁、危险等级）

### M7 — 打磨与平衡（持续）
- 数值平衡：导出 CSV，做"每波理论 DPS vs 敌人总 HP"曲线对照
- 打击感：屏幕震动、命中停顿、数字跳字、粒子、死亡溶解
- Editor 调试工具：一键给物品、跳波次、无敌、DPS 统计
- Steam 接入（Steamworks/Online Subsystem）、崩溃上报

**总计约 4~5 个月**（单人全职，含美术占位资源）。

---

## 五、性能红线与对应手段
| 瓶颈 | 手段 |
|---|---|
| Actor Tick 过多 | 统一由 Subsystem 批量 Tick，敌人 Actor 关闭自身 Tick |
| 物理查询 | 自写 Uniform Grid 空间哈希做圆碰撞，彻底绕开 PhysX/Chaos |
| Draw Call | HISM / Niagara 批渲染，材质数量压到 <10 |
| GC 峰值 | 全量对象池，运行时零 `SpawnActor`；`TArray::Reserve` 预分配 |
| 蓝图开销 | 热路径（伤害、移动、碰撞）100% C++，BP 不进 per-frame 循环 |
| 音效/特效风暴 | 同类特效/音效合并与频率限制（每帧最多 N 个跳字、M 个命中音） |
| 跳字 UI | 不要用 UMG Widget 做跳字，用 Niagara 或单一 Canvas 批绘 |

---

## 六、主要风险与对策
1. **词条系统膨胀失控** → 从 M2 起就用"Event/Condition/Action"数据化，并写单元测试；每加 10 个物品跑一次回归。
2. **过早优化或过晚优化** → M1~M4 用简单实现但**接口预留**（`IEnemyMover`、`IProjectileBackend`），M5 换实现不改调用方。
3. **GAS 学习成本拖慢进度** → 先用自研 StatsComponent 打通 M1~M3，若确认需要再迁移（接口隔离后迁移成本可控）。
4. **美术资源** → 先用 Kenney/itch.io 免费 2D 素材占位，玩法定稿后再替换，避免返工。
5. **法律风险** → 不要使用原作美术/音频/名称；玩法机制不受版权保护，但**资源与商标必须完全原创**。对外宣传用 "Brotato-like" 而非"土豆兄弟复刻"。
6. **单人全做易崩** → 每个里程碑必须产出"可玩版本"，杜绝三个月不能运行的重构。

---

## 七、立即可执行的第一步
1. 装 UE 5.4 + Rider，创建 C++ 空工程 `PotatoBros`，建好 `PotatoCore/PotatoGameplay/PotatoData/PotatoUI` 四个 Module
2. 定下 `EStatType` 枚举（18 种属性）与 `FStatModifier` 结构 —— 这是整个项目的地基
3. 写死一把武器、一种敌人、30 秒一波，跑通 M1
4. 建立 `Docs/Balance.xlsx`（武器/物品/波次三张表），数据从第一天就在表里

需要我接着做哪一步？我可以直接帮你：
- 生成 `EStatType` / `FStatModifier` / `UStatsComponent` 的完整 C++ 代码
- 搭出 M0 的工程目录与 Module 的 `.Build.cs`
- 设计武器/物品/波次三张 DataTable 的字段与示例数据
