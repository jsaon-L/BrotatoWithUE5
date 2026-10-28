// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRMenuWidgets.h"
#include "PRPlayerController.h"
#include "PRPlayerState.h"
#include "PRGameState.h"
#include "PRGameTypes.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"

// ---------------- 升级 ----------------

TSharedRef<SWidget> UPRLevelUpWidget::RebuildWidget()
{
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 26);
	const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle("Regular", 16);

	OptionTexts.Reset();
	OptionButtons.Reset();

	TSharedRef<SVerticalBox> OptionsBox = SNew(SVerticalBox);

	for (int32 i = 0; i < OptionCount; ++i)
	{
		TSharedPtr<STextBlock> OptionText;
		TSharedPtr<SButton> OptionButton;

		OptionsBox->AddSlot()
		.AutoHeight()
		.Padding(0.f, 5.f)
		[
			SAssignNew(OptionButton, SButton)
			.OnClicked_Lambda([this, i]() { return OnOptionClicked(i); })
			[
				SAssignNew(OptionText, STextBlock)
				.Font(BodyFont)
				.Text(NSLOCTEXT("PR", "LevelUp_Option", "属性"))
			]
		];

		OptionTexts.Add(OptionText);
		OptionButtons.Add(OptionButton);
	}

	return SNew(SOverlay)

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(480.f)
			[
				SNew(SBorder)
				.Padding(FMargin(20.f))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
					[
						SNew(STextBlock)
						.Font(TitleFont)
						.Text(NSLOCTEXT("PR", "LevelUp_Title", "升级！选择一项强化"))
					]

					+ SVerticalBox::Slot().AutoHeight()
					[
						OptionsBox
					]
				]
			]
		];
}

void UPRLevelUpWidget::Refresh()
{
	APRPlayerController* PC = Cast<APRPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		return;
	}

	const TArray<FPRStatMod>& Choices = PC->GetLevelUpChoices();
	for (int32 i = 0; i < OptionCount; ++i)
	{
		if (OptionTexts.IsValidIndex(i) && OptionTexts[i].IsValid())
		{
			OptionTexts[i]->SetText(Choices.IsValidIndex(i)
				? PRGameTypes::StatModToText(Choices[i])
				: NSLOCTEXT("PR", "LevelUp_Empty", "—"));
		}
	}
}

FReply UPRLevelUpWidget::OnOptionClicked(int32 Index)
{
	if (APRPlayerController* PC = Cast<APRPlayerController>(GetOwningPlayer()))
	{
		PC->ChooseLevelUpStat(Index);
	}
	return FReply::Handled();
}

// ---------------- 结算 ----------------

TSharedRef<SWidget> UPRGameOverWidget::RebuildWidget()
{
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 32);
	const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle("Regular", 18);

	return SNew(SOverlay)

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(460.f)
			[
				SNew(SBorder)
				.Padding(FMargin(24.f))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
					[
						SNew(STextBlock)
						.Font(TitleFont)
						.ColorAndOpacity(FLinearColor(1.f, 0.35f, 0.35f))
						.Text(NSLOCTEXT("PR", "GameOver_Title", "你倒下了"))
					]

					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f)
					[
						SAssignNew(SummaryText, STextBlock)
						.Font(BodyFont)
						.Text(FText::GetEmpty())
					]

					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SButton)
						.OnClicked_Lambda([this]() { return OnRestartClicked(); })
						[
							SNew(STextBlock)
							.Font(BodyFont)
							.Text(NSLOCTEXT("PR", "GameOver_Restart", "再来一局"))
						]
					]
				]
			]
		];
}

void UPRGameOverWidget::Refresh()
{
	APRGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APRGameState>() : nullptr;
	APRPlayerController* PC = Cast<APRPlayerController>(GetOwningPlayer());
	APRPlayerState* PS = PC ? PC->GetPlayerState<APRPlayerState>() : nullptr;

	if (SummaryText.IsValid() && GameState && PS)
	{
		SummaryText->SetText(FText::FromString(FString::Printf(
			TEXT("存活到第 %d 波\n等级 Lv.%d\n金币 %d"),
			GameState->WaveNumber, PS->Level, PS->Gold)));
	}
}

FReply UPRGameOverWidget::OnRestartClicked()
{
	if (APRPlayerController* PC = Cast<APRPlayerController>(GetOwningPlayer()))
	{
		PC->RestartRun();
	}
	return FReply::Handled();
}
