// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BRT_EnemyMovementBehaviorComponent.h"

#include "BrotatoGAS/AttributeSet/BRT_AttributeSet.h"
#include "Character/BRT_BaseCharacter.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"


UBRT_EnemyMovementBehaviorComponent::UBRT_EnemyMovementBehaviorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// 和 CharacterMovement 同一个 Tick 组，才能在 BeginPlay 里把它设成「等我先算完输入」的后置
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	// Godot TargetRandPosAroundPlayerMovementBehavior 的默认值：range=300 / randomization=100
	TargetRandPosAroundPlayerParams.RangeAroundPlayer = 300.f;
	TargetRandPosAroundPlayerParams.RangeRandomization = 100.f;
}

void UBRT_EnemyMovementBehaviorComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ABRT_BaseCharacter>(GetOwner());
	if (IsValid(OwnerCharacter))
	{
		CharacterMovement = OwnerCharacter->GetCharacterMovement();
		if (IsValid(CharacterMovement))
		{
			// 保证 CMC 在本组件写完移动输入之后再消费，避免输入晚一帧生效
			CharacterMovement->AddTickPrerequisiteComponent(this);
		}
	}

	// 对应 entity_spawner.gd 刷怪时把 _player 注进去；没配就用本地玩家兜底
	if (!MoveTargetActor)
	{
		MoveTargetActor = UGameplayStatics::GetPlayerPawn(this, 0);
	}

	ResetMoveBehavior();
}

void UBRT_EnemyMovementBehaviorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(OwnerCharacter))
	{
		return;
	}
	
	if (!OwnerCharacter->IsAlive())
	{
		StopMovement();
		return;
	}


	// 0) 每帧刷新一次全局敌人速度倍率缓存、目标点计时
	CachedEnemySpeedMultiplier = QueryEnemySpeedMultiplier();
	TargetAge += DeltaTime;

	// 1) 刷新目标点（对应各 Behavior 的 get_target_position，随机点类策略在这里「到点换点」）
	UpdateMoveTarget();

	// 2) 刷新移动方向（对应 Unit._physics_process：_current_movement = get_movement()）
	//    非冲撞时把单帧速度限制在「剩余距离 / DeltaTime」以内，避免到点后全速冲过头、来回抖动
	const FVector2D ToTarget = CurrentTargetPosition - GetActorLocation2D();
	MoveDirection = bMoveLocked ? LockedMoveDirection : ToTarget;
	MoveSpeedCap = bMoveLocked
		? TNumericLimits<float>::Max()
		: ToTarget.Size() / FMath::Max(DeltaTime, KINDA_SMALL_NUMBER);

	// 3) 衰减类数值结算（对应 Unit._physics_process 末尾 + get_next_knockback_value 的插值衰减）
	UpdateDecayingValues(DeltaTime);

	// 4) 把速度写进 CharacterMovement（对应 Unit._integrate_forces 写 linear_velocity）
	ApplyDesiredVelocity(GetDesiredVelocity());

	// 5) 被击退状态切换时广播，Owner 据此开关接触伤害（对应 enemy.gd 里 hitbox.disable() / enable()）
	const bool bKnockedBack = IsBeingKnockedBack();
	if (bKnockedBack != bKnockedBackLastFrame)
	{
		bKnockedBackLastFrame = bKnockedBack;
		OnKnockedBackChanged.Broadcast(bKnockedBack);
	}

	if (bDrawMoveDebug)
	{
		DrawMoveDebug();
	}
}

void UBRT_EnemyMovementBehaviorComponent::HandleOwnerDeath()
{
	StopLockedMove();
	StopMovement();
}

// =====================================================================================================================
// 目标点计算：一一对应 movement_behaviors/*.gd 的 get_target_position
// =====================================================================================================================

