// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/BRT_BaseCharacter.h"
#include "BRT_EnemyCharacter.generated.h"

class UPaperSpriteComponent;

// =====================================================================================================================
// 敌人移动策略
//
// 对应 Godot 工程 entities/units/movement_behaviors/ 下的 6 个 MovementBehavior 脚本。
// Godot 的做法是「在敌人场景里换挂哪个 Behavior 节点」，UE 这边用「一个类 + 枚举 + 每个策略一份参数结构体」表达，
// 蓝图里改 MoveMode 就等于换 Behavior，避免为每种行为造一堆几乎相同的蓝图子类。
// 每个策略的字段默认值都抄自 Godot 各场景（enemy.tscn / 3.tscn / 33.tscn...）里的实际配置。
//
// 职责划分与 Godot 一致：
//   MovementBehavior  -> 只负责算出「往哪走」的目标点/方向（GetTargetPosition / GetMoveDirection）
//   Unit              -> 把方向乘上速度、叠加击退，得到最终速度（GetMoveInput / GetDesiredVelocity）
//   RigidBody2D       -> 真正位移（UE 侧交给 CharacterMovementComponent，见 ApplyDesiredVelocity）
// =====================================================================================================================

/** 移动策略枚举 */
UENUM(BlueprintType)
enum class EBRT_EnemyMoveMode : uint8
{
	/** FollowPlayerMovementBehavior：每帧朝玩家当前位置直线移动（默认，绝大多数小怪） */
	FollowPlayer				UMETA(DisplayName = "追玩家"),
	/** StayInRangeFromPlayerMovementBehavior：与玩家保持固定距离（近战绕后走位 / 远程卡距离） */
	StayInRange					UMETA(DisplayName = "保持距离"),
	/** FollowRandPosAroundPlayerMovementBehavior：追「出生时抽到的玩家周围偏移点」（斜后方尾随、绕圈感） */
	FollowRandPosAroundPlayer	UMETA(DisplayName = "跟随玩家周围点"),
	/** TargetRandPosAroundPlayerMovementBehavior：到达后在玩家周围重新抽点（断续扑击） */
	TargetRandPosAroundPlayer	UMETA(DisplayName = "玩家周围随机点"),
	/** TargetRandPosMovementBehavior：全地图随机游走，不追人 */
	TargetRandPos				UMETA(DisplayName = "地图随机游走"),
	/** PatrolEdgesOfMapMovementBehavior：沿地图边缘巡逻，发现玩家后才转追击 */
	PatrolEdgesOfMap			UMETA(DisplayName = "地图边缘巡逻"),
};

/** 巡逻模式发现玩家后的子策略（对应 PatrolEdgesOfMap.movement_behavior_on_player_detection） */
UENUM(BlueprintType)
enum class EBRT_PatrolChaseMode : uint8
{
	FollowPlayer				UMETA(DisplayName = "追玩家"),
	StayInRange					UMETA(DisplayName = "保持距离"),
	FollowRandPosAroundPlayer	UMETA(DisplayName = "跟随玩家周围点"),
	TargetRandPosAroundPlayer	UMETA(DisplayName = "玩家周围随机点"),
};

/** 地图四条边（对应 Godot Direction.TOP/LEFT/BOTTOM/RIGHT，仅巡逻贴边用） */
UENUM()
enum class EBRT_MapEdge : uint8
{
	Top,
	Left,
	Bottom,
	Right,
	None
};

/** 保持距离参数（StayInRangeFromPlayerMovementBehavior，Godot 默认 target_range=300 / randomization=100） */
USTRUCT(BlueprintType)
struct FBRT_StayInRangeParams
{
	GENERATED_BODY()

	/** 期望与玩家保持的距离（px≈cm） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StayInRange", meta = (ClampMin = "0.0"))
	float TargetRange = 300.f;

	/** 距离随机化幅度：实际距离 = TargetRange ± 本值，出生时抽一次，避免同类怪挤在同一个圆上 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StayInRange", meta = (ClampMin = "0.0"))
	float RangeRandomization = 100.f;
};

/** 玩家周围随机点参数（FollowRandPosAroundPlayer / TargetRandPosAroundPlayer 共用） */
USTRUCT(BlueprintType)
struct FBRT_RandPosAroundPlayerParams
{
	GENERATED_BODY()

