// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BRT_EnemyCharacter.h"

#include "Animation/BRT_PaperDollAnimComponent.h"
#include "BrotatoGAS/AttributeSet/BRT_AttributeSet.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"


// Sets default values
ABRT_EnemyCharacter::ABRT_EnemyCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// ---- 2D 平面化：Brotato 是俯视 2D，敌人只在 XY 平面移动、不吃重力 ----
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = false;   // 朝向由纸娃娃镜像翻转控制，不能转 Actor
	MoveComp->bUseControllerDesiredRotation = false;
	MoveComp->bConstrainToPlane = true;
	MoveComp->SetPlaneConstraintNormal(FVector(0.f, 0.f, 1.f));
	MoveComp->bSnapToPlaneAtStart = true;
	MoveComp->GravityScale = 0.f;

	// 敌人没有 AIController（Godot 里敌人是纯物理体），允许「无 Controller 也走物理」
	MoveComp->bRunPhysicsWithNoController = true;

	// Godot 是每物理帧直接写 rigidbody 速度，没有加速过程；
	// 这里把加速度/刹车拉到极大，等价于「一帧内到目标速度 / 一帧内停住」
	MoveComp->MaxAcceleration = 1.0e6f;
	MoveComp->BrakingDecelerationWalking = 1.0e6f;
	MoveComp->BrakingDecelerationFlying = 1.0e6f;
	MoveComp->MaxWalkSpeed = BaseMoveSpeed;
	MoveComp->MaxFlySpeed = BaseMoveSpeed;

	// Godot TargetRandPosAroundPlayerMovementBehavior 的默认值：range=300 / randomization=100
	TargetRandPosAroundPlayerParams.RangeAroundPlayer = 300.f;
	TargetRandPosAroundPlayerParams.RangeRandomization = 100.f;
}

// Called when the game starts or when spawned
void ABRT_EnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (PaperDollAnim)
	{
		PaperDollAnim->PlayAnimByName(FName(TEXT("idle")));
		//参考来自Enemy.gd 给敌人动画随机变速
		PaperDollAnim->SetPlayRate(FMath::FRandRange(1.f, 3.f));
	}

	// 飞行模式 + 平面约束：2D 没有地板，不需要落地/爬坡这些逻辑
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Flying);
	}

	// 对应 entity_spawner.gd 刷怪时把 _player 注进去；没配就用本地玩家兜底
	if (!MoveTargetActor)
	{
		MoveTargetActor = UGameplayStatics::GetPlayerPawn(this, 0);
	}

	ResetMoveBehavior();
}

// Called every frame
void ABRT_EnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!IsAlive())
	{
		StopMovement();
		return;
	}

	// 1) 刷新目标点（对应各 Behavior 的 get_target_position，随机点类策略在这里「到点换点」）
	UpdateMoveTarget();

	// 2) 刷新移动方向（对应 Unit._physics_process：_current_movement = get_movement()）
	MoveDirection = bMoveLocked ? LockedMoveDirection : (CurrentTargetPosition - GetActorLocation2D());

	// 3) 衰减类数值结算（对应 Unit._physics_process 末尾 + get_next_knockback_value 的插值衰减）
	UpdateDecayingValues(DeltaTime);

	// 4) 把速度写进 CharacterMovement（对应 Unit._integrate_forces 写 linear_velocity）
	ApplyDesiredVelocity(GetDesiredVelocity());

	// 5) 被击退时不能造成接触伤害（对应 enemy.gd 里 hitbox.disable() / enable()）
	const bool bKnockedBack = IsBeingKnockedBack();
	if (bKnockedBack != bContactDamageSuppressed)
	{
		bContactDamageSuppressed = bKnockedBack;
		OnContactDamageSuppressedChanged(bContactDamageSuppressed);
	}

	if (bDrawMoveDebug)
	{
		DrawMoveDebug();
	}
}

// Called to bind functionality to input
void ABRT_EnemyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

