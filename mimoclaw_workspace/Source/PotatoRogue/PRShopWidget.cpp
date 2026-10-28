// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRShopWidget.h"
#include "PRGameState.h"
#include "PRPlayerState.h"
#include "PRPlayerController.h"
#include "PRDataAssets.h"
#include "PRGameTypes.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UPRShopWidget::RebuildWidget()
{
	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 24);
	const FSlateFontInfo NameFont = FCoreStyle::GetDefaultFontStyle("Bold", 16);
	const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle("Regular", 13);

	OfferNameTexts.Reset();
	OfferDescTexts.Reset();
	OfferPriceTexts.Reset();
	BuyButtons.Reset();

	TSharedRef<SVerticalBox> OfferBox = SNew(SVerticalBox);

	for (int32 i = 0; i < OfferCount; ++i)
	{
		TSharedPtr<STextBlock> NameText, DescText, PriceText;
		TSharedPtr<SButton> BuyButton;

		OfferBox->AddSlot()
		.AutoHeight()
		.Padding(0.f, 6.f)
		[
			SNew(SBorder)
			.Padding(FMargin(12.f, 8.f))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight()
					[
						SAssignNew(NameText, STextBlock)
						.Font(NameFont)
						.Text(NSLOCTEXT("PR", "Shop_Empty", "—"))
					]

					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 3.f, 0.f, 0.f)
					[
						SAssignNew(DescText, STextBlock)
						.Font(BodyFont)
						.Text(FText::GetEmpty())
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(12.f, 0.f)
				[
					SAssignNew(PriceText, STextBlock)
					.Font(NameFont)
					.ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.2f))
					.Text(FText::GetEmpty())
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(BuyButton, SButton)
					.OnClicked_Lambda([this, i]() { return OnBuyClicked(i); })
					[
						SNew(STextBlock)
						.Font(BodyFont)
						.Text(NSLOCTEXT("PR", "Shop_Buy", "购买"))
					]
				]
			]
		];

		OfferNameTexts.Add(NameText);
		OfferDescTexts.Add(DescText);
		OfferPriceTexts.Add(PriceText);
		BuyButtons.Add(BuyButton);
	}

	return SNew(SOverlay)

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(640.f)
			[
				SNew(SVerticalBox)

				// 标题栏
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SNew(STextBlock)
						.Font(TitleFont)
						.Text(NSLOCTEXT("PR", "Shop_Title", "商店"))
					]

					+ SHorizontalBox::Slot().AutoWidth().Padding(8.f, 0.f)
					[
						SAssignNew(GoldText, STextBlock)
						.Font(TitleFont)
						.ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.2f))
					]
				]

				// 报价列表
				+ SVerticalBox::Slot().AutoHeight()
				[
					OfferBox
				]

				// 底部按钮
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 8.f, 0.f)
					[
						SAssignNew(RerollButton, SButton)
						.OnClicked_Lambda([this]() { return OnRerollClicked(); })
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(STextBlock).Font(NameFont)
								.Text(NSLOCTEXT("PR", "Shop_Reroll", "重随 "))
							]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SAssignNew(RerollCostText, STextBlock)
								.Font(NameFont)
								.ColorAndOpacity(FLinearColor(1.f, 0.85f, 0.2f))
							]
						]
					]

					+ SHorizontalBox::Slot().FillWidth(1.f)
					[
						SAssignNew(NextWaveButton, SButton)
						.OnClicked_Lambda([this]() { return OnNextWaveClicked(); })
						[
							SNew(STextBlock)
							.Font(NameFont)
							.Text(NSLOCTEXT("PR", "Shop_Next", "开始下一波 →"))
						]
					]
				]
			]
		];
}

void UPRShopWidget::Refresh()
{
	APRGameState* GameState = GetWorld() ? GetWorld()->GetGameState<APRGameState>() : nullptr;
	APRPlayerController* PC = Cast<APRPlayerController>(GetOwningPlayer());
	APRPlayerState* PS = PC ? PC->GetPlayerState<APRPlayerState>() : nullptr;

	if (!GameState)
	{
		return;
	}

	if (GoldText.IsValid() && PS)
	{
		GoldText->SetText(FText::FromString(FString::Printf(TEXT("💰 %d"), PS->Gold)));
	}

	if (RerollCostText.IsValid())
	{
		RerollCostText->SetText(FText::FromString(FString::Printf(TEXT("(%d)"), GameState->RerollCost)));
	}

	for (int32 i = 0; i < OfferCount; ++i)
	{
		const bool bValid = GameState->ShopOffers.IsValidIndex(i);
		const FPRShopOffer Offer = bValid ? GameState->ShopOffers[i] : FPRShopOffer();

		if (OfferNameTexts.IsValidIndex(i) && OfferNameTexts[i].IsValid())
		{
			if (!bValid || Offer.IsEmpty())
			{
				OfferNameTexts[i]->SetText(NSLOCTEXT("PR", "Shop_Empty", "—"));
				OfferNameTexts[i]->SetColorAndOpacity(FSlateColor::UseForeground());
			}
			else if (Offer.bSold)
			{
				OfferNameTexts[i]->SetText(FText::FromString(FString::Printf(TEXT("%s（已售出）"),
					*Offer.GetDisplayName().ToString())));
				OfferNameTexts[i]->SetColorAndOpacity(FSlateColor(FLinearColor::Gray));
			}
			else
			{
				const EPRRarity Rarity = Offer.Item ? Offer.Item->Rarity
					: (Offer.Weapon ? Offer.Weapon->Rarity : EPRRarity::Common);
				OfferNameTexts[i]->SetText(Offer.GetDisplayName());
				OfferNameTexts[i]->SetColorAndOpacity(FSlateColor(PRGameTypes::RarityToColor(Rarity)));
			}
		}

		if (OfferDescTexts.IsValidIndex(i) && OfferDescTexts[i].IsValid())
		{
			FString Desc = Offer.GetDescription().ToString();
			if (Offer.Item)
			{
				for (const FPRStatMod& Mod : Offer.Item->StatMods)
				{
					Desc += FString::Printf(TEXT("  [%s]"), *PRGameTypes::StatModToText(Mod).ToString());
				}
			}
			else if (Offer.Weapon)
			{
				Desc += FString::Printf(TEXT("  [伤害 %.0f / 间隔 %.2fs]"),
					Offer.Weapon->Damage, Offer.Weapon->Cooldown);
			}
			OfferDescTexts[i]->SetText(FText::FromString(Desc));
		}

		if (OfferPriceTexts.IsValidIndex(i) && OfferPriceTexts[i].IsValid())
		{
			OfferPriceTexts[i]->SetText(bValid && !Offer.IsEmpty() && !Offer.bSold
				? FText::FromString(FString::Printf(TEXT("%d"), Offer.Price))
				: FText::GetEmpty());
		}
	}
}

FReply UPRShopWidget::OnBuyClicked(int32 Index)
{
	if (APRPlayerController* PC = Cast<APRPlayerController>(GetOwningPlayer()))
	{
		PC->TryBuyOffer(Index);
	}
	return FReply::Handled();
}

FReply UPRShopWidget::OnRerollClicked()
{
	if (APRPlayerController* PC = Cast<APRPlayerController>(GetOwningPlayer()))
	{
		PC->RerollShop();
	}
	return FReply::Handled();
}

FReply UPRShopWidget::OnNextWaveClicked()
{
	if (APRPlayerController* PC = Cast<APRPlayerController>(GetOwningPlayer()))
	{
		PC->StartNextWave();
	}
	return FReply::Handled();
}
