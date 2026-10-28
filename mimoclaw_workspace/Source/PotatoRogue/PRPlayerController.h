// Copyright (c) PotatoRogue. MIT Licensed.
// 玩家控制器：HUD/商店/升级/结算 界面流 + 购买与升级逻辑

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PRGameTypes.h"
#include "PRPlayerController.generated.h"

class UPRHUDWidget;
class UPRShopWidget;
class UPRLevelUpWidget;
class UPRGameOverWidget;

UCLASS()
class POTATOROGUE_API APRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APRPlayerController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ---------------- UI 回调 ----------------
	/** 购买商店格子，返回是否成功 */
	UFUNCTION(BlueprintCallable, Category = "PR|Shop")
	bool TryBuyOffer(int32 OfferIndex);

	/** 重随商店 */
	UFUNCTION(BlueprintCallable, Category = "PR|Shop")
	void RerollShop();

	/** 开始下一波 */
	UFUNCTION(BlueprintCallable, Category = "PR|Shop")
	void StartNextWave();

	/** 选择升级属性（0-3） */
	UFUNCTION(BlueprintCallable, Category = "PR|LevelUp")
	void ChooseLevelUpStat(int32 OptionIndex);

	/** 重新开始 */
	UFUNCTION(BlueprintCallable, Category = "PR|Flow")
	void RestartRun();

	/** 当前升级选项 */
	const TArray<FPRStatMod>& GetLevelUpChoices() const { return LevelUpChoices; }

protected:
	UPROPERTY()
	TObjectPtr<UPRHUDWidget> HUDWidget;

	UPROPERTY()
	TObjectPtr<UPRShopWidget> ShopWidget;

	UPROPERTY()
	TObjectPtr<UPRLevelUpWidget> LevelUpWidget;

	UPROPERTY()
	TObjectPtr<UPRGameOverWidget> GameOverWidget;

	EPRGamePhase LastPhase = EPRGamePhase::Preparation;
	TArray<FPRStatMod> LevelUpChoices;
	bool bPendingLevelUp = false;

	void HandleLevelUp(int32 NewLevel);
	void GenerateLevelUpChoices();
	void SetUIInputMode(bool bUI);
	void PauseGame(bool bPause);

	class APRPlayerState* GetPRPlayerState() const;
	class APRGameState* GetPRGameState() const;
	class APRGameMode* GetPRGameMode() const;
};