// =====================================================================================================================
// 目标点计算：一一对应 movement_behaviors/*.gd 的 get_target_position
// =====================================================================================================================

void ABRT_EnemyCharacter::UpdateMoveTarget()
{
	// 目标丢失（玩家死亡/切场景）时懒查找一次，和 Godot 里 player_ref 为 null 的兜底一致
	if (!IsValid(MoveTargetActor))
	{
		MoveTargetActor = UGameplayStatics::GetPlayerPawn(this, 0);
	}
	if (!IsValid(MoveTargetActor))
	{
		CurrentTargetPosition = GetActorLocation2D();
		bTargetValid = false;
		return;
	}

	const FVector2D MyPos = GetActorLocation2D();
	const FVector2D PlayerPos = GetTargetLocation2D();

	switch (MoveMode)
	{
	case EBRT_EnemyMoveMode::FollowPlayer:
		// FollowPlayerMovementBehavior：每帧直接盯玩家当前位置，没有感知范围，全图追
		CurrentTargetPosition = PlayerPos;
		bTargetValid = true;
		break;

	case EBRT_EnemyMoveMode::StayInRange:
		// StayInRangeFromPlayerMovementBehavior：站在「玩家指向自己」这条射线上固定距离处
		CurrentTargetPosition = MakeStayInRangeTarget(PlayerPos, MyPos);
		bTargetValid = true;
		break;

	case EBRT_EnemyMoveMode::FollowRandPosAroundPlayer:
		// FollowRandPosAroundPlayerMovementBehavior：偏移点是出生时抽好的，跟着玩家平移
		CurrentTargetPosition = MakeFollowAroundPlayerTarget(PlayerPos);
		bTargetValid = true;
		break;

	case EBRT_EnemyMoveMode::TargetRandPosAroundPlayer:
		// TargetRandPosAroundPlayerMovementBehavior：到点后才在玩家周围重新抽一个点
		if (HasReachedTarget(CurrentTargetPosition))
		{
			CurrentTargetPosition = MakeTargetAroundPlayerTarget(PlayerPos);
			bTargetValid = true;
		}
		break;

	case EBRT_EnemyMoveMode::TargetRandPos:
		// TargetRandPosMovementBehavior：全地图随机游走，不看玩家
		if (HasReachedTarget(CurrentTargetPosition))
		{
			CurrentTargetPosition = MakeRandomAreaTarget();
			bTargetValid = true;
		}
		break;

	case EBRT_EnemyMoveMode::PatrolEdgesOfMap:
		UpdatePatrolTarget(PlayerPos, MyPos);
		break;

	default:
		CurrentTargetPosition = PlayerPos;
		bTargetValid = true;
		break;
	}
}

void ABRT_EnemyCharacter::UpdatePatrolTarget(const FVector2D& PlayerPos, const FVector2D& MyPos)
{
	// 对应 patrol_edges_of_map.gd:35-38：进入感知范围就 emit detected_player 并切追击
	if (!bPlayerDetected && (PlayerPos - MyPos).Size() <= PatrolParams.DetectionRange)
	{
		NotifyPlayerDetected();
	}

	if (bPlayerDetected)
	{
		// 「玩家周围随机点」这类策略只在到点后换点，不能每帧重抽，否则会原地抖
		if (PatrolParams.ChaseMode == EBRT_PatrolChaseMode::TargetRandPosAroundPlayer)
		{
			if (HasReachedTarget(CurrentTargetPosition))
			{
				CurrentTargetPosition = MakeTargetAroundPlayerTarget(PlayerPos);
				bTargetValid = true;
			}
		}
		else
		{
			CurrentTargetPosition = MakeChaseTarget(PatrolParams.ChaseMode, PlayerPos, MyPos);
			bTargetValid = true;
		}
		return;
	}

	// 还没发现玩家：贴着地图边缘一个点一个点地巡
	if (HasReachedTarget(CurrentTargetPosition))
	{
		CurrentTargetPosition = MakePatrolTarget(MyPos);
		bTargetValid = true;
	}
}