	/** 偏移半径（px） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AroundPlayer", meta = (ClampMin = "0.0"))
	float RangeAroundPlayer = 200.f;

	/** 半径随机化幅度（px） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AroundPlayer", meta = (ClampMin = "0.0"))
	float RangeRandomization = 0.f;

	/** true = 偏移点落在圆内任意位置；false = 推回圆环上（对应 Godot 的 allow_within） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AroundPlayer")
	bool bAllowWithin = true;
};

/** 地图边缘巡逻参数（PatrolEdgesOfMapMovementBehavior） */
USTRUCT(BlueprintType)
struct FBRT_PatrolParams
{
	GENERATED_BODY()

	/** 距地图边缘多远算「贴边」（px），Godot 默认 300 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol", meta = (ClampMin = "0.0"))
	float EdgeDistance = 300.f;

	/** 边缘距离随机化（px），Godot 默认 100 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol", meta = (ClampMin = "0.0"))
	float EdgeRandomization = 100.f;

	/** 感知半径（px）：玩家进入该范围就转追击，Godot 默认 300 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol", meta = (ClampMin = "0.0"))
	float DetectionRange = 300.f;

	/** 单个巡逻目标点的最远距离（px），防止一次从地图一头走到另一头，Godot 默认 300 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol", meta = (ClampMin = "0.0"))
	float MaxTargetDistance = 300.f;

	/** 发现玩家后的追击子策略 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol")
	EBRT_PatrolChaseMode ChaseMode = EBRT_PatrolChaseMode::FollowPlayer;

	/** 发现玩家瞬间附带的永久加速（对应 PatrollingEnemy.speed_modification_on_player_detection = 300） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patrol")
	float SpeedBonusOnDetect = 300.f;
};

/** 发现玩家（对应 Godot 巡逻 Behavior 的 detected_player 信号） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBRTOnPlayerDetected);

/**
 * 敌人角色。
 *
 * 除移动外，还提供 Godot 侧 Unit / Enemy 里与移动相关的公共入口：
 *   BonusSpeed / DecayingBonusSpeed  -> unit.gd 的 bonus_speed / decaying_bonus_speed
 *   KnockbackVelocity                -> unit.gd 的 _knockback_vector
 *   StartLockedMove                  -> charging_attack_behavior.gd 的 _move_locked 冲撞
 */
UCLASS()
class BROTATOWITHUE5_API ABRT_EnemyCharacter : public ABRT_BaseCharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABRT_EnemyCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// =================================================================================
	// 策略配置（对应 Godot enemy.tscn 的 MovementBehavior 子节点 + 各 Behavior 的 export 字段）
	// =================================================================================

	/** 移动策略：换一个值等价于在 Godot 里换挂一个 MovementBehavior 脚本 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Move")
	EBRT_EnemyMoveMode MoveMode = EBRT_EnemyMoveMode::FollowPlayer;

	/** MoveMode = StayInRange 时生效 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Move",
		meta = (EditCondition = "MoveMode == EBRT_EnemyMoveMode::StayInRange", EditConditionHides))
	FBRT_StayInRangeParams StayInRangeParams;

	/** MoveMode = FollowRandPosAroundPlayer 时生效（也用于巡逻模式里选「跟随玩家周围点」追击） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Move",
		meta = (EditCondition = "MoveMode == EBRT_EnemyMoveMode::FollowRandPosAroundPlayer || MoveMode == EBRT_EnemyMoveMode::PatrolEdgesOfMap", EditConditionHides))
	FBRT_RandPosAroundPlayerParams FollowRandPosParams;

	/** MoveMode = TargetRandPosAroundPlayer 时生效（也用于巡逻模式里选「玩家周围随机点」追击） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Move",
		meta = (EditCondition = "MoveMode == EBRT_EnemyMoveMode::TargetRandPosAroundPlayer || MoveMode == EBRT_EnemyMoveMode::PatrolEdgesOfMap", EditConditionHides))
	FBRT_RandPosAroundPlayerParams TargetRandPosAroundPlayerParams;

	/** MoveMode = PatrolEdgesOfMap 时生效 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Move",
		meta = (EditCondition = "MoveMode == EBRT_EnemyMoveMode::PatrolEdgesOfMap", EditConditionHides))
	FBRT_PatrolParams PatrolParams;

	// ---------------------------------------------------------------------------------
	// 通用移动参数（对应 unit.gd / stats.gd 里跟速度相关的字段）
	// ---------------------------------------------------------------------------------

	/** 基础移动速度（px/s ≈ uu/s，对应 Godot stats.speed，001 号小怪为 250） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Move|Speed", meta = (ClampMin = "0.0"))
	float BaseMoveSpeed = 250.f;

	/** 速度个体随机化（对应 stats.speed_randomization），出生时抽一次，避免整群怪完全同步 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Move|Speed", meta = (ClampMin = "0.0"))
	float SpeedRandomization = 50.f;

	/** 击退抗性 0~1（对应 stats.knockback_resistance），1 = 完全免疫击退 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Move|Speed", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float KnockbackResistance = 0.f;

	/** 击退衰减系数：Godot 每物理帧 linear_interpolate(ZERO, 0.1)，这里换算成与帧率无关的衰减 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Move|Speed", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float KnockbackDecayPerFrame = 0.1f;

	/** 到达目标点的判定容差（px，对应 MovementBehavior.EQUALITY_PRECISION = 10） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Move|Speed", meta = (ClampMin = "0.0"))
	float TargetReachTolerance = 10.f;

	/** 活动区域左上角（对应 entity.init(zone_min_pos) 注入的范围） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Move|Area")
	FVector2D MoveAreaMin = FVector2D(-1500.f, -1500.f);

	/** 活动区域右下角（对应 entity.init(zone_max_pos) 注入的范围，需保证 > MoveAreaMin） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Move|Area")
	FVector2D MoveAreaMax = FVector2D(1500.f, 1500.f);

	/** 贴边巡逻时离边框的留白（对应 Utils.EDGE_MAP_DIST = 64） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Move|Area", meta = (ClampMin = "0.0"))
	float MoveAreaEdgePadding = 64.f;

	/** 画出当前目标点与移动方向（调参用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Move|Debug")
	bool bDrawMoveDebug = false;

	// =================================================================================
	// 运行时状态（对应 unit.gd 的成员，全部只读暴露给蓝图/调试）
	// =================================================================================

	/** 当前追击/游走目标点（对应 GetTargetPosition） */
	UPROPERTY(BlueprintReadOnly, Category = "BRT|Move|Runtime")
	FVector2D CurrentTargetPosition = FVector2D::ZeroVector;

