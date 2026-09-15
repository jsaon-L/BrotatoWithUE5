# Brotato UE5 复刻 · 完整设计与实现文档

> **版本**：v1.0
> **目标引擎**：Unreal Engine 5.8
> **数据来源**：本地 Godot 3.5 反编译工程 `E:\UE\UEProject\BrotatoProject-main`（Brotato v1.0.1.3，Steam AppID 1942280）逐文件逆向 + 网络补充
> **文档定位**：可直接驱动开发的复刻策划案 + 实现规范。所有数值均带源文件出处，未验证的内容标注 `[待确认]`。
> ★ **复刻口径（v2.0 修订）**：复刻目标是**玩家可观测的规则、数值与手感**，**不是 Godot 的引擎实现方式**。实现层一律采用 UE5 正统做法，详见 **[00_UE化移植准则与时间基规范](00_UE化移植准则与时间基规范.md)（最高仲裁）**。

---

## 0. 文档索引

| 编号 | 文档 | 内容 | 主要读者 |
|---|---|---|---|
| — | 本文件 | 项目总览 / 复刻策略 / 技术选型 / 里程碑 | 全员 |
| **00** | ★ [UE 化移植准则与时间基规范](00_UE化移植准则与时间基规范.md) | **最高仲裁**：行为等价原则、秒制时间基、GAS 原生用法、自建设施白名单、被取代的旧条目对照表 | 全员（先读） |
| 01 | [核心玩法与循环设计](01_核心玩法与循环设计.md) | 核心幻想、五级循环、一局完整时序、节奏曲线 | 策划 / 制作人 |
| 02 | [属性与伤害数值体系](02_属性与伤害数值体系.md) | 16 主属性 + 60 次级属性、三层属性叠加、伤害/护甲/闪避/暴击全公式、升级曲线 | 数值 / 程序 |
| 03 | [角色系统设计](03_角色系统设计.md) | 44 角色全量属性修正、特殊机制、解锁条件、Build 导向 | 策划 / 数值 |
| 04 | [武器系统与战斗手感](04_武器系统与战斗手感.md) | 55 武器家族 / 185 阶位全量数值、15 套装、攻击时序、打击感参数 | 战斗策划 / 程序 |
| 05 | [物品·效果框架·商店与经济](05_物品_效果框架_商店与经济.md) | Effect 数据驱动框架（复刻核心）、179 物品、商店定价/重掷、掉落与运气 | 程序 / 数值 |
| 06 | [波次·敌人·AI·难度](06_波次_敌人_AI_与难度.md) | 20 波配表、刷怪算法、21 种普通敌人 + 2 Boss + 8 精英全量数值、Danger 0–5、无尽 | 关卡 / 战斗 |
| 07 | [关卡地图·表现反馈·音画](07_关卡地图_表现反馈_音画.md) | 地图尺寸/相机/树、屏震/飘字/粒子/闪白全参数、音效触发表 | 技美 / 关卡 |
| 08 | [UI·流程·元进度·存档](08_UI_流程_元进度_存档.md) | 全局状态机、HUD/商店/属性面板信息架构、88 挑战解锁图、存档格式与设置项 | UI / 程序 |
| 09 | [UE5 实现架构与数据表](09_UE5实现架构与数据表.md) | **分级 GAS 架构**、模块划分、C++ 类设计、GameplayTag 字典、DataTable Schema、GAS 学习路径映射 | 主程 / 程序 |
| 10 | [开发排期与验收清单](10_开发排期与验收清单.md) | 里程碑拆分、每阶段验收标准、还原度自测用例 | 制作人 / QA |

### 0.1 实施级开发文档（`Docs/Dev/`）

`Docs/01–10` 回答 **What & How much**（设计与数值）；`Docs/Dev/D00–D08` 回答 **How**（可直接照着敲代码的实现规范）。