void UBRT_EnemyMovementBehaviorComponent::UpdateMoveTarget()
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
	const FVector2D PlayerPos = GetMoveTargetLocation2D();

	// 卡死兜底：同一个随机目标点久久走不到（被挡住等），作废它让「到点换点」类策略重抽
	if (TargetRepathTimeout > 0.f && TargetAge > TargetRepathTimeout)
	{
		bTargetValid = false;
	}

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
			AssignTarget(MakeTargetAroundPlayerTarget(PlayerPos));
		}
		break;

	case EBRT_EnemyMoveMode::TargetRandPos:
		// TargetRandPosMovementBehavior：全地图随机游走，不看玩家
		if (HasReachedTarget(CurrentTargetPosition))
		{
			AssignTarget(MakeRandomAreaTarget());
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

void UBRT_EnemyMovementBehaviorComponent::AssignTarget(const FVector2D& NewTarget)
{
	CurrentTargetPosition = NewTarget;
	bTargetValid = true;
	TargetAge = 0.f;
}

void UBRT_EnemyMovementBehaviorComponent::UpdatePatrolTarget(const FVector2D& PlayerPos, const FVector2D& MyPos)
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
				AssignTarget(MakeTargetAroundPlayerTarget(PlayerPos));
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
		AssignTarget(MakePatrolTarget(MyPos));
	}
}

FVector2D UBRT_EnemyMovementBehaviorComponent::MakeStayInRangeTarget(const FVector2D& PlayerPos, const FVector2D& MyPos) const
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

FVector2D UBRT_EnemyMovementBehaviorComponent::MakeFollowAroundPlayerTarget(const FVector2D& PlayerPos) const
{
	// Godot: player_ref.global_position + _distance_from_player（固定偏移，不重抽）
	return PlayerPos + AroundPlayerOffset;
}

FVector2D UBRT_EnemyMovementBehaviorComponent::MakeTargetAroundPlayerTarget(const FVector2D& PlayerPos) const
{
	// Godot: player_ref.global_position + Vector2(rand(-actual, actual), rand(-actual, actual))，再 clamp 回地图
	return ClampToMoveArea(PlayerPos + MakeRandomOffset(TargetAroundActualRange, true));
}

FVector2D UBRT_EnemyMovementBehaviorComponent::MakeRandomAreaTarget() const
{
	// 对应 target_rand_pos_movement_behavior.gd: 在 _min_pos/_max_pos 之间随机
	const FVector2D PaddedMin = MoveAreaMin + FVector2D(MoveAreaEdgePadding, MoveAreaEdgePadding);
	const FVector2D PaddedMax = MoveAreaMax - FVector2D(MoveAreaEdgePadding, MoveAreaEdgePadding);
	return ClampToMoveArea(FVector2D(
		FMath::FRandRange(FMath::Min(PaddedMin.X, PaddedMax.X), FMath::Max(PaddedMin.X, PaddedMax.X)),
		FMath::FRandRange(FMath::Min(PaddedMin.Y, PaddedMax.Y), FMath::Max(PaddedMin.Y, PaddedMax.Y))));
}

FVector2D UBRT_EnemyMovementBehaviorComponent::MakePatrolTarget(const FVector2D& MyPos) const
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

FVector2D UBRT_EnemyMovementBehaviorComponent::MakeChaseTarget(EBRT_PatrolChaseMode InMode, const FVector2D& PlayerPos, const FVector2D& MyPos) const
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

FVector2D UBRT_EnemyMovementBehaviorComponent::GetMoveInput() const
{
	// Godot: _current_movement.normalized() * max(0, speed + bonus)
	const float Speed = FMath::Min(GetCurrentMoveSpeed(), MoveSpeedCap);
	if (MoveDirection.IsNearlyZero() || Speed <= 0.f)
	{
		return FVector2D::ZeroVector;
	}
	return MoveDirection.GetSafeNormal() * Speed;
}

float UBRT_EnemyMovementBehaviorComponent::GetCurrentMoveSpeed() const
{
	// 冲撞锁定期间 LockedSpeedBonus 取代 BonusSpeed（对应 Godot 里 bonus_speed = charge_speed），解锁后 BonusSpeed 原样保留
	const float Bonus = bMoveLocked ? LockedSpeedBonus : BonusSpeed;
	return FMath::Max(0.f, GetBaseMoveSpeed() + Bonus + DecayingBonusSpeed + SpeedRandomValue);
}

FVector2D UBRT_EnemyMovementBehaviorComponent::GetKnockbackValue() const
{
	// Godot: _knockback_vector * (100 - knockback_resistance * 100)，这里用 0~1 的抗性表达同一件事
	return KnockbackVelocity * (1.f - FMath::Clamp(KnockbackResistance, 0.f, 1.f));
}

FVector2D UBRT_EnemyMovementBehaviorComponent::GetDesiredVelocity() const
{
	// Godot: move_input - knockback
	return GetMoveInput() - GetKnockbackValue();
}

