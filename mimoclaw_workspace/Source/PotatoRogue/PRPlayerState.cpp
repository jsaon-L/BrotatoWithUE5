// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRPlayerState.h"
#include "PRDataAssets.h"

APRPlayerState::APRPlayerState()
{
}

void APRPlayerState::AddGold(int32 Amount)
{
	Gold = FMath::Max(Gold + Amount, 0);
	OnGoldChanged.Broadcast();
}

bool APRPlayerState::SpendGold(int32 Amount)
{
	if (Amount < 0 || Gold < Amount)
	{
		return false;
	}
	Gold -= Amount;
	OnGoldChanged.Broadcast();
	return true;
}

int32 APRPlayerState::AddExperience(int32 Amount)
{
	Experience += Amount;

	int32 LevelUps = 0;
	while (Experience >= ExperienceToNext)
	{
		Experience -= ExperienceToNext;
		++Level;
		++LevelUps;
		ExperienceToNext = ExperienceForLevel(Level);
	}

	if (LevelUps > 0)
	{
		OnLevelUp.Broadcast(Level);
	}
	return LevelUps;
}

void APRPlayerState::AddOwnedItem(UPRItemDefinition* Item)
{
	if (Item)
	{
		OwnedItems.Add(Item);
	}
}

int32 APRPlayerState::ExperienceForLevel(int32 Level)
{
	// 肉鸽常见曲线：线性 + 平方混合
	return FMath::RoundToInt(10.f + 4.f * Level + 0.8f * Level * Level);
}