FVector2D ABRT_EnemyCharacter::MakeStayInRangeTarget(const FVector2D& PlayerPos, const FVector2D& MyPos) const
{
	// Godot: player_ref.global_position + _actual_target_range * (my_pos - player_pos).normalized()
	const FVector2D Dir = (MyPos - PlayerPos).GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		// 和玩家完全重合时随便给个方向，避免退化
		return PlayerPos + FVector2D(StayInRangeActualRange, 0.f);
	}
	return PlayerPos + Dir * StayInRangeActualRange;
}

FVector2D ABRT_EnemyCharacter::MakeFollowAroundPlayerTarget(const FVector2D& PlayerPos) const
{
	// Godot: player_ref.global_position + _distance_from_player（固定偏移，不重抽）
	return PlayerPos + AroundPlayerOffset;
}

FVector2D ABRT_EnemyCharacter::MakeTargetAroundPlayerTarget(const FVector2D& PlayerPos) const
{
	// Godot: player_ref.global_position + Vector2(rand(-actual, actual), rand(-actual, actual))，再 clamp 回地图
	return ClampToMoveArea(PlayerPos + MakeRandomOffset(TargetAroundActualRange, true));
}

FVector2D ABRT_EnemyCharacter::MakeRandomAreaTarget() const
{
	// 对应 target_rand_pos_movement_behavior.gd: 在 _min_pos/_max_pos 之间随机
	const FVector2D PaddedMin = MoveAreaMin + FVector2D(MoveAreaEdgePadding, MoveAreaEdgePadding);
	const FVector2D PaddedMax = MoveAreaMax - FVector2D(MoveAreaEdgePadding, MoveAreaEdgePadding);
	return ClampToMoveArea(FVector2D(
		FMath::FRandRange(FMath::Min(PaddedMin.X, PaddedMax.X), FMath::Max(PaddedMin.X, PaddedMax.X)),
		FMath::FRandRange(FMath::Min(PaddedMin.Y, PaddedMax.Y), FMath::Max(PaddedMin.Y, PaddedMax.Y))));
}

FVector2D ABRT_EnemyCharacter::MakePatrolTarget(const FVector2D& MyPos) const
{
	// 对应 patrol_edges_of_map_movement_behavior.gd: 先看自己贴着哪条边，再在同一条边附近随机一个点
	const EBRT_MapEdge Edge = GetNearestMapEdge(MyPos);
	FVector2D Target = GetRandPosOnEdge(Edge, PatrolActualEdgeDistance);

	// Godot 里 max_target_distance 的本意是「单次巡逻目标别太远」，
	// 反编译出来的代码把方向和绝对坐标混在一起了（会把怪拉向奇怪的位置），这里按设计意图实现
	const FVector2D ToTarget = Target - MyPos;
	if (PatrolParams.MaxTargetDistance > 0.f && ToTarget.Size() > PatrolParams.MaxTargetDistance)
	{
		Target = MyPos + ToTarget.GetSafeNormal() * PatrolParams.MaxTargetDistance;
	}

	return ClampToMoveArea(Target);
}

FVector2D ABRT_EnemyCharacter::MakeChaseTarget(EBRT_PatrolChaseMode InMode, const FVector2D& PlayerPos, const FVector2D& MyPos) const
{
	switch (InMode)
	{
	case EBRT_PatrolChaseMode::StayInRange:
		return MakeStayInRangeTarget(PlayerPos, MyPos);
	case EBRT_PatrolChaseMode::FollowRandPosAroundPlayer:
		return MakeFollowAroundPlayerTarget(PlayerPos);
	case EBRT_PatrolChaseMode::TargetRandPosAroundPlayer:
		return MakeTargetAroundPlayerTarget(PlayerPos);
	case EBRT_PatrolChaseMode::FollowPlayer:
	default:
		return PlayerPos;
	}
}