| 编号 | 文档 | 内容 | 对应里程碑 |
|---|---|---|---|
| D00 | [开发文档总览与工程初始化](Dev/D00_开发文档总览与工程初始化.md) | 工程初始化、7 模块与 Build.cs、目录/命名/编码规范、Config | M0 |
| D01 | [BrotatoCore·确定性与数学核心](Dev/D01_BrotatoCore_确定性与数学核心.md) | Sim 阶段编排、RNG 分流、★ 取整契约、伤害/经济/成长数学核心 | M0–M2 |
| D02 | [GameplayTag 与属性系统实现](Dev/D02_GameplayTag与属性系统实现.md) | Tag 字典全文、4 个 AttributeSet、ASC、gain 乘区、LinkedStats | M2–M3 |
| D03 | [GE 资产规范与 Effect 桥接](Dev/D03_GameplayEffect资产规范与Effect桥接.md) | 22 个 GE 资产逐字段配置、EffectDef、RunSubsystem、HookRegistry | M3 |
| D04 | [Ability/Task/Cue 与武器战斗](Dev/D04_Ability_AbilityTask_Cue与武器战斗实现.md) | RuntimeStats 11 步、GA_WeaponAttack、MeleeSwing Task、投射物、Cue Proxy | M1–M4 |
| D05 | [敌人·波次·碰撞·结构物](Dev/D05_敌人_波次_碰撞与结构物实现.md) | FEnemyState、6+3 行为、StateTree、WaveManager、Spawner、UE 原生碰撞（空间哈希兜底）、掉落 | M1–M4 |
| D06 | [商店·经济·UI·存档](Dev/D06_商店_经济_UI与存档实现.md) | Tier 抽取、定价、CommonUI 栈、HUD、SaveGame 与迁移、挑战解锁 | M3–M5 |
| D07 | [数据管线与编辑器工具](Dev/D07_数据管线与编辑器工具.md) | 全部 DataTable Schema、tres 导入器、Validator、资产命名 | M0–M4 |
| D08 | [测试·调试·性能与里程碑任务清单](Dev/D08_测试_调试_性能与里程碑任务清单.md) | 单测矩阵、双路径一致性、黄金基线、调试命令、Insights、CI、M0–M7 任务卡 | 全程 |

**冲突仲裁顺序**：**`Docs/00`（实现方式的最高仲裁）** → 反编译源码（规则与数值）→ `Docs/01–08`（数值）→ `Docs/09`（架构）→ `Docs/Dev/*`（实现）。
> 即：**数值与规则以源码为准；实现手段以 `Docs/00` 为准。** 凡 `01–10` / `Dev/*` 中残留的「物理帧、整数帧计数器、禁用 Duration/Periodic GE」等表述，均已被 `Docs/00` 取代。

---

## 0.1 项目双重目标

| 目标 | 说明 |
|---|---|
| **① 复刻** | **规则、数值、手感**还原 Brotato（公式逐值一致；时间类以秒表达、行为等价，见 §00 §1.1） |
| **② 学习** | 借本项目系统性掌握 **GAS（Gameplay Ability System）** 及 UE5 现代 Gameplay 架构（**GAS 能力全量使用，不为复刻而阉割**） |

因此本项目采用 **Tiered GAS（分级 GAS）** 架构 —— 玩家与 Boss 走完整 GAS，100+ 普通小兵走无 ASC 的纯数据路径，两条路径**共享同一份计算核心**保证数值一致。详见 [09_UE5实现架构与数据表](09_UE5实现架构与数据表.md)。

---

## 1. 项目定位

### 1.1 一句话定义
**俯视角固定竞技场、自动攻击、20 波次生存的 Roguelite Build 构筑游戏。**
玩家扮演一只土豆，同时携带最多 6 把自动开火的武器，在 20 波逐渐加压的外星人潮中活下来；操作只负责走位与拾取，战斗胜负由「构筑决策」决定。

### 1.2 核心幻想（Core Fantasy）
> **"我用 30 秒的商店决策，换来了 60 秒的屠杀快感。"**

三条互相咬合的爽点：
1. **走位爽**：无加速度、无惯性的 450 px/s 瞬时响应移动（见 §02），身法即生存。
2. **成长爽**：单局 20 波内属性可膨胀 10–50 倍，从 6 伤害打到 3000+ 伤害。
3. **构筑爽**：16 主属性 × 44 角色 × 179 物品 × 55 武器家族 × 15 套装 → 每局都是新的最优解求解题。

### 1.3 对标与差异（相对 Vampire Survivors 类）