	/** 当前移动方向，未归一化（对应 unit.gd 的 _current_movement） */
	UPROPERTY(BlueprintReadOnly, Category = "BRT|Move|Runtime")
	FVector2D MoveDirection = FVector2D::ZeroVector;

	/** 是否已被发现玩家（仅 PatrolEdgesOfMap 有意义） */
	UPROPERTY(BlueprintReadOnly, Category = "BRT|Move|Runtime")
	bool bPlayerDetected = false;

	/** 移动是否被攻击行为接管（冲撞中，对应 _move_locked） */
	UPROPERTY(BlueprintReadOnly, Category = "BRT|Move|Runtime")
	bool bMoveLocked = false;

	/** 永久速度加成（对应 bonus_speed） */
	UPROPERTY(BlueprintReadOnly, Category = "BRT|Move|Runtime")
	float BonusSpeed = 0.f;

	/** 衰减速度加成（对应 decaying_bonus_speed，撞人后减速等） */
	UPROPERTY(BlueprintReadOnly, Category = "BRT|Move|Runtime")
	float DecayingBonusSpeed = 0.f;

	/** 击退速度（对应 _knockback_vector），会从移动速度里被减掉 */
	UPROPERTY(BlueprintReadOnly, Category = "BRT|Move|Runtime")
	FVector2D KnockbackVelocity = FVector2D::ZeroVector;

	/** 移动目标，对应 Godot 的 player_ref；为空时自动取本地 PlayerPawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Move")
	TObjectPtr<AActor> MoveTargetActor = nullptr;

	// =================================================================================
	// 对外接口
	// =================================================================================

	/** 设置活动区域（由刷怪器/关卡传入，对应 entity.init(zone_min, zone_max)） */
	UFUNCTION(BlueprintCallable, Category = "BRT|Move")
	void SetMoveArea(const FVector2D& InMin, const FVector2D& InMax);

	/** 设置移动目标（对应 enemy.init(player_ref, ...)） */
	UFUNCTION(BlueprintCallable, Category = "BRT|Move")
	void SetMoveTarget(AActor* InTarget);

	/**
	 * 出生/重置时抽随机值，对应各 MovementBehavior 的 init()：
	 * 实际保持距离、玩家周围偏移点、边缘距离、速度个体随机值，以及目标点失效。
	 */
	UFUNCTION(BlueprintCallable, Category = "BRT|Move")
	void ResetMoveBehavior();

	/** 目标点（对应 MovementBehavior.get_target_position） */
	UFUNCTION(BlueprintPure, Category = "BRT|Move")
	FVector2D GetTargetPosition() const { return CurrentTargetPosition; }

