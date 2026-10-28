// Copyright (c) PotatoRogue. MIT Licensed.
// 游戏状态：波次管理、敌人注册表、商店生成、难度曲线

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "PRGameTypes.h"
#include "PRGameState.generated.h"

class APREnemyCharacter;
class APRPlayerCharacter;
class UPRItemDefinition;
class UPRWeaponDefinition;
class UPREnemyDefinition;

UCLASS()
class POTATOROGUE_API APRGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	APRGameState();

	virtual void Tick(float DeltaSeconds) override;

	// ---------------- 阶段 ----------------
	UPROPERTY(BlueprintReadOnly, Category = "PR|Flow")
	EPRGamePhase Phase = EPRGamePhase::Preparation;

	UPROPERTY(BlueprintReadOnly, Category = "PR|Flow")
	int32 WaveNumber = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PR|Flow")
	float WaveTimeRemaining = 0.f;

	/** 当前波次存活敌人数量上限 */
	UPROPERTY(EditDefaultsOnly, Category = "PR|Flow")
	int32 MaxAliveEnemies = 45;

	// ---------------- 波次控制 ----------------
	void StartWave(int32 InWaveNumber);
	void EndWave();

	/** 波次时长（秒）：随波次增长，封顶 60s */
	float GetWaveDuration(int32 Wave) const;

	// ---------------- 敌人注册表 ----------------
	void RegisterEnemy(APREnemyCharacter* Enemy);
	void UnregisterEnemy(APREnemyCharacter* Enemy);
	const TArray<TObjectPtr<APREnemyCharacter>>& GetAliveEnemies() const { return AliveEnemies; }

	/** 敌人死亡：发放经验/金币掉落 */
	void NotifyEnemyKilled(APREnemyCharacter* Enemy);

	// ---------------- 玩家 ----------------
	APRPlayerCharacter* GetPlayerCharacter() const;

	// ---------------- 数据池（由 GameMode 注入） ----------------
	UPROPERTY()
	TArray<TObjectPtr<UPRItemDefinition>> ItemPool;

	UPROPERTY()
	TArray<TObjectPtr<UPRWeaponDefinition>> WeaponPool;

	UPROPERTY()
	TArray<TObjectPtr<UPREnemyDefinition>> EnemyPool;

	// ---------------- 商店 ----------------
	UPROPERTY(BlueprintReadOnly, Category = "PR|Shop")
	TArray<FPRShopOffer> ShopOffers;

	UPROPERTY(BlueprintReadOnly, Category = "PR|Shop")
	int32 RerollCost = 5;

	UPROPERTY(BlueprintReadOnly, Category = "PR|Shop")
	int32 RerollCount = 0;

	void BuildShopOffers();
	FPRShopOffer MakeRandomOffer();

private:
	UPROPERTY()
	TArray<TObjectPtr<APREnemyCharacter>> AliveEnemies;

	float SpawnTimer = 2.f;

	/** 出怪间隔（秒），随波次加快 */
	float GetSpawnInterval() const;

	void SpawnEnemy();

	/** 掉落物生成 */
	void SpawnPickup(const FVector& Location, EPRAwardType Type, int32 Amount);
};
