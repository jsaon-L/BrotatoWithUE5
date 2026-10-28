// Copyright (c) PotatoRogue. MIT Licensed.
// 主 HUD：血量/经验条、波次信息、金币

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PRHUDWidget.generated.h"

class STextBlock;
class SProgressBar;

UCLASS()
class POTATOROGUE_API UPRHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	TSharedPtr<STextBlock> WaveText;
	TSharedPtr<STextBlock> TimerText;
	TSharedPtr<STextBlock> GoldText;
	TSharedPtr<STextBlock> LevelText;
	TSharedPtr<STextBlock> HealthText;
	TSharedPtr<STextBlock> WeaponText;

	TSharedPtr<SProgressBar> HealthBar;
	TSharedPtr<SProgressBar> XPBar;

	void UpdateFromState();
};