// =====================================================================================================================
// 速度 / 位移：对应 unit.gd 的 get_move_input / get_next_velocity / _integrate_forces
// =====================================================================================================================

FVector2D ABRT_EnemyCharacter::GetMoveInput() const
{
	// Godot: _current_movement.normalized() * max(0, speed + bonus)
	const float Speed = GetCurrentMoveSpeed();
	if (MoveDirection.IsNearlyZero() || Speed <= 0.f)
	{
		return FVector2D::ZeroVector;
	}
	return MoveDirection.GetSafeNormal() * Speed;
}

float ABRT_EnemyCharacter::GetCurrentMoveSpeed() const
{
	return FMath::Max(0.f, GetBaseMoveSpeed() + BonusSpeed + DecayingBonusSpeed + SpeedRandomValue);
}

FVector2D ABRT_EnemyCharacter::GetKnockbackValue() const
{
	// Godot: _knockback_vector * (100 - knockback_resistance * 100)，这里用 0~1 的抗性表达同一件事
	return KnockbackVelocity * (1.f - FMath::Clamp(KnockbackResistance, 0.f, 1.f));
}

FVector2D ABRT_EnemyCharacter::GetDesiredVelocity() const
{
	// Godot: move_input - knockback
	return GetMoveInput() - GetKnockbackValue();
}

bool ABRT_EnemyCharacter::IsBeingKnockedBack() const
{
	// Godot enemy.gd：get_knockback_value().length() > get_move_input().length()
	return GetKnockbackValue().SizeSquared() > GetMoveInput().SizeSquared();
}

void ABRT_EnemyCharacter::ApplyDesiredVelocity(const FVector2D& DesiredVelocity)
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!IsValid(MoveComp))
	{
		return;
	}

	const float DesiredSpeed = DesiredVelocity.Size();

	// 到点 / 死亡 / 速度为 0：不给输入，靠极大的刹车系数瞬间停住
	if (DesiredSpeed <= 0.01f)
	{
		StopMovement();
		return;
	}

	// CMC 的速度上限就是 MaxFlySpeed / MaxWalkSpeed：
	// 每帧把它对齐成期望速度，再用归一化方向喂输入，
	// 等价于 Godot 的 state.linear_velocity = move_input - knockback（击退可以比当前速度更大，所以不能用固定上限）
	MoveComp->MaxFlySpeed = DesiredSpeed;
	MoveComp->MaxWalkSpeed = DesiredSpeed;

	const FVector2D Dir = DesiredVelocity / DesiredSpeed;
	// bForce = true：敌人没有 Controller，也要把输入塞进 ControlInputVector
	AddMovementInput(FVector(Dir.X, Dir.Y, 0.f), 1.f, /*bForce=*/true);
}

void ABRT_EnemyCharacter::StopMovement()
{
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
	}
}

void ABRT_EnemyCharacter::UpdateDecayingValues(float DeltaTime)
{
	// 以 60Hz 物理步为基准做帧率归一（Godot 里这些都是「每物理帧」的量）
	const float PhysicsSteps = DeltaTime * 60.f;

	// 对应 Unit._physics_process 末尾的 decaying_bonus_speed 结算（加得慢、减得快）
	if (DecayingBonusSpeed > 0.f)
	{
		DecayingBonusSpeed = FMath::Max(0.f, DecayingBonusSpeed - PhysicsSteps);
	}
	else if (DecayingBonusSpeed < 0.f)
	{
		DecayingBonusSpeed = FMath::Min(0.f, DecayingBonusSpeed + PhysicsSteps * 5.f);
	}

	// 对应 _knockback_vector = _knockback_vector.linear_interpolate(Vector2.ZERO, 0.1)
	const float ClampedAlpha = FMath::Clamp(KnockbackDecayPerFrame, 0.f, 0.99f);
	const float Alpha = FMath::Clamp(1.f - FMath::Pow(1.f - ClampedAlpha, PhysicsSteps), 0.f, 1.f);
	KnockbackVelocity = FMath::Lerp(KnockbackVelocity, FVector2D::ZeroVector, Alpha);

	if (KnockbackVelocity.SizeSquared() < 1.f)
	{
		KnockbackVelocity = FVector2D::ZeroVector;
	}
}

