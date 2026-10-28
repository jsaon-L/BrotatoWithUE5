// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRHUDWidget.h"
#include "PRGameState.h"
#include "PRPlayerState.h"
#include "PRPlayerCharacter.h"
#include "PRAttributeSet.h"
#include "PRWeaponComponent.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SProgressBar.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UPRHUDWidget::RebuildWidget()
{
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 20);
	const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle("Regular", 15);

	return SNew(SOverlay)

		// 左上：波次 / 计时 / 金币
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(FMargin(20.f, 16.f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
			[
				SAssignNew(WaveText, STextBlock)
				.Font(TitleFont)
				.Text(NSLOCTEXT("PR", "HUD_Wave", "第 1 波"))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
			[
				SAssignNew(TimerText, STextBlock)
				.Font(BodyFont)
				.Text(NSLOCTEXT("PR", "HUD_Timer", "剩余 0s"))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
			[
				SAssignNew(GoldText, STextBlock)
				.Font(BodyFont)
				.ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.2f))
				.Text(NSLOCTEXT("PR", "HUD_Gold", "金币 0"))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
			[
				SAssignNew(WeaponText, STextBlock)
				.Font(BodyFont)
				.ColorAndOpacity(FLinearColor(0.8f, 0.9f, 1.f))
				.Text(FText::GetEmpty())
			]
		]

		// 底部：血条 + 经验条
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 24.f))
		[
			SNew(SBox)
			.WidthOverride(560.f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
				[
					SNew(SOverlay)

					+ SOverlay::Slot()
					[
						SAssignNew(HealthBar, SProgressBar)
						.Percent(1.f)
						.FillColorAndOpacity(FLinearColor(0.85f, 0.2f, 0.25f))
					]

					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SAssignNew(HealthText, STextBlock)
						.Font(BodyFont)
						.Text(NSLOCTEXT("PR", "HUD_Health", "100 / 100"))
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f)
				[
					SNew(SOverlay)

					+ SOverlay::Slot()
					[
						SAssignNew(XPBar, SProgressBar)
						.Percent(0.f)
						.FillColorAndOpacity(FLinearColor(0.25f, 0.6f, 1.f))
					]

					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SAssignNew(LevelText, STextBlock)
						.Font(BodyFont)
						.Text(NSLOCTEXT("PR", "HUD_Level", "Lv.1"))
					]
				]
			]
		];
}

void UPRHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateFromState();
}

void UPRHUDWidget::UpdateFromState()
{
	APRGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APRGameState>() : nullptr;
	if (!GameState)
	{
		return;
	}

	APRPlayerCharacter* Player = GameState->GetPlayerCharacter();
	APRPlayerState* PS = Player ? Player->GetPlayerState<APRPlayerState>() : nullptr;

	if (WaveText.IsValid())
	{
		WaveText->SetText(FText::FromString(FString::Printf(TEXT("第 %d 波"), GameState->WaveNumber)));
	}

	if (TimerText.IsValid())
	{
		if (GameState->Phase == EPRGamePhase::Wave)
		{
			TimerText->SetText(FText::FromString(FString::Printf(TEXT("剩余 %ds"), FMath::CeilToInt(GameState->WaveTimeRemaining))));
		}
		else
		{
			TimerText->SetText(NSLOCTEXT("PR", "HUD_Shop", "商店阶段"));
		}
	}

	if (GoldText.IsValid() && PS)
	{
		GoldText->SetText(FText::FromString(FString::Printf(TEXT("金币 %d"), PS->Gold)));
	}

	if (Player)
	{
		const float Health = Player->GetAttributeValue(UPRAttributeSet::GetHealthAttribute());
		const float MaxHealth = Player->GetAttributeValue(UPRAttributeSet::GetMaxHealthAttribute());
		if (HealthBar.IsValid())
		{
			HealthBar->SetPercent(MaxHealth > 0.f ? Health / MaxHealth : 0.f);
		}
		if (HealthText.IsValid())
		{
			HealthText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), Health, MaxHealth)));
		}

		if (WeaponText.IsValid() && Player->GetWeaponComponent())
		{
			const float DPS = Player->GetWeaponComponent()->GetEstimatedDPS();
			WeaponText->SetText(FText::FromString(FString::Printf(TEXT("武器 %d 把 · DPS %.0f"),
				Player->GetWeaponComponent()->GetWeaponCount(), DPS)));
		}
	}

	if (PS)
	{
		if (XPBar.IsValid())
		{
			XPBar->SetPercent(PS->ExperienceToNext > 0
				? (float)PS->Experience / (float)PS->ExperienceToNext : 0.f);
		}
		if (LevelText.IsValid())
		{
			LevelText->SetText(FText::FromString(FString::Printf(TEXT("Lv.%d"), PS->Level)));
		}
	}
}
