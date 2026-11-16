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
UCLASS()
class BROTATOWITHUE5_API UBrotatoCharacterData : public UBrotatoItemData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly) TArray<FGameplayTag> WantedTags;
	// UPROPERTY(EditDefaultsOnly) TArray<TSoftObjectPtr<UBrotatoWeaponData>> StartingWeapons;
	
	UPROPERTY(EditDefaultsOnly) TArray<FItemAppearance> ItemAppearances;
};