// =====================================================================================================================
// 外部接口
// =====================================================================================================================

void ABRT_EnemyCharacter::SetMoveArea(const FVector2D& InMin, const FVector2D& InMax)
{
	MoveAreaMin = FVector2D(FMath::Min(InMin.X, InMax.X), FMath::Min(InMin.Y, InMax.Y));
	MoveAreaMax = FVector2D(FMath::Max(InMin.X, InMax.X), FMath::Max(InMin.Y, InMax.Y));
}

void ABRT_EnemyCharacter::SetMoveTarget(AActor* InTarget)
{
	MoveTargetActor = InTarget;
	bPlayerDetected = false;
}

void ABRT_EnemyCharacter::ResetMoveBehavior()
{
	// 对应各 MovementBehavior 的 init()：出生时把随机值抽一次并固定下来
	SpeedRandomValue = FMath::FRandRange(-SpeedRandomization, SpeedRandomization);

	StayInRangeActualRange = FMath::Max(0.f, StayInRangeParams.TargetRange
		+ FMath::FRandRange(-StayInRangeParams.RangeRandomization, StayInRangeParams.RangeRandomization));

	FollowAroundActualRange = FMath::Max(0.f, FollowRandPosParams.RangeAroundPlayer
		+ FMath::FRandRange(-FollowRandPosParams.RangeRandomization, FollowRandPosParams.RangeRandomization));

	TargetAroundActualRange = FMath::Max(0.f, TargetRandPosAroundPlayerParams.RangeAroundPlayer
		+ FMath::FRandRange(-TargetRandPosAroundPlayerParams.RangeRandomization, TargetRandPosAroundPlayerParams.RangeRandomization));

	PatrolActualEdgeDistance = FMath::Max(0.f, PatrolParams.EdgeDistance
		+ FMath::FRandRange(-PatrolParams.EdgeRandomization, PatrolParams.EdgeRandomization));

	AroundPlayerOffset = MakeRandomOffset(FollowAroundActualRange, FollowRandPosParams.bAllowWithin);

	// 先解锁（会还原 BonusSpeed），再把速度类状态清空
	StopLockedMove();

	bPlayerDetected = false;
	bTargetValid = false;
	CurrentTargetPosition = GetActorLocation2D();
	MoveDirection = FVector2D::ZeroVector;
	BonusSpeed = 0.f;
	DecayingBonusSpeed = 0.f;
	KnockbackVelocity = FVector2D::ZeroVector;
}

void ABRT_EnemyCharacter::AddBonusSpeed(float Delta)
{
	BonusSpeed += Delta;
}

void ABRT_EnemyCharacter::AddDecayingSpeed(float Value)
{
	// 对应 Unit.add_decaying_speed：已经严重减速时不再叠加，负值再打折
	float NewValue = Value;
	if (DecayingBonusSpeed < -(GetBaseMoveSpeed() * 0.8f))
	{
		NewValue = 0.f;
	}
	else if (DecayingBonusSpeed < 0.f)
	{
		NewValue /= 20.f;
	}

	DecayingBonusSpeed += NewValue;
}

void ABRT_EnemyCharacter::ApplyKnockback(const FVector2D& Direction, float Strength)
{
	if (Direction.IsNearlyZero() || Strength <= 0.f)
	{
		return;
	}

	// 对应 Unit.take_damage：_knockback_vector = knockback_direction * knockback_amount
	KnockbackVelocity = Direction.GetSafeNormal() * Strength;
}

