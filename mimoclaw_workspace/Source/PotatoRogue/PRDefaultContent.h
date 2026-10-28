// Copyright (c) PotatoRogue. MIT Licensed.
// 运行时生成的默认内容：物品 / 武器 / 敌人
// 这样整个游戏零 Content 资源也能跑通完整玩法闭环

#pragma once

#include "CoreMinimal.h"

class UPRItemDefinition;
class UPRWeaponDefinition;
class UPREnemyDefinition;

namespace PRDefaultContent
{
	TArray<UPRItemDefinition*> CreateItems(UObject* Outer);
	TArray<UPRWeaponDefinition*> CreateWeapons(UObject* Outer);
	TArray<UPREnemyDefinition*> CreateEnemies(UObject* Outer);
}
