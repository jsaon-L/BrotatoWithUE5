// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BRT_PlayerCharacter.h"

#include "Animation/BRT_PaperDollAnimComponent.h"
#include "Animation/BRT_PaperDollAnimData.h"
#include "BrotatoData/BrotatoCharacterData.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"


namespace
{
	/** 判定范围统一配置：QueryOnly + 只与动态物体/角色产生 Overlap（Godot 侧三者都是 Area2D） */
	void ConfigureOverlapArea(USphereComponent* Area, float Radius)
	{
		Area->InitSphereRadius(Radius);
		Area->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Area->SetCollisionObjectType(ECC_WorldDynamic);
		Area->SetCollisionResponseToAllChannels(ECR_Ignore);
		Area->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		Area->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Area->SetGenerateOverlapEvents(true);
	}

	/**
	 * Godot 的 FItemAppearance::Depth（典型 -1000~1200）换算成 UE 的半透明排序值。
	 * 现有纸娃娃层级固定为：影子 -2、高亮 -1、腿 0、身体 1。
	 * 负深度要压到影子之下（对应 Godot 里 depth < -1 的外观被 move_child(0) 提到最底层，如披风），
	 * 非负深度从 2 起（保证盖住身体），量级按 /100 缩放以保留相对顺序。
	 */
	int32 DepthToSortPriority(int32 Depth)
	{
		if (Depth < 0)
		{
			return FMath::Clamp(FMath::RoundToInt(Depth / 100.f), -100, -3);
		}
		return FMath::Max(2, FMath::RoundToInt(Depth / 100.f));
	}
}


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

	// ---- 影子 / 高亮（Godot Animation/Shadow、Animation/Highlight，随身体一起做动画） ----

	ShadowSprite = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("ShadowSprite"));
	ShadowSprite->SetupAttachment(PaperDollBody);
	ShadowSprite->SetRelativeLocation(FVector(0.f, 0.f, -38.f)); // Godot Shadow (0,38)
	// Godot scale (1,-0.3) = 垂直翻转 + 压扁；对左右对称的影子，视觉等价于只把 Z 压到 0.3
	ShadowSprite->SetRelativeScale3D(FVector(1.f, 1.f, 0.3f));
	ShadowSprite->SetSpriteColor(FLinearColor(0.f, 0.f, 0.f, 0.392157f)); // Godot modulate (0,0,0,0.392)
	ShadowSprite->SetTranslucentSortPriority(-2); // 最底层
	ShadowSprite->SetCastShadow(false);
	ShadowSprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HighlightSprite = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("HighlightSprite"));
	HighlightSprite->SetupAttachment(PaperDollBody);
	HighlightSprite->SetRelativeLocation(FVector(0.f, 0.f, -36.f)); // Godot Highlight (0,36)
	HighlightSprite->SetSpriteColor(FLinearColor(0.447059f, 1.f, 0.94902f, 1.f)); // Godot modulate (0.447,1,0.949)
	HighlightSprite->SetTranslucentSortPriority(-1); // 影子之上，腿与身体之下（Godot show_behind_parent）
	HighlightSprite->SetCastShadow(false);
	HighlightSprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HighlightSprite->SetVisibility(false); // Godot visible = false

	// ---- 武器挂点树（Godot Weapons/One..Six/AttachN）：只建挂点，不含槽位逻辑 ----

	// 必须挂在 SpriteRoot 下：SpriteRoot 承载 2D 平面朝向（BP 中 rot=(Y90,R-90)，local XZ 对应世界 XY），
	// 挂到胶囊根上会把 Z 偏移变成世界深度轴，画面上全部塌到一条线。
	// Godot 里 Weapons 与 Animation 节点位置相同（都是精灵中心），故本地偏移为零。
	WeaponsRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponsRoot"));
	WeaponsRoot->SetupAttachment(SpriteRoot);
	WeaponsRoot->SetRelativeLocation(FVector::ZeroVector);

	// One：1 个挂点
	WeaponSlot1 = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponSlot1"));
	WeaponSlot1->SetupAttachment(WeaponsRoot);
	WeaponSlot1Attach1 = CreateWeaponAttachPoint(WeaponSlot1, TEXT("WeaponSlot1Attach1"), FVector2D(0.f, 30.f));

	// Two：2 个挂点
	WeaponSlot2 = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponSlot2"));
	WeaponSlot2->SetupAttachment(WeaponsRoot);
	WeaponSlot2Attach1 = CreateWeaponAttachPoint(WeaponSlot2, TEXT("WeaponSlot2Attach1"), FVector2D(40.f, 20.f));
	WeaponSlot2Attach2 = CreateWeaponAttachPoint(WeaponSlot2, TEXT("WeaponSlot2Attach2"), FVector2D(-40.f, 20.f));

	// Three：3 个挂点
	WeaponSlot3 = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponSlot3"));
	WeaponSlot3->SetupAttachment(WeaponsRoot);
	WeaponSlot3Attach1 = CreateWeaponAttachPoint(WeaponSlot3, TEXT("WeaponSlot3Attach1"), FVector2D(40.f, 20.f));
	WeaponSlot3Attach2 = CreateWeaponAttachPoint(WeaponSlot3, TEXT("WeaponSlot3Attach2"), FVector2D(-40.f, 20.f));
	WeaponSlot3Attach3 = CreateWeaponAttachPoint(WeaponSlot3, TEXT("WeaponSlot3Attach3"), FVector2D(0.f, -45.f));

	// Four：4 个挂点
	WeaponSlot4 = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponSlot4"));
	WeaponSlot4->SetupAttachment(WeaponsRoot);
	WeaponSlot4Attach1 = CreateWeaponAttachPoint(WeaponSlot4, TEXT("WeaponSlot4Attach1"), FVector2D(40.f, 35.f));
	WeaponSlot4Attach2 = CreateWeaponAttachPoint(WeaponSlot4, TEXT("WeaponSlot4Attach2"), FVector2D(-40.f, 35.f));
	WeaponSlot4Attach3 = CreateWeaponAttachPoint(WeaponSlot4, TEXT("WeaponSlot4Attach3"), FVector2D(-40.f, -30.f));
	WeaponSlot4Attach4 = CreateWeaponAttachPoint(WeaponSlot4, TEXT("WeaponSlot4Attach4"), FVector2D(40.f, -30.f));

	// Five：5 个挂点
	WeaponSlot5 = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponSlot5"));
	WeaponSlot5->SetupAttachment(WeaponsRoot);
	WeaponSlot5Attach1 = CreateWeaponAttachPoint(WeaponSlot5, TEXT("WeaponSlot5Attach1"), FVector2D(35.f, 35.f));
	WeaponSlot5Attach2 = CreateWeaponAttachPoint(WeaponSlot5, TEXT("WeaponSlot5Attach2"), FVector2D(-35.f, 35.f));
	WeaponSlot5Attach3 = CreateWeaponAttachPoint(WeaponSlot5, TEXT("WeaponSlot5Attach3"), FVector2D(-55.f, -20.f));
	WeaponSlot5Attach4 = CreateWeaponAttachPoint(WeaponSlot5, TEXT("WeaponSlot5Attach4"), FVector2D(55.f, -20.f));
	WeaponSlot5Attach5 = CreateWeaponAttachPoint(WeaponSlot5, TEXT("WeaponSlot5Attach5"), FVector2D(0.f, -55.f));

	// Six：6 个挂点
	WeaponSlot6 = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponSlot6"));
	WeaponSlot6->SetupAttachment(WeaponsRoot);
	WeaponSlot6Attach1 = CreateWeaponAttachPoint(WeaponSlot6, TEXT("WeaponSlot6Attach1"), FVector2D(35.f, 45.f));
	WeaponSlot6Attach2 = CreateWeaponAttachPoint(WeaponSlot6, TEXT("WeaponSlot6Attach2"), FVector2D(-35.f, 45.f));
	WeaponSlot6Attach3 = CreateWeaponAttachPoint(WeaponSlot6, TEXT("WeaponSlot6Attach3"), FVector2D(-60.f, 0.f));
	WeaponSlot6Attach4 = CreateWeaponAttachPoint(WeaponSlot6, TEXT("WeaponSlot6Attach4"), FVector2D(60.f, 0.f));
	WeaponSlot6Attach5 = CreateWeaponAttachPoint(WeaponSlot6, TEXT("WeaponSlot6Attach5"), FVector2D(-35.f, -45.f));
	WeaponSlot6Attach6 = CreateWeaponAttachPoint(WeaponSlot6, TEXT("WeaponSlot6Attach6"), FVector2D(35.f, -45.f));

	// ---- 判定范围（Godot Hurtbox / ItemPickupArea / ItemAttractArea） ----

	// 同样挂在 SpriteRoot 下：判定范围的偏移量是 Godot 的屏幕平面偏移，必须落在 2D 平面内。
	// 换算基准：BP 里 PaperDollBody 相对 SpriteRoot 为 0，即 SpriteRoot 原点 = 精灵中心；
	// 而 Godot 玩家根节点在精灵中心下方 24（Animation 节点 y = -24），故 局部Z = -(GodotY + 24)。

	Hurtbox = CreateDefaultSubobject<USphereComponent>(TEXT("Hurtbox"));
	Hurtbox->SetupAttachment(SpriteRoot);
	// Godot Hurtbox(0,-25) + Collision(0,13) = 玩家根 (0,-12) -> 精灵中心上方 12
	Hurtbox->SetRelativeLocation(FVector(0.f, 0.f, -12.f));
	ConfigureOverlapArea(Hurtbox, 21.f);

	ItemPickupArea = CreateDefaultSubobject<USphereComponent>(TEXT("ItemPickupArea"));
	ItemPickupArea->SetupAttachment(SpriteRoot);
	// Godot (0,-24) 正好是精灵中心
	ItemPickupArea->SetRelativeLocation(FVector::ZeroVector);
	ConfigureOverlapArea(ItemPickupArea, 32.f);

	ItemAttractArea = CreateDefaultSubobject<USphereComponent>(TEXT("ItemAttractArea"));
	ItemAttractArea->SetupAttachment(SpriteRoot);
	// Godot (0,0) 即玩家根节点 -> 精灵中心下方 24
	ItemAttractArea->SetRelativeLocation(FVector(0.f, 0.f, -24.f));
	ConfigureOverlapArea(ItemAttractArea, 150.f);

	Hurtbox->OnComponentBeginOverlap.AddDynamic(this, &ABRT_PlayerCharacter::OnHurtboxBeginOverlap);
	ItemPickupArea->OnComponentBeginOverlap.AddDynamic(this, &ABRT_PlayerCharacter::OnItemPickupAreaBeginOverlap);
	ItemAttractArea->OnComponentBeginOverlap.AddDynamic(this, &ABRT_PlayerCharacter::OnItemAttractAreaBeginOverlap);
}