bool UBRT_EnemyMovementBehaviorComponent::IsBeingKnockedBack() const
{
	// Godot enemy.gd：get_knockback_value().length() > get_move_input().length()
	return GetKnockbackValue().SizeSquared() > GetMoveInput().SizeSquared();
}

void UBRT_EnemyMovementBehaviorComponent::ApplyDesiredVelocity(const FVector2D& DesiredVelocity)
{
	if (!IsValid(OwnerCharacter) || !IsValid(CharacterMovement))
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
	CharacterMovement->MaxFlySpeed = DesiredSpeed;
	CharacterMovement->MaxWalkSpeed = DesiredSpeed;

	const FVector2D Dir = DesiredVelocity / DesiredSpeed;
	// bForce = true：敌人没有 Controller，也要把输入塞进 ControlInputVector
	OwnerCharacter->AddMovementInput(FVector(Dir.X, Dir.Y, 0.f), 1.f);
}

void UBRT_EnemyMovementBehaviorComponent::StopMovement()
{
	if (IsValid(CharacterMovement))
	{
		CharacterMovement->StopMovementImmediately();
	}
}

void UBRT_EnemyMovementBehaviorComponent::UpdateDecayingValues(float DeltaTime)
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

void UBRT_EnemyMovementBehaviorComponent::SetMoveArea(const FVector2D& InMin, const FVector2D& InMax)
{
	MoveAreaMin = FVector2D(FMath::Min(InMin.X, InMax.X), FMath::Min(InMin.Y, InMax.Y));
	MoveAreaMax = FVector2D(FMath::Max(InMin.X, InMax.X), FMath::Max(InMin.Y, InMax.Y));
}

void UBRT_EnemyMovementBehaviorComponent::SetMoveTarget(AActor* InTarget)
{
	MoveTargetActor = InTarget;

	// 换目标等于重新「未发现」，同时收回上次发现时加的永久加速，避免多次换目标后无限叠加
	if (bPlayerDetected)
	{
		BonusSpeed -= PatrolParams.SpeedBonusOnDetect;
		bPlayerDetected = false;
	}
}

void UBRT_EnemyMovementBehaviorComponent::ResetMoveBehavior()
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

	// 先解锁，再把速度类状态清空
	StopLockedMove();

	bPlayerDetected = false;
	bTargetValid = false;
	TargetAge = 0.f;
	CurrentTargetPosition = GetActorLocation2D();
	MoveDirection = FVector2D::ZeroVector;
	BonusSpeed = 0.f;
	DecayingBonusSpeed = 0.f;
	KnockbackVelocity = FVector2D::ZeroVector;
}

void UBRT_EnemyMovementBehaviorComponent::AddBonusSpeed(float Delta)
{
	BonusSpeed += Delta;
}

void UBRT_EnemyMovementBehaviorComponent::AddDecayingSpeed(float Value)
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

void UBRT_EnemyMovementBehaviorComponent::ApplyKnockback(const FVector2D& Direction, float Strength)
{
	if (Direction.IsNearlyZero() || Strength <= 0.f)
	{
		return;
	}

	// 对应 Unit.take_damage：_knockback_vector = knockback_direction * knockback_amount
	KnockbackVelocity = Direction.GetSafeNormal() * Strength;
}

void UBRT_EnemyMovementBehaviorComponent::StartLockedMove(const FVector2D& Direction, float LockedSpeed, float Duration)
{
	// 对应 ChargingAttackBehavior.shoot()：_move_locked = true + bonus_speed = charge_speed
	// 冲撞速度单独存放，不碰 BonusSpeed，重入冲撞 / 冲撞中发现玩家都不会丢失永久加速
	bMoveLocked = true;
	LockedMoveDirection = Direction.GetSafeNormal();
	LockedSpeedBonus = LockedSpeed;

	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		TimerManager.ClearTimer(UnlockMoveTimerHandle);
		if (Duration > 0.f)
		{
			TimerManager.SetTimer(UnlockMoveTimerHandle, this, &ThisClass::StopLockedMove, Duration, false);
		}
	}
}

void UBRT_EnemyMovementBehaviorComponent::StopLockedMove()
{
	// 对应 ChargingAttackBehavior.on_unlock_move_timer_timeout()
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UnlockMoveTimerHandle);
	}

	if (!bMoveLocked)
	{
		return;
	}

	bMoveLocked = false;
	LockedMoveDirection = FVector2D::ZeroVector;
	LockedSpeedBonus = 0.f;
}

void UBRT_EnemyMovementBehaviorComponent::NotifyPlayerDetected()
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

