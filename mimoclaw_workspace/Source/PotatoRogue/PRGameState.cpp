// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRGameState.h"
#include "PREnemyCharacter.h"
#include "PRPlayerCharacter.h"
#include "PRPlayerState.h"
#include "PRDataAssets.h"
#include "PRPickup.h"
#include "PRAttributeSet.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

APRGameState::APRGameState()
{
	PrimaryActorTick.bCanEverTick = true;
}

void APRGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Phase != EPRGamePhase::Wave)
	{
		return;
	}

	// 波次倒计时
	WaveTimeRemaining -= DeltaSeconds;
	if (WaveTimeRemaining <= 0.f)
	{
		EndWave();
		return;
	}

	// 持续刷怪
	SpawnTimer -= DeltaSeconds;
	if (SpawnTimer <= 0.f)
	{
		SpawnTimer = GetSpawnInterval();

		// 一波内敌人越刷越密
		const int32 AliveCount = AliveEnemies.Num();
		if (AliveCount < MaxAliveEnemies)
		{
			const int32 Batch = FMath::Clamp(1 + WaveNumber / 3, 1, 5);
			for (int32 i = 0; i < Batch && AliveEnemies.Num() < MaxAliveEnemies; ++i)
			{
				SpawnEnemy();
			}
		}
	}
}

float APRGameState::GetWaveDuration(int32 Wave) const
{
	return FMath::Min(20.f + 4.f * Wave, 60.f);
}

float APRGameState::GetSpawnInterval() const
{
	return FMath::Max(1.4f - 0.12f * WaveNumber, 0.28f);
}

void APRGameState::StartWave(int32 InWaveNumber)
{
	WaveNumber = InWaveNumber;
	WaveTimeRemaining = GetWaveDuration(WaveNumber);
	SpawnTimer = 1.f;
	Phase = EPRGamePhase::Wave;
}

void APRGameState::EndWave()
{
	Phase = EPRGamePhase::Shop;

	// 清理场上敌人（土豆兄弟波次结束即清场）
	for (int32 i = AliveEnemies.Num() - 1; i >= 0; --i)
	{
		if (APREnemyCharacter* Enemy = AliveEnemies[i])
		{
			Enemy->Destroy();
		}
	}
	AliveEnemies.Reset();

	// 收获结算：波次结束按"收获"属性发金币
	if (APRPlayerCharacter* Player = GetPlayerCharacter())
	{
		if (APRPlayerState* PS = Player->GetPlayerState<APRPlayerState>())
		{
			const float Harvesting = Player->GetAttributeValue(UPRAttributeSet::GetHarvestingAttribute());
			const int32 HarvestGold = FMath::RoundToInt(Harvesting * 0.6f) + 5 + WaveNumber;
			PS->AddGold(HarvestGold);
		}
	}

	RerollCount = 0;
	RerollCost = 5;
	BuildShopOffers();
}

void APRGameState::RegisterEnemy(APREnemyCharacter* Enemy)
{
	if (Enemy && !AliveEnemies.Contains(Enemy))
	{
		AliveEnemies.Add(Enemy);
	}
}

void APRGameState::UnregisterEnemy(APREnemyCharacter* Enemy)
{
	AliveEnemies.Remove(Enemy);
}

void APRGameState::NotifyEnemyKilled(APREnemyCharacter* Enemy)
{
	if (!Enemy)
	{
		return;
	}

	const FVector Location = Enemy->GetActorLocation();

	// 经验掉落
	SpawnPickup(Location + FVector(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(-40.f, 40.f), 0.f),
		EPRAwardType::Experience, Enemy->GetXPReward());

	// 金币掉落（幸运提高额外金币概率）
	const float Luck = GetPlayerCharacter()
		? GetPlayerCharacter()->GetAttributeValue(UPRAttributeSet::GetLuckAttribute()) : 0.f;
	const float CoinChance = FMath::Clamp(0.55f + Luck * 0.004f, 0.f, 0.95f);
	if (FMath::FRand() < CoinChance)
	{
		SpawnPickup(Location, EPRAwardType::Coin, Enemy->GetCoinReward());
	}

	// 小概率掉治疗包
	if (FMath::FRand() < 0.03f + Luck * 0.0008f)
	{
		SpawnPickup(Location, EPRAwardType::Health, 15);
	}
}

void APRGameState::SpawnPickup(const FVector& Location, EPRAwardType Type, int32 Amount)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (APRPickup* Pickup = GetWorld()->SpawnActor<APRPickup>(Location, FRotator::ZeroRotator, Params))
	{
		Pickup->InitAward(Type, Amount);
	}
}

