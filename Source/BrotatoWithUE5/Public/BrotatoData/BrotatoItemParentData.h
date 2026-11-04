// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BrotatoCore/Type/BRT_Types.h"
#include "Engine/DataAsset.h"
#include "BrotatoItemParentData.generated.h"

class UPaperFlipbook;
/**
 * 
 */
UCLASS()
class BROTATOWITHUE5_API UBrotatoItemParentData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	UPROPERTY(EditDefaultsOnly) FName MyId;
	UPROPERTY(EditDefaultsOnly) bool  bUnlockedByDefault = false;
	UPROPERTY(EditDefaultsOnly) TSoftObjectPtr<UPaperFlipbook> Icon;
	UPROPERTY(EditDefaultsOnly) FName NameKey;                   // 本地化键
	UPROPERTY(EditDefaultsOnly) EBrotatoTier Tier = EBrotatoTier::Common;
	UPROPERTY(EditDefaultsOnly) int32 Value = 1;
	// UPROPERTY(EditDefaultsOnly) TArray<FBrotatoEffectDef> Effects;
	// UPROPERTY(EditDefaultsOnly) FName TrackingTextKey;
	
};