	/** 移动方向，未归一化（对应 MovementBehavior.get_movement） */
	UFUNCTION(BlueprintPure, Category = "BRT|Move")
	FVector2D GetMoveDirection() const { return MoveDirection; }

	/** 移动输入 = 方向归一化 × 当前速度（对应 Unit.get_move_input） */
	UFUNCTION(BlueprintPure, Category = "BRT|Move")
	FVector2D GetMoveInput() const;

	/** 当前移动速度 = 基础速度 × 全局敌人速度% + 各种加成（对应 Unit.reset_stats + get_move_input） */
	UFUNCTION(BlueprintPure, Category = "BRT|Move")
	float GetCurrentMoveSpeed() const;

	/** 击退速度（已计入击退抗性，对应 Unit.get_knockback_value） */
	UFUNCTION(BlueprintPure, Category = "BRT|Move")
	FVector2D GetKnockbackValue() const;

	/** 最终期望速度 = 移动输入 - 击退（对应 Unit.get_next_velocity） */
	UFUNCTION(BlueprintPure, Category = "BRT|Move")
	FVector2D GetDesiredVelocity() const;

	/** 是否正处于被击退状态（对应 enemy.gd：|击退| > |移动输入|，此时不造成接触伤害） */
	UFUNCTION(BlueprintPure, Category = "BRT|Move")
	bool IsBeingKnockedBack() const;

	/** 永久加速（对应 bonus_speed += value） */
	UFUNCTION(BlueprintCallable, Category = "BRT|Move")
	void AddBonusSpeed(float Delta);

	/** 衰减加减速（对应 Unit.add_decaying_speed，撞到东西时传负值） */
	UFUNCTION(BlueprintCallable, Category = "BRT|Move")
	void AddDecayingSpeed(float Value);

	/** 施加击退（对应 Unit.take_damage 里写 _knockback_vector） */
	UFUNCTION(BlueprintCallable, Category = "BRT|Move")
	void ApplyKnockback(const FVector2D& Direction, float Strength);

	/**
	 * 锁定移动方向并持续 Duration 秒（对应 ChargingAttackBehavior 的冲撞）：
	 * 期间忽略策略、按 LockedSpeed 沿 Direction 直线冲，结束后自动还原。
	 * @param Direction 冲撞方向（会自动归一化）
	 * @param LockedSpeed 冲撞时叠加到基础速度上的额外速度（Godot charge_speed）
	 * @param Duration 锁定时长（Godot charge_duration）
	 */
	UFUNCTION(BlueprintCallable, Category = "BRT|Move")
	void StartLockedMove(const FVector2D& Direction, float LockedSpeed, float Duration);

	/** 手动解除移动锁定（一般由计时器自动调用） */
	UFUNCTION(BlueprintCallable, Category = "BRT|Move")
	void StopLockedMove();

	/** 标记「已发现玩家」：切追击 + 一次性加速（对应 patrol_edges_of_map.gd 的 detected_player） */
	UFUNCTION(BlueprintCallable, Category = "BRT|Move")
	void NotifyPlayerDetected();

	/** 发现玩家广播（C++/蓝图都能监听） */
	UPROPERTY(BlueprintAssignable, Category = "BRT|Move")
	FBRTOnPlayerDetected OnPlayerDetected;

	/** 接触伤害开关变化（对应 enemy.gd 里被击退时 _hitbox.disable()，由蓝图关掉攻击判定） */
	UFUNCTION(BlueprintImplementableEvent, Category = "BRT|Move")
	void OnContactDamageSuppressedChanged(bool bSuppressed);

protected:
	// =================================================================================
	// 移动内核
	// =================================================================================

	/** 每帧刷新目标点（对应各 Behavior 的 get_target_position，随机点类策略在这里「到点换点」） */
	virtual void UpdateMoveTarget();

	/** 巡逻模式的目标刷新：先判感知，再决定追击还是继续贴边 */
	void UpdatePatrolTarget(const FVector2D& PlayerPos, const FVector2D& MyPos);

	/** 把期望速度写进 CharacterMovement（对应 Unit._integrate_forces 里写 linear_velocity） */
	virtual void ApplyDesiredVelocity(const FVector2D& DesiredVelocity);

	/** 立即停下（到点、死亡、速度为 0） */
	void StopMovement();

	/** 衰减类数值的每帧结算（对应 Unit._physics_process 末尾 + get_next_knockback_value 的插值衰减） */
	void UpdateDecayingValues(float DeltaTime);

