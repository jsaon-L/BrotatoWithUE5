// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BRT_Types.generated.h"


/**
 * 物品类型枚举
 */
UENUM(BlueprintType)
enum class EBrotatoTier : uint8
{
	Common,
	Uncommon,
	Rare,
	Legendary
};