| 维度 | Vampire Survivors | **Brotato** | 复刻时必须守住的点 |
|---|---|---|---|
| 地图 | 无限滚动 | **固定竞技场 2048×1536 px** | 有边界 → 走位压力恒定，不能"无限拉扯" |
| 单局时长 | 30 min | **约 17.5 min（20 波累计 1050 s）** | 短平快，重开成本极低 |
| 成长节奏 | 只升级 | **升级（战斗中）+ 商店（波间）双轨** | 波间停顿是核心节奏器，不可省略 |
| 决策密度 | 低（升级 4 选 1） | **高（4 选 1 升级 + 4 格商店 + 锁定 + 重掷 + 合成 + 回收）** | 商店是本作灵魂 |
| 武器 | 自动进化 | **6 槽手动配装 + 同名同阶合成升阶 + 15 套装** | 槽位稀缺性是核心张力 |
| 角色 | 差异中等 | **44 角色，含颠覆规则的极端角色**（无武器/单武器/血当钱/12 槽） | 角色即"模组"，不是换皮 |

### 1.4 市场参考数据
- Steam 历史最高同时在线 **38,800**（`[社区]` steamplayercount.com，2026-09 查询）
- 44 角色中仅 5 个默认解锁，其余全部靠成就解锁 —— 元进度是留存主引擎（`[实测]` `items/characters/*_data.tres` 的 `unlocked_by_default` 字段）

### 1.5 ★ 内容规模口径（全文统一，勿再使用其他数字）

| 项 | 口径 | 说明 |
|---|---|---|
| 角色 | **44** | 目录 45 个 tres，`ItemService.characters` 注册 44 |
| 物品 | **179** | Tier I 55 / II 46 / III 47 / IV 31 |
| 消耗品 | **3** | fruit / item_box / legendary_item_box |
| 武器**家族** | **55** | 近战 33 + 远程 22（另有非商店 `knuckles` 1 把） |
| 武器**阶位条目** | **185** | 近战 113 + 远程 71 + knuckles 1 → 即 `DT_Weapons` 行数 |
| 套装 | **15** | 每套 5 档加成 |
| 升级条目 | **64** | 16 属性 × 4 档（另有 4 组废弃残留，忽略） |
| 普通敌人 | **21** | `001–009` + `012–019` + `023–026`（源码不存在 029–031） |
| Boss | **2** | `010` / `011` |
| 精英 | **8** | `020/021/022/027/028/032/033/034` |
| 中立单位 | **1** | 树 |
| 挑战 | **88** | 含 5 个仅展示用的 `unlock_difficulty_N` |
| 结构物 | **9** | 5 炮塔 + 地雷 + 花园 + Tyler + 游荡机器人 |
| 单局时长 | **1050 s** | 360（W1–9）+ 600（W10–19）+ 90（W20） |

---

## 2. 复刻范围界定

### 2.1 本次复刻目标（Scope In）

| 系统 | 目标还原度 | 说明 |
|---|---|---|
| 属性与伤害公式 | **100% 逐公式一致** | 包括所有 `max(1, ...)` 下限、取整时机、随机数比较符（`<` vs `<=`） |
| 44 角色 | **100%** | 全部特殊机制 |
| 55 武器家族 / 185 阶位 | **100%** | 含 Hitbox 尺寸、挥击 Tween 曲线 |
| 179 物品 + 3 消耗品 | **100%** | 含 27 个可追踪物品的统计文本 |
| 20 波 Zone 1 配表 | **100%** | 逐组 spawn_timing / repeating |
| 21 普通敌人 + 2 Boss + 8 精英 | **100%** | 含 Boss 状态机阈值 |
| Danger 0–5 + 无尽 | **100%** | |
| 88 挑战与解锁图 | **100%** | |
| 手感参数（屏震/飘字/无敌窗口/击退） | **行为等价** | 这是"像不像"的决定性因素；按**秒**对齐，容差 ±2%（见 §00 §1.1 P1） |
| Zone 2/3 | **不做** | 原版即为未完成占位（各仅 1 波） |
| Mod Loader | **不做** | 后续可用 UE Plugin/PAK 方案重做 |
| Steam 成就/云存档 | **P2（可选）** | 架构预留接口 |
| 本地化 13 语言 | **P1（先中英）** | 原工程 `translations.csv` 已丢失，需重建文本表 |

### 2.2 已知数据缺口（需人工补齐）

