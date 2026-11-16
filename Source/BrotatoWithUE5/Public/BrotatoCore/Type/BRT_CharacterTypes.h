// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BRT_CharacterTypes.generated.h"

class UPaperSprite;

/**
 * 物品挂载位置
 * 物品都是放到character的中心点的,具体位置是图片自己内容控制的
 */
UENUM(BlueprintType)
enum class EPosition : uint8
{
	OTHER, 
	ABOVE_HEAD, 
	TOP_LEFT, 
	TOP_RIGHT, 
	HAT, 
	FOREHEAD, 
	EYES, 
	NOSE, 
	MOUTH, 
	BACK, 
	NECK, 
	TORSO, 
	POCKET_LEFT, 
	POCKET_RIGHT, 
	SKIN, 
	TAIL, 
	ACCESSORY_LEFT, 
	ACCESSORY_RIGHT
};

/**
 * 物品的同位置挂载优先级,同位置只显示一张图,具体优先级由此字段控制
 */
UENUM(BlueprintType)
enum class EPriority : uint8 
{
	VERY_LOW, 
	LOW, 
	MEDIUM, 
	HIGH, 
	VERY_HIGH
};

/**
 * 物品外观
 * 描述物品挂到角色身上时使用的图、挂载位置、同位置优先级与渲染深度
 */
USTRUCT(BlueprintType)
struct FItemAppearance
{
	GENERATED_BODY()

	/** 挂载使用的精灵图（具体内容由图片自身控制） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Item")
	TObjectPtr<UPaperSprite> Sprite = nullptr;

	/** 挂载位置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Item")
	EPosition Position = EPosition::OTHER;

	/** 同位置挂载优先级（同位置只显示一张图） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Item")
	EPriority Priority = EPriority::MEDIUM;

	/** 同位置的渲染深度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BRT|Item")
	int32 Depth = 0;
};