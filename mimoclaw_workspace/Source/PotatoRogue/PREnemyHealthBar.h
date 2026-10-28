// Copyright (c) PotatoRogue. MIT Licensed.
// 敌人头顶血条（WidgetComponent 内容）

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PREnemyHealthBar.generated.h"

class SProgressBar;

UCLASS()
class POTATOROGUE_API UPREnemyHealthBar : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void SetHealthPercent(float Percent);

protected:
	TSharedPtr<SProgressBar> HealthBar;
};