| 缺口 | 影响 | 补齐方式 |
|---|---|---|
| `resources/translations/translations.csv` 二进制缺失 | 全部 UI 文案 | 从正版游戏导出，或按 §08 的键命名规范重建 |
| 部分 Tier I–III 物品的 `value` 字段未逐个读取（约 100 项） | 商店价格 | 复刻期批量扫描 `items/all/*/*_data.tres` 的 `value` |
| 美术资源（861 PNG / 62 WAV / 16 MP3） | 表现层 | 需自制或授权；本文档只规范**尺寸、时长、触发时机**（分项见 §07 §2.1） |
| 部分火焰系武器的 `burning_data` 具体 chance/damage/duration | 燃烧数值 | 扫描 `weapons/*/torch|flamethrower|wand|flaming_knuckles/*/**_burning_data.tres` |
| **角色默认起始武器池的 9 把清单** | **M1/M5 硬阻塞** | 扫描 `WeaponSelection` 的默认可选列表（见 §03 §6） |
| `item_tyler` / `item_wandering_bot` 的结构物参数 | 2 件 Tier III 物品无法实现 | 扫描对应 `*_effect.tres` + 场景（见 §05 §6.1） |

> **全部 `[待确认]` 项的汇总与责任里程碑见 [10_开发排期与验收清单](10_开发排期与验收清单.md) §7。**

---

## 3. 技术选型结论

### 3.1 渲染与表现层

| 决策点 | 选型 | 理由 |
|---|---|---|
| 2D 实现方式 | **3D 场景 + 正交相机 + Billboard/Plane 贴图**（而非 Paper2D） | UE5.8 的 Paper2D 维护弱；正交 3D 可直接用 Niagara、材质、Lumen 关闭后的纯 Unlit 管线，且 Sprite 排序可控 |
| 角色/敌人渲染 | Unlit Masked 材质 + `Nanite=Off` + 单 Quad | 同屏 100–200 单位，走 **ISM/HISM 批渲染** 或 Niagara-driven Sprite |
| 特效 | Niagara（命中粒子/爆炸烟雾/燃烧） | 对应原 `particles/*.tscn` |
| UI | **CommonUI**（`UCommonActivatableWidgetStack`） | 原作菜单是标准"页栈 + back()"模型，CommonUI 天然对应；且原生支持手柄焦点 |
| 飘字 | 自建 Widget 对象池（预热 50，上限 100） | 严格对齐原作对象池参数 |

### 3.2 逻辑层

| 决策点 | 选型 | 理由 |
|---|---|---|
| 时间模型 | **UE 标准可变 `DeltaTime`** + `UBrotatoSimSubsystem` 固定**阶段顺序**（非固定步长） | 原作冷却以物理帧计数只是 Godot 的实现方式；本项目把全部时间量换算为**秒**（`秒 = 帧/60`），一切随时间变化的量乘 `DeltaTime`，指数衰减用半衰期式 → 30/60/144/240 fps 手感一致（详见 §00 §2） |
| 属性系统 | **分级 GAS**：玩家 + Boss/精英用 `UAttributeSet` + GE；普通敌人用 `FEnemyState` 纯数据 | GAS 的 `(Base+Add)×Mul` 与原作 `(三层相加)×(1+gain/100)` **数学等价**（gain 做成 Multiplicative AttributeBased Modifier）；但 144 个 ASC 不可接受 → 分级 |
| 效果系统 | `TInstancedStruct<FBrotatoEffectBase>` 数据定义 → 运行时桥接成 **GE（属性类）+ HookRegistry（钩子类）** | 21 种 Effect 中约 9 种可直接映射 GE Modifier；其余 12 种钩子语义 GAS 无对应，保留自建注册表（事件分发走 GAS 原生 `GameplayEvent`）。**卸载必须用 `FActiveGameplayEffectHandle`**（原作值比较有 bug，不复刻） |
| GE 使用边界 | **Instant / Infinite / Duration / Periodic 四类全部启用** | 无敌窗口、吸血 CD、减速用 Duration GE；燃烧 tick、生命回复、每 5 s 叠加用 Periodic GE。时长/周期需要运行时决定时用 `SetByCaller` / `Spec->Period`。GE 计时走 World 时间，天然正确响应 Pause 与 TimeDilation |
| 武器攻击 | `UGameplayAbility` + `UAbilityTask`（挥击 Tween 走 `UCurveFloat`）；**冷却用 GAS Cooldown GE** | 重写 `ApplyCooldown` 以 `SetByCaller` 写入含随机抖动的秒数；「射击期间冷却不流逝」等价于**攻击序列结束时才 Apply Cooldown GE**；UI 直接读 `GetCooldownTimeRemaining()` |
| 敌人特效 | **Cue Proxy ASC**（World 级共享 ASC 播放无主 GameplayCue） | 让 144 个无 ASC 敌人也能走统一的 Cue 资产管线 |
| 敌人 AI | 小兵：`MovementBehavior + AttackBehavior` 数据驱动批处理；**Boss / 精英 / 结构物：`StateTree`** | 被否决的是行为树（144 个 BT 实例是性能灾难），而 Boss 阶段状态机的标准答案是 UE5 原生的 StateTree |
| 大规模单位 | Actor 池 + 批处理 + HISM 渲染；子弹/材料/飘字**非 Actor**；**Mass Entity 作为一等评估路径** | 原作 `max_enemies=100`（无尽 135–144）。M0 压测同时评估 Mass |
| 碰撞 | **默认用 UE 原生碰撞**（自定义 Object Channel + Collision Profile，锁 Z 做 2D，批量 `OverlapMultiByObjectType`） | 100–200 单位规模下 UE 查询完全够用；自建 2D 空间哈希降级为**压测不达标时的兜底方案**（§00 §9 登记） |
| 相机与打击感 | `UCameraShakeSourceComponent` + `ULegacyCameraShake`；飘字自建 Widget 对象池 | 屏震参数数据化、可被设置项统一缩放 |
| 随机数 | `FRandomStream` **按用途分流**（Shop / Loot / Spawn / Combat / Cosmetic），均由 per-run seed 派生 | Shop/Loot/Spawn 为离散事件调用，**与帧率无关 → 同 seed 严格可复现**（Daily Run、bug 复现）；ExecCalc / MMC 内禁用 `FMath::FRand()` |
| 确定性口径 | **不追求逐帧确定性**；回归测试用 `-UseFixedTimeStep -FixedDeltaTime=0.0166667` 的 headless 固定步长 | 用引擎原生手段满足测试需求，而不是为测试扭曲生产架构 |
| 存档 | `USaveGame` + JSON 多行 + 三级备份；GE 句柄不序列化，读档重建 | 完全对齐原作 save/save_latest/save_stable 策略 |