void ABRT_EnemyCharacter::StartLockedMove(const FVector2D& Direction, float LockedSpeed, float Duration)
{
	// 对应 ChargingAttackBehavior.shoot()：_move_locked = true + bonus_speed = charge_speed
	bMoveLocked = true;
	LockedMoveDirection = Direction.GetSafeNormal();

	CachedBonusSpeedBeforeLock = BonusSpeed;
	BonusSpeed = LockedSpeed;

	GetWorldTimerManager().ClearTimer(UnlockMoveTimerHandle);
	if (Duration > 0.f)
	{
		GetWorldTimerManager().SetTimer(UnlockMoveTimerHandle, this, &ThisClass::StopLockedMove, Duration, false);
	}
}

void ABRT_EnemyCharacter::StopLockedMove()
{
	// 对应 ChargingAttackBehavior.on_unlock_move_timer_timeout()
	bMoveLocked = false;
	LockedMoveDirection = FVector2D::ZeroVector;
	BonusSpeed = CachedBonusSpeedBeforeLock;
	CachedBonusSpeedBeforeLock = 0.f;

	GetWorldTimerManager().ClearTimer(UnlockMoveTimerHandle);
}

void ABRT_EnemyCharacter::NotifyPlayerDetected()
{
	if (bPlayerDetected)
	{
		return;
	}

	bPlayerDetected = true;

	// 对应 PatrollingEnemy：发现玩家立刻加速
	BonusSpeed += PatrolParams.SpeedBonusOnDetect;

	OnPlayerDetected.Broadcast();
}

// =====================================================================================================================
// 工具
// =====================================================================================================================

FVector2D ABRT_EnemyCharacter::MakeRandomOffset(float Range, bool bAllowWithin)
{
	FVector2D Offset(FMath::FRandRange(-Range, Range), FMath::FRandRange(-Range, Range));
	if (!bAllowWithin)
	{
		// Godot: _distance_from_player.normalized() * _actual
		Offset = Offset.GetSafeNormal() * Range;
	}
	return Offset;
}

EBRT_MapEdge ABRT_EnemyCharacter::GetNearestMapEdge(const FVector2D& Pos) const
{
	// 对应 Utils.get_direction_from_pos，但那边在「四条边都不近」时返回 NONE（反编译版会把目标点算成原点），
	// 这里改成永远返回最近的一条边，巡逻怪自然会先走向最近的边
	const float LeftDist = Pos.X - MoveAreaMin.X;
	const float RightDist = MoveAreaMax.X - Pos.X;
	const float TopDist = Pos.Y - MoveAreaMin.Y;
	const float BottomDist = MoveAreaMax.Y - Pos.Y;

	EBRT_MapEdge Edge = EBRT_MapEdge::Left;
	float MinDist = LeftDist;

	if (RightDist < MinDist)
	{
		MinDist = RightDist;
		Edge = EBRT_MapEdge::Right;
	}
	if (TopDist < MinDist)
	{
		MinDist = TopDist;
		Edge = EBRT_MapEdge::Top;
	}
	if (BottomDist < MinDist)
	{
		MinDist = BottomDist;
		Edge = EBRT_MapEdge::Bottom;
	}

	return Edge;
}

