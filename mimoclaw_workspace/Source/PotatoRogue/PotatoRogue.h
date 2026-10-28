// Copyright (c) PotatoRogue. MIT Licensed.
// 模块主头文件

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPotatoRogue, Log, All);

class FPotatoRogueModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
