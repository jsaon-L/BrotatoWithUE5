// Copyright (c) PotatoRogue. MIT Licensed.
// 玩家状态：金币 / 等级 / 经验 / 持有物品

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PRPlayerState.generated.h"

class UPRItemDefinition;
class UPRWeaponDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FPROnLevelUp, int32 /*NewLevel*/);
DECLARE_MULTICAST_DELEGATE(FPROnGoldChanged);

UCLASS()
class POTATOROGUE_API APRPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	APRPlayerState();

	UPROPERTY(BlueprintReadOnly, Category = "PR|Progress")
	int32 Gold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PR|Progress")
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly, Category = "PR|Progress")
	int32 Experience = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PR|Progress")
	int32 ExperienceToNext = 12;

	/** 已购入物品（叠加记录） */
	UPROPERTY(BlueprintReadOnly, Category = "PR|Items")
	TArray<TObjectPtr<UPRItemDefinition>> OwnedItems;

	// 事件
	FPROnLevelUp OnLevelUp;
	FPROnGoldChanged OnGoldChanged;

	UFUNCTION(BlueprintCallable, Category = "PR|Progress")
	void AddGold(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "PR|Progress")
	bool SpendGold(int32 Amount);

	/** 增加经验，返回升级次数 */
	int32 AddExperience(int32 Amount);

	void AddOwnedItem(UPRItemDefinition* Item);

	/** 升到下一级所需经验（递增曲线） */
	static int32 ExperienceForLevel(int32 Level);
};