### 3.3 模块划分

```
BrotatoCore        (Runtime)  纯数学核心（伤害/经济/成长公式）、随机数、类型定义
                              —— 零 UE Gameplay 依赖，可脱离引擎单测
BrotatoData        (Runtime)  所有 DataAsset / DataTable 定义 + FBrotatoEffectDef
BrotatoGAS         (Runtime)  ★ AttributeSet / ExecCalc / MMC / Ability / AbilityTask
                              / GameplayCue / CueProxy / HookRegistry
BrotatoGameplay    (Runtime)  Player(TierA) / Boss(TierB) / EnemySubsystem(TierC)
                              / Weapon / Projectile / Wave / Spawner / Collision
BrotatoUI          (Runtime)  CommonUI Widgets
BrotatoSave        (Runtime)  存档、设置、解锁、Steam
BrotatoEditor      (Editor)   数据表校验器、数值可视化、.tres 批量导入 Commandlet
```

**依赖方向（严格单向）**：
```
BrotatoCore ← BrotatoData ← BrotatoGAS ← BrotatoGameplay ← BrotatoUI
                                    ↖ BrotatoSave ↗
```

---

## 4. 关键复刻风险清单（Top 14 易踩坑）

> 这 14 条是逆向过程中发现的、**最容易做错且直接影响手感/平衡**的点。开发时逐条打勾。