USceneComponent* ABRT_PlayerCharacter::CreateWeaponAttachPoint(USceneComponent* Parent, const TCHAR* Name, const FVector2D& GodotOffset)
{
	// 注意：只能在构造函数期间调用（CreateDefaultSubobject 限制）
	USceneComponent* AttachPoint = CreateDefaultSubobject<USceneComponent>(Name);
	AttachPoint->SetupAttachment(Parent);
	// Godot 的 +Y 向下，对应 UE 的 -Z
	AttachPoint->SetRelativeLocation(FVector(GodotOffset.X, 0.f, -GodotOffset.Y));
	return AttachPoint;
}


// Called when the game starts or when spawned
void ABRT_PlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 影子默认复用身体精灵（Godot 里 Shadow 直接用 potato.png），蓝图未指定时自动补齐
	if (ShadowSprite && !ShadowSprite->GetSprite() && PaperDollBody)
	{
		ShadowSprite->SetSprite(PaperDollBody->GetSprite());
	}

	if (PaperDollAnim)
	{
		PaperDollAnim->PlayAnimByName(FName(TEXT("idle")));
	}

	// 参考 Godot player.apply_items_effects：用角色数据把自带外观（眼睛/嘴巴等）组装到身上
	InitFromCharacterData();
}


void ABRT_PlayerCharacter::InitFromCharacterData()
{
	ClearItemAppearances();

	if (!CharacterData)
	{
		return;
	}

	// 与 Godot RunData.sort_appearances 一致：按 Depth 升序即由底层到上层依次叠加
	TArray<FItemAppearance> SortedAppearances = CharacterData->GetItemAppearances();
	SortedAppearances.StableSort([](const FItemAppearance& A, const FItemAppearance& B)
	{
		return A.Depth < B.Depth;
	});

	for (const FItemAppearance& Appearance : SortedAppearances)
	{
		AddItemAppearance(Appearance);
	}
}


