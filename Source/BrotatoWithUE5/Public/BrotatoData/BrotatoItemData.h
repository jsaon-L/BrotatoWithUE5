// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BrotatoItemParentData.h"
#include "BrotatoItemData.generated.h"

/**
 * 
 */
UCLASS()
class BROTATOWITHUE5_API UBrotatoItemData : public UBrotatoItemParentData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly) int32 MaxNb = -1;                // -1 无限 / 1 唯一
	// UPROPERTY(EditDefaultsOnly) TArray<FItemAppearance> ItemAppearances;
	// UPROPERTY(EditDefaultsOnly) TArray<FGameplayTag> Tags;       // wanted_tags 
};
