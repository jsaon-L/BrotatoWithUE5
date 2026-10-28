// Copyright (c) PotatoRogue. MIT Licensed.
// 商店界面：4 个报价格 + 重随 + 开始下一波

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PRShopWidget.generated.h"

class STextBlock;
class SButton;

UCLASS()
class POTATOROGUE_API UPRShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** 刷新显示（购买/重随后调用） */
	void Refresh();

protected:
	static constexpr int32 OfferCount = 4;

	TArray<TSharedPtr<STextBlock>> OfferNameTexts;
	TArray<TSharedPtr<STextBlock>> OfferDescTexts;
	TArray<TSharedPtr<STextBlock>> OfferPriceTexts;
	TArray<TSharedPtr<SButton>> BuyButtons;

	TSharedPtr<STextBlock> GoldText;
	TSharedPtr<STextBlock> RerollCostText;
	TSharedPtr<SButton> RerollButton;
	TSharedPtr<SButton> NextWaveButton;

	FReply OnBuyClicked(int32 Index);
	FReply OnRerollClicked();
	FReply OnNextWaveClicked();
};