| # | 风险点 | 正确做法 | 详见 |
|---|---|---|---|
| 1 | 冷却单位换算错 | 原作 `cooldown` 是 **60 fps 物理帧**，本项目**导入期即换算为秒**（`秒 = 帧/60`），硬下限 **1/30 s**（原作 2 帧）。运行时不存在帧值 | §00 §2 / §04 |
| 2 | 暴击算法 | **用原始伤害重算**：`get_dmg_value(round(raw × crit_damage))`，**不是**已减甲伤害 × 倍率 | §02 |
| 3 | 护甲双公式 | **敌人 = 减法** `max(1, dmg - armor)`；**玩家 = 乘法** `10/(10+|armor|/1.5)` | §02 |
| 4 | 属性作用域 | `melee/ranged/elemental/engineering damage` **不是全局加成**，只对武器 `scaling_stats` 里声明的键生效 | §02/§04 |
| 5 | 吸血机制 | 每次命中独立 roll，成功**定量回 1 HP**，且有 **0.1 s CD**（上限 10 HP/s），与伤害无关 | §02 |
| 6 | 冷却抖动 | 每次射击冷却带 `±min(N×CD/5, N×0.0833s)` 秒随机（N=min(武器数,6)，上限 ±0.5 s），用于错开齐射；下限 `max(1/60 s, CD-R)`，**结果不取整** | §04 |
| 7 | Effect 卸载 | 原作用值比较 `Array.erase`，叠加同物品时会错删；UE5 侧**必须用句柄/索引** | §05 |
| 8 | Tier 抽取 | **只掷一次骰子**，从 Legendary 向 Common 遍历，先命中者胜；顺序错则分布完全不同 | §05 |
| 9 | bonus_gold 语义 | 它**不是金币**，是"下一波材料价值翻倍的额度池"，每消耗 1 点让 1 个材料 1→2 | §05 |
| 10 | 精英血量 | 精英 `health=1`、`health_increase_each_wave=750`，实际 HP = `1 + 750×(wave-1)` | §06 |
| 11 | `repeating = -1` | 表示**只触发一次**（因为 `if repeating > 0` 才注册重复），不是无限 | §06 |
| 12 | 屏震方向 | 原作用 `randf()∈[0,1)`，偏移**只向右下**，这是原版"抖动感"的一部分 | §07 |
| 13 | **取整/舍入差异** | Godot `round()` 是 half-**away-from-zero**、`as int` 是**截断向零**；UE `RoundToInt()` 是 half-**to-+∞**。负值场景（Renegade −400% 伤害、负护甲）会分叉。**仅对整数语义量适用；时间/距离/速度全程 float 不取整** | §00 §6 / §09 §2.5 |
| 14 | **帧率相关写法** | 禁止「每 Tick −1 / 每 Tick ×0.9」。随时间变化的量必须乘 `DeltaTime`；指数衰减用 `Value *= Exp(-Rate*DeltaTime)`，`Rate = -60·ln(k)`（击退 `k=0.9 → Rate=6.3216`） | §00 §2.3 |

### 4.1 GAS 专项风险（Top 8）

| # | 风险点 | 正确做法 | 详见 |
|---|---|---|---|
| G1 | 用自建计时器替代 GE 计时 | ✅ **反向**：燃烧 tick、生命回复、无敌窗口、吸血 CD、每 5 s 叠加**必须用 Periodic / Duration GE**（时长与周期用 `SetByCaller` / `Spec->Period`）。禁止关闭 ASC Tick 手动驱动 | §00 §3.1 §3.3 |
| G2 | `gain_*` 乘区映射 | 做成 Attribute，主属性挂 **Multiplicative + AttributeBased 非快照** Modifier，`Coefficient=0.01`、`PreMultiplyAdditive=100` → 恰好 `1+gain/100` | §09 §4.4 |
| G3 | "每 N 点 X 给 Y" 循环依赖 | **禁用非快照 MMC**（会递归死循环）。用「先移除旧 Linked GE → 单次快照计算 → 重新 Apply」模式，读源属性时天然排除 Linked 层 | §09 §4.5 |
| G4 | 整数除法 | LinkedStats 的 `Scaled / Nb` 必须 `FloorToInt`，不能用浮点 | §09 §4.5 |
| G5 | 一物品多 GE | 把一个物品的全部属性 Modifier 合成 **一个** GE Spec，否则 Aggregator 条目数 ×10 | §09 §5.3 |
| G6 | 敌人无 ASC 却要播 Cue | 用 World 级 **Cue Proxy ASC** 播放无主 Cue；限流在调用前做（100 上限） | §09 §8 |
| G7 | ExecCalc 里写公式 | ExecCalc 只做「捕获 → 调 `FBrotatoDamageMath` → 写回」；公式**只允许存在一份**，双路径共享 | §09 §7 |
| G8 | ExecCalc 里用 `FMath::FRand()` | 必须取 `UBrotatoRunSubsystem::GetRandom(EBrotatoRngChannel::Combat)`；Shop/Loot/Spawn 各走自己的流 | §00 §5.2 |

---

## 5. 里程碑总览