UPaperSpriteComponent* ABRT_PlayerCharacter::AddItemAppearance(const FItemAppearance& Appearance)
{
	if (!Appearance.Sprite || !SpriteRoot)
	{
		return nullptr;
	}
	
	//TODO:根据位置和优先级做过滤与替换,other例外全量显示

	// 外观精灵统一挂在 SpriteRoot 下：SpriteRoot 承载 2D 平面朝向，局部 XZ 即屏幕平面。
	// 位置恒为原点（Godot 侧 item_sprite.position 也是 (0,0)，位置由图片自身内容决定），
	// 叠放层级由 FItemAppearance::Depth 换算成 TranslucentSortPriority。
	UPaperSpriteComponent* SpriteComponent = NewObject<UPaperSpriteComponent>(this, NAME_None, RF_Transient);
	if (!SpriteComponent)
	{
		return nullptr;
	}

	SpriteComponent->SetupAttachment(PaperDollBody);
	SpriteComponent->SetMobility(EComponentMobility::Movable);
	SpriteComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpriteComponent->SetGenerateOverlapEvents(false);
	SpriteComponent->SetCastShadow(false);
	SpriteComponent->SetSprite(Appearance.Sprite);
	SpriteComponent->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	SpriteComponent->SetTranslucentSortPriority(DepthToSortPriority(Appearance.Depth));
	SpriteComponent->RegisterComponent();

	ItemAppearanceComponents.Add(SpriteComponent);
	return SpriteComponent;
}


void ABRT_PlayerCharacter::ClearItemAppearances()
{
	for (UPaperSpriteComponent* SpriteComponent : ItemAppearanceComponents)
	{
		if (IsValid(SpriteComponent))
		{
			SpriteComponent->DestroyComponent();
		}
	}

	ItemAppearanceComponents.Reset();
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

void ABRT_PlayerCharacter::OnHurtboxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// TODO: 受击判定（伤害来源过滤、免伤次数、无敌帧），对应 Godot Hurtbox 的 area/body_entered
}

void ABRT_PlayerCharacter::OnItemPickupAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// TODO: 拾取（靠近即拾取的消耗品/材料），对应 Godot ItemPickupArea.area_entered
}

void ABRT_PlayerCharacter::OnItemAttractAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// TODO: 吸引（把范围内的掉落物拉向玩家），对应 Godot ItemAttractArea.area_entered
}