FVector2D UBRT_EnemyMovementBehaviorComponent::MakeRandomOffset(float Range, bool bAllowWithin)
{
	FVector2D Offset(FMath::FRandRange(-Range, Range), FMath::FRandRange(-Range, Range));
	if (!bAllowWithin)
	{
		// Godot: _distance_from_player.normalized() * _actual
		// 随机到近乎零向量时 GetSafeNormal 会返回零，退化成固定方向
		const FVector2D Dir = Offset.GetSafeNormal();
		Offset = (Dir.IsNearlyZero() ? FVector2D(1.f, 0.f) : Dir) * Range;
	}
	return Offset;
}

EBRT_MapEdge UBRT_EnemyMovementBehaviorComponent::GetNearestMapEdge(const FVector2D& Pos) const
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

FVector2D UBRT_EnemyMovementBehaviorComponent::GetRandPosOnEdge(EBRT_MapEdge Edge, float EdgeDistance) const
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
	default:
		return MakeRandomAreaTarget();
	}
}

FVector2D UBRT_EnemyMovementBehaviorComponent::ClampToMoveArea(const FVector2D& Pos) const
{
	return FVector2D(
		FMath::Clamp(Pos.X, MoveAreaMin.X, MoveAreaMax.X),
		FMath::Clamp(Pos.Y, MoveAreaMin.Y, MoveAreaMax.Y));
}

bool UBRT_EnemyMovementBehaviorComponent::HasReachedTarget(const FVector2D& Target) const
{
	// Godot 用 _current_target == Vector2.ZERO 当「没有目标」的哨兵值，这里用显式标记
	if (!bTargetValid)
	{
		return true;
	}
	return (Target - GetActorLocation2D()).Size() <= TargetReachTolerance;
}

FVector2D UBRT_EnemyMovementBehaviorComponent::GetActorLocation2D() const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return FVector2D::ZeroVector;
	}
	const FVector Loc = Owner->GetActorLocation();
	return FVector2D(Loc.X, Loc.Y);
}

FVector2D UBRT_EnemyMovementBehaviorComponent::GetMoveTargetLocation2D() const
{
	if (!IsValid(MoveTargetActor))
	{
		return GetActorLocation2D();
	}
	const FVector Loc = MoveTargetActor->GetActorLocation();
	return FVector2D(Loc.X, Loc.Y);
}

float UBRT_EnemyMovementBehaviorComponent::GetBaseMoveSpeed() const
{
	// Godot unit.reset_stats(): base_speed = (stats.speed + stats.speed * enemy_speed%) * (1 + speed_coef)
	return BaseMoveSpeed * CachedEnemySpeedMultiplier;
}

float UBRT_EnemyMovementBehaviorComponent::QueryEnemySpeedMultiplier() const
{
	// 「敌人速度」是玩家身上的全局属性（Godot: Utils.get_stat 读玩家 stats），不是敌人自己的 AttributeSet
	const ABRT_BaseCharacter* TargetCharacter = Cast<ABRT_BaseCharacter>(MoveTargetActor);
	if (!IsValid(TargetCharacter))
	{
		return 1.f;
	}

	const UBRT_AttributeSet* Attributes = Cast<UBRT_AttributeSet>(TargetCharacter->GetAttributeSet());
	if (!IsValid(Attributes))
	{
		return 1.f;
	}

	// GAS 里 EnemySpeed 单位是 %（Godot: Utils.get_stat("enemy_speed") / 100）
	return FMath::Max(0.f, 1.f + Attributes->GetEnemySpeed() / 100.f);
}

void UBRT_EnemyMovementBehaviorComponent::DrawMoveDebug() const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return;
	}

	const FVector MyLoc = Owner->GetActorLocation();
	const FVector TargetLoc(CurrentTargetPosition.X, CurrentTargetPosition.Y, MyLoc.Z);
	const FVector2D Desired = GetDesiredVelocity();

	DrawDebugSphere(GetWorld(), TargetLoc, 12.f, 8, FColor::Yellow, false, -1.f, 0, 1.f);
	DrawDebugLine(GetWorld(), MyLoc, TargetLoc, FColor::Green, false, -1.f, 0, 1.f);

	DrawDebugLine(GetWorld(), MyLoc, MyLoc + FVector(Desired.X, Desired.Y, 0.f) * 0.1f, FColor::Red, false, -1.f, 0, 2.f);
}