| 阶段 | 名称 | 周期 | 核心交付 |
|---|---|---|---|
| M0 | 技术验证 | 1 周 | Sim 阶段编排 + **帧率无关性验证（30/60/144/240 fps）**、正交相机、200 单位 + 500 子弹压测（UE 碰撞 vs 空间哈希二选一定论） |
| M1 | 可玩核心 | 3 周 | 移动 + 1 武器自动攻击 + 1 敌人 + 单波次 + 死亡 |
| M2 | 数值骨架 | 2 周 | 完整属性系统 + 伤害公式 + 升级 + 材料/XP |
| M3 | 构筑闭环 | 3 周 | Effect 框架 + 商店 + 物品（先 30 件）+ 武器合成 |
| M4 | 内容填充 | 4 周 | 20 波配表 + 全敌人 + Boss/精英 + 全武器 + 全物品 |
| M5 | 角色与元进度 | 3 周 | 44 角色 + 88 挑战 + 解锁 + 存档 |
| M6 | 手感与打磨 | 2 周 | 全部反馈参数对齐 + Danger 0–5 + 无尽 |
| M7 | 本地化与发布准备 | 2 周 | 文本表、设置项、手柄适配、性能优化 |

详细拆分与验收标准见 [10_开发排期与验收清单](10_开发排期与验收清单.md)。

---

## 6. 文档使用约定

- **数值标注**：所有关键数值后括注来源文件，如 `450 px/s（player_stats.tres:9）`
- **单位约定**：距离 = 像素（原作 2D 坐标系，UE5 **1 px = 1 uu**，直接沿用数值）；**所有时间量 = 秒（float）**；角速度 = 度/秒
- **★ 帧值的读法**：文中残留的「N 帧」一律是**原作出处标注**，等价读作 `N/60 秒`；运行时与数据表中**不存在帧字段**（见 §00 §2.2）
- **信息可信度**：`[源码]` 直接读取 / `[推导]` 由源码推算 / `[社区]` 网络资料 / `[待确认]` 需补
- **未标注即为 `[源码]`**
- **内容规模数字**：一律引用 §1.5 的口径表，不要在各章另起数字
- **版本锚点**：本轮校对基准 = Brotato v1.0.1.3 反编译工程；最后全量交叉校对日期 **2026-09-14**。实现中若发现与源码不符，**以源码为准并立即回写本文档 + 更新此日期**

---

## 7. 术语表

| 术语 | 含义 | 常见误解 |
|---|---|---|
| **材料 / Material** | 掉落的绿色小球，**同时是金币和经验**（1:1） | 不是两种资源 |
| **bonus_gold** | 波末未拾取材料转成的"**下一波材料价值翻倍额度池**" | ❌ 不是金币 |
| **harvesting / 收获** | 波末固定给 N 材料 + N 经验的"利息"，每波自增 5% | 不是掉落率加成 |
| **Danger（危险度）** | 难度 0–5。只改敌人 HP/DMG、精英数量、`min_difficulty` 组解锁 | ❌ 不改速度、不改怪量系数 |
| **Horde / 兽潮** | 高密度刷怪波，Danger≥2 时有概率替代精英波 | |
| **Elite / 精英** | 8 种迷你 Boss，`health=1` + `health_increase_each_wave=750` | ❌ 不是 Boss 的低配版 |
| **scaling_stats** | 武器声明的「属性 → 固定伤害加值」系数表 | `melee/ranged/elemental/engineering damage` **只对声明了的武器生效**，不是全局加成 |
| **gain_\*** | 某属性的**获取倍率**（乘区），`1 + gain/100` | 不是属性本身 |
| **Tier** | 稀有度 I–IV（白蓝紫红）。武器 tier 同时是阶位 | 角色的 `tier` 字段运行时被 UI 改写成"已通关危险度"，不是真 tier |
| **cooldown** | 攻击间隔。原作以**物理帧**计（60 = 1 s），本项目统一为**秒**（`秒 = 帧/60`），硬下限 `1/30 s` | ❌ 运行时不再有帧值 |
| **burning duration** | 原作是 **tick 数**（每 tick 0.5 s）；本项目表达为 `Periodic GE：Period = tick 间隔、Duration = tick 数 × Period` | ❌ 不是"秒数直接填 duration" |
| **repeating = -1** | **只触发一次** | ❌ 不是无限 |
| **Endless Factor (EF)** | 无尽模式的全局强度因子，驱动敌人成长与物价 | |
| **Tier A/B/C/D** | 本项目 GAS 分级：玩家 / Boss精英 / 普通敌人 / 无 Actor 实体 | 与稀有度 Tier 无关，注意区分 |
