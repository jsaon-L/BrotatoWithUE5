// Copyright (c) PotatoRogue. MIT Licensed.
// 游戏模式：初始化数据池、发放初始武器、推进波次

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PRGameMode.generated.h"

class UPRWeaponDefinition;

UCLASS()
class POTATOROGUE_API APRGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APRGameMode();

	virtual void BeginPlay() override;

	/** 进入下一波（商店 UI 调用） */
	UFUNCTION(BlueprintCallable, Category = "PR|Flow")
	void StartNextWave();

	/** 玩家死亡 */
	void HandlePlayerDeath();

	/** 重新开始 */
	UFUNCTION(BlueprintCallable, Category = "PR|Flow")
	void RestartRun();

protected:
	/** 初始武器（默认给一把手枪） */
	UPROPERTY(EditDefaultsOnly, Category = "PR|Setup")
	FName StartingWeaponName = TEXT("手枪");

	void GiveStartingWeapon();
	void InjectDefaultContent();
};
