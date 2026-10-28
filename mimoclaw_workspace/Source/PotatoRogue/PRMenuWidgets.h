// Copyright (c) PotatoRogue. MIT Licensed.
// 升级选择界面 + 结算界面

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PRMenuWidgets.generated.h"

class STextBlock;
class SButton;

// ---------------- 升级四选一 ----------------
UCLASS()
class POTATOROGUE_API UPRLevelUpWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	void Refresh();

protected:
	static constexpr int32 OptionCount = 4;

	TArray<TSharedPtr<STextBlock>> OptionTexts;
	TArray<TSharedPtr<SButton>> OptionButtons;

	FReply OnOptionClicked(int32 Index);
};

// ---------------- 结算 ----------------
UCLASS()
class POTATOROGUE_API UPRGameOverWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	void Refresh();

protected:
	TSharedPtr<STextBlock> SummaryText;

	FReply OnRestartClicked();
};
