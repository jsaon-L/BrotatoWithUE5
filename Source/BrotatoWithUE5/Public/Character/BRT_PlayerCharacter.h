// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BrotatoCore/Type/BRT_CharacterTypes.h"
#include "Character/BRT_BaseCharacter.h"
#include "BRT_PlayerCharacter.generated.h"

class UBRT_PaperDollAnimAsset;
class UBRT_PaperDollAnimComponent;
class UBrotatoCharacterData;
class UPaperSprite;
class UPaperSpriteComponent;
class USceneComponent;
class USphereComponent;
class UPrimitiveComponent;

UCLASS()
class BROTATOWITHUE5_API ABRT_PlayerCharacter : public ABRT_BaseCharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ABRT_PlayerCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;


	// ---- 角色数据 & 外观组装（对应 Godot RunData.appearances_displayed + player.apply_items_effects） ----

	/** 角色数据：在蓝图里选择，BeginPlay 时用它初始化角色外观 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Data")
	TObjectPtr<UBrotatoCharacterData> CharacterData;

	/**
	 * 用 CharacterData 初始化角色外观：清空旧外观后，按 Depth 升序逐个生成外观精灵。
	 * 可重复调用（例如读档、换角色），每次都会重建。
	 */
	UFUNCTION(BlueprintCallable, Category = "BRT|Data")
	void InitFromCharacterData();

	/**
	 * 动态添加一个外观精灵：为 FItemAppearance 新建 UPaperSpriteComponent 并挂到 SpriteRoot 下。
	 * 外观坐标即角色中心（Godot 侧 position 恒为 0，具体位置由图片内容决定），
	 * Depth 换算为 TranslucentSortPriority 决定叠放层级。
	 * @return 创建的组件；Sprite 为空或 SpriteRoot 无效时返回 nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "BRT|Data")
	UPaperSpriteComponent* AddItemAppearance(const FItemAppearance& Appearance);

	/** 销毁所有由 AddItemAppearance 动态生成的外观精灵 */
	UFUNCTION(BlueprintCallable, Category = "BRT|Data")
	void ClearItemAppearances();

