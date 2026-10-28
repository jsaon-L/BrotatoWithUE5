// Copyright (c) PotatoRogue. MIT Licensed.

#include "PREnemyHealthBar.h"
#include "Widgets/SProgressBar.h"

TSharedRef<SWidget> UPREnemyHealthBar::RebuildWidget()
{
	HealthBar = SNew(SProgressBar)
		.Percent(1.f)
		.FillColorAndOpacity(FLinearColor(0.9f, 0.2f, 0.2f))
		.BorderPadding(FVector2D::ZeroVector);

	return SNew(SBox)
		.WidthOverride(80.f)
		.HeightOverride(10.f)
		[
			HealthBar.ToSharedRef()
		];
}

void UPREnemyHealthBar::SetHealthPercent(float Percent)
{
	if (HealthBar.IsValid())
	{
		HealthBar->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));
	}
}