FVector2D ABRT_EnemyCharacter::GetRandPosOnEdge(EBRT_MapEdge Edge, float EdgeDistance) const
{
	// 对应 Utils.get_rand_pos_from_direction_within_distance：
	// 沿着这条边跑，纵向/横向只在「距边 EdgeDistance」以内取值
	const FVector2D PaddedMin = MoveAreaMin + FVector2D(MoveAreaEdgePadding, MoveAreaEdgePadding);
	const FVector2D PaddedMax = MoveAreaMax - FVector2D(MoveAreaEdgePadding, MoveAreaEdgePadding);

	const float MinX = FMath::Min(PaddedMin.X, PaddedMax.X);
	const float MaxX = FMath::Max(PaddedMin.X, PaddedMax.X);
	const float MinY = FMath::Min(PaddedMin.Y, PaddedMax.Y);
	const float MaxY = FMath::Max(PaddedMin.Y, PaddedMax.Y);
	const float Range = FMath::Max(0.f, EdgeDistance);

	switch (Edge)
	{
	case EBRT_MapEdge::Top:
		return FVector2D(FMath::FRandRange(MinX, MaxX), FMath::FRandRange(MinY, FMath::Min(MinY + Range, MaxY)));
	case EBRT_MapEdge::Bottom:
		return FVector2D(FMath::FRandRange(MinX, MaxX), FMath::FRandRange(FMath::Max(MaxY - Range, MinY), MaxY));
	case EBRT_MapEdge::Left:
		return FVector2D(FMath::FRandRange(MinX, FMath::Min(MinX + Range, MaxX)), FMath::FRandRange(MinY, MaxY));
	case EBRT_MapEdge::Right:
		return FVector2D(FMath::FRandRange(FMath::Max(MaxX - Range, MinX), MaxX), FMath::FRandRange(MinY, MaxY));
	case EBRT_MapEdge::None:
	default:
		return MakeRandomAreaTarget();
	}
}

FVector2D ABRT_EnemyCharacter::ClampToMoveArea(const FVector2D& Pos) const
{
	return FVector2D(
		FMath::Clamp(Pos.X, MoveAreaMin.X, MoveAreaMax.X),
		FMath::Clamp(Pos.Y, MoveAreaMin.Y, MoveAreaMax.Y));
}

bool ABRT_EnemyCharacter::HasReachedTarget(const FVector2D& Target) const
{
	// Godot 用 _current_target == Vector2.ZERO 当「没有目标」的哨兵值，这里用显式标记
	if (!bTargetValid)
	{
		return true;
	}
	return (Target - GetActorLocation2D()).Size() <= TargetReachTolerance;
}

FVector2D ABRT_EnemyCharacter::GetActorLocation2D() const
{
	const FVector Loc = GetActorLocation();
	return FVector2D(Loc.X, Loc.Y);
}

FVector2D ABRT_EnemyCharacter::GetTargetLocation2D() const
{
	if (!IsValid(MoveTargetActor))
	{
		return GetActorLocation2D();
	}
	const FVector Loc = MoveTargetActor->GetActorLocation();
	return FVector2D(Loc.X, Loc.Y);
}

float ABRT_EnemyCharacter::GetBaseMoveSpeed() const
{
	// Godot unit.reset_stats(): base_speed = (stats.speed + stats.speed * enemy_speed%) * (1 + speed_coef)
	return BaseMoveSpeed * GetEnemySpeedMultiplier();
}

float ABRT_EnemyCharacter::GetEnemySpeedMultiplier() const
{
	const UBRT_AttributeSet* Attributes = Cast<UBRT_AttributeSet>(GetAttributeSet());
	if (!IsValid(Attributes))
	{
		return 1.f;
	}

	// GAS 里 EnemySpeed 单位是 %（Godot: Utils.get_stat("enemy_speed") / 100）
	return FMath::Max(0.f, 1.f + Attributes->GetEnemySpeed() / 100.f);
}

void ABRT_EnemyCharacter::DrawMoveDebug() const
{
	const FVector MyLoc = GetActorLocation();
	const FVector TargetLoc(CurrentTargetPosition.X, CurrentTargetPosition.Y, MyLoc.Z);
	const FVector2D Desired = GetDesiredVelocity();

	DrawDebugSphere(GetWorld(), TargetLoc, 12.f, 8, FColor::Yellow, false, -1.f, 0, 1.f);
	DrawDebugLine(GetWorld(), MyLoc, TargetLoc, FColor::Green, false, -1.f, 0, 1.f);
	DrawDebugLine(GetWorld(), MyLoc, MyLoc + FVector(Desired.X, Desired.Y, 0.f) * 0.1f, FColor::Red, false, -1.f, 0, 2.f);
}