protected:
	// ---- 纸娃娃层级（对齐 Godot player.tscn：Animation 根 + Legs/LegL + Legs/LegR） ----

	UPROPERTY(VisibleAnywhere, Category = "BRT|Anim")
	TObjectPtr<USceneComponent> SpriteRoot;
	
	/** 身体精灵（动画 Root 目标），腿部关节挂其下并跟随根缩放 */
	UPROPERTY(VisibleAnywhere, Category = "BRT|Anim")
	TObjectPtr<UPaperSpriteComponent> PaperDollBody;

	/** 左腿关节（动画 LegL 目标，对应 Godot Animation/Legs/LegL） */
	UPROPERTY(VisibleAnywhere, Category = "BRT|Anim")
	TObjectPtr<USceneComponent> LegLJoint;

	/** 右腿关节（动画 LegR 目标，对应 Godot Animation/Legs/LegR） */
	UPROPERTY(VisibleAnywhere, Category = "BRT|Anim")
	TObjectPtr<USceneComponent> LegRJoint;

	UPROPERTY(VisibleAnywhere, Category = "BRT|Anim")
	TObjectPtr<UPaperSpriteComponent> LegLSprite;

	UPROPERTY(VisibleAnywhere, Category = "BRT|Anim")
	TObjectPtr<UPaperSpriteComponent> LegRSprite;


	// ---- 影子 / 高亮（对齐 Godot player.tscn：Animation/Shadow、Animation/Highlight） ----

	/**
	 * 影子：Godot 里直接复用 potato.png，黑色 40% 透明、垂直压扁到 30%，画在腿和身体之后。
	 * 蓝图未指定精灵时，BeginPlay 会自动复用 PaperDollBody 的精灵。
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	TObjectPtr<UPaperSpriteComponent> ShadowSprite;

	/** 高亮描边：Godot 中 visible = false，默认隐藏，由受击/选中/拾取等逻辑按需显示 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Anim")
	TObjectPtr<UPaperSpriteComponent> HighlightSprite;


	// ---- 武器挂点树（对齐 Godot player.tscn：Weapons/One..Six/AttachN） ----
	// 只建容器与挂点，不含运行时槽位逻辑；用几个槽位、怎么挂武器由蓝图决定。
	// 坐标换算：以 SpriteRoot 的局部空间为准，Godot(x,y) -> 局部(x,0,-y)。
	// SpriteRoot 承载 2D 平面朝向（BP 中 rot=(Y90,R-90)，局部 XZ 平面 = 世界 XY 平面），
	// 所以所有 2D 布局节点都必须挂在 SpriteRoot 之下，否则偏移会落到世界深度轴上。

	/** 武器容器（Godot Weapons，与 Animation 同位置 = 精灵中心），随 SpriteRoot 的平面朝向 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponsRoot;

	/** 槽位 One（1 个挂点） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot1;

	/** One/Attach1 (0,30) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot1Attach1;

	/** 槽位 Two（2 个挂点） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot2;

	/** Two/Attach1 (40,20) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot2Attach1;

	/** Two/Attach2 (-40,20) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot2Attach2;

	/** 槽位 Three（3 个挂点） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot3;

	/** Three/Attach1 (40,20) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot3Attach1;

	/** Three/Attach2 (-40,20) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot3Attach2;

	/** Three/Attach3 (0,-45) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot3Attach3;

	/** 槽位 Four（4 个挂点） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot4;

	/** Four/Attach1 (40,35) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot4Attach1;

	/** Four/Attach2 (-40,35) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot4Attach2;

	/** Four/Attach3 (-40,-30) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot4Attach3;

	/** Four/Attach4 (40,-30) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot4Attach4;

	/** 槽位 Five（5 个挂点） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot5;

	/** Five/Attach1 (35,35) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot5Attach1;

	/** Five/Attach2 (-35,35) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot5Attach2;

	/** Five/Attach3 (-55,-20) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot5Attach3;

	/** Five/Attach4 (55,-20) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot5Attach4;

	/** Five/Attach5 (0,-55) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot5Attach5;

	/** 槽位 Six（6 个挂点） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot6;

	/** Six/Attach1 (35,45) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot6Attach1;

	/** Six/Attach2 (-35,45) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot6Attach2;

	/** Six/Attach3 (-60,0) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot6Attach3;

	/** Six/Attach4 (60,0) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot6Attach4;

	/** Six/Attach5 (-35,-45) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot6Attach5;

	/** Six/Attach6 (35,-45) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Weapon")
	TObjectPtr<USceneComponent> WeaponSlot6Attach6;


	// ---- 判定范围（对齐 Godot player.tscn：Hurtbox / ItemPickupArea / ItemAttractArea） ----
	// Godot 侧三者都是 Area2D，这里统一为 QueryOnly + 仅产生 Overlap 的球体，
	// 具体碰撞响应可在蓝图里按项目需求覆盖。

	/** 受击盒：Godot Hurtbox(0,-25)+CircleShape2D(r=21)@(0,13) -> 精灵中心上方 12，半径 21 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Collision")
	TObjectPtr<USphereComponent> Hurtbox;

	/** 物品拾取范围：Godot ItemPickupArea，半径 32 @ 精灵中心 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Collision")
	TObjectPtr<USphereComponent> ItemPickupArea;

	/** 物品吸引范围：Godot ItemAttractArea，半径 150 @ 玩家根（精灵中心下方 24） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BRT|Collision")
	TObjectPtr<USphereComponent> ItemAttractArea;


	/** 受击盒进入重叠（对应 Godot Hurtbox.area_entered），空实现，交给蓝图/子类接管 */
	UFUNCTION()
	virtual void OnHurtboxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 进入拾取范围（对应 Godot ItemPickupArea.area_entered），空实现 */
	UFUNCTION()
	virtual void OnItemPickupAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** 进入吸引范围（对应 Godot ItemAttractArea.area_entered），空实现 */
	UFUNCTION()
	virtual void OnItemAttractAreaBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);


	// ---- 动画与精灵资产（为空时按默认路径加载） ----
	
	/** 移动切换阈值（UE 单位/秒，PPU=1 时即像素/秒） */
	UPROPERTY(EditDefaultsOnly, Category = "BRT|Anim")
	float MoveSpeedThreshold = 10.f;

	/** 回到 idle 的速度阈值 = MoveSpeedThreshold × 该比例（滞回，避免阈值附近抖动） */
	UPROPERTY(EditDefaultsOnly, Category = "BRT|Anim")
	float IdleSpeedRatio = 0.4f;

	/** 动画最小停留时间（秒），抑制抖动频繁重播（对应 Godot MovingTimer/NotMovingTimer） */
	UPROPERTY(EditDefaultsOnly, Category = "BRT|Anim")
	float MinAnimDwellSeconds = 0.25f;



private:

	/** 运行时动态生成的外观精灵组件，用于重建时清理 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPaperSpriteComponent>> ItemAppearanceComponents;

	/** 当前是否处于 move 动画（滞回状态） */
	bool bMoveAnimActive = false;

	/** 动画切换冷却计时 */
	float AnimDwellCountdown = 0.f;

	/** 创建武器挂点：GodotOffset 直接填 .tscn 里的 (x,y)，内部换算为 UE(x,0,-y) */
	USceneComponent* CreateWeaponAttachPoint(USceneComponent* Parent, const TCHAR* Name, const FVector2D& GodotOffset);
};
