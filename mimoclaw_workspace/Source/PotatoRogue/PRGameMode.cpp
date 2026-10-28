// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRGameMode.h"
#include "PRGameState.h"
#include "PRPlayerState.h"
#include "PRPlayerCharacter.h"
#include "PRDataAssets.h"
#include "PRWeaponComponent.h"
#include "PRDefaultContent.h"
#include "PRAttributeSet.h"
#include "PRGameplayEffectLib.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

APRGameMode::APRGameMode()
{
	GameStateClass = APRGameState::StaticClass();
	PlayerStateClass = APRPlayerState::StaticClass();
	DefaultPawnClass = APRPlayerCharacter::StaticClass();
}

void APRGameMode::BeginPlay()
{
	Super::BeginPlay();

	InjectDefaultContent();
	GiveStartingWeapon();

	// 开局奖励一点属性成长空间
	if (APRGameState* PRGameState = GetGameState<APRGameState>())
	{
		// 小幅初始加成（相当于出生自带的"土豆"加成）
		if (APRPlayerCharacter* Player = PRGameState->GetPlayerCharacter())
		{
			TArray<FPRStatMod> StartMods;
			StartMods.Add(FPRStatMod(EPRStat::MaxHealth, 10.f));
			StartMods.Add(FPRStatMod(EPRStat::DamagePct, 5.f));
			PRGameplayEffects::ApplyStatMods(Player->GetPRAbilitySystemComponent(), StartMods);
		}

		// 稍作准备时间后进入第一波
		FTimerHandle StartHandle;
		GetWorldTimerManager().SetTimer(StartHandle, [this]()
		{
			if (APRGameState* GS = GetGameState<APRGameState>())
			{
				GS->StartWave(1);
			}
		}, 1.5f, false);
	}
}

void APRGameMode::InjectDefaultContent()
{
	APRGameState* PRGameState = GetGameState<APRGameState>();
	if (!PRGameState)
	{
		return;
	}

	// 只在池为空时注入，方便用蓝图/DataAsset 覆盖
	if (PRGameState->ItemPool.Num() == 0)
	{
		for (UPRItemDefinition* Item : PRDefaultContent::CreateItems(this))
		{
			PRGameState->ItemPool.Add(Item);
		}
	}
	if (PRGameState->WeaponPool.Num() == 0)
	{
		for (UPRWeaponDefinition* Weapon : PRDefaultContent::CreateWeapons(this))
		{
			PRGameState->WeaponPool.Add(Weapon);
		}
	}
	if (PRGameState->EnemyPool.Num() == 0)
	{
		for (UPREnemyDefinition* Enemy : PRDefaultContent::CreateEnemies(this))
		{
			PRGameState->EnemyPool.Add(Enemy);
		}
	}
}

void APRGameMode::GiveStartingWeapon()
{
	APRGameState* PRGameState = GetGameState<APRGameState>();
	APRPlayerCharacter* Player = PRGameState ? PRGameState->GetPlayerCharacter() : nullptr;
	if (!PRGameState || !Player)
	{
		return;
	}

	UPRWeaponDefinition* StartingWeapon = nullptr;
	for (UPRWeaponDefinition* Weapon : PRGameState->WeaponPool)
	{
		if (Weapon && Weapon->WeaponName.ToString().Contains(StartingWeaponName.ToString()))
		{
			StartingWeapon = Weapon;
			break;
		}
	}
	if (!StartingWeapon && PRGameState->WeaponPool.Num() > 0)
	{
		StartingWeapon = PRGameState->WeaponPool[0];
	}

	if (StartingWeapon && Player->GetWeaponComponent())
	{
		Player->GetWeaponComponent()->AddWeapon(StartingWeapon);
	}
}

void APRGameMode::StartNextWave()
{
	if (APRGameState* PRGameState = GetGameState<APRGameState>())
	{
		PRGameState->StartWave(PRGameState->WaveNumber + 1);
	}
}

void APRGameMode::HandlePlayerDeath()
{
	if (APRGameState* PRGameState = GetGameState<APRGameState>())
	{
		PRGameState->Phase = EPRGamePhase::GameOver;
	}
	// 由 PlayerController 显示结算界面
}

void APRGameMode::RestartRun()
{
	UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()), true);
}
