// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BrotatoItemData.h"
#include "BrotatoCore/Type/BRT_CharacterTypes.h"
#include "BrotatoCharacterData.generated.h"

struct FGameplayTag;
/**
 * 
 */
UCLASS(BlueprintType)
class BROTATOWITHUE5_API UBrotatoCharacterData : public UBrotatoItemData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly) TArray<FGameplayTag> WantedTags;
	// UPROPERTY(EditDefaultsOnly) TArray<TSoftObjectPtr<UBrotatoWeaponData>> StartingWeapons;
	
	UPROPERTY(EditDefaultsOnly) TArray<FItemAppearance> ItemAppearances;

public:
	/** 角色自带的外观列表（眼睛/嘴巴等），按 Depth 升序即叠加顺序 */
	const TArray<FItemAppearance>& GetItemAppearances() const { return ItemAppearances; }
};
