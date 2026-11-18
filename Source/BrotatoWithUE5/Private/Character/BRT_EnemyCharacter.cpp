// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BRT_EnemyCharacter.h"

#include "Animation/BRT_PaperDollAnimComponent.h"
#include "Character/BRT_EnemyMovementBehaviorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"


// Sets default values
ABRT_EnemyCharacter::ABRT_EnemyCharacter()
{

	// 移动策略 / 速度 / 击退 / 冲撞锁定都由这个组件驱动
	MovementBehavior = CreateDefaultSubobject<UBRT_EnemyMovementBehaviorComponent>(TEXT("MovementBehavior"));

	// ---- 2D 平面化：Brotato 是俯视 2D，敌人只在 XY 平面移动、不吃重力 ----
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	// MoveComp->bOrientRotationToMovement = false;   // 朝向由纸娃娃镜像翻转控制，不能转 Actor
	// MoveComp->bUseControllerDesiredRotation = false;
	// MoveComp->bConstrainToPlane = true;
	// MoveComp->SetPlaneConstraintNormal(FVector(0.f, 0.f, 1.f));
	// // 不能用 bSnapToPlaneAtStart：吸附用的是 PlaneConstraintOrigin，而它默认是世界原点，
	// // 会把关卡里摆在 Z=88（站在地板上）的敌人拽到 Z=0、半个胶囊埋进 Floor 里；
	// // Flying 模式不会自动脱困，水平 sweep 永远是 StartPenetrating → 位移 0 → 追不动。
	// MoveComp->bSnapToPlaneAtStart = false;
	// MoveComp->GravityScale = 0.f;
	//
	// // 敌人没有 AIController（Godot 里敌人是纯物理体），允许「无 Controller 也走物理」
	// MoveComp->bRunPhysicsWithNoController = true;

	// Godot 是每物理帧直接写 rigidbody 速度，没有加速过程；
	// 这里把加速度/刹车拉到极大，等价于「一帧内到目标速度 / 一帧内停住」
	// 
	// 
	// MoveComp->MaxWalkSpeed = MovementBehavior->BaseMoveSpeed;
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
	

	// 对应 enemy.gd：被击退时不能造成接触伤害
	if (IsValid(MovementBehavior))
	{
		MovementBehavior->OnKnockedBackChanged.AddUniqueDynamic(this, &ThisClass::HandleKnockedBackChanged);
	}
}

void ABRT_EnemyCharacter::HandleDeath()
{
	Super::HandleDeath();

	if (IsValid(MovementBehavior))
	{
		MovementBehavior->HandleOwnerDeath();
	}
}

void ABRT_EnemyCharacter::HandleKnockedBackChanged(bool bKnockedBack)
{
	OnContactDamageSuppressedChanged(bKnockedBack);
}