	// ---- 各策略的目标点计算（一一对应 movement_behaviors/*.gd） ----

	/** StayInRange：玩家位置往自己这边推 ActualRange 的距离 */
	FVector2D MakeStayInRangeTarget(const FVector2D& PlayerPos, const FVector2D& MyPos) const;

	/** FollowRandPosAroundPlayer：玩家位置 + 出生时抽好的固定偏移 */
	FVector2D MakeFollowAroundPlayerTarget(const FVector2D& PlayerPos) const;

	/** TargetRandPosAroundPlayer：在玩家周围重新抽一个点（会被夹进活动区域） */
	FVector2D MakeTargetAroundPlayerTarget(const FVector2D& PlayerPos) const;

	/** TargetRandPos：活动区域内随机一个点 */
	FVector2D MakeRandomAreaTarget() const;

	/** 巡逻：挑一个贴边的巡逻点 */
	FVector2D MakePatrolTarget(const FVector2D& MyPos) const;

	/** 巡逻发现玩家后按 ChaseMode 算追击目标 */
	FVector2D MakeChaseTarget(EBRT_PatrolChaseMode InMode, const FVector2D& PlayerPos, const FVector2D& MyPos) const;

	// ---- 工具 ----

	/** 在半径 Range 内随机一个偏移（bAllowWithin = false 时推到圆环上） */
	static FVector2D MakeRandomOffset(float Range, bool bAllowWithin);

	/** 判断 pos 贴哪条边最近（对应 Utils.get_direction_from_pos） */
	EBRT_MapEdge GetNearestMapEdge(const FVector2D& Pos) const;

	/** 在指定边附近随机一个点（对应 Utils.get_rand_pos_from_direction_within_distance） */
	FVector2D GetRandPosOnEdge(EBRT_MapEdge Edge, float EdgeDistance) const;

	/** 夹进活动区域（对应 Godot 的 clamp(min_pos, max_pos)） */
	FVector2D ClampToMoveArea(const FVector2D& Pos) const;

	/** 是否已到达目标点（无效目标/在容差内都算到达） */
	bool HasReachedTarget(const FVector2D& Target) const;

	/** 世界坐标取 XY 平面（Godot 的 2D 坐标就活在这个平面里） */
	FVector2D GetActorLocation2D() const;

	/** 目标 Actor 的 XY 坐标 */
	FVector2D GetTargetLocation2D() const;

	/** 只算基础速度部分（含全局敌人速度%） */
	float GetBaseMoveSpeed() const;

	/** 全局敌人速度加成（对应 Utils.get_stat("enemy speed") / 100，来自 GAS 的 EnemySpeed 属性） */
	float GetEnemySpeedMultiplier() const;

	/** 调试绘制 */
	void DrawMoveDebug() const;

protected:
	// =================================================================================
	// 内部状态（Godot 里散落在各 Behavior 脚本的 var，这里统一放在角色上）
	// =================================================================================

	/** 锁定的移动方向（冲撞中取代策略计算出来的方向） */
	FVector2D LockedMoveDirection = FVector2D::ZeroVector;

	/** 锁定前保存的 BonusSpeed，解锁时还原，避免冲撞把敌人自身的永久加速清掉 */
	float CachedBonusSpeedBeforeLock = 0.f;

	/** 速度个体随机值（对应 _speed_rand_value） */
	float SpeedRandomValue = 0.f;

	/** 保持距离的实际值（对应 stay_in_range._actual_target_range） */
	float StayInRangeActualRange = 0.f;

	/** 跟随玩家周围点的实际半径（对应 follow_rand_pos._actual） */
	float FollowAroundActualRange = 0.f;

	/** 玩家周围随机点的实际半径（对应 target_rand_pos_around_player._actual） */
	float TargetAroundActualRange = 0.f;

	/** 出生时抽好的玩家周围偏移（对应 _distance_from_player） */
	FVector2D AroundPlayerOffset = FVector2D::ZeroVector;

	/** 巡逻贴边的实际距离（对应 patrol._actual） */
	float PatrolActualEdgeDistance = 0.f;

	/** 目标点是否有效（Godot 用 _current_target == Vector2.ZERO 当哨兵值，这里用显式标记更稳） */
	bool bTargetValid = false;

	/** 接触伤害是否处于被抑制状态（被击退时） */
	bool bContactDamageSuppressed = false;

	/** 冲撞解锁计时器 */
	FTimerHandle UnlockMoveTimerHandle;
};
