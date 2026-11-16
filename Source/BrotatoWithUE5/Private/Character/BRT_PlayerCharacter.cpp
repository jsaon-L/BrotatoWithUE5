// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BRT_PlayerCharacter.h"

#include "Animation/BRT_PaperDollAnimComponent.h"
#include "Animation/BRT_PaperDollAnimData.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"


// Sets default values
ABRT_PlayerCharacter::ABRT_PlayerCharacter()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;


	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	//用来显示的组件的根
	SpriteRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SpriteRoot"));
	SpriteRoot->SetupAttachment(GetCapsuleComponent());
	
	// 纸娃娃层级：身体精灵（Root）→ 腿部关节（LegL/LegR）→ 腿部精灵。
	// 位置按 Godot player.tscn 换算：Godot(x,y) -> UE(x,0,-y)，PPU=1（与腿部精灵 PixelsPerUnrealUnit 一致）。
	PaperDollBody = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("PaperDollBody"));
	PaperDollBody->SetupAttachment(SpriteRoot);
	PaperDollBody->SetRelativeLocation(FVector(0.f, 0.f, 24.f)); // Godot Animation 基准位 (0,-24)
	PaperDollBody->SetTranslucentSortPriority(1); // 身体渲染在腿之前（Godot show_behind_parent）

	LegLJoint = CreateDefaultSubobject<USceneComponent>(TEXT("LegL"));
	LegLJoint->SetupAttachment(PaperDollBody);
	LegLJoint->SetRelativeLocation(FVector(15.f, 0.f, -18.f)); // Godot LegL (15,18)

	LegLSprite = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("LegLSprite"));
	LegLSprite->SetupAttachment(LegLJoint);
	// Godot Sprite 偏移 (4,7) + 精灵裁剪区域中心与帧中心差 (-0.5,0.5)
	LegLSprite->SetRelativeLocation(FVector(3.5f, 0.f, -7.5f));
	LegLSprite->SetTranslucentSortPriority(0);

	LegRJoint = CreateDefaultSubobject<USceneComponent>(TEXT("LegR"));
	LegRJoint->SetupAttachment(PaperDollBody);
	LegRJoint->SetRelativeLocation(FVector(-16.f, 0.f, -18.f)); // Godot LegR (-16,18)

	LegRSprite = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("LegRSprite"));
	LegRSprite->SetupAttachment(LegRJoint);
	// Godot Sprite 偏移 (-3,7) + 精灵裁剪区域中心与帧中心差 (1,0.5)
	LegRSprite->SetRelativeLocation(FVector(-2.f, 0.f, -7.5f));
	LegRSprite->SetTranslucentSortPriority(0);

}


// Called when the game starts or when spawned
void ABRT_PlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (PaperDollAnim)
	{
		PaperDollAnim->PlayAnimByName(FName(TEXT("idle")));
	}

}

// Called every frame
void ABRT_PlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 速度阈值切换 idle/move（切换即播，无融合）。
	// 带滞回 + 最小驻留时间，避免速度在阈值附近抖动导致动画频繁重播（对应 Godot MovingTimer/NotMovingTimer）。
	// 判定顺序：驻留期只递减计时，驻留结束后按滞回阈值切换。
	if (PaperDollAnim)
	{
		if (AnimDwellCountdown > 0.f)
		{
			AnimDwellCountdown -= DeltaTime;
		}
		else
		{
			const float Speed2D = GetVelocity().Size2D();
			const bool bShouldMove = bMoveAnimActive
				? (Speed2D > MoveSpeedThreshold * IdleSpeedRatio)
				: (Speed2D > MoveSpeedThreshold);
			if (bShouldMove != bMoveAnimActive)
			{
				bMoveAnimActive = bShouldMove;
				PaperDollAnim->PlayAnimByName(bShouldMove ? FName(TEXT("move")) : FName(TEXT("idle")));
				AnimDwellCountdown = MinAnimDwellSeconds;
			}
		}
	}
}

// Called to bind functionality to input
void ABRT_PlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}
