// Copyright (c) PotatoRogue. MIT Licensed.
// 全局 Gameplay Tag 定义（原生 Tag，编译期注册）

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace PRGameplayTags
{
	// 技能
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Weapon_Fire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Weapon_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Dash);

	// 状态
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dashing);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);

	// 冷却
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Dash);

	// 数据通道（SetByCaller）
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Heal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Knockback);

	// 事件
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Died);
}
