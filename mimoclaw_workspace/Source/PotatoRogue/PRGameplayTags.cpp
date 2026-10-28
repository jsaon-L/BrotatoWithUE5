// Copyright (c) PotatoRogue. MIT Licensed.

#include "PRGameplayTags.h"

namespace PRGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Weapon_Fire, "Ability.Weapon.Fire", "远程武器开火技能");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Weapon_Melee, "Ability.Weapon.Melee", "近战武器挥击技能");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Dash, "Ability.Dash", "冲刺技能");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "死亡状态");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dashing, "State.Dashing", "冲刺中（无敌帧）");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invulnerable, "State.Invulnerable", "无敌");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Dash, "Cooldown.Dash", "冲刺冷却");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Data.Damage", "伤害 SetByCaller 通道");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Heal, "Data.Heal", "治疗 SetByCaller 通道");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Knockback, "Data.Knockback", "击退强度");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Hit, "Event.Hit", "命中事件");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Died, "Event.Died", "死亡事件");
}
