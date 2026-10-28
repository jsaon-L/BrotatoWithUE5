// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRPlayerController.h"
#include "PRHUDWidget.h"
#include "PRShopWidget.h"
#include "PRMenuWidgets.h"
#include "PRGameState.h"
#include "PRGameMode.h"
#include "PRPlayerState.h"
#include "PRPlayerCharacter.h"
#include "PRDataAssets.h"
#include "PRWeaponComponent.h"
#include "PRAttributeSet.h"
#include "PRGameplayEffectLib.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

APRPlayerController::APRPlayerController()
{
	// 商店/升级阶段游戏暂停，控制器仍需 Tick 驱动 UI 流程
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void APRPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// HUD（常驻）
	HUDWidget = CreateWidget<UPRHUDWidget>(this, UPRHUDWidget::StaticClass());
	if (HUDWidget)
	{
		HUDWidget->AddToViewport(0);
	}

	// 绑定升级事件
	if (APRPlayerState* PS = GetPRPlayerState())
	{
		PS->OnLevelUp.AddUObject(this, &APRPlayerController::HandleLevelUp);
	}

	bShowMouseCursor = true;
	SetUIInputMode(false);
}

void APRPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APRGameState* GameState = GetPRGameState();
	if (!GameState)
	{
		return;
	}

	// 阶段切换驱动 UI
	if (GameState->Phase != LastPhase)
	{
		LastPhase = GameState->Phase;

		switch (GameState->Phase)
		{
		case EPRGamePhase::Shop:
		{
			PauseGame(true);
			SetUIInputMode(true);

			if (!ShopWidget)
			{
				ShopWidget = CreateWidget<UPRShopWidget>(this, UPRShopWidget::StaticClass());
			}
			if (ShopWidget)
			{
				ShopWidget->Refresh();
				ShopWidget->AddToViewport(10);
			}
			break;
		}
		case EPRGamePhase::Wave:
		{
			if (ShopWidget)
			{
				ShopWidget->RemoveFromParent();
			}
			PauseGame(false);
			SetUIInputMode(false);
			break;
		}
		case EPRGamePhase::GameOver:
		{
			PauseGame(true);
			SetUIInputMode(true);
			if (!GameOverWidget)
			{
				GameOverWidget = CreateWidget<UPRGameOverWidget>(this, UPRGameOverWidget::StaticClass());
			}
			if (GameOverWidget)
			{
				GameOverWidget->Refresh();
				GameOverWidget->AddToViewport(30);
			}
			break;
		}
		default:
			break;
		}
	}
}

APRPlayerState* APRPlayerController::GetPRPlayerState() const
{
	return Cast<APRPlayerState>(PlayerState);
}

APRGameState* APRPlayerController::GetPRGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<APRGameState>() : nullptr;
}

APRGameMode* APRPlayerController::GetPRGameMode() const
{
	return GetWorld() ? Cast<APRGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
}

void APRPlayerController::PauseGame(bool bPause)
{
	UGameplayStatics::SetGamePaused(this, bPause);
}

void APRPlayerController::SetUIInputMode(bool bUI)
{
	if (bUI)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
		bShowMouseCursor = true;
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
	}
}

// ---------------- 商店 ----------------

bool APRPlayerController::TryBuyOffer(int32 OfferIndex)
{
	APRGameState* GameState = GetPRGameState();
	APRPlayerState* PS = GetPRPlayerState();
	APRPlayerCharacter* Player = GameState ? GameState->GetPlayerCharacter() : nullptr;

	if (!GameState || !PS || !Player || !GameState->ShopOffers.IsValidIndex(OfferIndex))
	{
		return false;
	}

	FPRShopOffer& Offer = GameState->ShopOffers[OfferIndex];
	if (Offer.bSold || Offer.IsEmpty() || !PS->SpendGold(Offer.Price))
	{
		return false;
	}

	if (Offer.Item)
	{
		// 应用物品属性
		TArray<FPRStatMod> Mods = Offer.Item->StatMods;
		PRGameplayEffects::ApplyStatMods(Player->GetPRAbilitySystemComponent(), Mods);
		PS->AddOwnedItem(Offer.Item);
	}
	else if (Offer.Weapon)
	{
		if (!Player->GetWeaponComponent() || !Player->GetWeaponComponent()->AddWeapon(Offer.Weapon))
		{
			// 武器栏已满/添加失败 -> 退款
			PS->AddGold(Offer.Price);
			return false;
		}
	}

	Offer.bSold = true;
	if (ShopWidget)
	{
		ShopWidget->Refresh();
	}
	return true;
}