APRPlayerCharacter* APRGameState::GetPlayerCharacter() const
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		return Cast<APRPlayerCharacter>(PC->GetPawn());
	}
	return nullptr;
}

void APRGameState::SpawnEnemy()
{
	if (EnemyPool.Num() == 0)
	{
		return;
	}

	APRPlayerCharacter* Player = GetPlayerCharacter();
	if (!Player)
	{
		return;
	}

	// 按权重挑选可用敌人
	TArray<UPREnemyDefinition*> Candidates;
	float TotalWeight = 0.f;
	for (UPREnemyDefinition* Def : EnemyPool)
	{
		if (Def && Def->MinWave <= WaveNumber)
		{
			Candidates.Add(Def);
			TotalWeight += FMath::Max(Def->SpawnWeight, 0.01f);
		}
	}
	if (Candidates.Num() == 0)
	{
		return;
	}

	float Roll = FMath::FRand() * TotalWeight;
	UPREnemyDefinition* Chosen = Candidates.Last();
	for (UPREnemyDefinition* Def : Candidates)
	{
		Roll -= FMath::Max(Def->SpawnWeight, 0.01f);
		if (Roll <= 0.f)
		{
			Chosen = Def;
			break;
		}
	}

	// 玩家周围环形出生点
	const float Angle = FMath::FRandRange(0.f, 2.f * PI);
	const float Radius = FMath::FRandRange(1150.f, 1500.f);
	const FVector Location = Player->GetActorLocation()
		+ FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	if (APREnemyCharacter* Enemy = GetWorld()->SpawnActor<APREnemyCharacter>(Location, FRotator::ZeroRotator, Params))
	{
		Enemy->InitFromDefinition(Chosen, WaveNumber);
		RegisterEnemy(Enemy);
	}
}

void APRGameState::BuildShopOffers()
{
	ShopOffers.Reset();
	for (int32 i = 0; i < 4; ++i)
	{
		ShopOffers.Add(MakeRandomOffer());
	}
}

FPRShopOffer APRGameState::MakeRandomOffer()
{
	FPRShopOffer Offer;

	const bool bWeaponOffer = (WeaponPool.Num() > 0) && (ItemPool.Num() == 0 || FMath::FRand() < 0.42f);

	if (bWeaponOffer)
	{
		// 幸运影响高品质权重
		float TotalWeight = 0.f;
		TArray<TPair<UPRWeaponDefinition*, float>> Weighted;
		for (UPRWeaponDefinition* Def : WeaponPool)
		{
			if (!Def)
			{
				continue;
			}
			const float LuckBoost = GetPlayerCharacter()
				? GetPlayerCharacter()->GetAttributeValue(UPRAttributeSet::GetLuckAttribute()) : 0.f;
			const float Weight = (1.f + LuckBoost * 0.01f) * PRGameTypes::RarityToPriceScale(Def->Rarity);
			Weighted.Emplace(Def, Weight);
			TotalWeight += Weight;
		}

		if (Weighted.Num() > 0)
		{
			float Roll = FMath::FRand() * TotalWeight;
			for (const TPair<UPRWeaponDefinition*, float>& Pair : Weighted)
			{
				Roll -= Pair.Value;
				if (Roll <= 0.f)
				{
					Offer.Weapon = Pair.Key;
					break;
				}
			}
			if (!Offer.Weapon)
			{
				Offer.Weapon = Weighted.Last().Key;
			}
			Offer.Price = FMath::RoundToInt(Offer.Weapon->BasePrice * PRGameTypes::RarityToPriceScale(Offer.Weapon->Rarity));
			return Offer;
		}
	}

	// 物品报价
	float TotalWeight = 0.f;
	TArray<TPair<UPRItemDefinition*, float>> Weighted;
	for (UPRItemDefinition* Def : ItemPool)
	{
		if (!Def)
		{
			continue;
		}
		const float Weight = PRGameTypes::RarityToPriceScale(Def->Rarity);
		Weighted.Emplace(Def, Weight);
		TotalWeight += Weight;
	}

	if (Weighted.Num() > 0)
	{
		float Roll = FMath::FRand() * TotalWeight;
		for (const TPair<UPRItemDefinition*, float>& Pair : Weighted)
		{
			Roll -= Pair.Value;
			if (Roll <= 0.f)
			{
				Offer.Item = Pair.Key;
				break;
			}
		}
		if (!Offer.Item)
		{
			Offer.Item = Weighted.Last().Key;
		}
		Offer.Price = FMath::RoundToInt(Offer.Item->BasePrice * PRGameTypes::RarityToPriceScale(Offer.Item->Rarity));
	}

	return Offer;
}