void APRPlayerController::RerollShop()
{
	APRGameState* GameState = GetPRGameState();
	APRPlayerState* PS = GetPRPlayerState();
	if (!GameState || !PS)
	{
		return;
	}

	if (!PS->SpendGold(GameState->RerollCost))
	{
		return;
	}

	GameState->RerollCount++;
	GameState->RerollCost = 5 + GameState->RerollCount * 3;
	GameState->BuildShopOffers();

	if (ShopWidget)
	{
		ShopWidget->Refresh();
	}
}

void APRPlayerController::StartNextWave()
{
	if (APRGameMode* GameMode = GetPRGameMode())
	{
		if (ShopWidget)
		{
			ShopWidget->RemoveFromParent();
		}

		// 恢复游戏节奏后再开波（商店阶段是暂停的）
		PauseGame(false);
		SetUIInputMode(false);
		GameMode->StartNextWave();
	}
}

// ---------------- 升级 ----------------

void APRPlayerController::HandleLevelUp(int32 NewLevel)
{
	GenerateLevelUpChoices();
	bPendingLevelUp = true;

	// 升级时暂停（土豆兄弟行为）
	PauseGame(true);
	SetUIInputMode(true);

	if (!LevelUpWidget)
	{
		LevelUpWidget = CreateWidget<UPRLevelUpWidget>(this, UPRLevelUpWidget::StaticClass());
	}
	if (LevelUpWidget)
	{
		LevelUpWidget->Refresh();
		LevelUpWidget->AddToViewport(20);
	}
}

void APRPlayerController::GenerateLevelUpChoices()
{
	LevelUpChoices.Reset();

	// 4 个不重复属性 + 随机幅度
	const TArray<EPRStat> AllStats = {
		EPRStat::MaxHealth, EPRStat::HealthRegen, EPRStat::DamagePct,
		EPRStat::MeleeDamagePct, EPRStat::RangedDamagePct, EPRStat::AttackSpeedPct,
		EPRStat::CritChance, EPRStat::CritDamagePct, EPRStat::Range,
		EPRStat::Armor, EPRStat::Dodge, EPRStat::SpeedPct,
		EPRStat::Luck, EPRStat::Harvesting, EPRStat::EngineeringPct, EPRStat::PickupRange
	};

	TArray<EPRStat> Pool = AllStats;
	for (int32 i = 0; i < 4 && Pool.Num() > 0; ++i)
	{
		const int32 Index = FMath::RandRange(0, Pool.Num() - 1);
		const EPRStat Stat = Pool[Index];
		Pool.RemoveAt(Index);

		float Value = 5.f;
		switch (Stat)
		{
		case EPRStat::MaxHealth:		Value = FMath::FRandRange(5.f, 12.f); break;
		case EPRStat::HealthRegen:		Value = FMath::FRandRange(1.f, 3.f); break;
		case EPRStat::Range:			Value = FMath::FRandRange(25.f, 70.f); break;
		case EPRStat::Armor:			Value = FMath::FRandRange(2.f, 5.f); break;
		case EPRStat::Dodge:			Value = FMath::FRandRange(2.f, 5.f); break;
		case EPRStat::Luck:				Value = FMath::FRandRange(5.f, 12.f); break;
		case EPRStat::Harvesting:		Value = FMath::FRandRange(4.f, 10.f); break;
		case EPRStat::PickupRange:		Value = FMath::FRandRange(40.f, 120.f); break;
		default:						Value = FMath::FRandRange(4.f, 10.f); break;
		}

		LevelUpChoices.Add(FPRStatMod(Stat, FMath::RoundToFloat(Value)));
	}
}

void APRPlayerController::ChooseLevelUpStat(int32 OptionIndex)
{
	if (!LevelUpChoices.IsValidIndex(OptionIndex))
	{
		return;
	}

	if (APRPlayerCharacter* Player = Cast<APRPlayerCharacter>(GetPawn()))
	{
		PRGameplayEffects::ApplyStatMod(Player->GetPRAbilitySystemComponent(),
			LevelUpChoices[OptionIndex].Stat, LevelUpChoices[OptionIndex].Value);
	}

	LevelUpChoices.Reset();
	bPendingLevelUp = false;

	if (LevelUpWidget)
	{
		LevelUpWidget->RemoveFromParent();
	}

	// 只有非商店阶段才恢复游戏（商店阶段暂停由商店控制）
	APRGameState* GameState = GetPRGameState();
	if (!GameState || GameState->Phase != EPRGamePhase::Shop)
	{
		PauseGame(false);
		SetUIInputMode(false);
	}
}

void APRPlayerController::RestartRun()
{
	if (APRGameMode* GameMode = GetPRGameMode())
	{
		GameMode->RestartRun();
	}
}
